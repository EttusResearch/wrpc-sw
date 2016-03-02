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

extern struct wrc_temp __temp_begin[], __temp_end[];

/*
 * Library functions
 */
uint32_t wrc_temp_get(char *name)
{
	struct wrc_temp *ta;
	struct wrc_onetemp *wt;

	for (ta = __temp_begin; ta < __temp_end; ta++)
		for (wt = ta->t; wt->name; wt++) {
		if (!strcmp(wt->name, name))
			return wt->t;
	}
	return TEMP_INVALID;
}

struct wrc_onetemp *wrc_temp_getnext(struct wrc_onetemp *pt)
{
	struct wrc_temp *ta;
	struct wrc_onetemp *wt;

	if (!pt) { /* first one */
		if (__temp_begin != __temp_end)
			return __temp_begin->t;
	}
	if (pt[1].name)
		return pt + 1;
	/* get next array, if any */
	for (ta = __temp_begin; ta < __temp_end; ta++) {
		for (wt = ta->t; wt->name; wt++) {
			if (wt == pt) {
				ta++;
				if (ta >= __temp_end)
					return NULL;
				return ta->t;
			}
		}
	}
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
static void wrc_temp_init(void)
{
	struct wrc_temp *ta;

	/* Call all actors, so they can init themselves (using ->data) */
	for (ta = __temp_begin; ta < __temp_end; ta++)
		ta->read(ta);
}

static int wrc_temp_refresh(void)
{
	struct wrc_temp *ta;
	int ret = 0;

	for (ta = __temp_begin; ta < __temp_end; ta++)
		ret += ta->read(ta);
	return (ret > 0);
}

DEFINE_WRC_TASK(temp) = {
	.name = "temperature",
	.init = wrc_temp_init,
	.job = wrc_temp_refresh,
};

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

