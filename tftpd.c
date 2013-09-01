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
static const char *const OPC_RRQ = "01";
static const char *const OPC_WRQ = "02";
static const char *const OPC_DAT = "03";
static const char *const OPC_ACK = "04";
static const char *const OPC_ERR = "05";
static const char *const ERR_UNKNOWN      = "00";
static const char *const ERR_NOTFOUND     = "01";
static const char *const ERR_ACCESSDENIED = "02";
static const char *const ERR_DISKFULL     = "03";
static const char *const ERR_ILLEGALOPER  = "04";
static const char *const ERR_UNKNOWNTID   = "05";
static const char *const ERR_CLOBBER      = "06";
static const char *const ERR_UNKNOWNUSER  = "07";

static char *recvstr(int);
static char *recvstra(int, struct sockaddr_in *, socklen_t *);
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

	// Echo any client that connects:
	struct sockaddr_in saddr_remote;
	socklen_t sktaddrmt_len = sizeof saddr_remote;
	char *message = recvstra(socketfd, &saddr_remote, &sktaddrmt_len);
	if(message)
	{
		printf("%s\n", message);
		free(message);
	}

	return 0;
}

char *recvstr(int sfd)
{
	return recvstra(sfd, NULL, 0);
}

char *recvstra(int sfd, struct sockaddr_in *rmt_saddr, socklen_t *rsaddr_len)
{
	// Listen for an incoming message and note its length:
	ssize_t msg_len;
	if((msg_len = recvfrom(sfd, NULL, 0, MSG_TRUNC|MSG_PEEK, (struct sockaddr *)rmt_saddr, rsaddr_len)) < 0) // TODO Handle shutdown
		handle_error("recvfrom()");

	if(msg_len == 0) // Client closed connection
		return NULL;
	++msg_len; // Increment length to leave room for a NULL-terminator

	// Read the message:
	char *msg = malloc(msg_len);
	memset(msg, 0, msg_len);
	if(recv(sfd, msg, msg_len, 0) <= 0) // TODO Handle multiple clients
		handle_error("recv()");
	return msg;
}

void handle_error(const char *desc)
{
	perror(desc);
	exit(errno);
}
