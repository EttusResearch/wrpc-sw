/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2010 - 2015 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 * Author: Grzegorz Daniluk <grzegorz.daniluk@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <wrc.h>
#include "board.h"
#include "hw/softpll_regs.h"
#include "hw/pps_gen_regs.h"

#include "softpll_ng.h"

#include "irq.h"

#ifdef CONFIG_SPLL_FIFO_LOG
  struct spll_fifo_log fifo_log[FIFO_LOG_LEN];
  #define HAS_FIFO_LOG 1
#else
  #define HAS_FIFO_LOG 0
  extern struct spll_fifo_log *fifo_log;
#endif

volatile struct SPLL_WB *SPLL;
volatile struct PPSG_WB *PPSG;

int spll_n_chan_ref, spll_n_chan_out;



#define MAIN_CHANNEL (spll_n_chan_ref)

static const char *seq_states[] =
{
	[SEQ_START_EXT] = "start-ext",
	[SEQ_WAIT_EXT] = "wait-ext",
	[SEQ_START_HELPER] = "start-helper",
	[SEQ_WAIT_HELPER] = "wait-helper",
	[SEQ_START_MAIN] = "start-main",
	[SEQ_WAIT_MAIN] = "wait-main",
	[SEQ_DISABLED] = "disabled",
	[SEQ_READY] = "ready",
	[SEQ_CLEAR_DACS] = "clear-dacs",
	[SEQ_WAIT_CLEAR_DACS] = "wait-clear-dacs",
};
#define SEQ_STATES_NR  ARRAY_SIZE(seq_states)

volatile struct softpll_state softpll;

static volatile int ptracker_mask = 0;
/* fixme: should be done by spll_init() but spll_init is called to
 * switch modes (and we won't like messing around with ptrackers
 * there) */

static inline int aux_locking_enabled(int channel)
{
	uint32_t occr_aux_en = SPLL_OCCR_OUT_EN_R(SPLL->OCCR);
	
	return occr_aux_en & (1 << channel);
}

static inline void set_channel_status(int channel, int locked)
{
	if(!locked)
		SPLL->OCCR &= ~(SPLL_OCCR_OUT_LOCK_W((1 << channel)));
	else
		SPLL->OCCR |= (SPLL_OCCR_OUT_LOCK_W((1 << channel)));
}


static inline void start_ptrackers(struct softpll_state *s)
{
	int i;
	for (i = 0; i < spll_n_chan_ref; i++)
		if (ptracker_mask & (1 << i))
				ptracker_start(&s->ptrackers[i]);
}

static inline void update_ptrackers(struct softpll_state *s, int tag_value, int tag_source)
{
	if(tag_source > spll_n_chan_ref)
		return;
		
	ptrackers_update(s->ptrackers, tag_value, tag_source);
}

