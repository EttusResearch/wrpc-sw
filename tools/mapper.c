/*
 * mapper.c -- simple file that mmap()s a file region and prints it
 * Alessandro Rubini 1997-2008
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdint.h>
#include <arpa/inet.h>

int main(int argc, char **argv)
{
	char *fname;
	FILE *f;
	int pagesize = getpagesize();
	unsigned int pos, pos_pg, pos_off;
	unsigned int len, len_pg, len_off;
	void *address;
	char *rest;
	int i;

	if (argc !=4
	    || sscanf(argv[2],"%i", &pos) != 1
	    || sscanf(argv[3],"%i", &len) != 1) {
		fprintf(stderr, "%s: Usage \"%s <file> <offset> <len>\"\n",
			argv[0], argv[0]);
		exit(1);
	}
	/* note: pos may be more than 2G, so use strtoul */
	pos = strtoul(argv[2], &rest, 0);
	if (rest && *rest) {
		fprintf(stderr, "%s: Usage \"%s <file> <offset> <len>\"\n",
			argv[0], argv[0]);
		exit(1);
	}

	fname=argv[1];
	if (!(f=fopen(fname,"r"))) {
		fprintf(stderr, "%s: %s: %s\n", argv[0], fname,
			strerror(errno));
		exit(1);
	}

	/* page-align our numbers, as we depend on the MMU */
	pos_pg  = pos & ~(pagesize-1);
	pos_off = pos &  (pagesize-1);
	len_pg  = (len + pos_off + pagesize-1) & ~(pagesize-1);
	len_off = (len + pos_off) &  (pagesize-1);
	(void)len_off; /* no, I don't use it. Prevent a warning */

	address=mmap(0, len_pg,
		     PROT_READ, MAP_FILE | MAP_PRIVATE, fileno(f), pos_pg);

	if (address == (void *)-1) {
		fprintf(stderr,"%s: mmap(): %s\n",argv[0],strerror(errno));
		exit(1);
	}
	fclose(f);
	fprintf(stderr, "mapped \"%s\" from %i to %i (0x%x to 0x%x) \n",
		fname, pos, pos+len, pos_pg, pos_pg+len_pg);

	for (i = 0; i < (len + 3) / 4; i++) {
		write(fileno(stdout), address + pos_off + i * 4, 4);
	}

	return 0;
}
