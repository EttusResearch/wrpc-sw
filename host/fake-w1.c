#include <stdio.h>
#include <w1.h>

struct w1_bus wrpc_w1_bus;

void wrpc_w1_init(void)
{ printf("%s\n", __func__); }

int w1_scan_bus(struct w1_bus *bus)
{ printf("%s\n", __func__); return 0; }

int w1_read_eeprom_bus(struct w1_bus *bus,
			    int offset, uint8_t *buffer, int blen)
{ return -1; }

int w1_write_eeprom_bus(struct w1_bus *bus,
			     int offset, const uint8_t *buffer, int blen)
{ return -1; }

int w1_erase_eeprom_bus(struct w1_bus *bus, int offset, int blen)
{ return -1; }
