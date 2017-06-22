/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2010 - 2014 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 * Author: Grzegorz Daniluk <grzegorz.daniluk@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

/* spll_common.c - common data structures and functions used by the SoftPLL */

#include <string.h>
#include <wrc.h>
#include "softpll_ng.h"

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
	pll_verbose("EnableTagger %d %d\n", channel, enable);
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

	pll_verbose("%s: ch %d, OCER 0x%x, RCER 0x%x\n", __FUNCTION__, channel, SPLL->OCER, SPLL->RCER);
}

