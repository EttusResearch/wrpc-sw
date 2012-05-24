#include "syscon.h"

struct s_i2c_if i2c_if[2] = { {SYSC_GPSR_FMC_SCL, SYSC_GPSR_FMC_SDA}, 
                              {SYSC_GPSR_SFP_SCL, SYSC_GPSR_SFP_SDA} };

/****************************
 *        TIMER
 ***************************/
void timer_init(uint32_t enable)
{
  mprintf("MEMSIZE %x\n", syscon->HWFR & 0xf);
  if(enable)
    syscon->TCR |= SYSC_TCR_ENABLE; 
  else
    syscon->TCR &= ~SYSC_TCR_ENABLE;
}

uint32_t timer_get_tics()
{
  return syscon->TVR;
}

void timer_delay(uint32_t how_long)
{
  uint32_t t_start;

//  timer_init(1);
  do
  {
    t_start = timer_get_tics();
  } while(t_start > UINT32_MAX - how_long); //in case of overflow

  while(t_start + how_long > timer_get_tics());
}
