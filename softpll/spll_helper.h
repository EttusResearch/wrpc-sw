/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2010 - 2013 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

/* spll_helper.h - the helper PLL producing a clock (clk_dmtd_i) which is
   slightly offset in frequency from the recovered/reference clock
   (clk_rx_i or clk_ref_i), so the Main PLL can use it to perform
   linear phase measurements. */


#ifndef __SPLL_HELPER_H
#define __SPLL_HELPER_H

#include "spll_common.h"


#define HELPER_TAG_WRAPAROUND 100000000

/* Maximum abs value of the phase error. If the error is bigger, it's
 * clamped to this value. */
#define HELPER_ERROR_CLAMP 150000

struct spll_helper_state {
	int p_adder;		/* anti wrap-around adder */
	int p_setpoint, tag_d0;
	int ref_src;
	int sample_n;
	int delock_count;
	spll_pi_t pi;
	spll_lock_det_t ld;
	spll_biquad_t precomp;
};

void helper_init(struct spll_helper_state *s, int ref_channel);
int helper_update(struct spll_helper_state *s, int tag,
			 int source);

void helper_start(struct spll_helper_state *s);

#endif // __SPLL_HELPER_H
