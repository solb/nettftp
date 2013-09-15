// Simple TFTP client implementation.
// Author: Sol Boucher <slb1566@rit.edu>

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
	char *buf = malloc(1);
	size_t cap = 1;
	size_t len;

	do
	{
		do
		{
			printf("%s", SHL_PS1);
			readin(&buf, &cap);
			len = strlen(buf);
		}
		while(!len);

		if(strncmp(buf, CMD_CON, len) == 0)
			printf("connection establishment unimplemented\n");
		else if(strncmp(buf, CMD_PUT, len) == 0)
			printf("file submission unimplemented\n");
		else if(strncmp(buf, CMD_GET, len) == 0)
			printf("file retrieval unimplemented\n");
		else if(strncmp(buf, CMD_HLP, len) == 0)
		{
			printf("Commands may be abbreviated.  Commands are:\n\n");
			printf("%s\t\tconnect to remote tftp\n", CMD_CON);
			printf("%s\t\tsend file\n", CMD_PUT);
			printf("%s\t\treceive file\n", CMD_GET);
			printf("%s\t\texit tftp\n", CMD_GFO);
			printf("%s\t\tprint help information\n", CMD_HLP);
		}
		else if(strncmp(buf, CMD_GFO, len) != 0)
			printf("%s: unknown directive\n", buf);
	}
	while(strncmp(buf, CMD_GFO, len) != 0);

	free(buf);
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
