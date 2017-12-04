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
 * args[1] - where to write sdbfs image (0 - Flash, 1 - I2C EEPROM,
 *		2 - 1Wire EEPROM)
 * args[2] - base address for sdbfs image in Flash/EEPROM
 * args[3] - i2c address of EEPROM or blocksize of Flash
 */

static int cmd_sdb(const char *args[])
{
	uint8_t i2c_adr = FMC_EEPROM_ADR;
	int blocksize	= 1;

	if (!args[0]) {
		sdb_print_devices();
		return 0;
	}
	if (!args[1] || !HAS_GENSDBFS)
		return -EINVAL;

	/* interpret args[3] as i2c adr or blocksize depending on memory type */
	if (args[3] && atoi(args[1]) == MEM_FLASH)
		blocksize = atoi(args[3])*1024;
	else if (args[3])
		i2c_adr = atoi(args[3]);

	/* Writing SDBFS image */
	if (!strcasecmp(args[0], "fs") && args[2]) {
		/* if all the parameters were specified from the cmd line, we
		 * use these */
		storage_gensdbfs(atoi(args[1]), atoi(args[2]), blocksize,
				i2c_adr);
		return 0;
	}
	if (!strcasecmp(args[0], "fs") && storage_cfg.valid &&
			atoi(args[1]) == MEM_FLASH) {
		/* if available, we can also use Flash parameters specified with
		 * HDL generics */
		storage_gensdbfs(MEM_FLASH, storage_cfg.baseadr,
				storage_cfg.blocksize, 0);
		return 0;
	}
	/* Erasing SDBFS image */
	if (!strcasecmp(args[0], "fse") && args[2]) {
		storage_sdbfs_erase(atoi(args[1]), atoi(args[2]), blocksize,
				i2c_adr);
		return 0;
	}
	if (!strcasecmp(args[0], "fse") && storage_cfg.valid &&
			atoi(args[1]) == MEM_FLASH) {
		storage_sdbfs_erase(MEM_FLASH, storage_cfg.baseadr,
				storage_cfg.blocksize, 0);
		return 0;
	}

	return -EINVAL;
}

DEFINE_WRC_COMMAND(sdb) = {
	.name = "sdb",
	.exec = cmd_sdb,
};
