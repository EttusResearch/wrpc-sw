/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2011 CERN (www.cern.ch)
 * Author: Grzegorz Daniluk <grzegorz.daniluk@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include "syscon.h"

struct s_i2c_if i2c_if[2] = {
	{SYSC_GPSR_FMC_SCL, SYSC_GPSR_FMC_SDA},
	{SYSC_GPSR_SFP_SCL, SYSC_GPSR_SFP_SDA}
};

volatile struct SYSCON_WB *syscon;

/****************************
 *        TIMER
 ***************************/
void timer_init(uint32_t enable)
{
	syscon = (volatile struct SYSCON_WB *)BASE_SYSCON;

	if (enable)
		syscon->TCR |= SYSC_TCR_ENABLE;
	else
		syscon->TCR &= ~SYSC_TCR_ENABLE;
}

uint32_t timer_get_tics(void)
{
	return syscon->TVR;
}

void timer_delay(uint32_t tics)
{
	uint32_t t_end;

//  timer_init(1);

	t_end = timer_get_tics() + tics;
	while (time_before(timer_get_tics(), t_end))
	       ;
}
