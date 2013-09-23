// Simple TFTP client implementation.
// Author: Sol Boucher <slb1566@rit.edu>

#include "tftp_protoc.h"
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int bool;

// Sex appeal:
static const char *const SHL_PS1 = "tftp> ";

// Interactive commands (must be wholly unambiguous):
static const char *const CMD_CON = "connect";
static const char *const CMD_PUT = "put";
static const char *const CMD_GET = "get";
static const char *const CMD_GFO = "quit";
static const char *const CMD_HLP = "?";

static void readin(char **, size_t *);

int main(void)
{
	struct addrinfo hints;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET; // TODO IPv6 suppor
	hints.ai_socktype = SOCK_DGRAM;

	int sfd = openudp(0);
	struct addrinfo *server = NULL;

	char *buf = malloc(1);
	size_t cap = 1;
	char *cmd; // First word of buf
	size_t len; // Length of cmd

	do
	{
		do
		{
			printf("%s", SHL_PS1);
			readin(&buf, &cap);
			cmd = strtok(buf, " ");
			len = strlen(cmd);
		}
		while(!len);

		if(strncmp(cmd, CMD_CON, len) == 0)
		{
			const char *hostname = strtok(NULL, " "); // TODO Support direct IPs
			const char *tmp = strtok(NULL, " ");
			in_port_t port = PORT;
			if(tmp)
				port = atoi(tmp);

			if(!hostname)
			{
				printf("USAGE: %s <hostname> [port]\n", CMD_CON);
				printf("Required argument hostname not provided.\n");
				continue;
			}

			if(server)
			{
				freeaddrinfo(server);
				server = NULL;
			}

			if(getaddrinfo(hostname, NULL, &hints, &server))
				handle_error("getaddrinfo()");
			((struct sockaddr_in *)server->ai_addr)->sin_port = htons(port);
		}
		else if(strncmp(cmd, CMD_PUT, len) == 0)
			printf("file submission unimplemented\n");
		else if(strncmp(cmd, CMD_GET, len) == 0)
		{
			const char *pathname = strtok(NULL, " ");
			if(!pathname)
			{
				printf("USAGE: %s <pathname>\n", CMD_GET);
				printf("Required argument pathname not provided.\n");
				continue;
			}

			if(!server)
			{
				printf("%s: expects existing connection\n", cmd);
				printf("Did you call %s?\n", CMD_CON);
				continue;
			}

			uint8_t req[2+strlen(pathname)+1+strlen(MODE_OCTET)+1];
			*(uint16_t *)req = OPC_RRQ;
			memcpy(req+2, pathname, strlen(pathname)+1);
			memcpy(req+2+strlen(pathname)+1, MODE_OCTET, strlen(MODE_OCTET)+1);
			sendto(sfd, req, sizeof req, 0, server->ai_addr, sizeof(struct sockaddr_in));

			int fd;
			if((fd = open(pathname, O_WRONLY|O_CREAT|O_EXCL)) < 0)
				handle_error("open()"); // TODO Be user-friendly

			ssize_t msg_len;
			do
			{
				uint16_t *inc = recvpkt(sfd, &msg_len);
				if(msg_len > 4)
					write(fd, inc+2, msg_len-4);
				// TODO Send ACK
			}
			while(msg_len == 4+DATA_LEN);

			close(fd);
		}
		else if(strncmp(cmd, CMD_HLP, len) == 0)
		{
			printf("Commands may be abbreviated.  Commands are:\n\n");
			printf("%s\t\tconnect to remote tftp\n", CMD_CON);
			printf("%s\t\tsend file\n", CMD_PUT);
			printf("%s\t\treceive file\n", CMD_GET);
			printf("%s\t\texit tftp\n", CMD_GFO);
			printf("%s\t\tprint help information\n", CMD_HLP);
		}
		else if(strncmp(cmd, CMD_GFO, len) != 0)
			printf("%s: unknown directive\n", cmd);
	}
	while(strncmp(cmd, CMD_GFO, len) != 0);

	free(buf);
	if(server)
		freeaddrinfo(server);
}

void readin(char **bufptr, size_t *bufcap)
{
	char *buf = *bufptr;
	bool fits;
	int index = 0;

	while(1)
	{
		fits = 0;
		for(; index < *bufcap; ++index)
		{
			buf[index] = getc(stdin);
			if(buf[index] == '\n')
				buf[index] = '\0';

			if(!buf[index])
			{
				fits = 1;
				break;
			}
		}
		if(fits) break;

		buf = malloc(*bufcap*2);
		memcpy(buf, *bufptr, *bufcap);
		free(*bufptr);
		*bufptr = buf;
		*bufcap = *bufcap*2;
	}
}
