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

#include "types.h"
#include "i2c.h"
#include "storage.h"
#include "board.h"
#include "syscon.h"
#include "onewire.h"

/*
 * The SFP section is placed somewhere inside FMC EEPROM and it really does not
 * matter where (can be a binary data inside the Board Info section but can be
 * placed also outside the FMC standardized EEPROM structure. The only requirement
 * is that it starts with 0xdeadbeef pattern. The structure of SFP section is:
 *
 * ----------------------------------------------
 * | cal_ph_trans (4B) | SFP count (1B) |
 * --------------------------------------------------------------------------------------------
 * |   SFP(1) part number (16B)       | alpha (4B) | deltaTx (4B) | deltaRx (4B) | chksum(1B) |
 * --------------------------------------------------------------------------------------------
 * |   SFP(2) part number (16B)       | alpha (4B) | deltaTx (4B) | deltaRx (4B) | chksum(1B) |
 * --------------------------------------------------------------------------------------------
 * | (....)                           | (....)     | (....)       | (....)       | (...)      |
 * --------------------------------------------------------------------------------------------
 * |   SFP(count) part number (16B)   | alpha (4B) | deltaTx (4B) | deltaRx (4B) | chksum(1B) |
 * --------------------------------------------------------------------------------------------
 *
 * Fields description:
 * cal_ph_trans       - t2/t4 phase transition value (got from measure_t24p() ), contains
 *                      _valid_ bit (MSB) and 31 bits of cal_phase_transition value
 * count              - how many SFPs are described in the list (binary)
 * SFP(n) part number - SFP PN as read from SFP's EEPROM (e.g. AXGE-1254-0531)
 *                      (16 ascii chars)
 * checksum           - low order 8 bits of the sum of all bytes for the SFP(PN,alpha,dTx,dRx)
 *
 */

/*
 * The init script area consist of 2-byte size field and a set of shell commands
 * separated with '\n' character.
 *
 * -------------------
 * | bytes used (2B) |
 * ------------------------------------------------
 * | shell commands separated with '\n'.....      |
 * |                                              |
 * |                                              |
 * ------------------------------------------------
 */

#define SFP_DB_EMPTY 0xff

static uint8_t sfpcount = SFP_DB_EMPTY;

uint8_t has_eeprom = 0;

static int i2cif, i2c_addr; /* globals, using the names we always used */

void storage_init(int chosen_i2cif, int chosen_i2c_addr)
{
	/* Save these to globals, they are never passed any more */
	i2cif = chosen_i2cif;
	i2c_addr = chosen_i2c_addr;

	has_eeprom = 1;
	if (!mi2c_devprobe(i2cif, i2c_addr))
		if (!mi2c_devprobe(i2cif, i2c_addr))
			has_eeprom = 0;

	return;
}

static int eeprom_read(uint8_t i2cif, uint8_t i2c_addr, uint32_t offset,
		       uint8_t * buf, size_t size)
{
	int i;
	unsigned char c;

	if (!has_eeprom)
		return -1;

	mi2c_start(i2cif);
	if (mi2c_put_byte(i2cif, i2c_addr << 1) < 0) {
		mi2c_stop(i2cif);
		return -1;
	}
	mi2c_put_byte(i2cif, (offset >> 8) & 0xff);
	mi2c_put_byte(i2cif, offset & 0xff);
	mi2c_repeat_start(i2cif);
	mi2c_put_byte(i2cif, (i2c_addr << 1) | 1);
	for (i = 0; i < size - 1; ++i) {
		mi2c_get_byte(i2cif, &c, 0);
		*buf++ = c;
	}
	mi2c_get_byte(i2cif, &c, 1);
	*buf++ = c;
	mi2c_stop(i2cif);

	return size;
}

static int eeprom_write(uint8_t i2cif, uint8_t i2c_addr, uint32_t offset,
		 uint8_t * buf, size_t size)
{
	int i, busy;

	if (!has_eeprom)
		return -1;

	for (i = 0; i < size; i++) {
		mi2c_start(i2cif);

		if (mi2c_put_byte(i2cif, i2c_addr << 1) < 0) {
			mi2c_stop(i2cif);
			return -1;
		}
		mi2c_put_byte(i2cif, (offset >> 8) & 0xff);
		mi2c_put_byte(i2cif, offset & 0xff);
		mi2c_put_byte(i2cif, *buf++);
		offset++;
		mi2c_stop(i2cif);

		do {		/* wait until the chip becomes ready */
			mi2c_start(i2cif);
			busy = mi2c_put_byte(i2cif, i2c_addr << 1);
			mi2c_stop(i2cif);
		} while (busy);

	}
	return size;
}

int32_t storage_sfpdb_erase(void)
{
	sfpcount = 0;

	//just a dummy function that writes '0' to sfp count field of the SFP DB
	if (eeprom_write(i2cif, i2c_addr, EE_BASE_SFP, &sfpcount,
			 sizeof(sfpcount)) != sizeof(sfpcount))
		return EE_RET_I2CERR;
	else
		return sfpcount;
}

