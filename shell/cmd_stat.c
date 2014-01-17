#include "shell.h"
#include "endpoint.h"
#include <string.h>
#include <wrc.h>

static int cmd_stat(const char *args[])
{
	if (!strcasecmp(args[0], "bts"))
		mprintf("%d ps\n", ep_get_bitslide());
	else
		wrc_ui_mode = UI_STAT_MODE;

	return 0;
}

DEFINE_WRC_COMMAND(stat) = {
	.name = "stat",
	.exec = cmd_stat,
};
