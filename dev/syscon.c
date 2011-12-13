#include "syscon.h"

/****************************
 *        TIMER
 ***************************/
void timer_init(uint32_t enable)
{
  if(enable)
    syscon->TCR |= SYSC_TCR_ENABLE; 
  else
    syscon->TCR &= !SYSC_TCR_ENABLE;
}

uint32_t timer_get_tics()
{
  return syscon->TVR;
}

void timer_delay(uint32_t how_long)
{
  uint32_t t_start;

  timer_init(1);
  do
  {
    t_start = timer_get_tics();
  } while(t_start > UINT32_MAX - how_long); //in case of overflow

  while(t_start + how_long > timer_get_tics());
}
