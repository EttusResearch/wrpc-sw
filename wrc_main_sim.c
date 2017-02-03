/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2011,2012 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 * Author: Grzegorz Daniluk <grzegorz.daniluk@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <stdio.h>
#include <inttypes.h>

#include "system_checks.h"
void wrc_run_task(struct wrc_task *t);

int main(void)
{
	struct wrc_task *t;

	check_reset();

	/* initialization of individual tasks */
	for_each_task(t)
		if (t->init)
			t->init();

	for (;;) {
		for_each_task(t)
			wrc_run_task(t);

		/* better safe than sorry */
		check_stack();
	}
}
