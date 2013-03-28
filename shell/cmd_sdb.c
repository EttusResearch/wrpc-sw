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
