#include <wrc.h>
#include "shell.h"
#include "syscon.h"


extern const char *build_revision, *build_date;

#ifdef CONFIG_DEVELOPER
#define SUPPORT " (unsupported developer build)"
#else
#define SUPPORT ""
#endif

static int cmd_ver(const char *args[])
{
	int hwram = sysc_get_memsize();

	pp_printf("WR Core build: %s%s\n", build_revision, SUPPORT);
	pp_printf("%s", build_date); /* may be empty, or complete with \n */
	pp_printf("Built for %d kB RAM, stack is %d bytes\n",
		  CONFIG_RAMSIZE / 1024, CONFIG_STACKSIZE);
	/* hardware reports memory size, with a 16kB granularity */
	if ( hwram / 16 != CONFIG_RAMSIZE / 1024 / 16)
		pp_printf("WARNING: hardware says %ikB <= RAM < %ikB\n",
			  hwram, hwram + 16);

	return 0;
}

DEFINE_WRC_COMMAND(ver) = {
	.name = "ver",
	.exec = cmd_ver,
};
