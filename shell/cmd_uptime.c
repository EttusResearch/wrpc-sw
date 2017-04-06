#include <wrc.h>
#include "shell.h"

static int cmd_uptime(const char *args[])
{
	extern uint32_t uptime_sec;

	pp_printf("%u\n", uptime_sec);
	return 0;
}

DEFINE_WRC_COMMAND(uptime) = {
	.name = "uptime",
	.exec = cmd_uptime,
};
