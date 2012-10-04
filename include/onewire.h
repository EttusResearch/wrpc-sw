#ifndef PERSISTENT_MAC_H
#define PERSISTENT_MAC_H

#define ONEWIRE_PORT 0
#define EEPROM_MAC_PAGE 0
#define MAX_DEV1WIRE 8

#define FOUND_DS18B20 0x01

void own_scanbus(uint8_t portnum);
int16_t own_readtemp(uint8_t portnum, int16_t * temp, int16_t * t_frac);
/* 0 = success, -1 = error */
int8_t get_persistent_mac(uint8_t portnum, uint8_t * mac);
int8_t set_persistent_mac(uint8_t portnum, uint8_t * mac);

#endif
