#include "shell.h"
#include "syscon.h"
#include "hw/memlayout.h"

extern const char *build_revision, *build_date;

int cmd_sdb(const char *args[])
{
	sdb_print_devices();
	return 0;
}