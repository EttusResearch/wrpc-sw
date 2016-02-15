/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2016 GSI (www.gsi.de)
 * Author: Alessandro Rubini <a.rubini@gsi.de>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <wrc.h>
#include <string.h>
#include <shell.h>

extern struct wrc_task wrc_tasks[];
extern int wrc_n_tasks;

static int cmd_ps(const char *args[])
{
	int i, ena;

	if (args[0] && !strcasecmp(args[0], "reset")) {
		for (i = 0; i < wrc_n_tasks; i++) {
			wrc_tasks[i].nrun = 0;
		}
		return 0;
	}
	pp_printf("iterations   ena  name\n");
	for (i = 0; i < wrc_n_tasks; i++) {
		ena = 1;
		if (wrc_tasks[i].enable) ena = (*wrc_tasks[i].enable != 0);
		pp_printf("   %8i   %i   %s\n",
			  wrc_tasks[i].nrun, ena, wrc_tasks[i].name);
	}
	return 0;
}

DEFINE_WRC_COMMAND(ps) = {
	.name = "ps",
	.exec = cmd_ps,
};
