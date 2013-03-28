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

extern char _binary__config_bin_start[];

static int cmd_config(const char *args[])
{
	pp_printf("  Current WRPC-SW configuration:\n");
	puts(_binary__config_bin_start);
	return 0;
}

DEFINE_WRC_COMMAND(config) = {
	.name = "config",
	.exec = cmd_config,
};
