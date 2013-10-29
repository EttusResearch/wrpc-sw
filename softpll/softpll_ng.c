#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <wrc.h>
#include "board.h"
#include "trace.h"
#include "hw/softpll_regs.h"
#include "hw/pps_gen_regs.h"

#include "softpll_ng.h"

#include "irq.h"

volatile int irq_count = 0;

volatile struct SPLL_WB *SPLL;
volatile struct PPSG_WB *PPSG;

int spll_n_chan_ref, spll_n_chan_out;

/*
 * The includes below contain code (not only declarations) to enable
 * the compiler to inline functions where necessary and save some CPU
 * cycles
 */

#include "spll_defs.h"
#include "spll_common.h"
#include "spll_debug.h"
#include "spll_helper.h"
#include "spll_main.h"
#include "spll_ptracker.h"
#include "spll_external.h"

#define MAIN_CHANNEL (spll_n_chan_ref)

#define SEQ_START_EXT 1
#define SEQ_WAIT_EXT 2
#define SEQ_START_HELPER 3
#define SEQ_WAIT_HELPER 4
#define SEQ_START_MAIN 5
#define SEQ_WAIT_MAIN 6
#define SEQ_DISABLED 7
#define SEQ_READY 8
#define SEQ_CLEAR_DACS 9
#define SEQ_WAIT_CLEAR_DACS 10

#define AUX_DISABLED 1
#define AUX_LOCK_PLL 2
#define AUX_ALIGN_PHASE 3
#define AUX_READY 4

struct spll_aux_state {
	int seq_state;
	int32_t phase_target;
	union {
		struct spll_main_state dmtd;
		/* spll_external_state ch_bb */
	} pll;
};

struct softpll_state {
	int mode;
	int seq_state;
	int dac_timeout;
	int default_dac_main;
	int delock_count;
	int32_t mpll_shift_ps;

	struct spll_helper_state helper;
	struct spll_external_state ext;
	struct spll_main_state mpll;
	struct spll_aux_state aux[MAX_CHAN_AUX];
	struct spll_ptracker_state ptrackers[MAX_PTRACKERS];
};

static volatile struct softpll_state softpll;

static volatile int ptracker_mask = 0;
/* fixme: should be done by spll_init() but spll_init is called to
 * switch modes (and we won't like messing around with ptrackers
 * there) */

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
			
			softpll.dac_timeout = timer_get_tics();
			softpll.seq_state = SEQ_WAIT_CLEAR_DACS;
			
			break;
		}
		
		/* State "Wait until DACs have been cleared". Makes sure the VCO control inputs have stabilized before starting the PLL. */
		case SEQ_WAIT_CLEAR_DACS:
		{
			if (timer_get_tics() - softpll.dac_timeout >
			    TICS_PER_SECOND / 20)
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
			if (external_locked(&s->ext))
			    s->seq_state = SEQ_START_HELPER;
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
			}
			break;
		}

		case SEQ_READY:
		{
			if (!s->helper.ld.locked) 
			{	
				s->delock_count++;
				s->seq_state = SEQ_CLEAR_DACS;
			} else if (s->mode == SPLL_MODE_GRAND_MASTER && !external_locked(&s->ext))
			{
				s->delock_count++;
				s->seq_state = SEQ_START_EXT;
			} else if (s->mode == SPLL_MODE_SLAVE && !s->mpll.ld.locked)
			{
				s->delock_count++;
				s->seq_state = SEQ_CLEAR_DACS;
			}
			break;
		}
	}
}

