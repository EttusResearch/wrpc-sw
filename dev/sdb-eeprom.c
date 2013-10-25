/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012, 2013 CERN (www.cern.ch)
 * Author: Grzegorz Daniluk <grzegorz.daniluk@cern.ch>
 * Author: Alessandro Rubini <rubini@gnudd.com>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
//#include <string.h>
#include <wrc.h>
#include <w1.h>
#include <eeprom.h>

//#include "types.h"
//#include "i2c.h"
//#include "eeprom.h"
//#include "board.h"
//#include "syscon.h"
#include <sdb.h>

#define SDBFS_BIG_ENDIAN
#include <libsdbfs.h>

/*
 * This source file is a drop-in replacement of the legacy one: it manages
 * both i2c and w1 devices even if the interface is the old i2c-based one
 */
#define SDB_VENDOR	0x46696c6544617461LL /* "FileData" */
#define SDB_DEV_INIT	0x77722d69 /* wr-i (nit) */
#define SDB_DEV_MAC	0x6d61632d /* mac- (address) */
#define SDB_DEV_SFP	0x7366702d /* sfp- (database) */
#define SDB_DEV_CALIB	0x63616c69 /* cali (bration) */

/* The methods for W1 access */
static int sdb_w1_read(struct sdbfs *fs, int offset, void *buf, int count)
{
	return w1_read_eeprom_bus(fs->drvdata, offset, buf, count);
}

static int sdb_w1_write(struct sdbfs *fs, int offset, void *buf, int count)
{
	return w1_write_eeprom_bus(fs->drvdata, offset, buf, count);
}

/* The methods for I2C access -- FIXME */


/*
 * A trivial dumper, just to show what's up in there
 */
static void eeprom_sdb_list(struct sdbfs *fs)
{
	struct sdb_device *d;
	int new = 1;
	while ( (d = sdbfs_scan(fs, new)) != NULL) {
		d->sdb_component.product.record_type = '\0';
		pp_printf("file 0x%08x @ %4i, name %s\n",
			  (int)(d->sdb_component.product.device_id),
			  (int)(d->sdb_component.addr_first),
			  (char *)(d->sdb_component.product.name));
		new = 0;
	}
}
/* The sdb filesystem itself */
static struct sdbfs wrc_sdb = {
	.name = "wrpc-storage",
	.blocksize = 1, /* Not currently used */
	/* .read and .write according to device type */
};

uint8_t has_eeprom = 0; /* modified at init time */

/*
 * Init: returns 0 for success; it changes has_eeprom above
 *
 * This is called by wrc_main, after initializing both w1 and i2c
 */
uint8_t eeprom_present(uint8_t i2cif, uint8_t i2c_addr)
{
	uint32_t magic = 0;
	static unsigned entry_points[] = {0, 64, 128, 256, 512, 1024};
	int i, ret;

	/* Look for w1 first: if there is no eeprom if fails fast */
	for (i = 0; i < ARRAY_SIZE(entry_points); i++) {
		ret = w1_read_eeprom_bus(&wrpc_w1_bus, entry_points[i],
					 (void *)&magic, sizeof(magic));
		if (ret != sizeof(magic))
			break;
		if (magic == SDB_MAGIC)
			break;
	}
	if (magic == SDB_MAGIC) {
		pp_printf("sdbfs: found at %i in W1\n", entry_points[i]);
		wrc_sdb.drvdata = &wrpc_w1_bus;
		wrc_sdb.read = sdb_w1_read;
		wrc_sdb.write = sdb_w1_write;
		has_eeprom = 1;
		eeprom_sdb_list(&wrc_sdb);
		return 0;
	}

	/*
	 * If w1 failed, look for i2c: start from high offsets by now.
	 * FIXME: this is a hack, until we have subdirectory support
	 */
	for (i = ARRAY_SIZE(entry_points) - i; i >= 0; i--) {
		/* FIXME: i2c */
	}
	return 0;
}

