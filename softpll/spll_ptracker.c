/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2010 - 2013 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

/* spll_ptracker.c - implementation of phase trackers. */

#include "spll_ptracker.h"

static int tag_ref = -1;

void ptracker_init(struct spll_ptracker_state *s, int id, int num_avgs)
{
	s->id = id;
	s->ready = 0;
	s->n_avg = num_avgs;
	s->acc = 0;
	s->avg_count = 0;
	s->enabled = 0;

}

void ptracker_start(struct spll_ptracker_state *s)
{
	s->preserve_sign = 0;
	s->enabled = 1;
	s->ready = 0;
	s->acc = 0;
	s->avg_count = 0;

	spll_enable_tagger(s->id, 1);
	spll_enable_tagger(spll_n_chan_ref, 1);
}

int ptrackers_update(struct spll_ptracker_state *ptrackers, int tag,
			   int source)
{
	const int adj_tab[16] = { /* psign */ 
														/* 0   - 1/4   */  0, 0, 0, -(1<<HPLL_N),
														/* 1/4 - 1/2  */   0, 0, 0, 0,
														/* 1/2 - 3/4  */   0, 0, 0, 0,
														/* 3/4 - 1   */    (1<<HPLL_N), 0, 0, 0};
												
	if(source == spll_n_chan_ref)
	{
		tag_ref = tag;
		return 0;
	}


	register struct spll_ptracker_state *s = ptrackers + source;

	if(!s->enabled)
		return 0;

	register int delta = (tag_ref - tag) & ((1 << HPLL_N) - 1);
	register int index = delta >> (HPLL_N - 2);


	if (s->avg_count == 0) {
		/* hack: two since PTRACK_WRAP_LO/HI are in 1/4 and 3/4 of the scale,
		   we can use the two MSBs of delta and a trivial LUT instead, removing 2 branches */
		s->preserve_sign = index << 2;
		s->acc = delta;
		s->avg_count ++;
	} else {

		/* same hack again, using another lookup table to adjust for wraparound */
		s->acc += delta + adj_tab[ index + s->preserve_sign ];
		s->avg_count ++;

		if (s->avg_count == s->n_avg) {
			s->phase_val = s->acc / s->n_avg;
			s->ready = 1;
			s->acc = 0;
			s->avg_count = 0;
		}
	}

	return 0;
}
