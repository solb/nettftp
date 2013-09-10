// Simple raw string assembler.
// Author: Sol Boucher <slb1566@rit.edu>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
	void *msgs[5];
	size_t msg_lens[5];
	memset(msg_lens, 0, sizeof msg_lens);

	int index;
	for(index = 0; index < 5; ++index)
	{
		fprintf(stderr, "Type (i, a, or q)? ");
		char choice = getchar();
		if(choice == '\n')
			choice = getchar();

		tolower(choice);
		if(choice == 'i') // 16-bit int
		{
			msgs[index] = malloc(2);
			scanf("%hd", (short *)(msgs[index]));
			msg_lens[index] = 2;
		}
		else if(choice == 'a') // string
		{
			msgs[index] = malloc(1024);
			scanf("%s", (char *)(msgs[index]));
			msg_lens[index] = strlen(msgs[index])+1; // include terminator
		}
		else
			break;
	}

	int newind;
	for(newind = 0; newind < index; ++newind)
		fwrite(msgs[newind], 1, msg_lens[newind], stdout);
	putchar('\n');

	return 0;
}
