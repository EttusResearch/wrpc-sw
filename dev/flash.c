/*
 *==============================================================================
 * CERN (BE-CO-HT)
 * Source file for M25P flash controller
 *==============================================================================
 *
 * author: Theodor Stana (t.stana@cern.ch)
 *
 * date of creation: 2013-09-25
 *
 * version: 1.0
 *
 * description:
 *
 * dependencies:
 *
 * references:
 *
 *==============================================================================
 * GNU LESSER GENERAL PUBLIC LICENSE
 *==============================================================================
 * This source file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation; either version 2.1 of the License, or (at your
 * option) any later version. This source is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details. You should have
 * received a copy of the GNU Lesser General Public License along with this
 * source; if not, download it from http://www.gnu.org/licenses/lgpl-2.1.html
 *==============================================================================
 * last changes:
 *    2013-09-25   Theodor Stana     t.stana@cern.ch     File created
 *==============================================================================
 * TODO: -
 *==============================================================================
 */

#include "flash.h"
#include "types.h"
#include "syscon.h"

#define SDBFS_BIG_ENDIAN
#include "libsdbfs.h"

#include <wrc.h>

/*****************************************************************************/
/* 		Flash interface					     	     */
/*****************************************************************************/
/*
 * Bit-bang SPI transfer function
 */
static uint8_t bbspi_transfer(int cspin, uint8_t val)
{
	int i, retval = 0;
	gpio_out(GPIO_SPI_NCS, cspin);
	for (i = 0; i < 8; i++) {
		gpio_out(GPIO_SPI_SCLK, 0);
		if (val & 0x80) {
			gpio_out(GPIO_SPI_MOSI, 1);
		}
		else {
			gpio_out(GPIO_SPI_MOSI, 0);
		}
		gpio_out(GPIO_SPI_SCLK, 1);
		retval <<= 1;
		retval |= gpio_in(GPIO_SPI_MISO);
		val <<= 1;
	}

	gpio_out(GPIO_SPI_SCLK, 0);

	return retval;
}

/*
 * Init function (just set the SPI pins for idle)
 */
void flash_init()
{
	gpio_out(GPIO_SPI_NCS, 1);
	gpio_out(GPIO_SPI_SCLK, 0);
	gpio_out(GPIO_SPI_MOSI, 0);
}

/*
 * Write data to flash chip
 */
int flash_write(uint32_t addr, uint8_t *buf, int count)
{
	int i;

	bbspi_transfer(1,0);
	bbspi_transfer(0,0x06);
	bbspi_transfer(1,0);
	bbspi_transfer(0,0x02);
	bbspi_transfer(0,(addr & 0xFF0000) >> 16);
	bbspi_transfer(0,(addr & 0xFF00) >> 8);
	bbspi_transfer(0,(addr & 0xFF));
	for ( i = 0; i < count; i++ ) {
		bbspi_transfer(0,buf[i]);
	}
	bbspi_transfer(1,0);

	return count;
}

/*
 * Read data from flash
 */
int flash_read(uint32_t addr, uint8_t *buf, int count)
{
	int i;
	bbspi_transfer(1,0);
	bbspi_transfer(0,0x0b);
	bbspi_transfer(0,(addr & 0xFF0000) >> 16);
	bbspi_transfer(0,(addr & 0xFF00) >> 8);
	bbspi_transfer(0,(addr & 0xFF));
	bbspi_transfer(0,0);
	for ( i = 0; i < count; i++ ) {
		buf[i] = bbspi_transfer(0, 0);
	}
	bbspi_transfer(1,0);

	return count;
}

/*
 * Sector erase
 */
void flash_serase(uint32_t addr)
{
	bbspi_transfer(1,0);
	bbspi_transfer(0,0x06);
	bbspi_transfer(1,0);
	bbspi_transfer(0,0xD8);
	bbspi_transfer(0,(addr & 0xFF0000) >> 16);
	bbspi_transfer(0,(addr & 0xFF00) >> 8);
	bbspi_transfer(0,(addr & 0xFF));
	bbspi_transfer(1,0);
}

/*
 * Bulk erase
 */
void
flash_berase()
{
	bbspi_transfer(1,0);
	bbspi_transfer(0,0x06);
	bbspi_transfer(1,0);
	bbspi_transfer(0,0xc7);
	bbspi_transfer(1,0);
}

/*
 * Read status register
 */
uint8_t flash_rsr()
{
	uint8_t retval;
	bbspi_transfer(1,0);
	bbspi_transfer(0,0x05);
	retval = bbspi_transfer(0,0);
	bbspi_transfer(1,0);
	return retval;
}


/*****************************************************************************/
/* 			SDB						     */
/*****************************************************************************/

/* The sdb filesystem itself */
static struct sdbfs wrc_sdb = {
	.name = "wrpc-storage",
	.blocksize = 1, /* Not currently used */
	/* .read and .write according to device type */
};

/*
 * SDB read and write functions
 */
static int sdb_flash_read(struct sdbfs *fs, int offset, void *buf, int count)
{
	return flash_read(offset, buf, count);
}

static int sdb_flash_write(struct sdbfs *fs, int offset, void *buf, int count)
{
	return flash_write(offset, buf, count);
}


/*
 * A trivial dumper, just to show what's up in there
 */
static void flash_sdb_list(struct sdbfs *fs)
{
	struct sdb_device *d;
	int new = 1;
	while ( (d = sdbfs_scan(fs, new)) != NULL) {
		d->sdb_component.product.record_type = '\0';
		mprintf("file 0x%08x @ %4i, name %s\n",
			  (int)(d->sdb_component.product.device_id),
			  (int)(d->sdb_component.addr_first),
			  (char *)(d->sdb_component.product.name));
		new = 0;
	}
}

/*
 * Check for SDB presence on flash
 */
int flash_sdb_check()
{
	uint32_t magic = 0;
	int i;

	uint32_t entry_point[6] = {0x000000, 0x100, 0x200, 0x300, 0x170000, 0x2e0000};

	for (i = 0; i < ARRAY_SIZE(entry_point); i++) {
		flash_read(entry_point[i], (uint8_t *)&magic, 4);
		if (magic == SDB_MAGIC)
			break;
	}
	if (magic == SDB_MAGIC) {
		mprintf("Found SDB magic at address 0x%06X!\n", entry_point[i]);
		wrc_sdb.drvdata = NULL;
		wrc_sdb.entrypoint = entry_point[i];
		wrc_sdb.read = sdb_flash_read;
		wrc_sdb.write = sdb_flash_write;
		flash_sdb_list(&wrc_sdb);
	}
	return 0;
}