static inline void sequencing_fsm(struct softpll_state *s, int tag_value, int tag_source)
{
	switch (s->seq_state) {
		/* State "Clear DACs": initial SPLL sequnencer state. Brings both DACs (not the AUXs) to the default values
		   prior to starting the SPLL. */
		case SEQ_CLEAR_DACS:
		{
			/* Helper always starts at the maximum value (to make sure it locks on positive offset */
			SPLL->DAC_HPLL = s->helper.pi.y_max;

			/* Main starts at midscale */
			SPLL->DAC_MAIN = (s->mpll.pi.y_max + s->mpll.pi.y_min) / 2;
			
			/* we need tags from at least one channel, so that the IRQ that calls this function
			   gets called again */
			spll_enable_tagger(MAIN_CHANNEL, 1);

			softpll.dac_timeout = timer_get_tics()
				+ TICS_PER_SECOND / 20;
			softpll.seq_state = SEQ_WAIT_CLEAR_DACS;
			
			break;
		}
		
		/* State "Wait until DACs have been cleared". Makes sure the VCO control inputs have stabilized before starting the PLL. */
		case SEQ_WAIT_CLEAR_DACS:
		{
			if (time_after(timer_get_tics(), softpll.dac_timeout))
			{
				if(s->mode == SPLL_MODE_GRAND_MASTER)
					s->seq_state = SEQ_START_EXT;
				else
					s->seq_state = SEQ_START_HELPER;
			}
			break;
		}

		/* State "Disabled". Entered when the whole PLL is off */
		case SEQ_DISABLED:
			break;


		/* State "Start external reference PLL": starts up BB PLL for locking local reference to 10 MHz input */
		case SEQ_START_EXT:
		{
			spll_enable_tagger(MAIN_CHANNEL, 0);
			external_start(&s->ext);

			s->seq_state = SEQ_WAIT_EXT;
			break;
		}

		/* State "Wait until we are locked to external 10MHz clock" */
		case SEQ_WAIT_EXT:
		{
			if (external_locked(&s->ext)) {
				start_ptrackers(s);
				s->seq_state = SEQ_READY;
				set_channel_status(s->mpll.id_ref, 1);
			}
			break;
		}

		case SEQ_START_HELPER:
		{
			helper_start(&s->helper);

			s->seq_state = SEQ_WAIT_HELPER;
			break;
		}

		case SEQ_WAIT_HELPER:
		{
			if (s->helper.ld.locked && s->helper.ld.lock_changed)
			{
				if (s->mode == SPLL_MODE_SLAVE)
				{
					s->seq_state = SEQ_START_MAIN;
				} else {
					start_ptrackers(s);
					s->seq_state = SEQ_READY;	
					set_channel_status(s->mpll.id_ref, 1);
				}
			}
			break;
		}

		case SEQ_START_MAIN:
		{
			mpll_start(&s->mpll);
			s->seq_state = SEQ_WAIT_MAIN;
			break;
		}

		case SEQ_WAIT_MAIN:
		{
			if (s->mpll.ld.locked)
			{
				start_ptrackers(s);
				s->seq_state = SEQ_READY;
				set_channel_status(s->mpll.id_ref, 1);
			}
			break;
		}

		case SEQ_READY:
		{
			if (s->mode == SPLL_MODE_GRAND_MASTER && !external_locked(&s->ext)) {
				s->delock_count++;
				s->seq_state = SEQ_CLEAR_DACS;
				set_channel_status(s->mpll.id_ref, 0);
			} else if (!s->helper.ld.locked) {
				s->delock_count++;
				s->seq_state = SEQ_CLEAR_DACS;
				set_channel_status(s->mpll.id_ref, 0);
			} else if (s->mode == SPLL_MODE_SLAVE && !s->mpll.ld.locked) {
				s->delock_count++;
				s->seq_state = SEQ_CLEAR_DACS;
				set_channel_status(s->mpll.id_ref, 0);
			}
			break;
		}
	}
}

static inline void update_loops(struct softpll_state *s, int tag_value, int tag_source)
{

	helper_update(&s->helper, tag_value, tag_source);

	if(s->helper.ld.locked)
	{
		mpll_update(&s->mpll, tag_value, tag_source);

		if(s->seq_state == SEQ_READY) {
			if(s->mode == SPLL_MODE_SLAVE) {
				int i;
				for (i = 0; i < spll_n_chan_out - 1; i++)
					mpll_update(&s->aux[i].pll.dmtd, tag_value, tag_source);
			}

			update_ptrackers(s, tag_value, tag_source);
		}
	}
}

