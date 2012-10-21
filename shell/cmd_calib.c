/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Grzegorz Daniluk <grzegorz.daniluk@cern.ch>
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

/* 	Command: gui
		Arguments: none

		Description: launches the WR Core monitor GUI */

#include <string.h>
#include <wrc.h>
#include "shell.h"
#include "eeprom.h"
#include "syscon.h"

extern int measure_t24p(uint32_t *value);

int cmd_calib(const char *args[])
{
	uint32_t trans;

	if (args[0] && !strcasecmp(args[0], "force")) {
		if (measure_t24p(&trans) < 0)
			return -1;
		return eeprom_phtrans(WRPC_FMC_I2C, FMC_EEPROM_ADR, &trans, 1);
	} else if (!args[0]) {
		if (eeprom_phtrans(WRPC_FMC_I2C, FMC_EEPROM_ADR, &trans, 0) > 0) {
			mprintf("Found phase transition in EEPROM: %dps\n",
				trans);
			cal_phase_transition = trans;
			return 0;
		} else {
			mprintf("Measuring t2/t4 phase transition...\n");
			if (measure_t24p(&trans) < 0)
				return -1;
			cal_phase_transition = trans;
			return eeprom_phtrans(WRPC_FMC_I2C, FMC_EEPROM_ADR,
					      &trans, 1);
		}
	}

	return 0;
}
