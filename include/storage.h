/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012 - 2015 CERN (www.cern.ch)
 * Author: Grzegorz Daniluk <grzegorz.daniluk@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#ifndef __STORAGE_H
#define __STORAGE_H

#define SFP_SECTION_PATTERN 0xdeadbeef
#define SFPS_MAX 4
#define SFP_PN_LEN 16
#define EE_BASE_CAL 4*1024
#define EE_BASE_SFP 4*1024+4
#define EE_BASE_INIT 4*1024+SFPS_MAX*29

#define EE_RET_I2CERR -1
#define EE_RET_DBFULL -2
#define EE_RET_CORRPT -3
#define EE_RET_POSERR -4

extern int32_t sfp_alpha;
extern int32_t sfp_deltaTx;
extern int32_t sfp_deltaRx;
extern uint32_t cal_phase_transition;
extern uint8_t has_eeprom;

struct s_sfpinfo {
	char pn[SFP_PN_LEN];
	int32_t alpha;
	int32_t dTx;
	int32_t dRx;
	uint8_t chksum;
} __attribute__ ((__packed__));

void storage_init(int i2cif, int i2c_addr);

int storage_sfpdb_erase(void);
int storage_match_sfp(struct s_sfpinfo *sfp);
int storage_get_sfp(struct s_sfpinfo * sfp,
                       uint8_t add, uint8_t pos);

int storage_phtrans(uint32_t * val,
		      uint8_t write);

int storage_init_erase(void);
int storage_init_add(const char *args[]);
int storage_init_show(void);
int storage_init_readcmd(uint8_t *buf, uint8_t bufsize, uint8_t next);

#endif
