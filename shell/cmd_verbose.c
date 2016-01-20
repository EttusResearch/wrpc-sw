/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

#include <wrc.h>
#include <shell.h>
#include <ppsi/ppsi.h>

static int cmd_verbose(const char *args[])
{
	if (args[0])
		pp_global_d_flags = pp_diag_parse((char *)args[0]);
	pp_printf("PPSI verbosity: %08lx\n", pp_global_d_flags);
	return 0;
}

DEFINE_WRC_COMMAND(verbose) = {
	.name = "verbose",
	.exec = cmd_verbose,
};
