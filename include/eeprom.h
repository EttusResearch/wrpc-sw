#ifndef __EEPROM_H
#define __EEPROM_H

#define SFP_SECTION_PATTERN 0xdeadbeef
#define SFPINFO_MAX 4

__attribute__ ((packed)) struct s_sfpinfo
{
  char pn[16];
  int32_t alpha;
  int32_t deltaTx;
  int32_t deltaRx;
};

int eeprom_read(uint8_t i2cif, uint8_t i2c_addr, uint32_t offset, uint8_t *buf, size_t size);
int eeprom_write(uint8_t i2cif, uint8_t i2c_addr, uint32_t offset, uint8_t *buf, size_t size);


int32_t eeprom_sfp_section(uint8_t i2cif, uint8_t i2c_addr, size_t size, uint16_t *section_sz);
int8_t eeprom_get_sfpinfo(uint8_t i2cif, uint8_t i2c_addr, uint32_t offset, struct s_sfpinfo *sfpinfo, uint16_t section_sz);
int8_t access_eeprom(char *sfp_pn, int32_t *alpha, int32_t *deltaTx, int32_t *deltaRx);

#endif
