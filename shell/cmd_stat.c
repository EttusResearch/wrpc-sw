#include "shell.h"
#include "endpoint.h"
#include <string.h>
#include <wrc.h>
#include <errno.h>

int wrc_stat_running;

static int cmd_stat(const char *args[])
{
	/* no arguments: invert */
	if (!args[0]) {
		wrc_stat_running = !wrc_stat_running;
		if (!wrc_stat_running)
			pp_printf("statistics now off\n");
		return 0;
	}

	/* arguments: bts, on, off */
	if (!strcasecmp(args[0], "bts"))
		mprintf("%d ps\n", ep_get_bitslide());
	else if (!strcasecmp(args[0], "on"))
		wrc_stat_running = 1;
	else if (!strcasecmp(args[0], "off")) {
		wrc_stat_running = 0;
		pp_printf("statistics now off\n");
	} else
		return -EINVAL;
	return 0;

}

DEFINE_WRC_COMMAND(stat) = {
	.name = "stat",
	.exec = cmd_stat,
};
