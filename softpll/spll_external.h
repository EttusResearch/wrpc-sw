/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2010 - 2013 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

/* spll_external.h - definitions & prototypes for the 
   external (10 MHz - Grandmaster mode) reference channel */

#ifndef __SPLL_EXTERNAL_H
#define __SPLL_EXTERNAL_H

#include "spll_common.h"

/* Alignment FSM states */

/* 1st alignment stage, done before starting the ext channel PLL:
   alignment of the rising edge of the external clock (10 MHz), with
   the rising edge of the local reference (62.5/125 MHz) and the PPS
   signal. Because of non-integer ratio (6.25 or 12.5), the PLL must
   know which edges shall be kept at phase==0. We align to the edge of
   the 10 MHz clock which comes right after the edge of the PPS pulse
   (see drawing below):

PLL reference (62.5 MHz)   ____|^^^^|____|^^^^|____|^^^^|____|^^^^|____|^^^^|____|^^^^|____|^^^^|____|^^^^|____|^^^^|____|^^^^|____|^^^^|____|
External clock (10 MHz)    ^^^^^^^^^|________________________|^^^^^^^^^^^^^^^^^^^^^^^^^|________________________|^^^^^^^^^^^^^^^^^^^^^^^^^|___
External PPS               ___________|^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    */
#define REALIGN_STAGE1 1
#define REALIGN_STAGE1_WAIT 2

/* 2nd alignment stage, done after the ext channel PLL has locked. We
   make sure that the switch's internal PPS signal is produced exactly
   on the edge of PLL reference in-phase with 10 MHz clock edge, which
   has come right after the PPS input

PLL reference (62.5 MHz)   ____|^^^^|____|^^^^|____|^^^^|____|^^^^|____|^^^^|____|^^^^|____|^^^^|____|^^^^|____|^^^^|____|^^^^|____|^^^^|____|
External clock (10 MHz)    ^^^^^^^^^|________________________|^^^^^^^^^^^^^^^^^^^^^^^^^|________________________|^^^^^^^^^^^^^^^^^^^^^^^^^|___
External PPS               ___________|^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Internal PPS               __________________________________|^^^^^^^^^|______________________________________________________________________
                                                             ^ aligned clock edges and PPS
*/

#define REALIGN_STAGE2 3
#define REALIGN_STAGE2_WAIT 4

/* Error state - PPS signal missing or of bad frequency */
#define REALIGN_PPS_INVALID 5

/* Realignment is disabled (i.e. the switch inputs only the reference
 * frequency, but not time)  */
#define REALIGN_DISABLED 6

/* Realignment done */
#define REALIGN_DONE 7

struct spll_external_state {
	int ref_src;
	int sample_n;
	int ph_err_offset, ph_err_cur, ph_err_d0, ph_raw_d0;
	int realign_clocks;
	int realign_state;
	int realign_timer;
	spll_pi_t pi;
	spll_lowpass_t lp_short, lp_long;
	spll_lock_det_t ld;
};

void external_init(volatile struct spll_external_state *s, int ext_ref,
			  int realign_clocks);


int external_update(struct spll_external_state *s, int tag, int source);

void external_start(struct spll_external_state *s);

int external_locked(struct spll_external_state *s);

#endif // __SPLL_EXTERNAL_H
