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
#ifdef CONFIG_P2P
	{"p2p", wrc_ptp_sync_mech, PP_P2P_MECH},
	{"pdelay", wrc_ptp_sync_mech, PP_P2P_MECH},
#endif
	{"gm", wrc_ptp_set_mode, WRC_MODE_GM},
	{"master", wrc_ptp_set_mode, WRC_MODE_MASTER},
	{"slave", wrc_ptp_set_mode, WRC_MODE_SLAVE},
#ifdef CONFIG_ABSCAL
	{"abscal", wrc_ptp_set_mode, WRC_MODE_ABSCAL},
#endif
};

static char *is_run[] = {"stopped", "running"};
static char *is_mech[] = {[PP_E2E_MECH] = "e2e", [PP_P2P_MECH] = "p2p"};
static char *is_mode[] = {[WRC_MODE_GM] = "gm", [WRC_MODE_MASTER] = "master",
			  [WRC_MODE_SLAVE] = "slave"
#ifdef CONFIG_ABSCAL
 			  , [WRC_MODE_ABSCAL] = "abscal"
#endif
			 };

static int cmd_ptp(const char *args[])
{
	int i, j, ret;
	struct subcmd *c;


	if (!args[0]) {
		pp_printf("%s; %s %s\n",
			  is_run[wrc_ptp_run(-1)],
			  is_mech[wrc_ptp_sync_mech(-1)],
			  is_mode[wrc_ptp_get_mode()]);
		return 0;
	}

	for (j = 0; args[j]; j++) {
		for (i = 0, c = subcmd; i < ARRAY_SIZE(subcmd); i++, c++) {
			if (!strcasecmp(args[j], c->name)) {
				ret = c->fun(c->arg);
				if (ret < 0)
					return ret;
				break;
			}
		}
		if (i == ARRAY_SIZE(subcmd)) {
			pp_printf("Unknown subcommand \"%s\"\n", args[j]);
			return -EINVAL;
		}
	}
	return 0;
}

DEFINE_WRC_COMMAND(ptp) = {
	.name = "ptp",
	.exec = cmd_ptp,
};
DEFINE_WRC_COMMAND(mode) = {
	.name = "mode",
	.exec = cmd_ptp,
};
