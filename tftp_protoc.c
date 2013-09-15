// Simple TFTP protocol library implementation.
// Author: Sol Boucher <slb1566@rit.edu>

#include "tftp_protoc.h"
#include <errno.h>
#include <stdlib.h>

const in_port_t PORT = 1069;

const uint16_t OPC_RRQ = 1;
const uint16_t OPC_WRQ = 2;
const uint16_t OPC_DAT = 3;
const uint16_t OPC_ACK = 4;
const uint16_t OPC_ERR = 5;
const char *const MODE_ASCII = "netascii";
const char *const MODE_OCTET = "octet";
const uint16_t ERR_UNKNOWN = 0;
const uint16_t ERR_NOTFOUND = 1;
const uint16_t ERR_ACCESSDENIED = 2;
const uint16_t ERR_DISKFULL = 3;
const uint16_t ERR_ILLEGALOPER = 4;
const uint16_t ERR_UNKNOWNTID = 5;
const uint16_t ERR_CLOBBER = 6;
const uint16_t ERR_UNKNOWNUSER = 7;

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
