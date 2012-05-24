#ifndef __TIMER_H
#define __TIMER_H

#include "types.h"

#define TICS_PER_SECOND 1000

uint32_t timer_get_tics();
void timer_delay(uint32_t how_long);

#endif
