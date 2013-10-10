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
#include "eeprom.h"
#include "board.h"
#include "syscon.h"

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

uint8_t has_eeprom = 0;

uint8_t eeprom_present(uint8_t i2cif, uint8_t i2c_addr)
{
	has_eeprom = 1;
	if (!mi2c_devprobe(i2cif, i2c_addr))
		if (!mi2c_devprobe(i2cif, i2c_addr))
			has_eeprom = 0;

	return 0;
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

int32_t eeprom_sfpdb_erase(uint8_t i2cif, uint8_t i2c_addr)
{
	uint8_t sfpcount = 0;

	//just a dummy function that writes '0' to sfp count field of the SFP DB
	if (eeprom_write(i2cif, i2c_addr, EE_BASE_SFP, &sfpcount,
			 sizeof(sfpcount)) != sizeof(sfpcount))
		return EE_RET_I2CERR;
	else
		return sfpcount;
}

int32_t eeprom_get_sfp(uint8_t i2cif, uint8_t i2c_addr, struct s_sfpinfo * sfp,
		       uint8_t add, uint8_t pos)
{
	static uint8_t sfpcount = 0;
	uint8_t i, chksum = 0;
	uint8_t *ptr;

	if (pos >= SFPS_MAX)
		return EE_RET_POSERR;	//position in database outside the range

	//read how many SFPs are in the database, but only in the first call (pos==0)
	if (!pos
	    && eeprom_read(i2cif, i2c_addr, EE_BASE_SFP, &sfpcount,
			   sizeof(sfpcount)) != sizeof(sfpcount))
		return EE_RET_I2CERR;

	if (add && sfpcount == SFPS_MAX)	//no more space in the database to add new SFPs
		return EE_RET_DBFULL;
	else if (!pos && !add && sfpcount == 0)	//there are no SFPs in the database to read
		return sfpcount;

	if (!add) {
		if (eeprom_read(i2cif, i2c_addr,
				EE_BASE_SFP + sizeof(sfpcount)
				+ pos * sizeof(struct s_sfpinfo),
				(uint8_t*)sfp, sizeof(struct s_sfpinfo))
		    != sizeof(struct s_sfpinfo) )
			return EE_RET_I2CERR;

		ptr = (uint8_t *) sfp;
		for (i = 0; i < sizeof(struct s_sfpinfo) - 1; ++i)	//'-1' because we do not include chksum in computation
			chksum =
			    (uint8_t) ((uint16_t) chksum + *(ptr++)) & 0xff;
		if (chksum != sfp->chksum)
			return EE_RET_CORRPT;
	} else {
		/*count checksum */
		ptr = (uint8_t *) sfp;
		for (i = 0; i < sizeof(struct s_sfpinfo) - 1; ++i)	//'-1' because we do not include chksum in computation
			chksum =
			    (uint8_t) ((uint16_t) chksum + *(ptr++)) & 0xff;
		sfp->chksum = chksum;
		/*add SFP at the end of DB */
		eeprom_write(i2cif, i2c_addr,
			     EE_BASE_SFP + sizeof(sfpcount)
			     + sfpcount * sizeof(struct s_sfpinfo),
			     (uint8_t *) sfp, sizeof(struct s_sfpinfo));
		sfpcount++;
		eeprom_write(i2cif, i2c_addr, EE_BASE_SFP, &sfpcount,
			     sizeof(sfpcount));
	}

	return sfpcount;
}

int8_t eeprom_match_sfp(uint8_t i2cif, uint8_t i2c_addr, struct s_sfpinfo * sfp)
{
	uint8_t sfpcount = 1;
	int8_t i, temp;
	struct s_sfpinfo dbsfp;

	for (i = 0; i < sfpcount; ++i) {
		temp = eeprom_get_sfp(WRPC_FMC_I2C, FMC_EEPROM_ADR,
				      &dbsfp, 0, i);
		if (!i) {
			sfpcount = temp;	//only in first round valid sfpcount is returned from eeprom_get_sfp
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

int8_t eeprom_phtrans(uint8_t i2cif, uint8_t i2c_addr, uint32_t * val,
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

int8_t eeprom_init_erase(uint8_t i2cif, uint8_t i2c_addr)
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
int8_t eeprom_init_add(uint8_t i2cif, uint8_t i2c_addr, const char *args[])
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

int32_t eeprom_init_show(uint8_t i2cif, uint8_t i2c_addr)
{
	uint16_t used, i;
	uint8_t byte;

	if (eeprom_read(i2cif, i2c_addr, EE_BASE_INIT, (uint8_t *) & used,
	     sizeof(used)) != sizeof(used))
		return EE_RET_I2CERR;

	if (used == 0 || used == 0xffff) {
		used = 0;	//this means the memory is blank
		mprintf("Empty init script...\n");
	}
	//just read and print to the screen char after char
	for (i = 0; i < used; ++i) {
		if (eeprom_read(i2cif, i2c_addr, EE_BASE_INIT + sizeof(used)
				+ i, &byte, sizeof(byte)) != sizeof(byte))
			return EE_RET_I2CERR;
		mprintf("%c", byte);
	}

	return 0;
}

int8_t eeprom_init_readcmd(uint8_t i2cif, uint8_t i2c_addr, uint8_t *buf,
			   uint8_t bufsize, uint8_t next)
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
int8_t set_persistent_mac(uint8_t portnum, uint8_t * mac)
{
	pp_printf("Can't save persistent MAC address\n");
	return -1;
}

int32_t get_persistent_mac(uint8_t portnum, uint8_t * mac)
{
	int i, class;
	uint64_t rom;

	pp_printf("%s: Using W1 serial number\n", __func__);
	for (i = 0; i < W1_MAX_DEVICES; i++) {
		class = w1_class(wrpc_w1_bus.devs + i);
		if (class != 0x28 && class != 0x42)
			continue;
		rom = wrpc_w1_bus.devs[i].rom;
		mac[3] = rom >> 24;
		mac[4] = rom >> 16;
		mac[5] = rom >> 8;
	}
	return 0;
}

#endif /* CONFIG_W1 */
