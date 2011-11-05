#ifndef __SOFTPLL_H
#define __SOFTPLL_H

#define SOFTPLL_AUX_ENABLED 1
#define SOFTPLL_AUX_LOCKED 2

void softpll_enable();
int softpll_check_lock();
void softpll_disable();
int softpll_busy();
void softpll_set_phase(int ps);
int softpll_get_setpoint();
int softpll_get_aux_status();

#endif