void _irq_entry(void)
{
	struct softpll_state *s = (struct softpll_state *)&softpll;
	uint32_t trr;
	int i, tag_source, tag_value;
	static uint16_t tag_count;
	struct spll_fifo_log *l = NULL;
	uint32_t enter_stamp;

	if (HAS_FIFO_LOG)
		enter_stamp = (PPSG->CNTR_NSEC & 0xfffffff);

	/* check if there are more tags in the FIFO, and log them if so configured to */
	while (!(SPLL->TRR_CSR & SPLL_TRR_CSR_EMPTY)) {
		trr = SPLL->TRR_R0;

		if (HAS_FIFO_LOG) {
			/* save this to a circular buffer */
			i = tag_count % FIFO_LOG_LEN;
			l = fifo_log + i;
			l->tstamp = (PPSG->CNTR_NSEC & 0xfffffff);
			if (!enter_stamp)
				enter_stamp = l->tstamp;
			l->duration = 0;
			l->trr = trr;
			l->irq_count = s->irq_count & 0xffff;
			l->tag_count = tag_count++;
		}

		/* And process the values */
		tag_source = SPLL_TRR_R0_CHAN_ID_R(trr);
		tag_value  = SPLL_TRR_R0_VALUE_R(trr);

		sequencing_fsm(s, tag_value, tag_source);
		update_loops(s, tag_value, tag_source);
	}

	if (HAS_FIFO_LOG && l)
		l->duration = (PPSG->CNTR_NSEC & 0xfffffff) - enter_stamp;
	s->irq_count++;
	clear_irq();
}

void spll_very_init()
{
	PPSG = (volatile struct PPSG_WB *)BASE_PPS_GEN;
	PPSG->CR = PPSG_CR_CNT_EN | PPSG_CR_CNT_RST | PPSG_CR_PWIDTH_W(PPS_WIDTH);
}

void spll_init(int mode, int slave_ref_channel, int align_pps)
{
	static const char *modes[] = { "", "grandmaster", "freemaster", "slave", "disabled" };
	volatile int dummy;
	int i;

	struct softpll_state *s = (struct softpll_state *) &softpll;

	disable_irq();

	SPLL = (volatile struct SPLL_WB *)BASE_SOFTPLL;
	PPSG = (volatile struct PPSG_WB *)BASE_PPS_GEN;

	uint32_t csr = SPLL->CSR;

	spll_n_chan_ref = SPLL_CSR_N_REF_R(csr);
	spll_n_chan_out = SPLL_CSR_N_OUT_R(csr);
	
	s->mode = mode;
	s->delock_count = 0;

	SPLL->DAC_HPLL = 0;
	SPLL->DAC_MAIN = 0;

	SPLL->CSR = 0;
	SPLL->OCER = 0;
	SPLL->RCER = 0;
	SPLL->ECCR = 0;
	SPLL->OCCR = 0;
	SPLL->DEGLITCH_THR = 1000;

	PPSG->ESCR = 0;
	PPSG->CR = PPSG_CR_CNT_EN | PPSG_CR_PWIDTH_W(PPS_WIDTH);

	if(mode == SPLL_MODE_DISABLED)
		s->seq_state = SEQ_DISABLED;
	else
		s->seq_state = SEQ_CLEAR_DACS;

	int helper_ref;
	
	if( mode == SPLL_MODE_SLAVE)
		helper_ref = slave_ref_channel; // Slave mode: lock the helper to an uplink port
	else
		helper_ref = spll_n_chan_ref; // Master/GM mode: lock the helper to the local ref clock

	helper_init(&s->helper, helper_ref);
	mpll_init(&s->mpll, slave_ref_channel, spll_n_chan_ref);

	for (i = 0; i < spll_n_chan_out - 1; i++) {
		mpll_init(&s->aux[i].pll.dmtd, slave_ref_channel, spll_n_chan_ref + i + 1);
		s->aux[i].seq_state = AUX_DISABLED;
	}
	
	if(mode == SPLL_MODE_FREE_RUNNING_MASTER)
		PPSG->ESCR = PPSG_ESCR_PPS_VALID | PPSG_ESCR_TM_VALID;
	
	for (i = 0; i < spll_n_chan_ref; i++)
		ptracker_init(&s->ptrackers[i], i, PTRACKER_AVERAGE_SAMPLES);

	if(mode == SPLL_MODE_GRAND_MASTER) {
		if(SPLL->ECCR & SPLL_ECCR_EXT_SUPPORTED) {
			s->ext.helper = &s->helper;
			s->ext.main = &s->mpll;
			external_init(&s->ext, spll_n_chan_ref + spll_n_chan_out, align_pps);
		} else {
			pll_verbose("softpll: attempting to enable GM mode on non-GM hardware.\n");
			return;
		}
	}

	pll_verbose
	    ("softpll: mode %s, %d ref channels, %d out channels\n",
	     modes[mode], spll_n_chan_ref, spll_n_chan_out);

	/* Purge tag buffer */
	while (!(SPLL->TRR_CSR & SPLL_TRR_CSR_EMPTY))
		dummy = SPLL->TRR_R0;
	
	SPLL->EIC_IER = 1;
	SPLL->OCER |= 1;
	
	enable_irq();
}

