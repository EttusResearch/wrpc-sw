/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2013 CERN (www.cern.ch)
 * Author: Theodor Stana <t.stana@cern.ch>
 *
 * Released according to the GNU LGPL, version 2.1 or any later version.
 */
#include <wrc.h>
#include <flash.h>
#include <types.h>

#define SDBFS_BIG_ENDIAN
#include <libsdbfs.h>

/*
 * Delay function - limit SPI clock speed to 10 MHz
 */
static void delay()
{
	int i;
	for (i = 0; i < (int)(CPU_CLOCK/10000000); i++)
		asm volatile ("nop");
}

/*
 * Bit-bang SPI transfer function
 */
static uint8_t bbspi_transfer(int cspin, uint8_t val)
{
	int i;

	gpio_out(GPIO_SPI_NCS, cspin);
	delay();
	for (i = 0; i < 8; i++) {
		gpio_out(GPIO_SPI_SCLK, 0);
		if (val & 0x80) {
			gpio_out(GPIO_SPI_MOSI, 1);
		} else {
			gpio_out(GPIO_SPI_MOSI, 0);
		}
		delay();
		gpio_out(GPIO_SPI_SCLK, 1);
		val <<= 1;
		val |= gpio_in(GPIO_SPI_MISO);
		delay();
	}

	gpio_out(GPIO_SPI_SCLK, 0);

	return val;
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
		pp_printf("file 0x%08x @ %4i, name %19s\n",
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

	uint32_t entry_point[] = {
				0x000000,	// flash base
				0x100,		// second page in flash
				0x200,		// IPMI with MultiRecord
				0x300,		// IPMI with larger MultiRecord
				0x170000,	// after first FPGA bitstream
				0x2e0000	// after MultiBoot bitstream
				};

	for (i = 0; i < ARRAY_SIZE(entry_point); i++) {
		flash_read(entry_point[i], (uint8_t *)&magic, 4);
		if (magic == SDB_MAGIC)
			break;
	}
	if (i == ARRAY_SIZE(entry_point))
		return -1;

	pp_printf("Found SDB magic at address 0x%06x\n", entry_point[i]);
	wrc_sdb.drvdata = NULL;
	wrc_sdb.entrypoint = entry_point[i];
	wrc_sdb.read = sdb_flash_read;
	wrc_sdb.write = sdb_flash_write;
	flash_sdb_list(&wrc_sdb);
	return 0;
}
