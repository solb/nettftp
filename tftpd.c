// Simple TFTP server implementation.
// Author: Sol Boucher <slb1566@rit.edu>

#include <errno.h>
#include <netinet/in.h>
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

static void strtolower(char *, size_t);
static void *recvpkt(int, ssize_t *);
static void *recvpkta(int, ssize_t *, struct sockaddr_in *, socklen_t *);
static void handle_error(const char *);

int main(void)
{
	// Open UDP socket over IP:
	int socketfd;
	if((socketfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) // TODO IPv6 support
		handle_error("socket()");

	// Bind to interface port:
	struct sockaddr_in saddr_local;
	saddr_local.sin_family = AF_INET;
	saddr_local.sin_port = htons(PORT); // TODO Try privileged port, then fall back
	saddr_local.sin_addr.s_addr = INADDR_ANY;
	if(bind(socketfd, (struct sockaddr *)&saddr_local, sizeof saddr_local))
		handle_error("bind()");

	while(1)
	{
		struct sockaddr_in saddr_remote;
		socklen_t sktaddrmt_len = sizeof saddr_remote;
		void *request = NULL;
		ssize_t req_len = 0;

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
		// TODO Check whether string sizes exceed that of entire packet

		if(request)
			free(request);
	}

	return 0;
}

void strtolower(char *a, size_t l)
{
	int index;
	for(index = 0; index < l; ++index)
		a[index] = tolower(a[index]);
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
