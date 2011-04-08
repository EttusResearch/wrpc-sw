#ifndef __SOFTPLL_H
#define __SOFTPLL_H

void softpll_enable();
int softpll_check_lock();
void softpll_disable();
int softpll_busy();
void softpll_set_phase(int ps);

#endif

