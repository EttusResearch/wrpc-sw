/*  Command: ptp
    Arguments: one [start/stop]

    Description: (re)starts/stops the PTP session. */

#include <errno.h>
#include <string.h>
#include "wrc_ptp.h"
#include "shell.h"

int cmd_ptp(const char *args[])
{
	if (!strcasecmp(args[0], "start"))
		return wrc_ptp_start();
	else if (!strcasecmp(args[0], "stop"))
		return wrc_ptp_stop();
	else
		return -EINVAL;

	return 0;
}
