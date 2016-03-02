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
	struct wrc_task *t;

	if (args[0] && !strcasecmp(args[0], "reset")) {
		for_each_task(t)
			t->nrun = t->seconds = t->nanos = 0;
		return 0;
	}
	pp_printf(" iterations     seconds.micros    name\n");
		for_each_task(t)
		pp_printf("  %9li   %9li.%06li  %s\n", t->nrun,
			  t->seconds, t->nanos/1000, t->name);
	return 0;
}

DEFINE_WRC_COMMAND(ps) = {
	.name = "ps",
	.exec = cmd_ps,
};
