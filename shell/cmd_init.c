/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Grzegorz Daniluk <grzegorz.daniluk@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

#include <string.h>
#include <wrc.h>
#include "shell.h"
#include "eeprom.h"
#include "syscon.h"
#include "i2c.h"

int cmd_init(const char *args[])
{
	if (!mi2c_devprobe(WRPC_FMC_I2C, FMC_EEPROM_ADR)) {
		mprintf("EEPROM not found..\n");
		return -1;
	}

	if (args[0] && !strcasecmp(args[0], "erase")) {
		if (eeprom_init_erase(WRPC_FMC_I2C, FMC_EEPROM_ADR) < 0)
			mprintf("Could not erase init script\n");
	} else if (args[0] && !strcasecmp(args[0], "purge")) {
		eeprom_init_purge(WRPC_FMC_I2C, FMC_EEPROM_ADR);
	} else if (args[1] && !strcasecmp(args[0], "add")) {
		if (eeprom_init_add(WRPC_FMC_I2C, FMC_EEPROM_ADR, args) < 0)
			mprintf("Could not add the command\n");
		else
			mprintf("OK.\n");
	} else if (args[0] && !strcasecmp(args[0], "show")) {
		eeprom_init_show(WRPC_FMC_I2C, FMC_EEPROM_ADR);
	} else if (args[0] && !strcasecmp(args[0], "boot")) {
		shell_boot_script();
	}

	return 0;
}
