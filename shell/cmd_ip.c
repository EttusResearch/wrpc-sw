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

void decode_ip(const char *str, unsigned char *ip)
{
	int i, x;

	/* Don't try to detect bad input; need small code */
	for (i = 0; i < 4; ++i) {
		str = fromdec(str, &x);
		ip[i] = x;
		if (*str == '.')
			++str;
	}
}

char *format_ip(char *s, const unsigned char *ip)
{
	pp_sprintf(s, "%d.%d.%d.%d",
		   ip[0], ip[1], ip[2], ip[3]);
	return s;
}

static int cmd_ip(const char *args[])
{
	unsigned char ip[4];
	char buf[20];

	if (!args[0] || !strcasecmp(args[0], "get")) {
		getIP(ip);
	} else if (!strcasecmp(args[0], "set") && args[1]) {
		ip_status = IP_OK_STATIC;
		decode_ip(args[1], ip);
		setIP(ip);
	} else {
		return -EINVAL;
	}

	format_ip(buf, ip);
	switch (ip_status) {
	case IP_TRAINING:
		pp_printf("IP-address: in training\n");
		break;
	case IP_OK_BOOTP:
		pp_printf("IP-address: %s (from bootp)\n", buf);
		break;
	case IP_OK_STATIC:
		pp_printf("IP-address: %s (static assignment)\n", buf);
		break;
	}
	return 0;
}

DEFINE_WRC_COMMAND(ip) = {
	.name = "ip",
	.exec = cmd_ip,
};