void spll_shutdown()
{
	disable_irq();

	SPLL->OCER = 0;
	SPLL->RCER = 0;
	SPLL->ECCR = 0;
	SPLL->EIC_IDR = 1;
}

void spll_start_channel(int channel)
{
	struct softpll_state *s = (struct softpll_state *) &softpll;

	if (s->seq_state != SEQ_READY || !channel) {
		pll_verbose("Can't start channel %d, the PLL is not ready\n",
			  channel);
		return;
	}
	mpll_start(&s->aux[channel - 1].pll.dmtd);
}

void spll_stop_channel(int channel)
{
	struct softpll_state *s = (struct softpll_state *) &softpll;
	
	if (!channel)
		return;

	mpll_stop(&s->aux[channel - 1].pll.dmtd);
}

int spll_check_lock(int channel)
{
	if (!channel)
		return (softpll.seq_state == SEQ_READY);
	else
		return (softpll.seq_state == SEQ_READY)
		    && softpll.aux[channel - 1].pll.dmtd.ld.locked;
}

static int32_t to_picos(int32_t units)
{
	return (int32_t) (((int64_t) units *
			   (int64_t) CLOCK_PERIOD_PICOSECONDS) >> HPLL_N);
}

/* Channel 0 = local PLL reference, 1...N = aux oscillators */
static void set_phase_shift(int channel, int32_t value_picoseconds)
{
	struct spll_main_state *st = (struct spll_main_state *)
	    (!channel ? &softpll.mpll : &softpll.aux[channel - 1].pll.dmtd);
	mpll_set_phase_shift(st, value_picoseconds);
	softpll.mpll_shift_ps = value_picoseconds;
}

void spll_set_phase_shift(int channel, int32_t value_picoseconds)
{
	int i;
	if (channel == SPLL_ALL_CHANNELS) {
		set_phase_shift(0, value_picoseconds);
		for (i = 0; i < spll_n_chan_out - 1; i++)
			if (softpll.aux[i].seq_state == AUX_READY)
				set_phase_shift(i + 1, value_picoseconds);
	} else
		set_phase_shift(channel, value_picoseconds);
}

void spll_get_phase_shift(int channel, int32_t *current, int32_t *target)
{
	volatile struct spll_main_state *st = (struct spll_main_state *)
	    (!channel ? &softpll.mpll : &softpll.aux[channel - 1].pll.dmtd);
	int div = (DIVIDE_DMTD_CLOCKS_BY_2 ? 2 : 1);
	if (current)
		*current = to_picos(st->phase_shift_current * div);
	if (target)
		*target = to_picos(st->phase_shift_target * div);
}

int spll_read_ptracker(int channel, int32_t *phase_ps, int *enabled)
{
	volatile struct spll_ptracker_state *st = &softpll.ptrackers[channel];
	int phase = st->phase_val;
	if (phase < 0)
		phase += (1 << HPLL_N);
	else if (phase >= (1 << HPLL_N))
		phase -= (1 << HPLL_N);

	if (DIVIDE_DMTD_CLOCKS_BY_2) {
		phase <<= 1;
		phase &= (1 << HPLL_N) - 1;
	}

	*phase_ps = to_picos(phase);
	if (enabled)
		*enabled = ptracker_mask & (1 << st->id) ? 1 : 0;
	return st->ready;
}

void spll_get_num_channels(int *n_ref, int *n_out)
{
	if (n_ref)
		*n_ref = spll_n_chan_ref;
	if (n_out)
		*n_out = spll_n_chan_out;
}

