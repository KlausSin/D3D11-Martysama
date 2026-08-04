#include <stdio.h>

void WriteVersion()
{
#ifndef __WIN32__
	FILE* fp = fopen("VERSION.txt", "w");

	if (fp)
	{
		fprintf(fp, "__GAME_VERSION__: %s\n", __GAME_VERSION__);
		fprintf(fp, "%s@%s:%s\n", "bWl0aXR1", __HOSTNAME__, __PWD__);
		fclose(fp);
	}
#endif
}
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