/*
 * Reading/writing the MAC address used to be part of dev/onewire.c,
 * but is not onewire-specific.  What is w1-specific is the default
 * setting if no sdbfs is there, but CONFIG_SDB_EEPROM depends on
 * CONFIG_W1 anyways.
 */
int32_t get_persistent_mac(uint8_t portnum, uint8_t * mac)
{
	int ret;
	int i, class;
	uint64_t rom;

	if (sdbfs_open_id(&wrc_sdb, SDB_VENDOR, SDB_DEV_MAC) < 0)
		return -1;

	ret = sdbfs_fread(&wrc_sdb, 0, mac, 6);
	sdbfs_close(&wrc_sdb);

	if(ret < 0)
		pp_printf("%s: SDB error\n", __func__);
	if (mac[0] == 0xff ||
	    (mac[0] | mac[1] | mac[2] | mac[3] | mac[4] | mac[5]) == 0) {
		pp_printf("%s: SDB file is empty\n", __func__);
		ret = -1;
	}
	if (ret < 0) {
		pp_printf("%s: Using W1 serial number\n", __func__);
		for (i = 0; i < W1_MAX_DEVICES; i++) {
			class = w1_class(wrpc_w1_bus.devs + i);
			if (class != 0x28 && class != 0x42)
				continue;
			rom = wrpc_w1_bus.devs[i].rom;
			mac[3] = rom >> 24;
			mac[4] = rom >> 16;
			mac[5] = rom >> 8;
			ret = 0;
		}
	}
	if (ret < 0) {
		pp_printf("%s: failure\n", __func__);
		return -1;
	}
	return 0;
}

int8_t set_persistent_mac(uint8_t portnum, uint8_t * mac)
{
	int ret;

	ret = sdbfs_open_id(&wrc_sdb, SDB_VENDOR, SDB_DEV_MAC);
	if (ret >= 0)
		ret = sdbfs_fwrite(&wrc_sdb, 0, mac, 6);
	sdbfs_close(&wrc_sdb);

	if (ret < 0) {
		pp_printf("%s: SDB error, can't save\n", __func__);
		return -1;
	}
	return 0;
}

/*
 * The SFP section is placed somewhere inside EEPROM (W1 or I2C), using sdbfs.
 *
 * Initially we have a count of SFP records
 *
 * For each sfp we have
 *
 * - part number (16 bytes)
 * - alpha (4 bytes)
 * - deltaTx (4 bytes)
 * - delta Rx (4 bytes)
 * - checksum (1 byte)  (low order 8 bits of the sum of all bytes)
 *
 * the total is 29 bytes for each sfp (ugly, but we are byte-oriented anyways
 */


/* Just a dummy function that writes '0' to sfp count field of the SFP DB */
int32_t eeprom_sfpdb_erase(uint8_t i2cif, uint8_t i2c_addr)
{
	uint8_t sfpcount = 0;
	int ret;

	if (sdbfs_open_id(&wrc_sdb, SDB_VENDOR, SDB_DEV_SFP) < 0)
		return -1;
	ret = sdbfs_fwrite(&wrc_sdb, 0, (char *)&sfpcount, 1);
	sdbfs_close(&wrc_sdb);
	return ret == 1 ? 0 : -1;
}

