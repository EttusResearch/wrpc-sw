#include "board.h"

#include "syscon.h"

uint32_t timer_get_tics(void)
{
  return *(volatile uint32_t *) (BASE_TIMER);
}

void timer_delay(uint32_t tics)
{
  uint32_t t_start;

  t_start = timer_get_tics();

	if(t_start + tics < t_start)
		while(t_start + tics < timer_get_tics());

	while(t_start + tics > timer_get_tics());
}
