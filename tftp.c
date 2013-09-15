// Simple TFTP client implementation.
// Author: Sol Boucher <slb1566@rit.edu>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int bool;

static void readin(char **, size_t *);

int main(void)
{
	char *buf = malloc(1);
	size_t cap = 1;

	while(1)
	{
		readin(&buf, &cap);
		printf("inp: %s\nlen: %lu\ncap: %lu\n\n", buf, strlen(buf), cap);
	}

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
