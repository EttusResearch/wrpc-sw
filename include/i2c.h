#ifndef __I2C_H
#define __I2C_H

uint8_t mi2c_devprobe(uint8_t i2cif, uint8_t i2c_addr);
void mi2c_init(uint8_t i2cif);
void mi2c_start(uint8_t i2cif);
void mi2c_repeat_start(uint8_t i2cif);
void mi2c_stop(uint8_t i2cif);
void mi2c_get_byte(uint8_t i2cif, unsigned char *data, uint8_t last);
unsigned char mi2c_put_byte(uint8_t i2cif, unsigned char data);

void mi2c_delay();
//void mi2c_scan(uint8_t i2cif);

#endif
