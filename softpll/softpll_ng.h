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
#include "util.h"
#include "softpll_export.h"
#include "spll_defs.h"
#include "spll_common.h"
#include "spll_debug.h"
#include "spll_helper.h"
#include "spll_main.h"
#include "spll_ptracker.h"
#include "spll_external.h"


/* Shortcut for 'channels' parameter in various API functions to perform operation on all channels */
#define SPLL_ALL_CHANNELS 0xffffffff

/* Aux clock flags */
#define SPLL_AUX_ENABLED (1<<0) /* Locking the particular aux channel to the WR reference is enabled */
#define SPLL_AUX_LOCKED (1<<1)  /* The particular aux clock is already locked to WR reference */

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
void spll_very_init(void);

/* Disables the SoftPLL and cleans up stuff */
void spll_shutdown(void);

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

/* Enables/disables phase tracking on channel (ref_channel). Phase is always measured between
   the WR local reference (out_channel 0) and ref_channel */
void spll_enable_ptracker(int ref_channel, int enable);

/* Reads tracked phase shift value for given reference channel */
int spll_read_ptracker(int ref_channel, int32_t *phase_ps, int *enabled);

/* Calls non-realtime update state machine. Must be called regularly (although
 * it is not time-critical) in the main loop of the program if aux clocks or
 * external reference are used in the design. */
int spll_update(void);

/* Returns the status of given aux clock output (SPLL_AUX_) */
int spll_get_aux_status(int out_channel);

/* Debug/testing functions */

/* Returns how many time the PLL has de-locked since last call of spll_init() */
int spll_get_delock_count(void);

void spll_show_stats(void);

/* Sets VCXO tuning DAC corresponding to output (out_channel) to a given value */
void spll_set_dac(int out_channel, int value);

/* Returns current DAC sample value for output (out_channel) */
int spll_get_dac(int out_channel);

void check_vco_frequencies(void);

/*
 * Aux and main state:
 * used to be in .c file, but we need it here for memory dumping
 */
/* NOTE: Please increment WRPC_SHMEM_VERSION if you change this structure */
struct spll_aux_state {
	int seq_state;
	int32_t phase_target;
	union {
		struct spll_main_state dmtd;
		/* spll_external_state ch_bb */
	} pll;
};

/* NOTE: Please increment WRPC_SHMEM_VERSION if you change this structure */
struct softpll_state {
	int mode;
	int seq_state;
	int dac_timeout;
	int delock_count;
	unsigned irq_count;
	int32_t mpll_shift_ps;

	struct spll_helper_state helper;
	struct spll_external_state ext;
	struct spll_main_state mpll;
	struct spll_aux_state aux[MAX_CHAN_AUX];
	struct spll_ptracker_state ptrackers[MAX_PTRACKERS];
};

/* NOTE: Please increment WRPC_SHMEM_VERSION if you change this structure */
struct spll_fifo_log {
	uint32_t trr;
	uint32_t tstamp;
	uint32_t duration;
	uint16_t irq_count;
	uint16_t tag_count;
};
#define FIFO_LOG_LEN 16


#endif // __SOFTPLL_NG_H

