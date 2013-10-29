/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2010 - 2013 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

/* spll_common.h - common data structures and functions used by the SoftPLL */

#ifndef __SPLL_COMMON_H
#define __SPLL_COMMON_H

#include <stdint.h>
#include <stdlib.h>
#include <syscon.h>

#include <hw/softpll_regs.h>
#include <hw/pps_gen_regs.h>

#include "spll_defs.h"

#define SPLL_LOCKED 	1
#define SPLL_LOCKING 	0

/* Number of reference/output channels. We don't plan to have more than one
   SoftPLL instantiation per project, so these can remain global. */
extern int spll_n_chan_ref, spll_n_chan_out;

extern volatile struct SPLL_WB *SPLL;
extern volatile struct PPSG_WB *PPSG;


/* PI regulator state */
typedef struct {
	int ki, kp;		/* integral and proportional gains (1<<PI_FRACBITS == 1.0f) */
	int integrator;		/* current integrator value */
	int bias;		/* DC offset always added to the output */
	int anti_windup;	/* when non-zero, anti-windup is enabled */
	int y_min;		/* min/max output range, used by clapming and antiwindup algorithms */
	int y_max;
	int x, y;		/* Current input (x) and output value (y) */
} spll_pi_t;

/* lock detector state */
typedef struct {
	int lock_cnt;		/* Lock sample counter */
	int lock_samples;	/* Number of samples below the (threshold) to assume that we are locked */
	int delock_samples;	/* Accumulated number of samples that causes the PLL go get out of lock.
				   delock_samples < lock_samples.  */
	int threshold;		/* Error threshold */
	int locked;		/* Non-zero: we are locked */
	int lock_changed;
} spll_lock_det_t;

/* simple, 1st-order lowpass filter */
typedef struct {
	int alpha;
	int y_d;
} spll_lowpass_t;

typedef struct {
	int coefs[5]; /* Biquad coefficients: b0 b1 b2 a1 a2 */
	int shift;		/* bit shift for the coeffs / output */
	int yd[2], xd[2]; /* I/O delay lines */
} spll_biquad_t;

/* initializes the PI controller state. Currently almost a stub. */
void pi_init(spll_pi_t *pi);

/* Processes a single sample (x) with PI control algorithm
 (pi). Returns the value (y) to drive the actuator. */
int pi_update(spll_pi_t *pi, int x);

void ld_init(spll_lock_det_t *ld);
int ld_update(spll_lock_det_t *ld, int y);
void lowpass_init(spll_lowpass_t *lp, int alpha);
int lowpass_update(spll_lowpass_t *lp, int x);

void spll_enable_tagger(int channel, int enable);

void biquad_init(spll_biquad_t *bq, const int *coefs, int shift);
int biquad_update(spll_biquad_t *bq, int x);

#endif // __SPLL_COMMON_H
