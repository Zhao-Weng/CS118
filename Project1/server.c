/* A simple server in the internet domain using TCP
   The port number is passed as an argument 
   This version runs forever, forking off a separate 
   process for each connection
*/
#include <stdio.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>




void processRequest(int);
char* readFileByBytes(const char *);

void error(char *msg)
{
    perror(msg);
    exit(1);
}

void sigchld_handler(int s)
{
    while(waitpid(-1, NULL, WNOHANG) > 0);
}


int main(int argc, char *argv[])
{
     int sockfd, newsockfd, portno, processId;
     socklen_t clilen;
     struct sockaddr_in serv_addr, cli_addr;
     clilen = sizeof(cli_addr);
     struct sigaction sigactionstruct;    

     if (argc < 2) 
     {
         fprintf(stderr,"ERROR, no port provided\n");
         exit(1);
     }
     sockfd = socket(AF_INET, SOCK_STREAM, 0);
     if (sockfd < 0) error("ERROR opening socket");
     bzero((char *) &serv_addr, sizeof(serv_addr));
     portno = atoi(argv[1]);
     serv_addr.sin_family = AF_INET;
     serv_addr.sin_addr.s_addr = INADDR_ANY;
     serv_addr.sin_port = htons(portno);
     
     if (bind(sockfd, (struct sockaddr *) &serv_addr,
              sizeof(serv_addr)) < 0) 
              error("ERROR on binding");
     
     listen(sockfd,5);
     
     
     sigactionstruct.sa_handler = sigchld_handler; // remove all zombine processes with sigaction
     sigemptyset(&sigactionstruct.sa_mask);
     sigactionstruct.sa_flags = SA_RESTART;
     if (sigaction(SIGCHLD, &sigactionstruct, NULL) == -1) {
         perror("sigaction");
         exit(1);
     }
     
     while (1) {
         newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
		 
         if (newsockfd < 0) 
         {
             error("ERROR on accept");
         }
         
         processId = fork(); // get a new process by using fork
         if (processId < 0)
         {
             error("ERROR on fork");
         }
         
         if (processId == 0)  
         {
			 processRequest(newsockfd);
			 close(sockfd);
             exit(0);
         }
         else
             close(newsockfd); // parent don't need handle the connected socket
     }
     return 0; // never reach here
}

void processRequest(int sock)
{
	int num;
	int buf_size = 1024;
	char buf[buf_size];
	char request[buf_size];	
	bzero(request, buf_size);
	bzero(buf, buf_size);
	
	num = read(sock, buf, buf_size - 1);
	
	if (num < 0) 
	{
		error("ERROR reading from socket");
	}
	
	// print request on the server
	printf("Here is the message: %s\n",buf);
	
	
	int bufferIdx = 0;
	int reqIdx = 0;
	int spaces = 0;
	while (bufferIdx < buf_size)
	{
		if (buf[bufferIdx] == ' ')
		{
			spaces++;
		}
		if (spaces == 2)
		{
			break;

		} 
		else if (spaces == 1)
		{
			if (buf[bufferIdx] != '/' && buf[bufferIdx] != ' ')
			{
				request[reqIdx] = buf[bufferIdx];
				reqIdx++; // careful!
			}
		}
		bufferIdx++;
	}
	request[reqIdx] = '\0';
	// printf("pre request: %s\n", request);
	int fast = 0, slow = 0;
	while (fast < reqIdx)
	{
		if (strncmp(request + fast, "%20", 3) == 0) 
		{
			request[slow++] = ' ';
			fast += 3;
		}
		else 
		{
			request[slow++] = request[fast ++];
		}
	}
	request[slow] = '\0';
	// printf("request: %s\n", request);
	
	// get status
	char *status = "HTTP/1.1 200 OK\nConnection: close\n";
	int statusLen = strlen(status);
	
	// get date information
	char date[512];
	int dateLen;
	time_t curTime = time(0);
	struct tm tm = *gmtime(&curTime);
	strftime(date, sizeof(date), "Date: %a, %d %b %Y %H:%M:%S %Z\n", &tm);
	dateLen = strlen(date);
	
	
	// get server name
	char *serverName = "Server: Apache\n";
	int serverLength = strlen(serverName);
	
	char *file;
	struct stat st;
	
	// get file size
	stat(request, &st);
	int size = st.st_size;

	file = readFileByBytes(request);
	
	if (file == NULL)
	{
		// send error 404 back to client if not fuond
		char *notFoundStatus = "HTTP/1.1 404 Not Found\nConnection: close\n";
		statusLen = strlen(notFoundStatus);
		
		int responseLen = dateLen + statusLen + serverLength;
		char *error404 = (char *)malloc(responseLen);
		sprintf(error404, "%s%s%s", notFoundStatus, date, serverName);
		printf("%s", error404);
		
		num = write(sock, error404, responseLen);
		
		error("ERROR 404 not Found");
		return;
	}

	// get file modification time
	struct stat attrib;
    stat(request, &attrib);
    char modificationDate[512];
    strftime(modificationDate, sizeof(modificationDate), "Last-Modified: %a, %d %m %y %H:%M:%S %Z\n", gmtime(&(attrib.st_ctime)));
	int fileModLen = strlen(modificationDate);
	
	// get content length
	char contentLen[512];
	sprintf(contentLen, "Content-Length: %d\n", size);
	int conLenLen = strlen(contentLen);

	// get content type	
	char *type;
	if (request[reqIdx - 1] == 'l')
	{
			type = "text/html";
	}
	else if (request[reqIdx - 1] == 'g')
	{
			type = "image/jpeg";
	}
	else if (request[reqIdx - 1] == 'f')
	{
			type = "image/gif";
	}
	char contentType[512];
	sprintf(contentType, "Content-Type: %s\n\n", type); 
	int typeLen = strlen(contentType);
	
	int responseLen = statusLen + dateLen + serverLength + fileModLen + typeLen + conLenLen;
	
	// combine header and file to get the final response
	char *header = (char *)malloc(responseLen + size);
	sprintf(header, "%s%s%s%s%s%s", status, date, serverName, modificationDate, contentLen, contentType);
	printf("%s", header);
	num = write(sock, header, responseLen);
	num = write(sock, file, size + 300);
	if (num < 0)
	{
		error("ERROR writing to socket");
	}
	
}

char* readFileByBytes(const char *name)
{
    FILE *fpointer = fopen(name, "r");
    if (fpointer)
    {
	    fseek(fpointer, 0, SEEK_END);
	    long len = ftell(fpointer);
	    char *ret = (char *)malloc(len);
	    fseek(fpointer, 0, SEEK_SET);
	    fread(ret, 1, len, fpointer);
	    fclose(fpointer);
	    return ret;
	}
	return NULL;
}
