#include "shell.h"
#include "endpoint.h"
#include <string.h>
#include <wrc.h>

int cmd_stat(const char *args[])
{
	if (!strcasecmp(args[0], "cont")) {
		wrc_ui_mode = UI_STAT_MODE;
	} else if (!strcasecmp(args[0], "bts"))
		mprintf("%d ps\n", ep_get_bitslide());
	else
		wrc_log_stats(1);

	return 0;
}
