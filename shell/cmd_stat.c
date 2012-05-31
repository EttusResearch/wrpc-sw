#include "shell.h"

int cmd_stat(const char *args[])
{
	wrc_log_stats();
	return 0;
}