/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

#include <errno.h>
#include <string.h>
#include <wrpc.h>
#include "wrc_ptp.h"
#include "shell.h"

struct subcmd {
	char *name;
	int (*fun)(int);
	int arg;
} subcmd[] = {
	{"start", wrc_ptp_run, 1},
	{"stop", wrc_ptp_run, 0},
	{"e2e", wrc_ptp_sync_mech, PP_E2E_MECH},
	{"delay", wrc_ptp_sync_mech, PP_E2E_MECH},
	{"p2p", wrc_ptp_sync_mech, PP_P2P_MECH},
	{"pdelay", wrc_ptp_sync_mech, PP_P2P_MECH},
};

static int cmd_ptp(const char *args[])
{
	int i;
	struct subcmd *c;

	for (i = 0, c = subcmd; i < ARRAY_SIZE(subcmd); i++, c++)
		if (!strcasecmp(args[0], c->name))
			return c->fun(c->arg);
	return -EINVAL;
}

DEFINE_WRC_COMMAND(ptp) = {
	.name = "ptp",
	.exec = cmd_ptp,
};
