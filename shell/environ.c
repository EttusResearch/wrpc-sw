/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <wrc.h>

#include "shell.h"

#define SH_ENVIRON_SIZE 256

/* Environment-related functions */

static char env_buf[SH_ENVIRON_SIZE];

void env_init()
{
	env_buf[0] = 0xff;
}

static char *_env_get(const char *var)
{
	int i = 0;
	while (i < SH_ENVIRON_SIZE && env_buf[i] != 0xff) {
		if (env_buf[i] == 0xaa
		    && !strcasecmp((char *)env_buf + i + 1, var))
			return (char *)env_buf + i;

		i++;
	}

	return NULL;
}

char *env_get(const char *var)
{
	char *p = _env_get(var);

	if (!p)
		return NULL;

	return p + 2 + strlen(p + 1);
}

static int _env_end()
{
	int i;
	for (i = 0; i < SH_ENVIRON_SIZE; i++)
		if (env_buf[i] == 0xff)
			return i;
	return 0;
}

int env_set(const char *var, const char *value)
{
	char *vstart = _env_get(var), *p;
	int end;

	if (vstart) {		/* entry already present? remove current and append at the end of environment */
		p = vstart + 1;
		while (*p != 0xaa && *p != 0xff)
			p++;

		memmove(vstart, p, SH_ENVIRON_SIZE - (p - env_buf));
	}

	end = _env_end();

	if ((end + strlen(var) + strlen(value) + 3) >= SH_ENVIRON_SIZE)
		return -ENOMEM;

	p = &env_buf[end];

	*p++ = 0xaa;
	memcpy(p, var, strlen(var) + 1);
	p += strlen(var) + 1;
	memcpy(p, value, strlen(value) + 1);
	p += strlen(value) + 1;
	*p++ = 0xff;

	p = env_buf;

	return 0;
}

int cmd_env(const char *args[])
{
	char *p = env_buf;

	while (*p != 0xff) {
		if (*p == 0xaa)
			mprintf("%s=%s\n", p + 1, p + strlen(p + 1) + 2);
		p++;
	}

	return 0;
}

int cmd_saveenv(const char *args[])
{

	return -ENOTSUP;
}

int cmd_set(const char *args[])
{
	if (!args[1])
		return -EINVAL;

	return env_set(args[0], args[1]);
}
