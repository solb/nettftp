// Simple TFTP server implementation.
// Author: Sol Boucher <slb1566@rit.edu>

#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const in_port_t PORT = 1069;

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

	// Listen for an incoming message and note its length:
	struct sockaddr saddr_remote;
	socklen_t sarmt_len = sizeof saddr_remote;
	ssize_t msg_len;
	if((msg_len = recvfrom(socketfd, NULL, 0, MSG_TRUNC|MSG_PEEK, &saddr_remote, &sarmt_len)) <= 0) // TODO Handle shutdown
		handle_error("recvfrom()");
	++msg_len; // Increment length to leave room for a NULL-terminator

	// Read the message:
	char message[msg_len];
	memset(message, 0, msg_len);
	if(recv(socketfd, &message, msg_len, 0) <= 0) // TODO Handle multiple clients
		handle_error("recvfrom()");
	printf("%s\n", message);

	return 0;
}

void handle_error(const char *desc)
{
	perror(desc);
	exit(errno);
}
