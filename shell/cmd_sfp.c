/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Grzegorz Daniluk <grzegorz.daniluk@cern.ch>
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
/*  Command: sfp
    Arguments: subcommand [subcommand-specific args]

    Description: SFP detection/database manipulation.

		Subcommands:
			add vendor_type delta_tx delta_rx alpha - adds an SFP to the database, with given alpha/delta_rx/delta_rx values
			show - shows the SFP database
      match - tries to get calibration parameters from DB for a detected SFP
      erase - cleans the SFP database
			detect - detects the transceiver type
*/

#include <string.h>
#include <stdlib.h>
#include <wrc.h>

#include "shell.h"
#include "eeprom.h"
#include "syscon.h"

#include "sfp.h"

int cmd_sfp(const char *args[])
{
	int8_t sfpcount = 1, i, temp;
	struct s_sfpinfo sfp;
	static char pn[SFP_PN_LEN + 1] = "\0";

	if (args[0] && !strcasecmp(args[0], "detect")) {
		if (!sfp_present())
			mprintf("No SFP.\n");
		else
			sfp_read_part_id(pn);
		pn[16] = 0;
		mprintf("%s\n", pn);
		return 0;
	}
//  else if (!strcasecmp(args[0], "i2cscan"))
//  {
//    mi2c_scan(WRPC_FMC_I2C);
//    return 0;
//  }
	else if (!strcasecmp(args[0], "erase")) {
		if (eeprom_sfpdb_erase(WRPC_FMC_I2C, FMC_EEPROM_ADR) ==
		    EE_RET_I2CERR)
			mprintf("Could not erase DB\n");
	} else if (args[4] && !strcasecmp(args[0], "add")) {
		if (strlen(args[1]) > 16)
			temp = 16;
		else
			temp = strlen(args[1]);
		for (i = 0; i < temp; ++i)
			sfp.pn[i] = args[1][i];
		while (i < 16)
			sfp.pn[i++] = ' ';	//padding
		sfp.dTx = atoi(args[2]);
		sfp.dRx = atoi(args[3]);
		sfp.alpha = atoi(args[4]);
		temp = eeprom_get_sfp(WRPC_FMC_I2C, FMC_EEPROM_ADR, &sfp, 1, 0);
		if (temp == EE_RET_DBFULL)
			mprintf("SFP DB is full\n");
		else if (temp == EE_RET_I2CERR)
			mprintf("I2C error\n");
		else
			mprintf("%d SFPs in DB\n", temp);
	} else if (args[0] && !strcasecmp(args[0], "show")) {
		for (i = 0; i < sfpcount; ++i) {
			temp = eeprom_get_sfp(WRPC_FMC_I2C, FMC_EEPROM_ADR,
					      &sfp, 0, i);
			if (!i) {
				sfpcount = temp;	//only in first round valid sfpcount is returned from eeprom_get_sfp
				if (sfpcount == 0 || sfpcount == 0xFF) {
					mprintf("SFP database empty...\n");
					return 0;
				} else if (sfpcount == -1) {
					mprintf("SFP database corrupted...\n");
					return 0;
				}
			}
			mprintf("%d: PN:", i + 1);
			for (temp = 0; temp < 16; ++temp)
				mprintf("%c", sfp.pn[temp]);
			mprintf(" dTx: %d, dRx: %d, alpha: %d\n", sfp.dTx,
				sfp.dRx, sfp.alpha);
		}
	} else if (args[0] && !strcasecmp(args[0], "match")) {
		if (pn[0] == '\0') {
			mprintf("Run sfp detect first\n");
			return 0;
		}
		strncpy(sfp.pn, pn, SFP_PN_LEN);
		if (eeprom_match_sfp(WRPC_FMC_I2C, FMC_EEPROM_ADR, &sfp) > 0) {
			mprintf("SFP matched, dTx=%d, dRx=%d, alpha=%d\n",
				sfp.dTx, sfp.dRx, sfp.alpha);
			sfp_deltaTx = sfp.dTx;
			sfp_deltaRx = sfp.dRx;
			sfp_alpha = sfp.alpha;
		} else
			mprintf("Could not match to DB\n");
		return 0;
	}

	return 0;
}
