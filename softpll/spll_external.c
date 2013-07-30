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

#include "spll_external.h"
#include "spll_debug.h"

#define BB_ERROR_BITS 16

void external_init(volatile struct spll_external_state *s, int ext_ref,
			  int realign_clocks)
{

	s->pi.y_min = 5;
	s->pi.y_max = (1 << DAC_BITS) - 5;
	s->pi.kp = (int)(300);
	s->pi.ki = (int)(1);

	s->pi.anti_windup = 1;
	s->pi.bias = 32768;

	/* Phase branch lock detection */
	s->ld.threshold = 250;
	s->ld.lock_samples = 10000;
	s->ld.delock_samples = 9990;
	s->ref_src = ext_ref;
	s->ph_err_cur = 0;
	s->ph_err_d0 = 0;
	s->ph_raw_d0 = 0;

	s->realign_clocks = realign_clocks;
	s->realign_state = (realign_clocks ? REALIGN_STAGE1 : REALIGN_DISABLED);

	pi_init((spll_pi_t *)&s->pi);
	ld_init((spll_lock_det_t *)&s->ld);
	lowpass_init((spll_lowpass_t *)&s->lp_short, 4000);
	lowpass_init((spll_lowpass_t *)&s->lp_long, 300);
}

static inline void realign_fsm(struct spll_external_state *s)
{
	switch (s->realign_state) {
	case REALIGN_STAGE1:
		SPLL->ECCR |= SPLL_ECCR_ALIGN_EN;

		s->realign_state = REALIGN_STAGE1_WAIT;
		s->realign_timer = timer_get_tics();
		break;

	case REALIGN_STAGE1_WAIT:

		if (SPLL->ECCR & SPLL_ECCR_ALIGN_DONE)
			s->realign_state = REALIGN_STAGE2;
		else if (timer_get_tics() - s->realign_timer >
			 2 * TICS_PER_SECOND) {
			SPLL->ECCR &= ~SPLL_ECCR_ALIGN_EN;
			s->realign_state = REALIGN_PPS_INVALID;
		}
		break;

	case REALIGN_STAGE2:
		if (s->ld.locked) {
			PPSG->CR = PPSG_CR_CNT_RST | PPSG_CR_CNT_EN;
			PPSG->ADJ_UTCLO = 0;
			PPSG->ADJ_UTCHI = 0;
			PPSG->ADJ_NSEC = 0;
			PPSG->ESCR = PPSG_ESCR_SYNC;

			s->realign_state = REALIGN_STAGE2_WAIT;
			s->realign_timer = timer_get_tics();
		}
		break;

	case REALIGN_STAGE2_WAIT:
		if (PPSG->ESCR & PPSG_ESCR_SYNC) {
			PPSG->ESCR = PPSG_ESCR_PPS_VALID | PPSG_ESCR_TM_VALID;
			s->realign_state = REALIGN_DONE;
		} else if (timer_get_tics() - s->realign_timer >
			   2 * TICS_PER_SECOND) {
			PPSG->ESCR = 0;
			s->realign_state = REALIGN_PPS_INVALID;
		}
		break;

	case REALIGN_PPS_INVALID:
	case REALIGN_DISABLED:
	case REALIGN_DONE:
		return;
	}
}

int external_update(struct spll_external_state *s, int tag, int source)
{
	int err, y, y2, ylt;

	if (source == s->ref_src) {
		int wrap = tag & (1 << BB_ERROR_BITS) ? 1 : 0;

		realign_fsm(s);

		tag &= ((1 << BB_ERROR_BITS) - 1);

//              mprintf("err %d\n", tag);
		if (wrap) {
			if (tag > s->ph_raw_d0)
				s->ph_err_offset -= (1 << BB_ERROR_BITS);
			else if (tag <= s->ph_raw_d0)
				s->ph_err_offset += (1 << BB_ERROR_BITS);
		}

		s->ph_raw_d0 = tag;

		err = (tag + s->ph_err_offset) - s->ph_err_d0;
		s->ph_err_d0 = (tag + s->ph_err_offset);

		y = pi_update(&s->pi, err);

		y2 = lowpass_update(&s->lp_short, y);
		ylt = lowpass_update(&s->lp_long, y);

		if (!(SPLL->ECCR & SPLL_ECCR_EXT_REF_PRESENT)) {
			/* no reference? de-lock now */
			ld_init(&s->ld);
			y2 = 32000;
		}

		SPLL->DAC_MAIN = y2 & 0xffff;

		spll_debug(DBG_ERR | DBG_EXT, ylt, 0);
		spll_debug(DBG_SAMPLE_ID | DBG_EXT, s->sample_n++, 0);
		spll_debug(DBG_Y | DBG_EXT, y2, 1);

		if (ld_update(&s->ld, y2 - ylt))
			return SPLL_LOCKED;
	}
	return SPLL_LOCKING;
}

void external_start(struct spll_external_state *s)
{

	SPLL->ECCR = 0;

	s->sample_n = 0;
	s->realign_state =
	    (s->realign_clocks ? REALIGN_STAGE1 : REALIGN_DISABLED);

	SPLL->ECCR = SPLL_ECCR_EXT_EN;

	spll_debug(DBG_EVENT | DBG_EXT, DBG_EVT_START, 1);
}

int external_locked(struct spll_external_state *s)
{
	return (s->ld.locked
		&& (s->realign_clocks ? s->realign_state == REALIGN_DONE : 1));
}
