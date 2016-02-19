/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2016 GSI (www.gsi.de)
 * Author: Alessandro rubini
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

#include <wrc.h>
#include <string.h>
#include <temperature.h>
#include <shell.h>

extern struct wrc_temp temp_w1; /* The only one by now */

/*
 * Library functions
 */
uint32_t wrc_temp_get(char *name)
{
	struct wrc_onetemp *wt = temp_w1.t;

	for (; wt->name; wt++) {
		if (!strcmp(wt->name, name))
			return wt->t;
	}
	return TEMP_INVALID;
}

struct wrc_onetemp *wrc_temp_getnext(struct wrc_onetemp *pt)
{
	if (!pt) /* first one */
		return temp_w1.t;
	if (pt[1].name)
		return pt + 1;
	/* get next array, if any -- none by now */
	return NULL;
}

extern int wrc_temp_format(char *buffer, int len)
{
	struct wrc_onetemp *p;
	int l = 0, i = 0;
	int32_t t;

	for (p = wrc_temp_getnext(NULL); p; p = wrc_temp_getnext(p), i++) {
		if (l + 16 > len) {
			l += sprintf(buffer + l, " ENOSPC");
			return l;
		}
		t = p->t;
		l += sprintf(buffer + l, "%s%s:", i ? " " : "", p->name);
		if (t == TEMP_INVALID) {
			l += sprintf(buffer + l, "INVALID");
			continue;
		}
		if (t < 0) {
			t = -(signed)t;
			l += sprintf(buffer + l, "-");
		}
		l += sprintf(buffer + l,"%d.%04d", t >> 16,
			     ((t & 0xffff) * 10 * 1000 >> 16));
	}
	return l;
}

/*
 * The task
 */
void wrc_temp_init(void)
{
	/* Call all actors, so they can init themselves (using ->data) */
	temp_w1.read(&temp_w1);
}

int wrc_temp_refresh(void)
{
	return temp_w1.read(&temp_w1);
}

/*
 * The shell command
 */

static int cmd_temp(const char *args[])
{
	char buffer[80];

	wrc_temp_format(buffer, sizeof(buffer));
	pp_printf("%s\n", buffer);
	return 0;
}


DEFINE_WRC_COMMAND(temp) = {
	.name = "temp",
	.exec = cmd_temp,
};