static uint8_t sfp_chksum(uint8_t *ptr)
{
	int i;
	uint8_t chksum = 0;

	/* '-1' because we do not include chksum in computation */
	for (i = 0; i < sizeof(struct s_sfpinfo) - 1; ++i)
		chksum = (uint8_t) ((uint16_t) chksum + *(ptr++)) & 0xff;
	return chksum;
}

int storage_get_sfp(struct s_sfpinfo *sfp, uint8_t oper, uint8_t pos)
{
	uint8_t i;
	struct s_sfpinfo dbsfp;

	if (pos >= SFPS_MAX) {
		/* position in database outside the range */
		return EE_RET_POSERR;
	}

	/* Read how many SFPs are in the database, but only in the first call
	 */
	if (sfpcount == SFP_DB_EMPTY
	    && eeprom_read(i2cif, i2c_addr, EE_BASE_SFP, &sfpcount,
			   sizeof(sfpcount)) != sizeof(sfpcount))
		return EE_RET_I2CERR;

	/* for not written flash set sfpcount to 0 */
	if (sfpcount == SFP_DB_EMPTY)
		sfpcount = 0;

	if (oper == SFP_GET) {
		if (sfpcount == 0) {
			/* There are no SFPs in the database to read */
			return 0;
		}

		if (eeprom_read(i2cif, i2c_addr,
				EE_BASE_SFP + sizeof(sfpcount)
				+ pos * sizeof(struct s_sfpinfo),
				(uint8_t*)sfp, sizeof(struct s_sfpinfo))
		    != sizeof(struct s_sfpinfo) )
			return EE_RET_I2CERR;

		if (sfp_chksum((uint8_t *)sfp) != sfp->chksum)
			return EE_RET_CORRPT;
	}
	if (oper == SFP_ADD) {
		for (i = 0; i < sfpcount; i++) {
			if (eeprom_read(i2cif, i2c_addr,
				EE_BASE_SFP + sizeof(sfpcount)
				+ i * sizeof(struct s_sfpinfo),
				(uint8_t *)&dbsfp, sizeof(struct s_sfpinfo))
			    != sizeof(struct s_sfpinfo))
				return EE_RET_I2CERR;
			if (!strncmp(dbsfp.pn, sfp->pn, 16)) { /* sfp matched */
				pp_printf("Update existing SFP entry\n");
				break;
			}
		}

		if (i >= SFPS_MAX) { /* database is full */
			return EE_RET_DBFULL;
		}

		/* Count checksum */
		sfp->chksum = sfp_chksum((uint8_t *)sfp);

		/* Add an entry at the given pos in the DB */
		eeprom_write(i2cif, i2c_addr,
			     EE_BASE_SFP + sizeof(sfpcount)
			     + i * sizeof(struct s_sfpinfo),
			     (uint8_t *) sfp, sizeof(struct s_sfpinfo));
		if (i >= sfpcount) {
			pp_printf("Adding new SFP entry\n");
			/* We're adding a new entry, update sfpcount */
			sfpcount++;
			eeprom_write(i2cif, i2c_addr, EE_BASE_SFP, &sfpcount,
				    sizeof(sfpcount));
		}
	}

	return sfpcount;
}

int storage_match_sfp(struct s_sfpinfo * sfp)
{
	int8_t i;
	int sfp_cnt = 1;
	struct s_sfpinfo dbsfp;

	for (i = 0; i < sfp_cnt; ++i) {
		sfp_cnt = storage_get_sfp(&dbsfp, SFP_GET, i);
		if (sfp_cnt <= 0)
			return sfp_cnt;

		if (!strncmp(dbsfp.pn, sfp->pn, 16)) {
			sfp->dTx = dbsfp.dTx;
			sfp->dRx = dbsfp.dRx;
			sfp->alpha = dbsfp.alpha;
			return 1;
		}
	}

	return 0;
}

int storage_phtrans(uint32_t * val,
		      uint8_t write)
{
	int8_t ret;
	if (write) {
		*val |= (1 << 31);
		if (eeprom_write(i2cif, i2c_addr, EE_BASE_CAL, (uint8_t *) val,
		     sizeof(*val)) != sizeof(*val))
			ret = EE_RET_I2CERR;
		else
			ret = 1;

		*val &= 0x7fffffff;	//return ph_trans value without validity bit
		return ret;
	} else {
		if (eeprom_read(i2cif, i2c_addr, EE_BASE_CAL, (uint8_t *) val,
		     sizeof(*val)) != sizeof(*val))
			return EE_RET_I2CERR;

		if (!(*val & (1 << 31)))
			return 0;

		*val &= 0x7fffffff;	//return ph_trans value without validity bit
		return 1;
	}
}

