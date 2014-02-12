/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>
#include <wrc.h>

#include "board.h"
#include "trace.h"
#include "syscon.h"
#include "endpoint.h"
#include "softpll_ng.h"
#include "wrc_ptp.h"
#include "eeprom.h"
#include "ptpd_netif.h"

/* New calibrator for the transition phase value. A major pain in the ass for
   the folks who frequently rebuild their gatewares. The idea is described
   below:
   - lock the PLL to the master
   - scan the whole phase shifter range
   - at each scanning step, generate a fake RX timestamp.
   - check if the rising edge counter is ahead of the falling edge counter
     (added a special bit for it in the TSU).
   - determine phases at which positive/negative transitions occur
   - transition phase value is in the middle between the rising and falling
     edges.
   
   This calibration procedure is fast enough to be run on slave nodes whenever
   the link goes up. For master mode, the core must be run at least once as a
   slave to calibrate itself and store the current transition phase value in
   the EEPROM.
*/

/* how finely we scan the phase shift range to determine where we have the bit
 * flip */
#define CAL_SCAN_STEP 100

/* deglitcher threshold (to remove 1->0->1 flip bit glitches that might occur
   due to jitter) */
#define CAL_DEGLITCH_THRESHOLD 5

/* we scan at least one clock period to look for rising->falling edge transition
   plus some headroom */
#define CAL_SCAN_RANGE (REF_CLOCK_PERIOD_PS + \
		(3 * CAL_DEGLITCH_THRESHOLD * CAL_SCAN_STEP))

#define TD_WAIT_INACTIVE	0
#define TD_GOT_TRANSITION	1
#define TD_DONE			2

/* state of transition detector */
struct trans_detect_state {
	int prev_val;
	int sample_count;
	int state;
	int trans_phase;
};

/* finds the transition in the value of flip_bit and returns phase associated
   with it. If no transition phase has been found yet, returns 0. Non-zero
   polarity means we are looking for positive transitions, 0 - negative
   transitions */
static int lookup_transition(struct trans_detect_state *state, int flip_bit,
			     int phase, int polarity)
{
	if (polarity)
		polarity = 1;

	switch (state->state) {
	case TD_WAIT_INACTIVE:
		/* first, wait until we have at least CAL_DEGLITCH_THRESHOLD of
		   inactive state samples */
		if (flip_bit != polarity)
			state->sample_count++;
		else
			state->sample_count = 0;

		if (state->sample_count >= CAL_DEGLITCH_THRESHOLD) {
			state->state = TD_GOT_TRANSITION;
			state->sample_count = 0;
		}

		break;

	case TD_GOT_TRANSITION:
		if (flip_bit != polarity)
			state->sample_count = 0;
		else {
			state->sample_count++;
			if (state->sample_count >= CAL_DEGLITCH_THRESHOLD) {
				state->state = TD_DONE;
				state->trans_phase =
				    phase -
				    CAL_DEGLITCH_THRESHOLD * CAL_SCAN_STEP;
			}
		}
		break;

	case TD_DONE:
		return 1;
		break;
	}
	return 0;
}

static struct trans_detect_state det_rising, det_falling;
static int cal_cur_phase;

/* Starts RX timestamper calibration process state machine. Invoked by
   ptpnetif's check lock function when the PLL has already locked, to avoid
   complicating the API of ptp-noposix/ppsi. */

void rxts_calibration_start()
{
	cal_cur_phase = 0;
	det_rising.prev_val = det_falling.prev_val = -1;
	det_rising.state = det_falling.state = TD_WAIT_INACTIVE;
	det_rising.sample_count = 0;
	det_falling.sample_count = 0;
	det_rising.trans_phase = 0;
	det_falling.trans_phase = 0;
	spll_set_phase_shift(0, 0);
}

/* Updates RX timestamper state machine. Non-zero return value means that
   calibration is done. */
