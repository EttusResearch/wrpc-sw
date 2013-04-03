/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2013 CERN (www.cern.ch)
 * Author: Alessandro Rubini <rubini@gnudd.com>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <wrc.h>
#include <w1.h>

/* This used to be part of dev/onewire.c, but is not onewire-specific */

/* 0 = success, -1 = error */
int8_t get_persistent_mac(uint8_t portnum, uint8_t * mac)
{
	int i, class;
	uint64_t rom;

	for (i = 0; i < W1_MAX_DEVICES; i++) {
		class = w1_class(wrpc_w1_bus.devs + i);
		if (class != 0x28 && class != 0x42)
			continue;
		rom = wrpc_w1_bus.devs[i].rom;
		mac[3] = rom >> 24;
		mac[4] = rom >> 16;
		mac[5] = rom >> 8;
		return 0;
	}
	return -1;

	/* FIXME: add eeprom support */

}

/* 0 = success, -1 = error */
int8_t set_persistent_mac(uint8_t portnum, uint8_t * mac)
{
	/* FIXME: add eeprom support */
	return -1;
}