int storage_init_erase(void)
{
	uint16_t used = 0;

	if (eeprom_write(i2cif, i2c_addr, EE_BASE_INIT, (uint8_t *) & used,
	     sizeof(used)) != sizeof(used))
		return EE_RET_I2CERR;
	else
		return used;
}

/*
 * Appends a new shell command at the end of boot script
 */
int storage_init_add(const char *args[])
{
	uint8_t i = 1;
	uint8_t separator = ' ';
	uint16_t used, readback;

	if (eeprom_read(i2cif, i2c_addr, EE_BASE_INIT, (uint8_t *) & used,
	     sizeof(used)) != sizeof(used))
		return EE_RET_I2CERR;

	if (used == 0xffff)
		used = 0;	//this means the memory is blank

	while (args[i] != '\0') {
		if (eeprom_write(i2cif, i2c_addr, EE_BASE_INIT + sizeof(used)
				 + used, (uint8_t *) args[i], strlen(args[i]))
		    != strlen(args[i]))
			return EE_RET_I2CERR;
		used += strlen(args[i]);
		if (eeprom_write(i2cif, i2c_addr, EE_BASE_INIT + sizeof(used)
				 + used, &separator, sizeof(separator))
		    != sizeof(separator))
			return EE_RET_I2CERR;
		++used;
		++i;
	}
	//the end of the command, replace last separator with '\n'
	separator = '\n';
	if (eeprom_write(i2cif, i2c_addr, EE_BASE_INIT + sizeof(used) + used-1,
	     &separator, sizeof(separator)) != sizeof(separator))
		return EE_RET_I2CERR;
	//and finally update the size of the script
	if (eeprom_write(i2cif, i2c_addr, EE_BASE_INIT, (uint8_t *) & used,
	     sizeof(used)) != sizeof(used))
		return EE_RET_I2CERR;

	if (eeprom_read(i2cif, i2c_addr, EE_BASE_INIT, (uint8_t *) & readback,
	     sizeof(readback)) != sizeof(readback))
		return EE_RET_I2CERR;

	return 0;
}

int storage_init_show(void)
{
	uint16_t used, i;
	uint8_t byte;

	if (eeprom_read(i2cif, i2c_addr, EE_BASE_INIT, (uint8_t *) & used,
	     sizeof(used)) != sizeof(used))
		return EE_RET_I2CERR;

	if (used == 0 || used == 0xffff) {
		used = 0;	//this means the memory is blank
		pp_printf("Empty init script...\n");
	}
	//just read and print to the screen char after char
	for (i = 0; i < used; ++i) {
		if (eeprom_read(i2cif, i2c_addr, EE_BASE_INIT + sizeof(used)
				+ i, &byte, sizeof(byte)) != sizeof(byte))
			return EE_RET_I2CERR;
		pp_printf("%c", byte);
	}

	return 0;
}

int storage_init_readcmd(uint8_t *buf, uint8_t bufsize, uint8_t next)
{
	static uint16_t ptr;
	static uint16_t used = 0;
	uint8_t i = 0;

	if (next == 0) {
		if (eeprom_read(i2cif, i2c_addr, EE_BASE_INIT,
				(uint8_t *) & used, sizeof(used))
		    != sizeof(used))
			return EE_RET_I2CERR;
		ptr = sizeof(used);
	}

	if (ptr - sizeof(used) >= used)
		return 0;

	do {
		if (ptr - sizeof(used) > bufsize)
			return EE_RET_CORRPT;
		if (eeprom_read(i2cif, i2c_addr, EE_BASE_INIT + (ptr++),
				&buf[i], sizeof(char)) != sizeof(char))
			return EE_RET_I2CERR;
	} while (buf[i++] != '\n');

	return i;
}

#ifdef CONFIG_W1
#include <w1.h>
/*
 * The "persistent mac" thing was part of onewire.c, and it's not something
 * I can understand, I admit.
 *
 * Now, cards with w1 eeprom already use sdb-eeprom.c as far as I
 * know, while this eeprom.c file is still selected for devices that
 * have i2c eeprom and never saved a persistent mac address. But maybe
 * they prefer w1 for temperature (well, sockitowm will go)
 */
int set_persistent_mac(uint8_t portnum, uint8_t * mac)
{
	pp_printf("Can't save persistent MAC address\n");
	return -1;
}

int get_persistent_mac(uint8_t portnum, uint8_t * mac)
{
	int i, class;
	uint64_t rom;

	pp_printf("%s: Using W1 serial number\n", __func__);
	for (i = 0; i < W1_MAX_DEVICES; i++) {
		class = w1_class(wrpc_w1_bus.devs + i);
		if (class != 0x28 && class != 0x42)
			continue;
		rom = wrpc_w1_bus.devs[i].rom;
		mac[0] = 0x22;
		mac[1] = 0x33;
		mac[2] = rom >> 32;
		mac[3] = rom >> 24;
		mac[4] = rom >> 16;
		mac[5] = rom >> 8;
	}
	return 0;
}

#endif /* CONFIG_W1 */
