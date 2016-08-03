/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Grzegorz Daniluk <grzegorz.daniluk@cern.ch>
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
/* Command: sfp
 * Arguments: subcommand [subcommand-specific args]
 *
 * Description: SFP detection/database manipulation.
 * Subcommands:
 * add <product_number> <delta_tx> <delta_rx> <alpha> - adds an SFP to
 *                      the database, with given alpha/delta_rx/delta_tx values
 * show - shows the SFP database
 * match - detects the transceiver type and tries to get calibration parameters
 *         from DB for a detected SFP
 * erase - cleans the SFP database
 */

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <wrc.h>

#include "shell.h"
#include "storage.h"
#include "syscon.h"
#include "endpoint.h"

#include "sfp.h"

static int cmd_sfp(const char *args[])
{
	int8_t sfpcount = 1, i, temp, ret;
	struct s_sfpinfo sfp;

	if (!args[0]) {
		pp_printf("Wrong parameter\n");
		return -EINVAL;
	}
	if (!strcasecmp(args[0], "erase")) {
		if (storage_sfpdb_erase() == EE_RET_I2CERR) {
			pp_printf("Could not erase DB\n");
			return -EIO;
		}
		return 0;
	} else if (args[4] && !strcasecmp(args[0], "add")) {
		temp = strnlen(args[1], SFP_PN_LEN);
		for (i = 0; i < temp; ++i)
			sfp.pn[i] = args[1][i];
		while (i < SFP_PN_LEN)
			sfp.pn[i++] = ' ';	//padding
		sfp.dTx = atoi(args[2]);
		sfp.dRx = atoi(args[3]);
		sfp.alpha = atoi(args[4]);
		temp = storage_get_sfp(&sfp, SFP_ADD, 0);
		if (temp == EE_RET_DBFULL) {
			pp_printf("SFP DB is full\n");
			return -ENOSPC;
		} else if (temp == EE_RET_I2CERR) {
			pp_printf("I2C error\n");
			return -EIO;
		} else if (temp < 0) {
			pp_printf("SFP database error (%d)\n", temp);
			return -EFAULT;
		}
		pp_printf("%d SFPs in DB\n", temp);
		return 0;
	} else if (!strcasecmp(args[0], "show")) {
		for (i = 0; i < sfpcount; ++i) {
			sfpcount = storage_get_sfp(&sfp, SFP_GET, i);
			if (sfpcount == 0) {
				pp_printf("SFP database empty\n");
				return 0;
			} else if (sfpcount < 0) {
				pp_printf("SFP database error (%d)\n",
					  sfpcount);
				return -EFAULT;
			}
			pp_printf("%d: PN:", i + 1);
			for (temp = 0; temp < SFP_PN_LEN; ++temp)
				pp_printf("%c", sfp.pn[temp]);
			pp_printf(" dTx: %8d dRx: %8d alpha: %8d\n", sfp.dTx,
				sfp.dRx, sfp.alpha);
		}
		return 0;
	} else if (!strcasecmp(args[0], "match")) {
		ret = sfp_match();
		if (ret == -ENODEV) {
			pp_printf("No SFP.\n");
			return ret;
		}
		if (ret == -EIO) {
			pp_printf("SFP read error\n");
			return ret;
		}

		/* SFP read correctly */
		for (temp = 0; temp < SFP_PN_LEN; ++temp)
			pp_printf("%c", sfp_pn[temp]);
		pp_printf("\n");

		if (ret == -ENXIO) {
			pp_printf("Could not match to DB\n");
			return ret;
		}
		/* match successful */
		pp_printf("SFP matched, dTx=%d dRx=%d alpha=%d\n",
			sfp_deltaTx, sfp_deltaRx, sfp_alpha);
		return ret;
	} else if (args[1] && !strcasecmp(args[0], "ena")) {
		ep_sfp_enable(atoi(args[1]));
		return 0;
	} else {
		pp_printf("Wrong parameter\n");
		return -EINVAL;
	}
	return 0;
}

DEFINE_WRC_COMMAND(sfp) = {
	.name = "sfp",
	.exec = cmd_sfp,
};
