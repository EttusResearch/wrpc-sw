/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2010 - 2013 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

/* spll_common.c - common data structures and functions used by the SoftPLL */

#include <string.h>
#include "spll_defs.h"
#include "spll_common.h"

int pi_update(spll_pi_t *pi, int x)
{
	int i_new, y;
	pi->x = x;
	i_new = pi->integrator + x;

	y = ((i_new * pi->ki + x * pi->kp) >> PI_FRACBITS) + pi->bias;

	/* clamping (output has to be in <y_min, y_max>) and
	   anti-windup: stop the integrator if the output is already
	   out of range and the output is going further away from
	   y_min/y_max. */
	if (y < pi->y_min) {
		y = pi->y_min;
		if ((pi->anti_windup && (i_new > pi->integrator))
		    || !pi->anti_windup)
			pi->integrator = i_new;
	} else if (y > pi->y_max) {
		y = pi->y_max;
		if ((pi->anti_windup && (i_new < pi->integrator))
		    || !pi->anti_windup)
			pi->integrator = i_new;
	} else			/* No antiwindup/clamping? */
		pi->integrator = i_new;

	pi->y = y;
	return y;
}

void pi_init(spll_pi_t *pi)
{
	pi->integrator = 0;
	pi->y = pi->bias;
}

/* Lock detector state machine. Takes an error sample (y) and checks
   if it's withing an acceptable range (i.e. <-ld.threshold,
   ld.threshold>. If it has been inside the range for
   (ld.lock_samples) cyckes, the FSM assumes the PLL is locked.
   
   Return value:
   0: PLL not locked
   1: PLL locked
   -1: PLL just got out of lock
 */
int ld_update(spll_lock_det_t *ld, int y)
{
	ld->lock_changed = 0;

	if (abs(y) <= ld->threshold) {
		if (ld->lock_cnt < ld->lock_samples)
			ld->lock_cnt++;

		if (ld->lock_cnt == ld->lock_samples) {
			ld->lock_changed = 1;
			ld->locked = 1;
			return 1;
		}
	} else {
		if (ld->lock_cnt > ld->delock_samples)
			ld->lock_cnt--;

		if (ld->lock_cnt == ld->delock_samples) {
			ld->lock_cnt = 0;
			ld->lock_changed = 1;
			ld->locked = 0;
			return -1;
		}
	}
	return ld->locked;
}

void ld_init(spll_lock_det_t *ld)
{
	ld->locked = 0;
	ld->lock_cnt = 0;
	ld->lock_changed = 0;
}

void lowpass_init(spll_lowpass_t *lp, int alpha)
{
	lp->y_d = 0x80000000;
	lp->alpha = alpha;
}

int lowpass_update(spll_lowpass_t *lp, int x)
{
	if (lp->y_d == 0x80000000) {
		lp->y_d = x;
		return x;
	} else {
		int scaled = (lp->alpha * (x - lp->y_d)) >> 15;
		lp->y_d = lp->y_d + (scaled >> 1) + (scaled & 1);
		return lp->y_d;
	}
}

/* Enables/disables DDMTD tag generation on a given (channel). 

Channels (0 ... splL_n_chan_ref - 1) are the reference channels
	(e.g. transceivers' RX clocks or a local reference)

Channels (spll_n_chan_ref ... spll_n_chan_out + spll_n_chan_ref-1) are the output
	channels (local voltage controlled oscillators). One output
	(usually the first one) is always used to drive the oscillator
	which produces the reference clock for the transceiver. Other
	outputs can be used to discipline external oscillators
	(e.g. on FMCs).
*/

void spll_enable_tagger(int channel, int enable)
{
	if (channel >= spll_n_chan_ref) {	/* Output channel? */
		if (enable)
			SPLL->OCER |= 1 << (channel - spll_n_chan_ref);
		else
			SPLL->OCER &= ~(1 << (channel - spll_n_chan_ref));
	} else {		/* Reference channel */
		if (enable)
			SPLL->RCER |= 1 << channel;
		else
			SPLL->RCER &= ~(1 << channel);
	}

//      TRACE("%s: ch %d, OCER 0x%x, RCER 0x%x\n", __FUNCTION__, channel, SPLL->OCER, SPLL->RCER);
}

void biquad_init(spll_biquad_t *bq, const int *coefs, int shift)
{
	memset(bq, 0, sizeof(spll_biquad_t));
	memcpy(bq->coefs, coefs, 5 * sizeof(int));
	bq->shift = shift;
}

int biquad_update(spll_biquad_t *bq, int x)
{
	register int y = 0;
	
	y += bq->coefs[0] * x;
	y += bq->coefs[1] * bq->xd[0];
	y += bq->coefs[2] * bq->xd[1];
	y -= bq->coefs[3] * bq->yd[0];
	y -= bq->coefs[4] * bq->yd[1];
	y >>= bq->shift;
	
	bq->xd[1] = bq->xd[0];
	bq->xd[0] = x;
	
	bq->yd[1] = bq->yd[0];
	bq->yd[0] = y;
	
	return y;
}

