/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
/*  Command: time
    Arguments:
    	set UTC NSEC - sets time
    	raw - dumps raw time
    	<none> - dumps pretty time

    Description: (re)starts/stops the PTP session. */

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <wrc.h>

#include "shell.h"
#include "util.h"
#include "wrc_ptp.h"
#include "pps_gen.h"

static int cmd_time(const char *args[])
{
	uint64_t sec;
	uint32_t nsec;

	shw_pps_gen_get_time(&sec, &nsec);

	if (args[2] && !strcasecmp(args[0], "set")) {
		if (wrc_ptp_get_mode() != WRC_MODE_SLAVE) {
			shw_pps_gen_set_time((uint64_t) atoi(args[1]),
					 atoi(args[2]), PPSG_SET_ALL);
			return 0;
		} else
			return -EBUSY;
	} else if (args[0] && !strcasecmp(args[0], "setsec")) {
		if (wrc_ptp_get_mode() != WRC_MODE_SLAVE) {
			shw_pps_gen_set_time((int64_t) atoi(args[1]), 0, PPSG_SET_SEC);
			return 0;
		}
	} else if (args[0] && !strcasecmp(args[0], "setnsec")) {
		if (wrc_ptp_get_mode() != WRC_MODE_SLAVE) {
			shw_pps_gen_set_time(0, atoi(args[1]), PPSG_SET_NSEC);
			return 0;
		}
	} else if (args[0] && !strcasecmp(args[0], "raw")) {
		pp_printf("%d %d\n", (uint32_t) sec, nsec);
		return 0;
	}

	pp_printf("%s +%d nanoseconds.\n",
		  format_time(sec,  TIME_FORMAT_LEGACY), nsec);
	/* fixme: clock freq is not always 125 MHz */

	return 0;
}

DEFINE_WRC_COMMAND(time) = {
	.name = "time",
	.exec = cmd_time,
};
