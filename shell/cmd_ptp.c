/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

#include <errno.h>
#include <string.h>
#include "wrc_ptp.h"
#include "shell.h"

static int cmd_ptp(const char *args[])
{
	if (!strcasecmp(args[0], "start"))
		return wrc_ptp_start();
	else if (!strcasecmp(args[0], "stop"))
		return wrc_ptp_stop();
	else
		return -EINVAL;

	return 0;
}

DEFINE_WRC_COMMAND(ptp) = {
	.name = "ptp",
	.exec = cmd_ptp,
};
