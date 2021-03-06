/************************************************************
*			     receiver.c
*		Simple FTP Client Implementation
*	      Authors: Mihir Joshi, Fenil Kavathia
*			    csc573 NCSU
************************************************************/

#include "sock/ssock.h"
#include "config.h"
#include "fops/fileop.h"

#define SERVER_PORT 7735
#define SLIDE_WIN() RN = (RN + 1) % WINSIZE

uint RN; // receiver window variable
char *buffer;

int isValid(uchar segment[MSS]);

void removeHeader(uchar segment[MSS])
{
	int i;

	for(i=0; i<MSS - HEADSIZE; i++)
		segment[i] = segment[i + HEADSIZE];

	segment[i] = '\0';
}

void storeSegment(uchar segment[MSS])
{
	int i;
	uint bufSize = WINSIZE * MSS * 2;

	for(i=0;i<MSS-HEADSIZE;i++)
	{
		buffer[(RN * MSS + i) % bufSize] = segment[i];
	}
}

void sendAck(int sock, char senderIP[50], int senderPort)
{
	int i;
	uint ackNo = (RN * MSS);
	uchar segment[HEADSIZE];

	segment[3] = ackNo & 0xFF;
	segment[2] = (ackNo >> 8) & 0xFF;
	segment[1] = (ackNo >> 16) & 0xFF;
	segment[0] = (ackNo >> 24) & 0xFF;

#ifdef APP
	printf("[log] ack sent for %d:\n", ackNo);

	for(i=0;i<HEADSIZE;i++)
		printf("%d, ", (int) segment[i]);

	printf("\n");
#endif

	write_to(sock, segment, HEADSIZE, senderIP, senderPort);
}

void writeToFile(int file, uchar segment[MSS], int buf_len)
{
	int i, validCount = 0;

	for(i=0; i<buf_len; i++)
	{
		if((int) segment[i] != 0)
			validCount++;
		else
			break;
	}

#ifdef GRAN1
	printf("[log] writing to file %d bytes\n", validCount);
#endif

	output_to(file, segment, validCount);
}

int main()
{
	int sock, in_port;
	uchar request[MSS], req_from[50];
	struct sockaddr_in clientCon;
	char ack[HEADSIZE];
	int file, i, bytesRead, packetCount = 0;

/*
	|_|_|_(|_|)_|_|_|_|
		RN
*/

	RN = 0;
	
	buffer = (char *) malloc(WINSIZE * MSS * 2);

	sock = get_sock();

	file = get_file_descriptor("test/received.txt", Create);

	bind_sock(sock, SERVER_PORT);

	listen_sock(sock);

	while(1) // listen continuosly
	{
		bytesRead = read_from(sock, request, MSS, &clientCon);

		if(strcmp(request, "<FINMJ>") == 0)
		{
#ifdef APP
	printf("<FINMJ> received\n");
#endif
			break;
		}

		sprintf(req_from, "%d.%d.%d.%d", (int)(clientCon.sin_addr.s_addr&0xFF),
    					(int)((clientCon.sin_addr.s_addr&0xFF00)>>8),
    					(int)((clientCon.sin_addr.s_addr&0xFF0000)>>16),
    					(int)((clientCon.sin_addr.s_addr&0xFF000000)>>24));

		in_port = ntohs(clientCon.sin_port);

#ifdef APP
	printf("[log]\nGot request from: %s\nPort: %d\n", req_from, in_port);

	printf("request: ");

	for(i=0;i<4;i++)
		printf("%d, ", (int) request[i]);

	for(i=4; i<MSS; i++)
		printf("%c(%d), ", request[i], (int) request[i]);

	printf("\n[/log]\n");
#endif
		if(isValid(request))
		{
#ifdef APP
	printf("[log] valid segment found for seq no: %d\n", RN * MSS);
#endif
#ifdef DROP
	if(packetCount % 3 != 0 && packetCount != 0)
	{
		sleep(2);
#endif
			removeHeader(request);

			storeSegment(request);

			sendAck(sock, req_from, in_port);
			
			writeToFile(file, request, bytesRead - HEADSIZE);

			SLIDE_WIN();
#ifdef DROP
	}
	else
	{
		printf("[drop log]\n-------------- dropping packet: %dat seq no: %d\n---------------\n", packetCount, RN * MSS);
	}
#endif
		}	
		else
		{
#ifdef APP
	printf("[log] discarding packet\n");
#endif

			// take no action. sliently discard
		}

		packetCount++;
	}
	
	close_sock(sock);
	close(file);

	return 0;
}

int isValid(uchar segment[MSS])
{
	uint seqNo = 0;

	seqNo = (uint) segment[0];
	seqNo = seqNo << 24;

	seqNo = seqNo + (((uint) segment[1]) << 16);
	seqNo = seqNo + (((uint) segment[2]) << 8);
	seqNo = seqNo + ((uint) segment[3]);

#ifdef APP
	printf("[log]received sequence number: %d\nexpected sequence number: %d\n[/log]\n", seqNo, RN * MSS);
#endif

	return seqNo == (RN * MSS);
}
