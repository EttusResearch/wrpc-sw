/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2016 GSI (www.gsi.de)
 * Author: Alessandro rubini
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

#include <wrc.h>
#include <w1.h>
#include <temperature.h>

static struct wrc_onetemp temp_w1_data[] = {
	{"pcb", TEMP_INVALID},
	{NULL,}
};

static unsigned long nextt;
static int niterations;

/* Returns 1 if it did something */
static int temp_w1_refresh(struct wrc_temp *t)
{

	static int done;

	if (!done) {
		nextt = timer_get_tics() + 1000;
		done++;
	}
	/* Odd iterations: send the command, even iterations: read back */
	int phase = niterations & 1;
	static int intervals[] = {
		200, (1000 * CONFIG_TEMP_POLL_INTERVAL) - 200
	};

	if (time_before(timer_get_tics(), nextt))
		return 0;
	nextt += intervals[phase];
	niterations++;

	switch(phase) {
	case 0:
		w1_read_temp_bus(&wrpc_w1_bus, W1_FLAG_NOWAIT);
		break;
	case 1:
		temp_w1_data[0].t =
			w1_read_temp_bus(&wrpc_w1_bus, W1_FLAG_COLLECT);
		break;
	}
	return 1;
}

/* not static at this point, because it's the only one */
DEFINE_TEMPERATURE(w1) = {
	.read = temp_w1_refresh,
	.t = temp_w1_data,
};
