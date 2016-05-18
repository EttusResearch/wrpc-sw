#include "softpll_ng.h"

void spll_very_init(void)
{}

void spll_init(int mode, int ref_channel, int align_pps)
{}

void spll_enable_ptracker(int ref_channel, int enable)
{}

void spll_set_phase_shift(int out_channel, int32_t value_picoseconds)
{}

int spll_shifter_busy(int out_channel)
{ return 1; }

int spll_check_lock(int out_channel)
{ return 0; }

int spll_update(void)
{ return 0; }

int spll_read_ptracker(int ref_channel, int32_t *phase_ps, int *enabled)
{ return 0; }
