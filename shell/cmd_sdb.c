#include "shell.h"
#include "syscon.h"
#include "hw/memlayout.h"

int cmd_sdb(const char *args[])
{
	sdb_print_devices();
	return 0;
}
