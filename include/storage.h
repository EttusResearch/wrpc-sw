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

#include "sfp.h"

#define SFP_SECTION_PATTERN 0xdeadbeef

#if defined CONFIG_LEGACY_EEPROM

#define EE_BASE_CAL (4 * 1024)
#define EE_BASE_SFP (4 * 1024 + 4)
/* Limit SFPs to 3, see comments below why. */
#define SFPS_MAX 3
/* The definition of EE_BASE_INIT below is wrong! But kept for backward
 * compatibility. */
#define EE_BASE_INIT (4 * 1024 + 4 * 29)
/* It should be:
 * #define EE_BASE_INIT (EE_BASE_SFP + sizeof(sfpcount) + \
 *                       SFPS_MAX * sizeof(struct s_sfpinfo))
 * The used definition define the start of the init script 5 bytes
 * (sizeof(sfpcount) + sizeof(t24p)) before the end of SFP database.
 * To make the init script working during the update of old versions of wrpc
 * SFPS_MAX is limited to 3. Adding the 4th SFP will corrupt the init script
 * anyway.
 * If someone needs to have 4 SFPs in the database SFPS_MAX can be set to 4 and
 * the proper define should be used.
 * SDB is not affected by this bug.
 */
#endif

#if defined CONFIG_SDB_STORAGE
#define SFPS_MAX 4
#endif


#define EE_RET_I2CERR -1
#define EE_RET_DBFULL -2
#define EE_RET_CORRPT -3
#define EE_RET_POSERR -4

#ifdef CONFIG_GENSDBFS
#define HAS_GENSDBFS 1
#else
#define HAS_GENSDBFS 0
#endif

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
int storage_get_sfp(struct s_sfpinfo *sfp, uint8_t add, uint8_t pos);

int storage_phtrans(uint32_t *val, uint8_t write);

int storage_init_erase(void);
int storage_init_add(const char *args[]);
int storage_init_show(void);
int storage_init_readcmd(uint8_t *buf, uint8_t bufsize, uint8_t next);

struct storage_config {
	int memtype;
	int valid;
	uint32_t blocksize;
	uint32_t baseadr;
};

extern struct storage_config storage_cfg;

#define MEM_FLASH     0
#define MEM_EEPROM    1
#define MEM_1W_EEPROM 2
#define SDBFS_REC 5

int storage_read_hdl_cfg(void);

int storage_sdbfs_erase(int mem_type, uint32_t base_adr, uint32_t blocksize,
	uint8_t i2c_adr);
int storage_gensdbfs(int mem_type, uint32_t base_adr, uint32_t blocksize,
	uint8_t i2c_adr);

#endif
