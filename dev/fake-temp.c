/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2016 GSI (www.gsi.de)
 * Author: Alessandro rubini
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <wrc.h>
#include <temperature.h>
#include <shell.h>

static struct wrc_onetemp temp_fake_data[] = {
	{"roof", TEMP_INVALID},
	{"core", TEMP_INVALID},
	{"case", TEMP_INVALID},
	{NULL,}
};

static int temp_fake_refresh(struct wrc_temp *t)
{
	/* nothing to do */
	return 0;
}

/* not static at this point, because it's the only one */
DEFINE_TEMPERATURE(w1) = {
	.read = temp_fake_refresh,
	.t = temp_fake_data,
};


static int cmd_faketemp(const char *args[])
{
	int i;
	const char *dot;

	if (!args[0]) {
		pp_printf("%08x %08x %08x\n", temp_fake_data[0].t,
			  temp_fake_data[1].t, temp_fake_data[2].t);
		return 0;
	}

	for (i = 0; i < 3 && args[i]; i++) {
		int val;

		/* accept at most one decimal */
		dot = fromdec(args[i], &val);
		val <<= 16;
		if (dot[0] == '.' && dot[1] >= '0' && dot[1] <= '9')
			val += 0x10000 / 10 * (dot[1] - '0');
		temp_fake_data[i].t = val;
	}
	return 0;
}


DEFINE_WRC_COMMAND(faketemp) = {
	.name = "faketemp",
	.exec = cmd_faketemp,
};


