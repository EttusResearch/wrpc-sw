/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2013 CERN (www.cern.ch)
 * Author: Grzegorz Daniluk <grzegorz.daniluk@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

#include <string.h>
#include <wrc.h>
#include "shell.h"
#ifdef CONFIG_PPSI
#  include <ppsi/ppsi.h>
#  include "wr-api.h"
#else
#  include "ptpd.h"
#endif

extern int wrc_phase_tracking;

static int cmd_ptrack(const char *args[])
{
	if (args[0] && !strcasecmp(args[0], "enable")) {
		wr_servo_enable_tracking(1);
		wrc_phase_tracking = 1;
	}
	else if (args[0] && !strcasecmp(args[0], "disable")) {
		wr_servo_enable_tracking(0);
		wrc_phase_tracking = 0;
	}
	pp_printf("phase tracking %s\n", wrc_phase_tracking?"ON":"OFF");

	return 0;
}

DEFINE_WRC_COMMAND(ptrack) = {
	.name = "ptrack",
	.exec = cmd_ptrack,
};
