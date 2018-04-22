#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <sys/stat.h>
#include <time.h>
#include <math.h> 
#include "helper.h"
#include "packet.h"

void fillInDataPacket(struct Packet *dataPacket, int nextSeqNum, FILE *file, int packetCount, int fileSize)
{
	dataPacket->seqNum = nextSeqNum;
	dataPacket->type = DATA;
	if (nextSeqNum == packetCount) 
	{
		dataPacket -> dataSize = fileSize % DATASIZE;
	}
	else 
	{
		dataPacket -> dataSize = DATASIZE;
	}
	fseek(file, (nextSeqNum - 1) * DATASIZE, SEEK_SET);
	size_t newLen = fread(dataPacket->data, sizeof(char), dataPacket->dataSize, file);
	if (newLen != 0) 
	{
		dataPacket->data[newLen] = '\0'; 	
	}
	else {
		fputs("Can't read file", stderr);
		exit(0);
	}	

}

void sendFin(char* meg, char* fileName, struct sockaddr_storage addrOtherSide, socklen_t address_len, int sockfd)
{
	printf("%s %s\n", meg, fileName);
	perror("stat");
	struct Packet finPacket;
	finPacket.seqNum = 0;
	finPacket.type = FIN;
	strcpy(finPacket.data, meg);
	finPacket.dataSize = strlen(finPacket.data);
	int numberOfBytes;
	if ((numberOfBytes = sendto(sockfd, &finPacket, sizeof(finPacket), 0,
		(struct sockaddr *)&addrOtherSide, address_len)) == -1) {
			perror("server: send to client");
			exit(1);
		}
	printf(" sent FIN message to client\n");
	exit(0);
}

int sendPacketToClient(struct Packet reqPacket, struct sockaddr_storage addrOtherSide, socklen_t address_len, int sockfd, 
						int CWND, float ProbCorruption, float ProbLoss)
{
	int numberOfBytes;
	char *fileName = reqPacket.data;
	struct stat st;	
	struct Packet dataPacket, ackPacket;

	if (stat(fileName, &st) == -1) 
	{
		sendFin("File not found", fileName, addrOtherSide, address_len, sockfd);
   	}

	FILE *file = fopen(fileName, "rb");
	
	if(file == NULL)
	{
		sendFin("can not open the file", fileName, addrOtherSide, address_len, sockfd);
	}
	int currentSeq = 1;
	int inFlight = 0;
	int nextSeqNum = 1;

	fd_set readSet;	

	
	int packetCount = (int)ceil((float)st.st_size / DATASIZE);
	srand(time(NULL));
	while(currentSeq <= packetCount)
	{
		int i, tempSeqNum = nextSeqNum;

		for(i = 0; i < currentSeq + CWND - tempSeqNum && nextSeqNum <= packetCount; i++)
		{
			fillInDataPacket(&dataPacket, nextSeqNum, file, packetCount, st.st_size);
			if ((numberOfBytes = sendto(sockfd, &dataPacket, sizeof(dataPacket), 0,
						 (struct sockaddr *)&addrOtherSide, address_len)) == -1) {
					perror("server: sendto");
					exit(1);
			}

			printf(" sent %d bytes, SEQ: %d, WINSIZE: %d, \n", dataPacket.dataSize, dataPacket.seqNum, CWND * 1024);

			inFlight++;
			nextSeqNum++;
		}

		// start waiting for FIN message
		FD_ZERO(&readSet);
		FD_SET(sockfd, &readSet);
		// timeout after one second
		struct timeval timeout = {1, 0}; 
		int res = select(sockfd + 1, &readSet, NULL, NULL, &timeout);
		if(res == 0)
		{
			printf(" Time out for ACK: %d\n", currentSeq + 1);
			nextSeqNum = currentSeq;
			
		}
		else if (res < 0)
		{
			perror("error select");
		}
		else
		{
			if ((numberOfBytes = recvfrom(sockfd, &ackPacket, sizeof(ackPacket) , 0,
				(struct sockaddr *)&addrOtherSide, &address_len)) == -1) {
				perror("recvfrom");
				exit(1);
			}

			float loss = (float)(rand() % 100) / 100;
			float corrupt = (float)(rand() % 100) / 100;
			if(ProbLoss > 0 && loss < ProbLoss)
			{
				printf("ACK: %d received, discarded because of loss, currentSeq %d\n", ackPacket.seqNum, currentSeq);
			}
			else
			{
				if(ProbCorruption > 0 && corrupt < ProbCorruption)
					printf(" ACK: %d received, discarded because of corruption, currentSeq %d\n", ackPacket.seqNum, currentSeq);
				else
				{
					printf("received ACK: %d, currentSeq ", ackPacket.seqNum);
					if(ackPacket.seqNum > currentSeq)
					{	
						currentSeq = ackPacket.seqNum;
					}
					else
					{
						printf("sequence number %d unchanged\n", currentSeq);
					}
				}
			}
		}
	}
	printf("Complete File transfer.\n");
	fclose(file);
	return nextSeqNum;
}

