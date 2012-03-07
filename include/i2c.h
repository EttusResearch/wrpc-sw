#ifndef __I2C_H
#define __I2C_H


void mi2c_init(uint8_t i2cif);
void mi2c_start(uint8_t i2cif);
void mi2c_repeat_start(uint8_t i2cif);
void mi2c_stop(uint8_t i2cif);
void mi2c_get_byte(uint8_t i2cif, unsigned char *data, uint8_t last);
unsigned char mi2c_put_byte(uint8_t i2cif, unsigned char data);

void mi2c_delay();
void mi2c_scan(uint8_t i2cif);

int eeprom_read(uint8_t i2cif, uint8_t i2c_addr, uint32_t offset, uint8_t *buf, size_t size);
uint16_t eeprom_sfp_section(uint8_t i2cif, uint8_t i2c_addr, size_t size, uint16_t *section_sz);
int eeprom_write(uint8_t i2cif, uint8_t i2c_addr, uint32_t offset, uint8_t *buf, size_t size);

#endif
