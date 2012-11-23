/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Alessandro Rubini <rubini@gnudd.com>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <stdlib.h>

/*
 * This is a minimal atoi, that doesn't call strtol. Since we are only
 * calling atoi, it saves XXXX bytes of library code
 */
int atoi(const char *s)
{
	int sign = 1, res = 0;

	/* as (mis)designed in atoi, we make no error check */
	if (*s == '-') {
		sign = -1;
		s++;
	}
	while (*s >= '0' && *s <= '9')
		res = res * 10 + *s++ - '0';
	return res * sign;
}

#ifdef TEST_ATOI
/*
 *  You can try this:
 *
    make atoi CFLAGS="-DTEST_ATOI -Wall" && \
          ./atoi foo 0 45 -45 9876543210 -9345.2 97.5 666devils
 *
 */
#include <stdio.h>

/* just to be sure I call mine and not a #define somewhere in the headers */
int __local_atoi (const char *s) __attribute__((alias("atoi")));

int main(int argc, char **argv)
{
	int i;

	for (i = 1; i < argc; i++)
		printf("%i\n", __local_atoi(argv[i]));
	return 0;
}

#endif /* TEST_ATOI */
