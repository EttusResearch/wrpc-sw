/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2017 CERN (www.cern.ch)
 * Author: Grzegorz Daniluk <grzegorz.daniluk@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

const char *byte_to_binary(int x)
{
	unsigned int z;
	static char b[33];
	b[0] = '\0';

	for (z = 0x80000000; z > 0; z >>= 1)
	{
		strcat(b, ((x & z) == z) ? "1" : "0");
	}

	return b;
}


int main(int argc, char *argv[])
{
	FILE *f = fopen(argv[1], "rb");
	if (!f)
		return -1;
	int tmp;
	int i = 0;
	int ram_size = atoi(argv[2]);

	ram_size = (ram_size+3)/4;

	while (fread(&tmp, 1, 4, f)) {
		printf("%s\n", byte_to_binary(htonl(tmp)));
		++i;
	}
	//padding
	for(;i<ram_size; ++i) {
		printf("00000000000000000000000000000000\n");
	}

	fclose(f);
	return 0;
}
