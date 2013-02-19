/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <wrc.h>

#include "softpll_ng.h"
#include "shell.h"

int cmd_pll(const char *args[])
{
	int cur, tgt;

	if (!strcasecmp(args[0], "init")) {
		if (!args[3])
			return -EINVAL;
		spll_init(atoi(args[1]), atoi(args[2]), atoi(args[3]));
	} else if (!strcasecmp(args[0], "cl")) {
		if (!args[1])
			return -EINVAL;
		mprintf("%d\n", spll_check_lock(atoi(args[1])));
	} else if (!strcasecmp(args[0], "sps")) {
		if (!args[2])
			return -EINVAL;
		spll_set_phase_shift(atoi(args[1]), atoi(args[2]));
	} else if (!strcasecmp(args[0], "gps")) {
		if (!args[1])
			return -EINVAL;
		spll_get_phase_shift(atoi(args[1]), &cur, &tgt);
		mprintf("%d %d\n", cur, tgt);
	} else if (!strcasecmp(args[0], "start")) {
		if (!args[1])
			return -EINVAL;
		spll_start_channel(atoi(args[1]));
	} else if (!strcasecmp(args[0], "stop")) {
		if (!args[1])
			return -EINVAL;
		spll_stop_channel(atoi(args[1]));
	} else if (!strcasecmp(args[0], "sdac")) {
		if (!args[2])
			return -EINVAL;
		spll_set_dac(atoi(args[1]), atoi(args[2]));
	} else if (!strcasecmp(args[0], "gdac")) {
		if (!args[1])
			return -EINVAL;
		mprintf("%d\n", spll_get_dac(atoi(args[1])));
	} else
		return -EINVAL;

	return 0;
}
