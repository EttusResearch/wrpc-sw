/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#ifndef __TYPES_H
#define __TYPES_H

#include <inttypes.h>

struct hw_timestamp {
	uint8_t valid;
	int ahead;
	uint64_t sec;
	uint32_t nsec;
	uint32_t phase;
};

#endif
