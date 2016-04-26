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
 *                      the database, with given alpha/delta_rx/delta_rx values
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
	int8_t sfpcount = 1, i, temp;
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
		temp = storage_get_sfp(&sfp, 1, 0);
		if (temp == EE_RET_DBFULL) {
			pp_printf("SFP DB is full\n");
			return -ENOSPC;
		} else if (temp == EE_RET_I2CERR) {
			pp_printf("I2C error\n");
			return -EIO;
		}
		pp_printf("%d SFPs in DB\n", temp);
		return 0;
	} else if (!strcasecmp(args[0], "show")) {
		for (i = 0; i < sfpcount; ++i) {
			temp = storage_get_sfp(&sfp, 0, i);
			if (!i) {
				sfpcount = temp;	//only in first round valid sfpcount is returned from storage_get_sfp
				if (sfpcount == 0 || sfpcount == 0xFF) {
					pp_printf("SFP database empty...\n");
					return 0;
				} else if (sfpcount == -1) {
					pp_printf("SFP database corrupted...\n");
					return -EFAULT;
				}
			}
			pp_printf("%d: PN:", i + 1);
			for (temp = 0; temp < SFP_PN_LEN; ++temp)
				pp_printf("%c", sfp.pn[temp]);
			pp_printf(" dTx: %8d dRx: %8d alpha: %8d\n", sfp.dTx,
				sfp.dRx, sfp.alpha);
		}
		return 0;
	} else if (!strcasecmp(args[0], "match")) {
		sfp_pn[0] = '\0';
		if (!sfp_present()) {
			pp_printf("No SFP.\n");
			return -ENODEV;
		}
		if (sfp_read_part_id(sfp_pn)) {
			pp_printf("SFP read error\n");
			return -EIO;
		}
		for (temp = 0; temp < SFP_PN_LEN; ++temp)
			pp_printf("%c", sfp_pn[temp]);
		pp_printf("\n");

		strncpy(sfp.pn, sfp_pn, SFP_PN_LEN);
		if (storage_match_sfp(&sfp) == 0) {
			pp_printf("Could not match to DB\n");
			sfp_in_db = SFP_NOT_MATCHED;
			return -ENXIO;
		}
		pp_printf("SFP matched, dTx=%d dRx=%d alpha=%d\n",
			sfp.dTx, sfp.dRx, sfp.alpha);
		sfp_deltaTx = sfp.dTx;
		sfp_deltaRx = sfp.dRx;
		sfp_alpha = sfp.alpha;
		sfp_in_db = SFP_MATCHED;
		return 0;
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
