
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <time.h>
#include "packet.h"
#include "helper.h"

int main(int argc, char *argv[])
{
	int sockfd;
	struct addrinfo rawInfo, *servinfo, *pointer;
	int statusValue;
	int numberOfBytes;
	char *serverIPAddr, *portnumber, *filename;
	float ProbLoss, ProbCorruption;

	if (argc != 6) {
		fprintf(stderr,"6 parameters as follow: client serverIPAddr portnumber filename probLoss probCorr\n");
		exit(1);
	}
	ProbCorruption = atof(argv[1]);
	ProbLoss = atof(argv[2]);
	serverIPAddr = argv[3];
	portnumber = argv[4];
	filename = argv[5];
	checkCorrLossValidity(ProbLoss, ProbCorruption);

	memset(&rawInfo, 0, sizeof(rawInfo));
	rawInfo.ai_family = AF_UNSPEC;
	rawInfo.ai_socktype = SOCK_DGRAM;

	if ((statusValue = getaddrinfo(serverIPAddr, portnumber, &rawInfo, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(statusValue));
		return 1;
	}

	for(pointer = servinfo; pointer != NULL; pointer = pointer->ai_next) {
		if ((sockfd = socket(pointer->ai_family, pointer->ai_socktype,
				pointer->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

		break;
	}

	if (pointer == NULL) {
		fprintf(stderr, "client: failed to create socket\n");
		return 2;
	}

	struct Packet reqPacket;
	reqPacket.seqNum = 0;
	reqPacket.type = REQ;
	reqPacket.dataSize = strlen(filename);
	strcpy(reqPacket.data, filename);

	if ((numberOfBytes = sendto(sockfd, &reqPacket, sizeof(reqPacket), 0,
			 pointer->ai_addr, pointer->ai_addrlen)) == -1) {
		perror("client: sendto");
		exit(1);
	}

	printf("sent SYC message, %d bytes to %s\n", numberOfBytes, serverIPAddr);
	printf("message is %d bytes: \"%s\"\n", reqPacket.dataSize, reqPacket.data);

	
	
	FILE *file = fopen("receivedFile", "wb");

	if (file == NULL)
	{
		printf("Error creating file\n");
		exit(1);
	}

	int expectedSeqNum = 1;
	struct Packet dataPacket, ackPacket;
	ackPacket.seqNum = 0;
	ackPacket.type = ACK;
	ackPacket.data[0] = '\0';
	ackPacket.dataSize = 0;
	

	while(1)
	{   
		if ((numberOfBytes = recvfrom(sockfd, &dataPacket, sizeof(dataPacket) , 0,
			pointer->ai_addr, &pointer->ai_addrlen)) == -1) {
			perror("recvfrom");
			exit(1);
		}	

		if(dataPacket.type == FIN)
		{
			printf("received FIN from %s\n", serverIPAddr);
			printf("FIN message is \"%s\"\n", dataPacket.data);
			break;
		}

		float loss = (float)(rand() % 100) / 100;
		float corrupt = (float)(rand() % 100) / 100;
		if(ProbLoss > 0 && loss < ProbLoss)
		{
			printf("packet SEQ: %d received, discarded because of loss\n", dataPacket.seqNum);
		}
		else
		{
			if(ProbCorruption > 0 && corrupt < ProbCorruption)
			{
				printf("packet SEQ: %d received, discarded because of corruption\n", dataPacket.seqNum);
				printf("sent repeated ACK: %d\n", ackPacket.seqNum);
			}
			else
			{
				if(dataPacket.seqNum != expectedSeqNum)
				{
					printf("received packet out of order SEQ: %d\n", dataPacket.seqNum);
					printf("sent repeated ACK to %s, ACK: %d\n", serverIPAddr, ackPacket.seqNum);
				}
				else
				{	
					printf("received %d bytes from %s, SEQ: %d\n", dataPacket.dataSize, serverIPAddr, dataPacket.seqNum);
					fwrite(dataPacket.data, sizeof(char), dataPacket.dataSize, file);
					expectedSeqNum++;
					ackPacket.seqNum = expectedSeqNum;
					printf("sent new ACK: %d\n", ackPacket.seqNum);
				}
			}
			if ((numberOfBytes = sendto(sockfd, &ackPacket, sizeof(ackPacket), 0,
						 pointer->ai_addr, pointer->ai_addrlen)) == -1) {
					perror("client: sendto");
					exit(1);
			}
		}
	}

	struct Packet finPacket;
	finPacket.seqNum = expectedSeqNum;
	finPacket.type = FIN;
	strcpy(finPacket.data, "file receiving finished.\n");
	finPacket.dataSize = strlen(finPacket.data);

	if ((numberOfBytes = sendto(sockfd, &finPacket, sizeof(finPacket), 0,
						 pointer->ai_addr, pointer->ai_addrlen)) == -1) {
					perror("client: sendto");
					exit(1);
	}
	printf("sent packet with type FIN\n");
	printf("client shutting down connection\n");
	freeaddrinfo(servinfo);
	fclose(file);
	close(sockfd);
	return 0;
}
