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