void sendFinAfterFile(int nextSeqNum, struct sockaddr_storage addrOtherSide, socklen_t address_len, int sockfd)
{
	struct Packet ackPacket, finPacket;
	int numberOfBytes;
	finPacket.seqNum = nextSeqNum;
	finPacket.type = FIN;
	strcpy(finPacket.data, "File sending finished.\n");
	finPacket.dataSize = strlen(finPacket.data);
	
	if ((numberOfBytes = sendto(sockfd, &finPacket, sizeof(finPacket), 0,
		(struct sockaddr *)&addrOtherSide, address_len)) == -1) {
			perror("server: sendto");
			exit(1);
		}

	while(1)
	{
		if ((numberOfBytes = recvfrom(sockfd, &ackPacket, sizeof(ackPacket) , 0,
			(struct sockaddr *)&addrOtherSide, &address_len)) == -1) 
		{
				perror("recvfrom");
				exit(1);
		}
		if(ackPacket.type != FIN)
		{
			printf("received packe with type other than FIN\n");

		}
		else
		{
			printf("received packet with type FIN from client\n");
			printf("FIN message content is \"%s\"\n", ackPacket.data);
			printf("server shutting down connection\n");
			break;			
		}
	}
	
	close(sockfd);
}

int main(int argc, char *argv[])
{
	int sockfd;
	struct addrinfo rawInfo, *servinfo, *bindinfo;
	int statusValue;
	int numberOfBytes;
	struct sockaddr_storage addrOtherSide;
	socklen_t address_len;
	char *portNumber;
	
	int CWND;
	float ProbLoss, ProbCorruption;

	if (argc != 5) {
		fprintf(stderr,"5 parameters as follow: server portNumber windowSize probLoss probCorr\n");
		exit(1);
	}
	portNumber = argv[3];
	CWND = atoi(argv[4]);
	ProbLoss = atof(argv[1]);
	ProbCorruption = atof(argv[2]);
	checkValidity(CWND, ProbLoss, ProbCorruption);


	memset(&rawInfo, 0, sizeof rawInfo);
	rawInfo.ai_family = AF_UNSPEC; // not specify IPV4 or IPV6
	rawInfo.ai_socktype = SOCK_DGRAM;
	rawInfo.ai_flags = AI_PASSIVE; 

	if ((statusValue = getaddrinfo(NULL, portNumber, &rawInfo, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(statusValue));
		return 1;
	}

	// bind the first 
	for(bindinfo = servinfo; bindinfo != NULL; bindinfo = bindinfo->ai_next) {
		if ((sockfd = socket(bindinfo->ai_family, bindinfo->ai_socktype, bindinfo->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		if (bind(sockfd, bindinfo->ai_addr, bindinfo->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}

	if (bindinfo == NULL) {
		fprintf(stderr, "failed to bind socket on server side\n");
		return 2;
	}

	freeaddrinfo(servinfo);

	printf("server: waiting for client to request ......\n");

	struct Packet reqPacket;
	address_len = sizeof addrOtherSide;
	if ((numberOfBytes = recvfrom(sockfd, &reqPacket, sizeof(reqPacket) , 0,
		(struct sockaddr *)&addrOtherSide, &address_len)) == -1) {
		perror("recvfrom on server\n");
		exit(1);
	}
	
	printf("server: request content is \"%s\", %d bytes\n", 
		reqPacket.data, reqPacket.dataSize);

	int nextSeqNum = sendPacketToClient(reqPacket, addrOtherSide, address_len, sockfd, CWND, ProbCorruption, ProbLoss);
	sendFinAfterFile(nextSeqNum, addrOtherSide, address_len, sockfd);
	return 0;
}

