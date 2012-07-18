#ifndef __EEPROM_H
#define __EEPROM_H

#define SFP_SECTION_PATTERN 0xdeadbeef
#define SFPS_MAX 4
#define SFP_PN_LEN 16
#define EE_BASE_SFP 4*1024
#define EE_BASE_INIT 4*1024+SFPS_MAX*29

#define EE_RET_I2CERR -1
#define EE_RET_DBFULL -2
#define EE_RET_CHKSUM -3
#define EE_RET_POSERR -4

extern int32_t sfp_alpha;
extern int32_t sfp_deltaTx;
extern int32_t sfp_deltaRx;

struct s_sfpinfo
{
  char pn[SFP_PN_LEN];
  int32_t alpha;
  int32_t dTx;
  int32_t dRx;
  uint8_t chksum;
} __attribute__((__packed__));

int eeprom_read(uint8_t i2cif, uint8_t i2c_addr, uint32_t offset, uint8_t *buf, size_t size);
int eeprom_write(uint8_t i2cif, uint8_t i2c_addr, uint32_t offset, uint8_t *buf, size_t size);


int32_t eeprom_sfpdb_erase(uint8_t i2cif, uint8_t i2c_addr);
int32_t eeprom_sfp_section(uint8_t i2cif, uint8_t i2c_addr, size_t size, uint16_t *section_sz);
int8_t eeprom_match_sfp(uint8_t i2cif, uint8_t i2c_addr, struct s_sfpinfo* sfp);
int8_t eeprom_get_sfpinfo(uint8_t i2cif, uint8_t i2c_addr, uint32_t offset, struct s_sfpinfo *sfpinfo, uint16_t section_sz);
int8_t access_eeprom(char *sfp_pn, int32_t *alpha, int32_t *deltaTx, int32_t *deltaRx);

#endif
