/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2010 - 2013 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

/* spll_main.h - the main DDMTD PLL. Locks output clock to any reference
   with programmable phase shift. */

#ifndef __SPLL_MAIN_H
#define __SPLL_MAIN_H

#include "spll_common.h"

/* State of the Main PLL */
struct spll_main_state {
	int state;

	spll_pi_t pi;
	spll_lock_det_t ld;

	int adder_ref, adder_out, tag_ref, tag_out, tag_ref_d, tag_out_d;

	// tag sequencing stuff
	uint32_t seq_ref, seq_out;
	int match_state;
	int match_seq;

	int phase_shift_target;
	int phase_shift_current;
	int id_ref, id_out;	/* IDs of the reference and the output channel */
	int sample_n;
	int delock_count;
	int dac_index;
};

void mpll_init(struct spll_main_state *s, int id_ref,
		      int id_out);

void mpll_stop(struct spll_main_state *s);

void mpll_start(struct spll_main_state *s);

int mpll_update(struct spll_main_state *s, int tag, int source);

int mpll_set_phase_shift(struct spll_main_state *s,
				int desired_shift);

int mpll_shifter_busy(struct spll_main_state *s);

#endif // __SPLL_MAIN_H
