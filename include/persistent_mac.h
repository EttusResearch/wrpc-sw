#ifndef PERSISTENT_MAC_H
#define PERSISTENT_MAC_H

#define ONEWIRE_PORT 0
#define EEPROM_MAC_PAGE 0

/* 0 = success, -1 = error */
int get_persistent_mac(unsigned char* mac);
int set_persistent_mac(unsigned char* mac);

#endif
