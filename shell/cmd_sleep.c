/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2013 CERN (www.cern.ch)
 * Author: Alessandro Rubini <rubini@gnudd.com>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <wrc.h>
#include <shell.h>

static int cmd_sleep(const char *args[])
{
	int sec = 1;

	if (args[0])
		fromdec(args[0], &sec);
	while (sec--)
		usleep(1000 * 1000);
	return 0;
}

DEFINE_WRC_COMMAND(sleep) = {
	.name = "sleep",
	.exec = cmd_sleep,
};
