// Simple TFTP server implementation.
// Author: Sol Boucher <slb1566@rit.edu>

#include <netinet/in.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Connection parameters:
static const in_port_t PORT = 1069;

// Protocol details:
static const uint16_t OPC_RRQ = 1;
static const uint16_t OPC_WRQ = 2;
static const uint16_t OPC_DAT = 3;
static const uint16_t OPC_ACK = 4;
static const uint16_t OPC_ERR = 5;
static const char *const MODE_ASCII = "netascii";
static const char *const MODE_OCTET = "octet";
static const uint16_t ERR_UNKNOWN      = 0;
static const uint16_t ERR_NOTFOUND     = 1;
static const uint16_t ERR_ACCESSDENIED = 2;
static const uint16_t ERR_DISKFULL     = 3;
static const uint16_t ERR_ILLEGALOPER  = 4;
static const uint16_t ERR_UNKNOWNTID   = 5;
static const uint16_t ERR_CLOBBER      = 6;
static const uint16_t ERR_UNKNOWNUSER  = 7;

static void *connection(void *);

static void strtolower(char *, size_t);
static int openudp(uint16_t);
static void *recvpkt(int, ssize_t *);
static void *recvpkta(int, ssize_t *, struct sockaddr_in *, socklen_t *);
static void handle_error(const char *);

int main(void)
{
	int socketfd = openudp(PORT);

	while(1)
	{
		struct sockaddr_in saddr_remote;
		socklen_t sktaddrmt_len = sizeof saddr_remote;
		void *request = NULL;
		ssize_t req_len = 0; // Includes extra terminator

		request = recvpkta(socketfd, &req_len, &saddr_remote, &sktaddrmt_len);

		uint16_t opcode = *(uint16_t *)request;
		char *filename = (char *)(request+2);
		size_t fname_len = strlen(filename);
		char *mode = (char *)(filename+fname_len+1);
		size_t mode_len = strlen(mode);
		strtolower(mode, mode_len);

		if(opcode == OPC_RRQ)
			printf("opcode: RRQ\n");
		else if(opcode == OPC_WRQ)
			printf("opcode: WRQ\n");
		else
			printf("unexpected opcode!\n");
		printf("filename: %s\n", filename);
		printf("xfermode: %s\n", mode);
		putchar('\n');

		if((opcode == OPC_RRQ || opcode == OPC_WRQ) && req_len-1 == 2+fname_len+1+mode_len+1)
		{
			pthread_t thread;
			void *actuals = malloc(sktaddrmt_len+2+fname_len+1);
			*(struct sockaddr_in *)actuals = saddr_remote;
			*(uint16_t *)(actuals+sktaddrmt_len) = opcode;
			memcpy(actuals+sktaddrmt_len+2, filename, fname_len+1);
			pthread_create(&thread, NULL, &connection, actuals);
		}
		else
			printf("bad: %ld != %ld\n", req_len-1, 2+fname_len+1+mode_len+1);

		if(request)
			free(request);
	}

	return 0;
}

void *connection(void *args)
{
	printf("spawned a thread!\n");

	struct sockaddr_in *rmtsocket = (struct sockaddr_in *)args;
	socklen_t rmtskt_len = sizeof *rmtsocket;
	uint16_t oper = *(uint16_t *)(args+rmtskt_len);
	char *filename = (char *)(args+rmtskt_len+2);
	printf("oper: %hu\nname: %s\n", oper, filename);
	int locsocket = openudp(0);

	struct sockaddr_in mysock;
	socklen_t mysck_len = sizeof mysock;
	getsockname(locsocket, (struct sockaddr *)&mysock, &mysck_len);
	printf("%hu\n", ntohs(mysock.sin_port)); // TODO Remove all this if unused

	uint16_t ack[2]; // TODO Send DATA instead for RRQs
	ack[0] = OPC_ACK;
	ack[1] = (uint16_t)0;
	sendto(locsocket, ack, sizeof ack, 0, (struct sockaddr *)rmtsocket, sizeof rmtsocket);

	free(args);
	return NULL;
}

void strtolower(char *a, size_t l)
{
	int index;
	for(index = 0; index < l; ++index)
		a[index] = tolower(a[index]);
}

int openudp(uint16_t port)
{
	// Open UDP socket over IP:
	int socketfd;
	if((socketfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) // TODO IPv6 support
		handle_error("socket()");

	// Bind to interface port:
	struct sockaddr_in saddr_local;
	saddr_local.sin_family = AF_INET;
	saddr_local.sin_port = htons(port); // TODO Try privileged port, then fall back
	saddr_local.sin_addr.s_addr = INADDR_ANY;
	if(bind(socketfd, (struct sockaddr *)&saddr_local, sizeof saddr_local))
		handle_error("bind()");

	return socketfd;
}

void *recvpkt(int sfd, ssize_t *msg_len)
{
	return recvpkta(sfd, msg_len, NULL, 0);
}

void *recvpkta(int sfd, ssize_t *msg_len, struct sockaddr_in *rmt_saddr, socklen_t *rsaddr_len)
{
	// Listen for an incoming message and note its length:
	if((*msg_len = recvfrom(sfd, NULL, 0, MSG_TRUNC|MSG_PEEK, (struct sockaddr *)rmt_saddr, rsaddr_len)) < 0) // TODO Handle shutdown
		handle_error("recvfrom()");

	// Check for client closing connection:
	if(*msg_len == 0)
		return NULL;

	// Read the message:
	void *msg = malloc(*msg_len);
	if(recv(sfd, msg, *msg_len, 0) <= 0)
		handle_error("recv()");
	return msg;
}

void handle_error(const char *desc)
{
	int errcode = errno;
	perror(desc);
	exit(errcode);
}