void spll_show_stats()
{
	struct softpll_state *s = (struct softpll_state *)&softpll;
	const char *statename = seq_states[s->seq_state];

	if (s->seq_state >= SEQ_STATES_NR)
		statename = "<Unknown>";

	if (softpll.mode > 0)
		    pp_printf("softpll: irqs %d seq %s mode %d "
		     "alignment_state %d HL%d ML%d HY=%d MY=%d DelCnt=%d\n",
		      s->irq_count, statename,
			      s->mode, s->ext.align_state,
			      s->helper.ld.locked, s->mpll.ld.locked,
			      s->helper.pi.y, s->mpll.pi.y,
			      s->delock_count);
}

int spll_shifter_busy(int channel)
{
	if (!channel)
		return mpll_shifter_busy((struct spll_main_state *)&softpll.mpll);
	else
		return mpll_shifter_busy((struct spll_main_state *)&softpll.aux[channel - 1].pll.dmtd);
}

void spll_enable_ptracker(int ref_channel, int enable)
{
	if (enable) {
		spll_enable_tagger(ref_channel, 1);
		ptracker_start((struct spll_ptracker_state *)&softpll.
			       ptrackers[ref_channel]);
		ptracker_mask |= (1 << ref_channel);
		pll_verbose("Enabling ptracker channel: %d\n", ref_channel);

	} else {
		ptracker_mask &= ~(1 << ref_channel);
		if (ref_channel != softpll.mpll.id_ref)
			spll_enable_tagger(ref_channel, 0);
		pll_verbose("Disabling ptracker tagger: %d\n", ref_channel);
	}
}

int spll_get_delock_count()
{
	return softpll.delock_count;
}

static int spll_update_aux_clocks(void)
{
	int ch;
	int done_sth = 0;

	for (ch = 1; ch < spll_n_chan_out; ch++) 
	{
		struct spll_aux_state *s = (struct spll_aux_state *) &softpll.aux[ch - 1];

		if(s->seq_state != AUX_DISABLED && !aux_locking_enabled(ch))
		{
			pll_verbose("softpll: disabled aux channel %d\n", ch);
			spll_stop_channel(ch);
			set_channel_status(ch, 0);
			s->seq_state = AUX_DISABLED;
			done_sth++;
		}

		switch (s->seq_state) {
			case AUX_DISABLED:
				if (softpll.mpll.ld.locked && aux_locking_enabled(ch)) {
					pll_verbose("softpll: enabled aux channel %d\n", ch);
					spll_start_channel(ch);
					s->seq_state = AUX_LOCK_PLL;
					done_sth++;
				}
				break;

			case AUX_LOCK_PLL:
				if (s->pll.dmtd.ld.locked) {
					pll_verbose ("softpll: channel %d locked [aligning @ %d ps]\n", ch, softpll.mpll_shift_ps);
					set_phase_shift(ch, softpll.mpll_shift_ps);
					s->seq_state = AUX_ALIGN_PHASE;
					done_sth++;
				}
				break;

			case AUX_ALIGN_PHASE:
				if (!mpll_shifter_busy(&s->pll.dmtd)) {
					pll_verbose("softpll: channel %d phase aligned\n", ch);
					set_channel_status(ch, 1);
					s->seq_state = AUX_READY;
					done_sth++;
				}
				break;

			case AUX_READY:
				if (!softpll.mpll.ld.locked || !s->pll.dmtd.ld.locked) {
					pll_verbose("softpll: aux channel %d or mpll lost lock\n", ch);
					set_channel_status(ch, 0); 
					s->seq_state = AUX_DISABLED;
					done_sth++;
				}
				break;
			}
	}
	return done_sth != 0;
}

int spll_get_aux_status(int channel)
{
	int rval = 0;

	if (softpll.aux[channel].seq_state != AUX_DISABLED)
		rval |= SPLL_AUX_ENABLED;

	if (softpll.aux[channel].seq_state == AUX_READY)
		rval |= SPLL_AUX_LOCKED;

	return rval;
}

