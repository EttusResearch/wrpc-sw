#ifndef __PPS_GEN_H
#define __PPS_GEN_H

#include <inttypes.h>

void pps_gen_init();
void pps_gen_adjust_nsec(int32_t how_much);
void shw_pps_gen_adjust_utc(int32_t how_much);
int pps_gen_busy();
void pps_gen_get_time(uint32_t *utc, uint32_t *cntr_nsec);

#endif
