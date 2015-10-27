/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Grzegorz Daniluk <grzegorz.daniluk@cern.ch>
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

/* 	Command: calibration
		Arguments: [force]

		Description: launches RX timestamper calibration. */

#include <string.h>
#include <wrc.h>
#include "shell.h"
#include "storage.h"
#include "syscon.h"
#include "rxts_calibrator.h"

static int cmd_calibration(const char *args[])
{
	uint32_t trans;

	if (args[0] && !strcasecmp(args[0], "force")) {
		if (measure_t24p(&trans) < 0)
			return -1;
		return storage_phtrans(&trans, 1);
	} else if (!args[0]) {
		if (storage_phtrans(&trans, 0) > 0) {
			pp_printf("Found phase transition in EEPROM: %dps\n",
				trans);
			cal_phase_transition = trans;
			return 0;
		} else {
			pp_printf("Measuring t2/t4 phase transition...\n");
			if (measure_t24p(&trans) < 0)
				return -1;
			cal_phase_transition = trans;
			return storage_phtrans(&trans, 1);
		}
	}

	return 0;
}

DEFINE_WRC_COMMAND(calibration) = {
	.name = "calibration",
	.exec = cmd_calibration,
};