int spll_get_dac(int index)
{
	if (index < 0)
		return softpll.helper.pi.y;
	else if (index == 0)
		return softpll.mpll.pi.y;
	else if (index > 0)
		return softpll.aux[index - 1].pll.dmtd.pi.y;
	return 0;
}

void spll_set_dac(int index, int value)
{
	if (index < 0) {
		softpll.helper.pi.y = value;
		SPLL->DAC_HPLL = value;
	} else {
		SPLL->DAC_MAIN =
		    SPLL_DAC_MAIN_DAC_SEL_W(index) | (value & 0xffff);

		if (index == 0)
			softpll.mpll.pi.y = value;
		else if (index > 0)
			softpll.aux[index - 1].pll.dmtd.pi.y = value;
	}
}

int spll_update()
{
	int ret = 0;

	switch(softpll.mode) {
		case SPLL_MODE_GRAND_MASTER:
			ret = external_align_fsm(&softpll.ext);
			break;
	}
	ret += spll_update_aux_clocks();

	/* store statistics */
	stats.sequence++;
	stats.mode  = softpll.mode;
	stats.irq_cnt = softpll.irq_count;
	stats.seq_state = softpll.seq_state;
	stats.align_state = softpll.ext.align_state;
	stats.H_lock = softpll.helper.ld.locked;
	stats.M_lock = softpll.mpll.ld.locked;
	stats.H_y = softpll.helper.pi.y;
	stats.M_y = softpll.mpll.pi.y;
	stats.del_cnt = softpll.delock_count;
	stats.sequence++;

	return ret != 0;
}

static int spll_measure_frequency(int osc)
{
	volatile uint32_t *reg;

	switch(osc) {
		case SPLL_OSC_REF:
			reg = &SPLL->F_REF;
			break;
		case SPLL_OSC_DMTD:
			reg = &SPLL->F_DMTD;
			break;
		case SPLL_OSC_EXT:
			reg = &SPLL->F_EXT;
			break;
		default:
			return 0;
	}

    timer_delay_ms(2000);
    return (*reg ) & (0xfffffff);
}

static int calc_apr(int meas_min, int meas_max, int f_center )
{
	// apr_min is in PPM

	int64_t delta_low =  meas_min - f_center;
	int64_t delta_hi = meas_max - f_center;
	uint64_t u_delta_low, u_delta_hi;
	int ppm_lo, ppm_hi;

	if(delta_low >= 0)
		return -1;
	if(delta_hi <= 0)
		return -1;

	/* __div64_32 divides 64 by 32; result is in the 64 argument. */
	u_delta_low = -delta_low * 1000000LL;
	__div64_32(&u_delta_low, f_center);
	ppm_lo = (int)u_delta_low;

	u_delta_hi = delta_hi * 1000000LL;
	__div64_32(&u_delta_hi, f_center);
	ppm_hi = (int)u_delta_hi;

	return ppm_lo < ppm_hi ? ppm_lo : ppm_hi;
}

void check_vco_frequencies()
{
	disable_irq();

	int f_min, f_max;
	pll_verbose("SoftPLL VCO Frequency/APR test:\n");

	spll_set_dac(-1, 0);
	f_min = spll_measure_frequency(SPLL_OSC_DMTD);
	spll_set_dac(-1, 65535);
	f_max = spll_measure_frequency(SPLL_OSC_DMTD);
	pll_verbose("DMTD VCO:  Low=%d Hz Hi=%d Hz, APR = %d ppm.\n", f_min, f_max, calc_apr(f_min, f_max, 62500000));

	spll_set_dac(0, 0);
	f_min = spll_measure_frequency(SPLL_OSC_REF);
	spll_set_dac(0, 65535);
	f_max = spll_measure_frequency(SPLL_OSC_REF);
	pll_verbose("REF VCO:   Low=%d Hz Hi=%d Hz, APR = %d ppm.\n", f_min, f_max, calc_apr(f_min, f_max, REF_CLOCK_FREQ_HZ));

	f_min = spll_measure_frequency(SPLL_OSC_EXT);
	pll_verbose("EXT clock: Freq=%d Hz\n", f_min);
}
