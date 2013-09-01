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

	// Listen for an incoming message:
	char resp[10]; // TODO Set this length correctly
	struct sockaddr saddr_remote;
	socklen_t sarmt_len = sizeof saddr_remote;
	memset(resp, 0, sizeof resp);
	if(recvfrom(socketfd, resp, sizeof resp, 0, &saddr_remote, &sarmt_len) <= 0)
		handle_error("recvfrom()");

	printf("%s", resp);

	return 0;
}

void handle_error(const char *desc)
{
	perror(desc);
	exit(errno);
}
