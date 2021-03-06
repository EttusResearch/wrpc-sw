/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include "board.h"

#include "syscon.h"

uint32_t timer_get_tics(void)
{
  return *(volatile uint32_t *) (BASE_TIMER);
}

void timer_delay(uint32_t tics)
{
	uint32_t t_end = timer_get_tics() + tics;

	while (time_before(timer_get_tics(), t_end))
	       ;
}
