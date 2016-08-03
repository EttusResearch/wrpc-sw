/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2010 - 2013 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

/* spll_external.h - implementation of SoftPLL servo for the 
   external (10 MHz - Grandmaster mode) reference channel */

#include <wrc.h>
#include "softpll_ng.h"
#include "irq.h"


#define ALIGN_SAMPLE_PERIOD 100000
#define ALIGN_TARGET 0

#define EXT_PERIOD_NS 100
#define EXT_FREQ_HZ 10000000
#define EXT_PPS_LATENCY_PS 30000 // fixme: make configurable


void external_init(volatile struct spll_external_state *s, int ext_ref,
			  int realign_clocks)
{
    int idx = spll_n_chan_ref + spll_n_chan_out;


    helper_init(s->helper, idx);
    mpll_init(s->main, idx, spll_n_chan_ref);

    s->align_state = ALIGN_STATE_EXT_OFF;
    s->enabled = 0;
}

void external_start(struct spll_external_state *s)
{
    helper_start(s->helper);

	SPLL->ECCR = SPLL_ECCR_EXT_EN;

	s->align_state = ALIGN_STATE_WAIT_CLKIN;
	s->enabled = 1;
	spll_debug (DBG_EVENT | DBG_EXT, DBG_EVT_START, 1);
}

int external_locked(volatile struct spll_external_state *s)
{
	if (!s->helper->ld.locked || !s->main->ld.locked ||
		!(SPLL->ECCR & SPLL_ECCR_EXT_REF_LOCKED) ||  // ext PLL became unlocked
		 (SPLL->ECCR & SPLL_ECCR_EXT_REF_STOPPED))   // 10MHz unplugged (only SPEC)
		return 0;

	switch(s->align_state) {
		case ALIGN_STATE_EXT_OFF:
		case ALIGN_STATE_WAIT_CLKIN:
		case ALIGN_STATE_WAIT_PLOCK:
		case ALIGN_STATE_START:
		case ALIGN_STATE_START_MAIN:
		case ALIGN_STATE_INIT_CSYNC:
		case ALIGN_STATE_WAIT_CSYNC:
			return 0;
		default:
			return 1;
	}
}

static int align_sample(int channel, int *v)
{
	int mask = (1 << channel);

	if(SPLL->AL_CR & mask) {
		SPLL->AL_CR = mask; // ack
		int ci = SPLL->AL_CIN;
		if(ci > 100 && ci < (EXT_FREQ_HZ - 100) ) { // give some metastability margin, when the counter transitions from EXT_FREQ_HZ-1 to 0
			*v = ci * EXT_PERIOD_NS;
			return 1;
		}
	}
	return 0; // sample not valid
}

int external_align_fsm(volatile struct spll_external_state *s)
{
	int v, done_sth = 0;

	switch(s->align_state) {
		case ALIGN_STATE_EXT_OFF:
			break;

		case ALIGN_STATE_WAIT_CLKIN:
			if( !(SPLL->ECCR & SPLL_ECCR_EXT_REF_STOPPED) ) {
				SPLL->ECCR |= SPLL_ECCR_EXT_REF_PLLRST;
				s->align_state = ALIGN_STATE_WAIT_PLOCK;
				done_sth++;
			}
			break;

		case ALIGN_STATE_WAIT_PLOCK:
			SPLL->ECCR &= (~SPLL_ECCR_EXT_REF_PLLRST);
			if( SPLL->ECCR & SPLL_ECCR_EXT_REF_STOPPED )
				s->align_state = ALIGN_STATE_WAIT_CLKIN;
			else if( SPLL->ECCR & SPLL_ECCR_EXT_REF_LOCKED )
				s->align_state = ALIGN_STATE_START;
			done_sth++;
			break;

		case ALIGN_STATE_START:
			if(s->helper->ld.locked) {
				disable_irq();
				mpll_start(s->main);
				enable_irq();
				s->align_state = ALIGN_STATE_START_MAIN;
				done_sth++;
			}
			break;

		case ALIGN_STATE_START_MAIN:
			SPLL->AL_CR = 2;
			if(s->helper->ld.locked && s->main->ld.locked) {
				PPSG->CR = PPSG_CR_CNT_EN | PPSG_CR_PWIDTH_W(10);
				PPSG->ADJ_NSEC = 3;
				PPSG->ESCR = PPSG_ESCR_SYNC;
				s->align_state = ALIGN_STATE_INIT_CSYNC;
				pll_verbose("EXT: DMTD locked.\n");
				done_sth++;
			}
			break;

		case ALIGN_STATE_INIT_CSYNC:
			if (PPSG->ESCR & PPSG_ESCR_SYNC) {
			    PPSG->ESCR = PPSG_ESCR_PPS_VALID; // enable PPS output (even though it's not aligned yet)
				s->align_timer = timer_get_tics() + 2 * TICS_PER_SECOND;
				s->align_state = ALIGN_STATE_WAIT_CSYNC;
				done_sth++;
			}
			break;

		case ALIGN_STATE_WAIT_CSYNC:
			if(time_after_eq(timer_get_tics(), s->align_timer)) {
				s->align_state = ALIGN_STATE_START_ALIGNMENT;
				s->align_shift = 0;
				pll_verbose("EXT: CSync complete.\n");
				done_sth++;
			}
			break;

		case ALIGN_STATE_START_ALIGNMENT:
			if(align_sample(1, &v)) {
				v %= ALIGN_SAMPLE_PERIOD;
				if(v == 0 || v >= ALIGN_SAMPLE_PERIOD / 2) {
					s->align_target = EXT_PERIOD_NS;
					s->align_step = -100;
				} else if (s > 0) {
					s->align_target = 0;
					s->align_step = 100;
				}

				pll_verbose("EXT: Align target %d, step %d.\n", s->align_target, s->align_step);
				s->align_state = ALIGN_STATE_WAIT_SAMPLE;
				done_sth++;
			}
			break;

		case ALIGN_STATE_WAIT_SAMPLE:
			if(!mpll_shifter_busy(s->main) && align_sample(1, &v)) {
				v %= ALIGN_SAMPLE_PERIOD;
				if(v != s->align_target) {
					s->align_shift += s->align_step;
					mpll_set_phase_shift(s->main, s->align_shift);
				} else if (v == s->align_target) {
					s->align_shift += EXT_PPS_LATENCY_PS;
				mpll_set_phase_shift(s->main, s->align_shift);
					s->align_state = ALIGN_STATE_COMPENSATE_DELAY;
				}
				done_sth++;
			}
			break;

		case ALIGN_STATE_COMPENSATE_DELAY:
			if(!mpll_shifter_busy(s->main)) {
				pll_verbose("EXT: Align done.\n");
				s->align_state = ALIGN_STATE_LOCKED;
				done_sth++;
			}
			break;

		case ALIGN_STATE_LOCKED:
			if(!external_locked(s)) {
				s->align_state = ALIGN_STATE_WAIT_CLKIN;
				done_sth++;
			}
			break;

		default:
			break;
	}
	return done_sth != 0;
}