int rxts_calibration_update(uint32_t *t24p_value)
{
	int32_t ttrans = 0;

	if (spll_shifter_busy(0))
		return 0;

	/* generate a fake RX timestamp and check if falling edge counter is
	   ahead of rising edge counter */
	int flip = ep_timestamper_cal_pulse();

	/* look for transitions (with deglitching) */
	lookup_transition(&det_rising, flip, cal_cur_phase, 1);
	lookup_transition(&det_falling, flip, cal_cur_phase, 0);

	if (cal_cur_phase >= CAL_SCAN_RANGE) {
		if (det_rising.state != TD_DONE || det_falling.state != TD_DONE) 
		{
			TRACE_DEV("RXTS calibration error.\n");
			return -1;
		}

		/* normalize */
		while (det_falling.trans_phase >= REF_CLOCK_PERIOD_PS)
			det_falling.trans_phase -= REF_CLOCK_PERIOD_PS;
		while (det_rising.trans_phase >= REF_CLOCK_PERIOD_PS)
			det_rising.trans_phase -= REF_CLOCK_PERIOD_PS;

		/* Use falling edge as second sample of rising edge */
		if (det_falling.trans_phase > det_rising.trans_phase)
			ttrans = det_falling.trans_phase - REF_CLOCK_PERIOD_PS/2;
		else if(det_falling.trans_phase < det_rising.trans_phase)
			ttrans = det_falling.trans_phase + REF_CLOCK_PERIOD_PS/2;
		ttrans += det_rising.trans_phase;
		ttrans /= 2;

		/*normalize ttrans*/
		if(ttrans < 0) ttrans += REF_CLOCK_PERIOD_PS;
		if(ttrans >= REF_CLOCK_PERIOD_PS) ttrans -= REF_CLOCK_PERIOD_PS;


		TRACE_DEV("RXTS calibration: R@%dps, F@%dps, transition@%dps\n",
			  det_rising.trans_phase, det_falling.trans_phase,
			  ttrans);

		*t24p_value = (uint32_t)ttrans;
		return 1;
	}

	cal_cur_phase += CAL_SCAN_STEP;

	spll_set_phase_shift(0, cal_cur_phase);

	return 0;
}

/* legacy function for 'calibration force' command */
int measure_t24p(uint32_t *value)
{
	int rv;
	pp_printf("Waiting for link...\n");
	while (!ep_link_up(NULL))
		timer_delay_ms(100);

	spll_init(SPLL_MODE_SLAVE, 0, 1);
	pp_printf("Locking PLL...\n");
	while (!spll_check_lock(0))
		timer_delay_ms(100);
	pp_printf("\n");

	pp_printf("Calibrating RX timestamper...\n");
	rxts_calibration_start();

	while (!(rv = rxts_calibration_update(value))) ;
	return rv;
}

/*SoftPLL must be locked prior calling this function*/
static int calib_t24p_slave(uint32_t *value)
{
	int rv;

	rxts_calibration_start();
	while (!(rv = rxts_calibration_update(value))) ;

	if (rv < 0) {
		pp_printf("Could not calibrate t24p, trying to read from EEPROM\n");
		if(eeprom_phtrans(WRPC_FMC_I2C, FMC_EEPROM_ADR, value, 0) < 0) {
			pp_printf("Something went wrong while writing EEPROM\n");
			return -1;
		}

	}
	else {
		pp_printf("t24p value is %d ps, storing to EEPROM\n", *value);
		if(eeprom_phtrans(WRPC_FMC_I2C, FMC_EEPROM_ADR, value, 1) < 0) {
			pp_printf("Something went wrong while writing EEPROM\n");
			return -1;
		}
	}

	return 0;
}

static int calib_t24p_master(uint32_t *value)
{
	int rv;

	rv = eeprom_phtrans(WRPC_FMC_I2C, FMC_EEPROM_ADR, value, 0);
	if(rv < 0)
		pp_printf("Something went wrong while reading from EEPROM: %d\n", rv);
	else
		pp_printf("t24p read from EEPROM: %d ps\n", *value);

	return rv;
}

int calib_t24p(int mode, uint32_t *value)
{
	int ret;

	if (mode == WRC_MODE_SLAVE)
		ret = calib_t24p_slave(value);
	else
		ret = calib_t24p_master(value);

	//update phtrans value in socket struct
	ptpd_netif_set_phase_transition(*value);
	return ret;
}
