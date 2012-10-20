#include <wrc.h>
#include "shell.h"
#include "syscon.h"


extern const char *build_revision, *build_date;

int cmd_version(const char *args[])
{
	mprintf("WR Core build: %s%s (memory size: %d kB)\n",
		build_revision, build_date, sysc_get_memsize());
	return 0;
}
