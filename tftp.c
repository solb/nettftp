/*
 * Copyright (C) 2013 Sol Boucher
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with it.  If not, see <http://www.gnu.org/licenses/>.
 */

// Simple TFTP client implementation.
// Author: Sol Boucher <slb1566@rit.edu>

#include "tftp_protoc.h"
#include <fcntl.h>
#include <libgen.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Sex appeal:
static const char *const SHL_PS1 = "tftp> ";

// Interactive commands (must be wholly unambiguous):
static const char *const CMD_CON = "connect";
static const char *const CMD_PUT = "put";
static const char *const CMD_GET = "get";
static const char *const CMD_GFO = "quit";
static const char *const CMD_HLP = "?";

static void readin(char **, size_t *);
static bool homog(const char *, char);
static void usage(const char *, const char *, const char *);
static void noconn(const char *);
static void sendreq(int, const char*, int, struct sockaddr *);

// Runs the interactive loop and all file transfers.
// Returns: exit status
int main(void)
{
	// Used to control DNS resolution requests:
	struct addrinfo hints;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;

	// Bind to an ephemeral network port:
	struct addrinfo *server = NULL;
	int sfd = openudp(0);
	if(sfd < 0)
		handle_error("bind()");

	// Allocate (small) space to store user input:
	char *buf = malloc(1);
	size_t cap = 1;
	char *cmd; // First word of buf
	size_t len; // Length of cmd

	// Main input loop, which normally only breaks upon a GFO:
	do
	{
		// Keep prompting until the user brings us back something good:
		do
		{
			printf("%s", SHL_PS1);
			readin(&buf, &cap);
		}
		while(homog(buf, ' '));

		// Cleave off the command (first word):
		cmd = strtok(buf, " ");
		len = strlen(cmd);

		if(strncmp(cmd, CMD_CON, len) == 0)
		{
			// Read the arguments:
			const char *hostname = strtok(NULL, " ");
			const char *tmp = strtok(NULL, " ");
			in_port_t port = PORT_UNPRIVILEGED;
			if(tmp)
				port = atoi(tmp);

			// Ensure a hostname or IP has been provided:
			if(!hostname)
			{
				usage(CMD_CON, "hostname", "port");
				continue;
			}

			// Avoid leaking any existing address:
			if(server)
			{
				freeaddrinfo(server);
				server = NULL;
			}

			// Try to resolve the requested hostname:
			if(getaddrinfo(hostname, NULL, &hints, &server))
			{
				fprintf(stderr, "Unable to resolve hostname\n");
				freeaddrinfo(server);
				server = NULL;
				continue;
			}
			((struct sockaddr_in *)server->ai_addr)->sin_port = htons(port);
		}
		else if(strncmp(cmd, CMD_GET, len) == 0 || strncmp(cmd, CMD_PUT, len) == 0)
		{
			bool putting = strncmp(cmd, CMD_PUT, 1) == 0;

			// Ensure we're already connected to a server:
			if(!server)
			{
				noconn(cmd);
				continue;
			}

			// Make sure we were given a path argument:
			char *pathname = strtok(NULL, "");
			if(!pathname)
			{
				usage(putting ? CMD_PUT : CMD_GET, "pathname", NULL);
				continue;
			}

			// Since basename() might modify pathname, copy it:
			char filename[strlen(pathname)+1];
			memcpy(filename, pathname, sizeof filename);

			int fd;
			if(putting)
			{
				// Try opening the file for reading:
				if((fd = open(pathname, O_RDONLY)) < 0)
				{
					fprintf(stderr, "local: Unable to read specified file\n");
					continue;
				}

				// Send a request and record the port used to acknowledge:
				struct sockaddr_in dest_addr;
				sendreq(sfd, basename(filename), OPC_WRQ, server->ai_addr);
				uint8_t *rmtack = recvpkta(sfd, &dest_addr);
				if(iserr(rmtack))
				{
					fprintf(stderr, "remote: %s\n", strerr(rmtack));
					free(rmtack);
					continue;
				}
				free(rmtack);

				// Transmit the file:
				sendfile(sfd, fd, &dest_addr);
			}
			else // getting
			{
				// Try opening a file of that name for writing:
				if((fd = open(basename(filename), O_WRONLY|O_CREAT|O_EXCL, 0666)) < 0)
				{
					fprintf(stderr, "local: Unable to create the new file\n");
					continue;
				}

				// Send a request and await the incoming file:
				sendreq(sfd, pathname, OPC_RRQ, server->ai_addr);
				const char *res = recvfile(sfd, fd);
				if(res)
				{
					fprintf(stderr, "remote: %s\n", res);
					close(fd);
					fd = -1;
					unlink(basename(filename));
				}
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
		{
			fprintf(stderr, "%s: unknown directive\n", cmd);
			fprintf(stderr, "Try ? for help.\n");
		}
	}
	while(strncmp(cmd, CMD_GFO, len) != 0);

	free(buf);
	if(server)
		freeaddrinfo(server);

	return 0;
}

// Reads one line of input from standard input into the provided buffer.  Each time the buffer would overflow, it is reallocated at double its previous size.
// Accepts: the target buffer, its length in bytes
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

// Tests whether a string is entirely composed of a particular character.
// Accepts: the string and the character
bool homog(const char *str, char chr)
{
	size_t len = strlen(str);
	int ind;
	for(ind = 0; ind < len; ++ind)
		if(str[ind] != chr)
			return 0;
	return 1;
}

// Prints to standard error the usage string describing a command expecting one required argument and up to one optional argument.
// Accepts: the command, its required argument, and its optional argument or NULL
void usage(const char *cmd, const char *reqd, const char *optl)
{
	char trailer[optl ? strlen(optl)+4 : 1];
	trailer[0] = '\0';
	if(optl)
		sprintf(trailer, " [%s]", optl);

	fprintf(stderr, "USAGE: %s <%s>%s\n", cmd, reqd, trailer);
	fprintf(stderr, "Required argument %s not provided.\n", reqd);
}

// Complains to standard error that a connection should have been established before typing something.
// Accepts: that which was typed
void noconn(const char *typed)
{
	fprintf(stderr, "%s: expects existing connection\n", typed);
	fprintf(stderr, "Did you call %s?\n", CMD_CON);
}

// Sends a request datagram over the network specifying transfer type octal.
// Accepts: local socket file descriptor, requested filename, OPC_RRQ or OPC_WRQ, and remote socket address
void sendreq(int sfd, const char* pathname, int opcode, struct sockaddr *dest)
{
	uint8_t req[2+strlen(pathname)+1+strlen(MODE_OCTET)+1];
	*(uint16_t *)req = opcode;
	strcpy(req+2, pathname);
	strcpy(req+2+strlen(pathname)+1, MODE_OCTET);
	sendto(sfd, req, sizeof req, 0, dest, sizeof(struct sockaddr_in));
}
