/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Grzegorz Daniluk <grzegorz.daniluk@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

#include <string.h>
#include <wrc.h>
#include "shell.h"
#include "storage.h"
#include "syscon.h"
#include "i2c.h"

static int cmd_init(const char *args[])
{
	if (args[0] && !strcasecmp(args[0], "erase")) {
		if (storage_init_erase() < 0)
			pp_printf("Could not erase init script\n");
	} else if (args[1] && !strcasecmp(args[0], "add")) {
		if (storage_init_add(args) < 0)
			pp_printf("Could not add the command\n");
		else
			pp_printf("OK.\n");
	} else if (args[0] && !strcasecmp(args[0], "show")) {
		shell_show_build_init();
		storage_init_show();
	} else if (args[0] && !strcasecmp(args[0], "boot")) {
		shell_boot_script();
	}

	return 0;
}

DEFINE_WRC_COMMAND(init) = {
	.name = "init",
	.exec = cmd_init,
};
