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
	{"gm", wrc_ptp_set_mode, WRC_MODE_GM},
	{"master", wrc_ptp_set_mode, WRC_MODE_MASTER},
	{"slave", wrc_ptp_set_mode, WRC_MODE_SLAVE},
};

static char *is_run[] = {"stopped", "running"};
static char *is_mech[] = {[PP_E2E_MECH] = "e2e", [PP_P2P_MECH] = "p2p"};
static char *is_mode[] = {[WRC_MODE_GM] = "gm", [WRC_MODE_MASTER] = "master",
			  [WRC_MODE_SLAVE] = "slave"};

static int cmd_ptp(const char *args[])
{
	int i;
	struct subcmd *c;


	if (!args[0]) {
		pp_printf("%s; %s %s\n",
			  is_run[wrc_ptp_run(-1)],
			  is_mech[wrc_ptp_sync_mech(-1)],
			  is_mode[wrc_ptp_get_mode()]);
		return 0;
	}

	for (i = 0, c = subcmd; i < ARRAY_SIZE(subcmd); i++, c++)
		if (!strcasecmp(args[0], c->name))
			return c->fun(c->arg);
	return -EINVAL;
}

DEFINE_WRC_COMMAND(ptp) = {
	.name = "ptp",
	.exec = cmd_ptp,
};
DEFINE_WRC_COMMAND(mode) = {
	.name = "mode",
	.exec = cmd_ptp,
};
