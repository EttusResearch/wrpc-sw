/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012 - 2015 CERN (www.cern.ch)
 * Author: Grzegorz Daniluk <grzegorz.daniluk@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include "shell.h"
#include "endpoint.h"
#include <string.h>
#include <wrc.h>
#include <errno.h>

int wrc_stat_running;
extern uint32_t wrc_stats_last;

static int cmd_stat(const char *args[])
{
	/* no arguments: invert */
	if (!args[0]) {
		wrc_stat_running = !wrc_stat_running;
		wrc_stats_last--; /* force a line to be printed */
		if (!wrc_stat_running)
			pp_printf("statistics now off\n");
		return 0;
	}

	/* arguments: bts, on, off */
	if (!strcasecmp(args[0], "bts")) {
		pp_printf("%d ps\n", ep_get_bitslide());
	} else if (!strcasecmp(args[0], "on")) {
		wrc_stat_running = 1;
		wrc_stats_last--; /* force a line to be printed */
	} else if (!strcasecmp(args[0], "off")) {
		wrc_stat_running = 0;
		pp_printf("statistics now off\n");
	} else
		return -EINVAL;
	return 0;

}

DEFINE_WRC_COMMAND(stat) = {
	.name = "stat",
	.exec = cmd_stat,
};
