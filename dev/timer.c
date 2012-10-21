/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2011 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include "board.h"

#include "timer.h"

uint32_t timer_get_tics()
{
	return *(volatile uint32_t *)(BASE_TIMER);
}

void timer_delay(uint32_t how_long)
{
	uint32_t t_start;

	t_start = timer_get_tics();

	while (t_start + how_long > timer_get_tics()) ;
}
