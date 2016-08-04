/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2016 CERN (www.cern.ch)
 * Author: Grzegorz Daniluk <grzegorz.daniluk@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

#include "shell.h"
#include <wrc.h>
#include <syscon.h>
#include <string.h>
#include <errno.h>

static int cmd_diag(const char *args[])
{
	uint32_t id, ver, nrw, nro;
	uint32_t addr, val;
	int ret = 0;

	if (!args[0]) {
		diag_read_info(&id, &ver, &nrw, &nro);
		pp_printf("Aux diagnostics info:\n");
		pp_printf("id: %d.%d, r/w words: %d, r/o words: %d\n", id, ver,
				nrw, nro);
		return 0;
	}

	if (!strcasecmp(args[0], "ro") && args[1]) {
		addr = atoi(args[1]);
		ret = diag_read_word(addr, DIAG_RO_BANK, &val);
		if (!ret)
			pp_printf("Word %d is 0x%08x\n", addr, val);
		return ret;
	}

	if (!strcasecmp(args[0], "rw") && args[1]) {
		addr = atoi(args[1]);
		ret = diag_read_word(addr, DIAG_RW_BANK, &val);
		if (!ret)
			pp_printf("Word %d is 0x%08x\n", addr, val);
		return ret;
	}

	if (!strcasecmp(args[0], "w") && args[1] && args[2]) {
		addr = atoi(args[1]);
		val  = atoi(args[2]);
		ret = diag_write_word(addr, val);
		if (!ret)
			pp_printf("Value 0x%08x written to the word %d\n",
				  val, addr);
		return ret;
	}

	return -EINVAL;
}

DEFINE_WRC_COMMAND(diag) = {
	.name = "diag",
	.exec = cmd_diag,
};
