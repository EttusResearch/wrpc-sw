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

#include "softpll_ng.h"
#include "shell.h"
#include "../lib/ipv4.h"

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

void print_ip(char *head, unsigned char *ip, char *tail)
{
	pp_printf("%s%d.%d.%d.%d%s",
		  head, ip[0], ip[1], ip[2], ip[3], tail);
}

static int cmd_ip(const char *args[])
{
	unsigned char ip[4];

	if (!args[0] || !strcasecmp(args[0], "get")) {
		getIP(ip);
	} else if (!strcasecmp(args[0], "set") && args[1]) {
		decode_ip(args[1], ip);
		setIP(ip);
	} else {
		return -EINVAL;
	}

	if (needIP) {
		pp_printf("IP-address: in training\n");
	} else {
		print_ip("IP-address: ", ip, "\n");
	}
	return 0;
}

DEFINE_WRC_COMMAND(ip) = {
	.name = "ip",
	.exec = cmd_ip,
};
