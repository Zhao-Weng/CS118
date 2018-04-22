#define DATASIZE 1024

typedef enum {REQ, ACK, DATA, FIN} FLAG;

struct Packet 
{
	FLAG type;
	int seqNum;	
	int dataSize;
	char data[DATASIZE];
} Packet;
