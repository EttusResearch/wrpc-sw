/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
/*  Command: mode
    Arguments: PTP mode: gm = grandmaster, master = free-running master, slave = slave

    Description: (re)starts/stops the PTP session. */

#include <errno.h>
#include <string.h>
#include <wrc.h>

#include "shell.h"
#include "wrc_ptp.h"

int cmd_mode(const char *args[])
{
	int mode;
	static const char *modes[] =
	    { "unknown", "grandmaster", "master", "slave" };

	if (!strcasecmp(args[0], "gm"))
		mode = WRC_MODE_GM;
	else if (!strcasecmp(args[0], "master"))
		mode = WRC_MODE_MASTER;
	else if (!strcasecmp(args[0], "slave"))
		mode = WRC_MODE_SLAVE;
	else {
		mprintf("%s\n", modes[wrc_ptp_get_mode()]);
		return 0;
	}
	return wrc_ptp_set_mode(mode);
}