static inline void update_loops(struct softpll_state *s, int tag_value, int tag_source)
{

	if(s->mode == SPLL_MODE_GRAND_MASTER) {
		switch(s->seq_state) {
			case SEQ_WAIT_EXT:
			case SEQ_START_HELPER:
			case SEQ_WAIT_HELPER:
			case SEQ_START_MAIN:
			case SEQ_WAIT_MAIN:
			case SEQ_READY:
				external_update(&s->ext, tag_value, tag_source);
				break;
		}
	}

	switch(s->seq_state) {
		case SEQ_WAIT_HELPER:
		case SEQ_START_MAIN:
		case SEQ_WAIT_MAIN:
		case SEQ_READY:
			helper_update(&s->helper, tag_value, tag_source);
			break;
	}
	
	if(s->seq_state == SEQ_WAIT_MAIN)
	{
		mpll_update(&s->mpll, tag_value, tag_source);
	}

	if(s->seq_state == SEQ_READY)
	{
		if(s->mode == SPLL_MODE_SLAVE)
		{
			int i;

			mpll_update(&s->mpll, tag_value, tag_source);
			for (i = 0; i < spll_n_chan_out - 1; i++)
				mpll_update(&s->aux[i].pll.dmtd, tag_value, tag_source); // fixme: bb hooks here

		}

		update_ptrackers(s, tag_value, tag_source);
	}
}

void _irq_entry()
{
	struct softpll_state *s = (struct softpll_state *)&softpll;

/* check if there are more tags in the FIFO */
	while (!(SPLL->TRR_CSR & SPLL_TRR_CSR_EMPTY)) {
	
		volatile uint32_t trr = SPLL->TRR_R0;
		int tag_source = SPLL_TRR_R0_CHAN_ID_R(trr);
		int tag_value  = SPLL_TRR_R0_VALUE_R(trr);

		sequencing_fsm(s, tag_value, tag_source);
		update_loops(s, tag_value, tag_source);
	}

	irq_count++;
	clear_irq();
}