int32_t eeprom_get_sfp(uint8_t i2cif, uint8_t i2c_addr, struct s_sfpinfo * sfp,
		       uint8_t add, uint8_t pos)
{
	static uint8_t sfpcount = 0;
	int ret = -1;
	uint8_t i, chksum = 0;
	uint8_t *ptr;

	if (pos >= SFPS_MAX)
		return EE_RET_POSERR;	//position outside the range

	if (sdbfs_open_id(&wrc_sdb, SDB_VENDOR, SDB_DEV_SFP) < 0)
		return -1;

	//read how many SFPs are in the database, but only in the first call
	if (!pos
	    && sdbfs_fread(&wrc_sdb, 0, &sfpcount, sizeof(sfpcount))
	    != sizeof(sfpcount))
		goto out;

	if (add && sfpcount == SFPS_MAX)	//no more space to add new SFPs
		return EE_RET_DBFULL;
	if (!pos && !add && sfpcount == 0)	// no SFPs in the database
		return 0;

	if (!add) {
		if (sdbfs_fread(&wrc_sdb, sizeof(sfpcount) + pos * sizeof(*sfp),
				sfp, sizeof(*sfp))
		    != sizeof(*sfp))
			goto out;

		ptr = (uint8_t *)sfp;
		/* read sizeof() - 1 because we don't include checksum */
		for (i = 0; i < sizeof(struct s_sfpinfo) - 1; ++i)
			chksum = chksum + *(ptr++);
		if (chksum != sfp->chksum) {
			pp_printf("sfp: corrupted checksum\n");
			goto out;
		}
	} else {
		/*count checksum */
		ptr = (uint8_t *)sfp;
		/* use sizeof() - 1 because we don't include checksum */
		for (i = 0; i < sizeof(struct s_sfpinfo) - 1; ++i)
			chksum = chksum + *(ptr++);
		sfp->chksum = chksum;
		/* add SFP at the end of DB */
		if (sdbfs_fwrite(&wrc_sdb, sizeof(sfpcount)
				  + sfpcount * sizeof(*sfp), sfp, sizeof(*sfp))
		    != sizeof(*sfp))
			goto out;
		sfpcount++;
		if (sdbfs_fwrite(&wrc_sdb, 0, &sfpcount, sizeof(sfpcount))
		    != sizeof(sfpcount))
			goto out;
	}
	ret = sfpcount;
out:
	sdbfs_close(&wrc_sdb);
	return ret;
	return 0;
}

int8_t eeprom_match_sfp(uint8_t i2cif, uint8_t i2c_addr, struct s_sfpinfo * sfp)
{
	uint8_t sfpcount = 1;
	int8_t i, temp;
	struct s_sfpinfo dbsfp;

	for (i = 0; i < sfpcount; ++i) {
		temp = eeprom_get_sfp(i2cif, i2c_addr,
				      &dbsfp, 0, i);
		if (!i) {
			// first round: valid sfpcount is returned
			sfpcount = temp;
			if (sfpcount == 0 || sfpcount == 0xFF)
				return 0;
			else if (sfpcount < 0)
				return sfpcount;
		}
		if (!strncmp(dbsfp.pn, sfp->pn, 16)) {
			sfp->dTx = dbsfp.dTx;
			sfp->dRx = dbsfp.dRx;
			sfp->alpha = dbsfp.alpha;
			return 1;
		}
	}

	return 0;
}

/*
 * Phase transition ("calibration" file)
 */
#define VALIDITY_BIT 0x80000000
int8_t eeprom_phtrans(uint8_t i2cif, uint8_t i2c_addr, uint32_t * valp,
		      uint8_t write)
{
	int ret = -1;
	uint32_t value;

	if (sdbfs_open_id(&wrc_sdb, SDB_VENDOR, SDB_DEV_CALIB) < 0)
		return -1;
	if (write) {
		value = *valp | VALIDITY_BIT;
		if (sdbfs_fwrite(&wrc_sdb, 0, &value, sizeof(value))
		    != sizeof(value))
			goto out;
		ret = 1;
	} else {
		if (sdbfs_fread(&wrc_sdb, 0, &value, sizeof(value))
		    != sizeof(value))
			goto out;
		*valp = value & ~VALIDITY_BIT;
		ret = (value & VALIDITY_BIT) != 0;
	}
out:
	sdbfs_close(&wrc_sdb);
	return ret;
}

/*
 * The init script area consist of 2-byte size field and a set of
 * shell commands separated with '\n' character.
 *
 * -------------------
 * | bytes used (2B) |
 * ------------------------------------------------
 * | shell commands separated with '\n'.....      |
 * |                                              |
 * |                                              |
 * ------------------------------------------------
 */

