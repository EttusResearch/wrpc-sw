/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include "shell.h"
#include "syscon.h"
#include "hw/memlayout.h"

static int cmd_sdb(const char *args[])
{
	sdb_print_devices();
	return 0;
}

DEFINE_WRC_COMMAND(sdb) = {
	.name = "sdb",
	.exec = cmd_sdb,
};