void spll_clear_dacs()
{
	SPLL->DAC_HPLL = 0;
	SPLL->DAC_MAIN = 0;
	timer_delay(100);
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
	PPSG->CR = PPSG_CR_CNT_EN | PPSG_CR_CNT_RST | PPSG_CR_PWIDTH_W(PPS_WIDTH);

	if(mode == SPLL_MODE_GRAND_MASTER)
	{
		if(SPLL->ECCR & SPLL_ECCR_EXT_SUPPORTED)
			external_init(&s->ext, spll_n_chan_ref + spll_n_chan_out, align_pps);
		else {
			TRACE_DEV("softpll: attempting to enable GM mode on non-GM hardware.\n");
			return;
		}
	}

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

	TRACE_DEV
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
		TRACE_DEV("Can't start channel %d, the PLL is not ready\n",
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

#ifdef CONFIG_PPSI /* use __div64_32 from ppsi library to save libgcc memory */
static int32_t from_picos(int32_t ps)
{
	extern uint32_t __div64_32(uint64_t *n, uint32_t base);
	uint64_t ups = ps;

	if (ps >= 0) {
		ups *= 1 << HPLL_N;
		__div64_32(&ups, CLOCK_PERIOD_PICOSECONDS);
		return ups;
	}
	ups = -ps * (1 << HPLL_N);
	__div64_32(&ups, CLOCK_PERIOD_PICOSECONDS);
	return -ups;
}
#else /* previous implementation: ptp-noposix has no __div64_32 available */
static int32_t from_picos(int32_t ps)
{
	return (int32_t) ((int64_t) ps * (int64_t) (1 << HPLL_N) /
			  (int64_t) CLOCK_PERIOD_PICOSECONDS);
}
#endif

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
	int div = (DIVIDE_DMTD_CLOCKS_BY_2 ? 2 : 1);
	mpll_set_phase_shift(st, from_picos(value_picoseconds) / div);
	softpll.mpll_shift_ps = value_picoseconds;
}

void spll_set_phase_shift(int channel, int32_t value_picoseconds)
{
	int i;
	if (channel == SPLL_ALL_CHANNELS) {
		spll_set_phase_shift(0, value_picoseconds);
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
	if (softpll.mode > 0)
		TRACE_DEV
		    ("softpll: irq_count %d sequencer_state %d mode %d "
		     "alignment_state %d HL%d EL%d ML%d HY=%d "
		     "MY=%d EY=%d DelCnt=%d extsc=%d\n",
		     irq_count, softpll.seq_state, softpll.mode,
		     softpll.ext.realign_state, softpll.helper.ld.locked,
		     softpll.ext.ld.locked, softpll.mpll.ld.locked,
		     softpll.helper.pi.y, softpll.mpll.pi.y, softpll.ext.pi.y,
		     softpll.delock_count, softpll.ext.sample_n);
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
		TRACE_DEV("Enabling ptracker channel: %d\n", ref_channel);

	} else {
		ptracker_mask &= ~(1 << ref_channel);
		if (ref_channel != softpll.mpll.id_ref)
			spll_enable_tagger(ref_channel, 0);
		TRACE_DEV("Disabling ptracker tagger: %d\n", ref_channel);
	}
}

int spll_get_delock_count()
{
	return softpll.delock_count;
}

static inline int aux_locking_enabled(int channel)
{
	uint32_t occr_aux_en = SPLL_OCCR_OUT_EN_R(SPLL->OCCR);
	
	return occr_aux_en & (1 << channel);
}

static inline void aux_set_channel_status(int channel, int locked)
{
	if(!locked)
		SPLL->OCCR &= ~(SPLL_OCCR_OUT_LOCK_W((1 << channel)));
	else
		SPLL->OCCR |= (SPLL_OCCR_OUT_LOCK_W((1 << channel)));
}

int spll_update_aux_clocks()
{
	int ch;

	for (ch = 1; ch < spll_n_chan_out; ch++) 
	{
		struct spll_aux_state *s = (struct spll_aux_state *) &softpll.aux[ch - 1];

		if(s->seq_state != AUX_DISABLED && !aux_locking_enabled(ch))
		{
			TRACE_DEV("softpll: disabled aux channel %d\n", ch);
			spll_stop_channel(ch);
			aux_set_channel_status(ch, 0);
			s->seq_state = AUX_DISABLED;
		}

		switch (s->seq_state) {
			case AUX_DISABLED:
				if (softpll.mpll.ld.locked && aux_locking_enabled(ch)) {
					TRACE_DEV("softpll: enabled aux channel %d\n", ch);
					spll_start_channel(ch);
					s->seq_state = AUX_LOCK_PLL;
				}
				break;

			case AUX_LOCK_PLL:
				if (s->pll.dmtd.ld.locked) {
					TRACE_DEV ("softpll: channel %d locked [aligning @ %d ps]\n", ch, softpll.mpll_shift_ps);
					set_phase_shift(ch, softpll.mpll_shift_ps);
					s->seq_state = AUX_ALIGN_PHASE;
				}

				break;

			case AUX_ALIGN_PHASE:
				if (!mpll_shifter_busy(&s->pll.dmtd)) {
					TRACE_DEV("softpll: channel %d phase aligned\n", ch);
					aux_set_channel_status(ch, 1);
					s->seq_state = AUX_READY;
				}
				break;

			case AUX_READY:
				if (!softpll.mpll.ld.locked || !s->pll.dmtd.ld.locked) {
					TRACE_DEV("softpll: aux channel %d or mpll lost lock\n", ch);
					aux_set_channel_status(ch, 0); 
					s->seq_state = AUX_DISABLED;
				}
				break;
			}
	}
	return 0;
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

const char *spll_get_aux_status_string(int channel)
{
	const char *aux_stat[] = {"disabled", "locking", "aligning", "locked"};
	struct spll_aux_state *s = (struct spll_aux_state* )&softpll.aux[channel];

	switch(s->seq_state)
	{
		case AUX_DISABLED: return aux_stat[0];
		case AUX_LOCK_PLL: return aux_stat[1];
		case AUX_ALIGN_PHASE: return aux_stat[2];
		case AUX_READY: return aux_stat[3];
	}
	return "";
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
