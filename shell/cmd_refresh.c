/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2014 UGR (www.ugr.es)
 * Author: Miguel Jimenez Lopez <klyone@ugr.es>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
/*  Command: refresh
    Arguments: seconds: Time interval to update gui/stat statistics (>= 0)

    Description: Configures time interval to update gui/stat statistics by monitor. */

#include "shell.h"
#include "wrc_ptp.h"
#include <syscon.h>

extern int UI_REFRESH_PERIOD;

static int cmd_refresh(const char *args[])
{
	int sec;
	
	if (args[0] && !args[1]) {
		fromdec(args[0], &sec);
	}
	else {
		mprintf("Usage: refresh <seconds>\n");
		return 0;
	}
	
	UI_REFRESH_PERIOD = sec*TICS_PER_SECOND;
	mprintf("\n");
}

DEFINE_WRC_COMMAND(refresh) = {
	.name = "refresh",
	.exec = cmd_refresh,
};
