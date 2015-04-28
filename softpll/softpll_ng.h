/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2010 - 2013 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

/* softpll_ng.h: public SoftPLL API header */

#ifndef __SOFTPLL_NG_H
#define __SOFTPLL_NG_H

#include <stdint.h>

/* SoftPLL operating modes, for mode parameter of spll_init(). */

/* Grand Master - lock to 10 MHz external reference */
#define SPLL_MODE_GRAND_MASTER 1

/* Free running master - 125 MHz refrence free running, DDMTD locked to it */
#define SPLL_MODE_FREE_RUNNING_MASTER 2

/* Slave mode - 125 MHz reference locked to one of the input clocks */
#define SPLL_MODE_SLAVE 3

/* Disabled mode: SoftPLL inactive */
#define SPLL_MODE_DISABLED 4

/* Shortcut for 'channels' parameter in various API functions to perform operation on all channels */
#define SPLL_ALL_CHANNELS 0xffffffff

/* Aux clock flags */
#define SPLL_AUX_ENABLED (1<<0) /* Locking the particular aux channel to the WR reference is enabled */
#define SPLL_AUX_LOCKED (1<<1)  /* The particular aux clock is already locked to WR reference */

/* Phase detector types */
#define SPLL_PD_DDMTD 0
#define SPLL_PD_BANGBANG 1

/* Channels for spll_measure_frequency() */
#define SPLL_OSC_REF 0
#define SPLL_OSC_DMTD 1
#define SPLL_OSC_EXT 2

/* Note on channel naming:
 - ref_channel means a PHY recovered clock input. There can be one (as in WR core) or more (WR switch).
 - out_channel means an output channel, which represents PLL feedback signal from a local, tunable oscillator. Every SPLL implementation
   has at least one output channel, connected to the 125 / 62.5 MHz transceiver (WR) reference. This channel has always 
   index 0 and is compared against all reference channels by the phase tracking mechanism.
*/

/* PUBLIC API */

/* 
Initializes the SoftPLL to work in mode (mode). Extra parameters depend on choice of the mode:
- for SPLL_MODE_GRAND_MASTER: non-zero (align_pps) value enables realignment of the WR reference rising edge to the 
  rising edge of 10 MHz external clock that comes immediately after a PPS pulse
- for SPLL_MODE_SLAVE: (ref_channel) indicates the reference channel to which we are locking our PLL. 
*/
void spll_init(int mode, int ref_channel, int align_pps);

/* Disables the SoftPLL and cleans up stuff */
void spll_shutdown();

/* Returns number of reference and output channels implemented in HW. */
void spll_get_num_channels(int *n_ref, int *n_out);

/* Starts locking output channel (out_channel) */
void spll_start_channel(int out_channel);

/* Stops locking output channel (out_channel) */
void spll_stop_channel(int out_channel);

/* Returns non-zero if output channel (out_channel) is locked to a WR reference */
int spll_check_lock(int out_channel);

/* Sets phase setpoint for given output channel. */
void spll_set_phase_shift(int out_channel, int32_t value_picoseconds);

/* Retreives the current phase shift and desired setpoint for given output channel */
void spll_get_phase_shift(int out_channel, int32_t *current, int32_t *target);

/* Returns non-zero if the given output channel is busy phase shifting to a new preset */
int spll_shifter_busy(int out_channel);

/* Returns phase detector type used by particular output channel. There are two phase detectors available:
- DDMTD: locks only 62.5 / 125 MHz. Provides independent phase shift control for each output.
- Bang-Bang: locks to any frequency that is a result of rational (M/N) multiplication of the reference frequency.
  The frequency can be set by spll_set_aux_frequency(). BB detector follows phase setpoint of channel 0 (WR reference),
  there is no per-output shift control.
*/
int spll_get_phase_detector_type(int out_channel);

/* Sets the aux clock freuency when a BB detector is in use. 
   Must be called prior to spll_start_channel(). If the frequency is out of available range,
   returns negative value */
int spll_set_aux_frequency(int out_channel, int32_t frequency);

/* Enables/disables phase tracking on channel (ref_channel). Phase is always measured between
   the WR local reference (out_channel 0) and ref_channel */
void spll_enable_ptracker(int ref_channel, int enable);

/* Reads tracked phase shift value for given reference channel */
int spll_read_ptracker(int ref_channel, int32_t *phase_ps, int *enabled);

/* Calls non-realtime update state machine. Must be called regularly (although
 * it is not time-critical) in the main loop of the program if aux clocks or
 * external reference are used in the design. */
void spll_update();

/* Returns the status of given aux clock output (SPLL_AUX_) */
int spll_get_aux_status(int out_channel);
const char *spll_get_aux_status_string(int channel);

/* Debug/testing functions */

/* Returns how many time the PLL has de-locked since last call of spll_init() */
int spll_get_delock_count();

void spll_show_stats(void);

/* Sets VCXO tuning DAC corresponding to output (out_channel) to a given value */
void spll_set_dac(int out_channel, int value);

/* Returns current DAC sample value for output (out_channel) */
int spll_get_dac(int out_channel);

void check_vco_frequencies();

#define SPLL_STATS_VER 2

/* info reported through .stat section */
/* due to endiannes problem strings has to be 4 bytes alligned */
struct spll_stats {
	int magic;	/* 0x5b1157a7 = SPLLSTAT ?;)*/
	int ver;	/* version of the structure */
	int sequence;	/* sequence number, so the host can retry */
	int mode;
	int irq_cnt;
	int seq_state;
	int align_state;
	int H_lock;
	int M_lock;
	int H_y, M_y;
	int del_cnt;
	int start_cnt;
	char commit_id[32];
	char build_date[16];
	char build_time[16];
};

/* This only exists in wr-switch, but we should use it always */
extern struct spll_stats stats;

#endif // __SOFTPLL_NG_H

