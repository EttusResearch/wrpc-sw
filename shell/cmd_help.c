/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Alessandro Rubini <rubini@gnudd.com>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <stdlib.h>
#include <string.h>
#include <wrc.h>
#include <shell.h>

static int cmd_help(const char *args[])
{
	struct wrc_shell_cmd *p;

	pp_printf("Available commands:\n");
	for (p = __cmd_begin; p < __cmd_end; p++)
		pp_printf("  %s\n", p->name);
	return 0;
}

DEFINE_WRC_COMMAND(help) = {
	.name = "help",
	.exec = cmd_help,
};
