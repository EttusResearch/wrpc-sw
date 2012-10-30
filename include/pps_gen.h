#ifndef __PPS_GEN_H
#define __PPS_GEN_H

#include <stdint.h>

#define PPSG_ADJUST_SEC 0x1
#define PPSG_ADJUST_NSEC 0x2

/* Initializes the PPS Generator. 0 on success, negative on failure. */
void shw_pps_gen_init(void);

/* Adjusts the <counter> (PPSG_ADJUST_SEC/NSEC) by (how_much) seconds/nanoseconds */
int shw_pps_gen_adjust(int counter, int64_t how_much);

/* Returns 1 when the PPS is busy adjusting its time counters, 0 if PPS gen idle */
int shw_pps_gen_busy(void);

/* Enables/disables PPS Generator PPS output */
int shw_pps_gen_enable_output(int enable);

/* Reads the current time and stores at <seconds,nanoseconds>. */
void shw_pps_gen_get_time(uint64_t * seconds, uint32_t * nanoseconds);

/* Sets the time to <seconds,nanoseconds>. */
void shw_pps_gen_set_time(uint64_t seconds, uint32_t nanoseconds);

#endif
