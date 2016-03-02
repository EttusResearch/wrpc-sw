/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2016 GSI (www.gsi.de)
 * Author: Alessandro rubini
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#ifndef __TEMPERATURE_H__
#define __TEMPERATURE_H__

struct wrc_onetemp {
	char *name;
	int32_t t;  /* fixed point, 16.16 (signed!) */
};
#define TEMP_INVALID (0x8000 << 16)

struct wrc_temp {
	int (*read)(struct wrc_temp *);
	void *data;
	struct wrc_onetemp *t; /* zero-terminated */
};

#define DEFINE_TEMPERATURE(_name) \
        static struct wrc_temp __wrc_temp_ ## _name \
        __attribute__((section(".temp"), __used__))

/* lib functions  */
extern uint32_t wrc_temp_get(char *name);
struct wrc_onetemp *wrc_temp_getnext(struct wrc_onetemp *);
extern int wrc_temp_format(char *buffer, int len);

#endif /* __TEMPERATURE_H__ */
