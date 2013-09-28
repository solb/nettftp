// Simple TFTP client implementation.
// Author: Sol Boucher <slb1566@rit.edu>

#include "tftp_protoc.h"
#include <fcntl.h>
#include <libgen.h>
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
static void usage(const char *, const char *, const char *);
static void noconn(const char *);
static void sendreq(int, const char*, int, struct sockaddr *);

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
				usage(CMD_CON, "hostname", "port");
				continue;
			}
			if(server)
			{
				freeaddrinfo(server);
				server = NULL;
			}

			if(getaddrinfo(hostname, NULL, &hints, &server))
				handle_error("getaddrinfo()"); // TODO Handle failed lookups
			((struct sockaddr_in *)server->ai_addr)->sin_port = htons(port);
		}
		else if(strncmp(cmd, CMD_PUT, len) == 0)
		{
			char *pathname = strtok(NULL, "");
			if(!pathname)
			{
				usage(CMD_PUT, "pathname", NULL);
				continue;
			}
			if(!server)
			{
				noconn(cmd);
				continue;
			}

			uint8_t rmtack[4];
			uint8_t *rmtack_ptr;
			ssize_t rmak_len;
			struct sockaddr_in dest_addr;
			socklen_t dest_adln;
			sendreq(sfd, pathname, OPC_WRQ, server->ai_addr);
			rmtack_ptr = recvpkta(sfd, &rmak_len, &dest_addr, &dest_adln);

			int fd;
			if((fd = open(pathname, O_RDONLY)) < 0)
				handle_error("open()"); // TODO Be user-friendly

			sendfile(sfd, fd, &dest_addr);

			if(fd >= 0)
				close(fd);
		}
		else if(strncmp(cmd, CMD_GET, len) == 0)
		{
			char *pathname = strtok(NULL, "");
			if(!pathname)
			{
				usage(CMD_GET, "pathname", NULL);
				continue;
			}
			if(!server)
			{
				noconn(cmd);
				continue;
			}

			sendreq(sfd, pathname, OPC_RRQ, server->ai_addr);

			int fd;
			if((fd = open(basename(pathname), O_WRONLY|O_CREAT|O_EXCL, 0666)) < 0)
				handle_error("open()"); // TODO Be user-friendly

			const char *res = recvfile(sfd, fd);
			if(res)
			{
				printf("%s\n", res);
				close(fd);
				fd = -1;
				unlink(basename(pathname));
			}

			if(fd >= 0)
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

void usage(const char *cmd, const char *reqd, const char *optl)
{
	char trailer[optl ? strlen(optl)+4 : 1];
	trailer[0] = '\0';
	if(optl)
		sprintf(trailer, " [%s]", optl);

	printf("USAGE: %s <%s>%s\n", cmd, reqd, trailer);
	printf("Required argument %s not provided.\n", reqd);
}

void noconn(const char *typed)
{
	printf("%s: expects existing connection\n", typed);
	printf("Did you call %s?\n", CMD_CON);
}

void sendreq(int sfd, const char* pathname, int opcode, struct sockaddr *dest)
{
	uint8_t req[2+strlen(pathname)+1+strlen(MODE_OCTET)+1];
	*(uint16_t *)req = opcode;
	memcpy(req+2, pathname, strlen(pathname)+1);
	memcpy(req+2+strlen(pathname)+1, MODE_OCTET, strlen(MODE_OCTET)+1);
	sendto(sfd, req, sizeof req, 0, dest, sizeof(struct sockaddr_in));
}
