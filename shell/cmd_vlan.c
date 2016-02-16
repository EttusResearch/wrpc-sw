/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2016 GSI (www.gsi.de)
 * Author: Alessandro Rubini <a.rubini@gsi.de>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <wrc.h>
#include <string.h>
#include <errno.h>
#include <shell.h>
#include <endpoint.h>

static int cmd_vlan(const char *args[])
{
	int i;

	if (!args[0] || !strcasecmp(args[0], "get")) {
		/* nothing... */
	} else if (!strcasecmp(args[0], "set") && args[1]) {
		fromdec(args[1], &i);
		if (i < 1 || i > 4095) {
			pp_printf("%i (\"%s\") out of range\n", i, args[1]);
			return -EINVAL;
		}
		wrc_vlan_number = i;
		pfilter_init_default();
	} else if (!strcasecmp(args[0], "off")) {
		wrc_vlan_number = 0;
		pfilter_init_default();

	} else {
		return -EINVAL;
	}
	pp_printf("current vlan: %i (0x%x)\n",
		  wrc_vlan_number, wrc_vlan_number);
	return 0;
}

DEFINE_WRC_COMMAND(vlan) = {
	.name = "vlan",
	.exec = cmd_vlan,
};
