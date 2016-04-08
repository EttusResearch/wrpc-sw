/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012 GSI (www.gsi.de)
 * Author: Wesley W. Terpstra <w.terpstra@gsi.de>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <wrc.h>
#include <lib/ipv4.h>

#include "softpll_ng.h"
#include "shell.h"
#include "onewire.h"
#include "endpoint.h"

void decode_mac(const char *str, unsigned char *mac)
{
	int i, x;

	/* Don't try to detect bad input; need small code */
	for (i = 0; i < 6; ++i) {
		str = fromhex(str, &x);
		mac[i] = x;
		if (*str == ':')
			++str;
	}
}

char *format_mac(char *s, const unsigned char *mac)
{
	pp_sprintf(s, "%02x:%02x:%02x:%02x:%02x:%02x",
		   mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	return s;
}


static int cmd_mac(const char *args[])
{
	unsigned char mac[6];
	char buf[32];

	if (!args[0] || !strcasecmp(args[0], "get")) {
		/* get current MAC */
		get_mac_addr(mac);
	} else if (!strcasecmp(args[0], "getp")) {
		/* get persistent MAC */
		get_mac_addr(mac);
		get_persistent_mac(ONEWIRE_PORT, mac);
	} else if (!strcasecmp(args[0], "set") && args[1]) {
		decode_mac(args[1], mac);
		set_mac_addr(mac);
		pfilter_init_default();
	} else if (!strcasecmp(args[0], "setp") && args[1]) {
		decode_mac(args[1], mac);
		set_persistent_mac(ONEWIRE_PORT, mac);
	} else {
		return -EINVAL;
	}

	pp_printf("MAC-address: %s\n", format_mac(buf, mac));
	return 0;
}

DEFINE_WRC_COMMAND(mac) = {
	.name = "mac",
	.exec = cmd_mac,
};
