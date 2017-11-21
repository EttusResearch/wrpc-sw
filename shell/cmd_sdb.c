/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <string.h>
#include <errno.h>

#include "shell.h"
#include "syscon.h"
#include "hw/memlayout.h"
#include "storage.h"

/*
 * args[1] - where to write sdbfs image (0 - Flash, 1 - I2C EEPROM, 2 - 1Wire EEPROM)
 * args[2] - base address for sdbfs image in Flash/EEPROM
 * args[3] - i2c address of EEPROM
 */

static int cmd_sdb(const char *args[])
{
	uint8_t i2c_adr;

	if (!args[0]) {
		sdb_print_devices();
		return 0;
	}
	if (args[3])
		i2c_adr = atoi(args[3]);
	else
		i2c_adr = FMC_EEPROM_ADR;
	if (args[2] && !strcasecmp(args[0], "fs")) {
		storage_gensdbfs(atoi(args[1]), atoi(args[2]), i2c_adr);
		return 0;
	}
	if (args[2] && !strcasecmp(args[0], "fse")) {
		storage_sdbfs_erase(atoi(args[1]), atoi(args[2]), i2c_adr);
		return 0;
	}
	return -EINVAL;
}

DEFINE_WRC_COMMAND(sdb) = {
	.name = "sdb",
	.exec = cmd_sdb,
};