int8_t eeprom_init_erase(uint8_t i2cif, uint8_t i2c_addr)
{
	uint16_t used = 0;
	int ret;

	if (sdbfs_open_id(&wrc_sdb, SDB_VENDOR, SDB_DEV_INIT) < 0)
		return -1;
	ret = sdbfs_fwrite(&wrc_sdb, 0, &used, sizeof(used));
	sdbfs_close(&wrc_sdb);
	return ret == sizeof(used) ? 0 : -1;
}

/*
 * Appends a new shell command at the end of boot script
 */
int8_t eeprom_init_add(uint8_t i2cif, uint8_t i2c_addr, const char *args[])
{
	int len, i = 1; /* args[0] is "add" */
	uint8_t separator = ' ';
	uint16_t used, readback;
	int ret = -1;

	if (sdbfs_open_id(&wrc_sdb, SDB_VENDOR, SDB_DEV_INIT) < 0)
		return -1;
	if (sdbfs_fread(&wrc_sdb, 0, &used, sizeof(used)) != sizeof(used))
		goto out;
	if (used > 256 /* 0xffff or wrong */)
		used = 0;

	while (args[i] != NULL) {
		len = strlen(args[i]);
		if (sdbfs_fwrite(&wrc_sdb, sizeof(used) + used,
				 (void *)args[i], len) != len)
			goto out;
		used += len;
		if (sdbfs_fwrite(&wrc_sdb, sizeof(used) + used,
				&separator, sizeof(separator))
		    != sizeof(separator))
			goto out;
		++used;
		++i;
	}
	/*  At end of command, replace last separator with '\n' */
	separator = '\n';
	if (sdbfs_fwrite(&wrc_sdb, sizeof(used) + used - 1,
			&separator, sizeof(separator)) != sizeof(separator))
		goto out;
	/* and finally update the size of the script */
	if (sdbfs_fwrite(&wrc_sdb, 0, &used, sizeof(used)) != sizeof(used))
		goto out;

	if (sdbfs_fread(&wrc_sdb, 0, &readback, sizeof(readback))
	    != sizeof(readback))
		goto out;

	ret = 0;
out:
	sdbfs_close(&wrc_sdb);
	return ret;
}

int32_t eeprom_init_show(uint8_t i2cif, uint8_t i2c_addr)
{
	int i, ret = -1;
	uint16_t used;
	uint8_t byte;

	if (sdbfs_open_id(&wrc_sdb, SDB_VENDOR, SDB_DEV_INIT) < 0)
		return -1;
	i = sdbfs_fread(&wrc_sdb, 0, &used, sizeof(used));
	if (i != sizeof(used))
		goto out;
	/* sdbfs configuration sets it to 256: if insanely large refuse it */
	if (used == 0 || used > 256 /* 0xffff or wrong */) {
		pp_printf("Empty init script...\n");
		goto out_ok;
	}

	/* Just read and print to the screen char after char */
	for (i = 0; i < used; ++i) {
		if (sdbfs_fread(&wrc_sdb, -1 /* sequentially */, &byte, 1) != 1)
			goto out;
		pp_printf("%c", byte);
	}
out_ok:
	ret = 0;
out:
	sdbfs_close(&wrc_sdb);
	return ret;
}

int8_t eeprom_init_readcmd(uint8_t i2cif, uint8_t i2c_addr, uint8_t *buf,
			   uint8_t bufsize, uint8_t next)
{
	int i = 0, ret = -1;
	uint16_t used;
	static uint16_t ptr;

	if (sdbfs_open_id(&wrc_sdb, SDB_VENDOR, SDB_DEV_INIT) < 0)
		return -1;
	if (sdbfs_fread(&wrc_sdb, 0, &used, sizeof(used)) != sizeof(used))
		goto out;

	if (next == 0)
		ptr = sizeof(used);
	if (ptr - sizeof(used) >= used)
		return 0;
	do {
		if (ptr - sizeof(used) > bufsize)
			goto out;
		if (sdbfs_fread(&wrc_sdb, (ptr++),
				&buf[i], sizeof(char)) != sizeof(char))
			goto out;
	} while (buf[i++] != '\n');
	ret = i;
out:
	sdbfs_close(&wrc_sdb);
	return ret;
}
