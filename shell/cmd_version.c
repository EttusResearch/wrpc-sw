#include <wrc.h>
#include "shell.h"
#include "syscon.h"


extern const char *build_revision, *build_date;

static int cmd_ver(const char *args[])
{
	mprintf("WR Core build: %s%s (memory size: %d kB)\n",
		build_revision, build_date, sysc_get_memsize());
	return 0;
}

DEFINE_WRC_COMMAND(ver) = {
	.name = "ver",
	.exec = cmd_ver,
};
