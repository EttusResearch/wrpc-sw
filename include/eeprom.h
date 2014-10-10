#ifndef __EEPROM_H
#define __EEPROM_H

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

void eeprom_init(int i2cif, int i2c_addr);

int eeprom_sfpdb_erase(void);
int eeprom_match_sfp(struct s_sfpinfo *sfp);
int eeprom_get_sfp(struct s_sfpinfo * sfp,
                       uint8_t add, uint8_t pos);

int8_t eeprom_phtrans(uint8_t i2cif, uint8_t i2c_addr, uint32_t * val,
		      uint8_t write);

int8_t eeprom_init_erase(uint8_t i2cif, uint8_t i2c_addr);
int8_t eeprom_init_add(uint8_t i2cif, uint8_t i2c_addr, const char *args[]);
int32_t eeprom_init_show(uint8_t i2cif, uint8_t i2c_addr);
int8_t eeprom_init_readcmd(uint8_t i2cif, uint8_t i2c_addr, uint8_t *buf,
			   uint8_t bufsize, uint8_t next);


#endif
