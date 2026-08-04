#include <stdio.h>
#include <stdlib.h>

void WriteVersion()
{
#ifndef __WIN32__
	FILE* fp(fopen("VERSION.txt", "w"));

	if (NULL != fp)
	{
		fprintf(fp, "__DB_VERSION__: %s\n", __DB_VERSION__);
		fprintf(fp, "%s@%s:%s\n", "bWl0aXR1", __HOSTNAME__, __PWD__);
		fclose(fp);
	}
	else
	{
		fprintf(stderr, "cannot open VERSION.txt\n");
		exit(0);
	}
#endif
}
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
