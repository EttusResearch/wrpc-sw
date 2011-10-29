#include <stdio.h>
#include <stdlib.h>

#include "board.h"
#include "irq.h"

#include <hw/softpll_regs.h>

#include "gpio.h"

#define TAG_BITS 20

#define HPLL_N 14

#define HPLL_DAC_BIAS 30000
#define PI_FRACBITS 12

#define CHAN_FB 8
#define CHAN_REF 4
#define CHAN_PERIOD 2
//#define CHAN_HPLL 1

#define READY_FB (8<<4)
#define READY_REF (4<<4)
#define READY_PERIOD (2<<4)
//#define READY_HPLL (1<<4)
             
static volatile struct SPLL_WB *SPLL = (volatile struct SPLL_WB *) BASE_SOFTPLL;


typedef struct {
  	int ki, kp;
  	int integrator;
  	int bias;
	int anti_windup;
	int y_min;
	int y_max;
	int x,y;
} spll_pi_t;

static inline int pi_update(spll_pi_t *pi, int x)
{
	int i_new, y;
	pi->x = x;
	i_new = pi->integrator + x;
	
	y = ((i_new * pi->ki + x * pi->kp) >> PI_FRACBITS) + pi->bias;
	
    if(y < pi->y_min)
    {
       	y = pi->y_min;
       	if((pi->anti_windup && (i_new > pi->integrator)) || !pi->anti_windup)
       		pi->integrator = i_new;
	} else if (y > pi->y_max) {
       	y = pi->y_max;
      	if((pi->anti_windup && (i_new < pi->integrator)) || !pi->anti_windup)
       		pi->integrator = i_new;
	} else
		pi->integrator = i_new;

    pi->y = y;
    return y;
   
}

static inline void pi_init(spll_pi_t *pi)
{
 	pi->integrator = 0;
}

typedef struct {
  	int lock_cnt;
  	int lock_samples;
  	int delock_samples;
  	int threshold;
  	int locked;
} spll_lock_det_t;


static inline int ld_update(spll_lock_det_t *ld, int y)
{
	if (abs(y) <= ld->threshold)
	{
		if(ld->lock_cnt < ld->lock_samples)
			ld->lock_cnt++;
		
		if(ld->lock_cnt == ld->lock_samples)
			ld->locked = 1;
	} else {
	 	if(ld->lock_cnt > ld->delock_samples)
	 		ld->lock_cnt--;
		
		if(ld->lock_cnt == ld->delock_samples)
		{
		 	ld->lock_cnt=  0;
		 	ld->locked = 0;
		}
	}
	return ld->locked;
}

static void ld_init(spll_lock_det_t *ld)
{
 	ld->locked = 0;
 	ld->lock_cnt = 0;
}

struct spll_helper_state {
 	spll_pi_t pi_freq, pi_phase;
 	spll_lock_det_t ld_freq, ld_phase;
 	int f_setpoint;
 	int p_setpoint;
 	int freq_mode;
};

struct dmpll_state {
 	spll_pi_t pi_freq, pi_phase;
 	spll_lock_det_t ld_freq, ld_phase;
 	int setpoint;
 	int freq_mode;

 	int tag_ref_d0;
 	int tag_fb_d0;
 	
 	int period_ref;
 	int period_fb;
 	
 	int phase_shift;
};


struct softpll_config {
	int hpll_f_kp;
	int hpll_f_ki;
	int hpll_f_setpoint;
	int hpll_ld_f_samples;
	int hpll_ld_f_threshold;
	
	int hpll_p_kp;
	int hpll_p_ki;
	
	int hpll_ld_p_samples;
	int hpll_ld_p_threshold;

	int hpll_delock_threshold;
	
	int hpll_dac_bias;

	int dpll_f_kp;
	int dpll_f_ki;

	int dpll_p_kp;
	int dpll_p_ki;

	int dpll_ld_samples;
	int dpll_ld_threshold;
	int dpll_delock_threshold;
	int dpll_dac_bias;
	int dpll_deglitcher_threshold;
};	
	
	
	
	
void helper_init(struct spll_helper_state *s)
{
	
	/* Frequency branch PI controller */
	s->pi_freq.y_min = 5;
	s->pi_freq.y_max = 65530;
	s->pi_freq.anti_windup = 0;
	s->pi_freq.kp = 28*32*16;
	s->pi_freq.ki = 50*32*16;
	s->pi_freq.bias = 32000;

	/* Freqency branch lock detection */
	s->ld_freq.threshold = 2;
	s->ld_freq.lock_samples = 1000;
	s->ld_freq.delock_samples = 990;
	
	/* Phase branch PI controller */
	s->pi_phase.y_min = 5;
	s->pi_phase.y_max = 65530;
   	s->pi_phase.kp = (int)(2.0 * 32.0 * 4.0);
	s->pi_phase.ki = (int)(0.05 * 32.0 * 0.1);
	s->pi_phase.anti_windup = 0;
	s->pi_phase.bias = 32000;
	
	/* Phase branch lock detection */
	s->ld_phase.threshold = 500;
	s->ld_phase.lock_samples = 1000;
	s->ld_phase.delock_samples = 990;
	
	s->freq_mode = 1;
	s->f_setpoint = 16;

	pi_init(&s->pi_freq);
	ld_init(&s->ld_freq);
	pi_init(&s->pi_phase);
	ld_init(&s->ld_phase);
}


static struct spll_helper_state helper;
static volatile int irq_cnt = 0;

#define HELPER_PERIOD_BITS 8

static inline void helper_irq(struct spll_helper_state *hpll, int tag_ref)
{
	int err, y;

	/* HPLL: active frequency branch */
	if(hpll->freq_mode)
	{
     	err = SPLL->PER_HPLL;

		if (err & (1<<HELPER_PERIOD_BITS)) 
			err |= ~ ((1<<HELPER_PERIOD_BITS) - 1); /* sign-extend the freq error */

		err += hpll->f_setpoint;
		y = pi_update(&hpll->pi_freq, err);

		SPLL->DAC_HPLL = y;
		
		if(ld_update(&hpll->ld_freq, err))
		{
			hpll->freq_mode = 0;
			hpll->p_setpoint = -1;
			hpll->pi_phase.bias = y;
			pi_init(&hpll->pi_phase);
			ld_init(&hpll->ld_phase);
			SPLL->CSR = SPLL_CSR_TAG_EN_W(CHAN_REF);
		}
	} else { 	/* HPLL: active phase branch */
		if(hpll->p_setpoint < 0)
		{
		 	hpll->p_setpoint = tag_ref;
		 	return;
		}
		err = tag_ref - hpll->p_setpoint;
        hpll->p_setpoint += 16384;
		hpll->p_setpoint &= ((1<<TAG_BITS)-1);

		y = pi_update(&hpll->pi_phase, err);
		SPLL->DAC_HPLL = y;
		
		ld_update(&hpll->ld_phase, err);
	}
}

void _irq_entry()
{
	volatile uint32_t csr;
	int tag_ref = -1;
	int tag_fb = -1;
	
	irq_cnt ++;
	
	csr = SPLL->CSR;
	
	if(csr & READY_REF)
	 	tag_ref = SPLL->TAG_REF;
	
	if(csr & READY_FB)
	 	tag_fb = SPLL->TAG_FB;
	
	helper_irq(&helper, tag_ref);
	

#if 0
	/* HPLL: active phase branch */
	} else if (tag_ref_ready) {
	    if(pstate.h_p_setpoint < 0) 		/* we don't have yet any phase samples? */
	     	pstate.h_p_setpoint = tag_ref; // & 0x3fff;
	 	else {
	 		int phase_err;
	 		
//	 		if(tag_ref > 16384) tag_ref -= 16384;
//			tag_ref &x7fff;

         	phase_err = tag_ref - pstate.h_p_setpoint;

			if(pstate.h_dac_val > 0 && pstate.h_dac_val < 65530)
			pstate.h_i += phase_err;
			
			pstate.h_p_setpoint += 16384;
			pstate.h_p_setpoint &= ((1<<TAG_BITS)-1);

			dv = ((pstate.h_i * pll_cfg.hpll_p_ki + phase_err * pll_cfg.hpll_p_kp) >> PI_FRACBITS) + pstate.h_dac_bias;
                   
            if(dv >= 65530) dv = 65530;
			if(dv < 0) dv = 0;

            pstate.h_dac_val = dv;
            SPLL->DAC_HPLL = dv; /* Update DAC */

            pstate.h_tag = tag_ref;
            
            if(abs(phase_err) >= pll_cfg.hpll_delock_threshold && pstate.h_locked)
            {
            	SPLL->CSR = SPLL_CSR_TAG_EN_W(CHAN_PERIOD); /* fall back to freq mode */
             	pstate.h_locked = 0;
             	pstate.h_freq_mode = 1;
            }
            
            if(abs(phase_err) <= pll_cfg.hpll_ld_p_threshold && !pstate.h_locked)
            	pstate.h_lock_counter++;
			else
				pstate.h_lock_counter = 0;
				
			if(pstate.h_lock_counter == pll_cfg.hpll_ld_p_samples)
			{
#if 1
				SPLL->CSR |= SPLL_CSR_TAG_EN_W(CHAN_FB); /* enable feedback channel and start DMPLL */
				pstate.h_locked = 1;
				pstate.d_tag_ref_d0 = -1;
				pstate.d_tag_fb_d0 = -1;
				pstate.d_freq_mode = 1;
				pstate.d_p_setpoint = 0;
				pstate.d_lock_counter = 0;
				pstate.d_i = 0;
				pstate.d_locked = 0;      
#endif
			}
           
		}
	}
 
    /* DMPLL */
	if(pstate.h_locked && pstate.d_freq_mode)
	{
		int freq_err;
		if(tag_ref_ready)
		{
			if(pstate.d_tag_ref_d0 > 0)
			{
			  	int tmp  = tag_ref - pstate.d_tag_ref_d0;
			 	if(tmp < 0) tmp += (1<<TAG_BITS)-1;
			 	pstate.d_period_ref = tmp;
			 }
			 pstate.d_tag_ref_d0 = tag_ref;
		}

		if(tag_fb_ready)
		{
			if(pstate.d_tag_fb_d0 > 0)
			{
			 	int tmp  = tag_fb - pstate.d_tag_fb_d0;
			 	if(tmp < 0) tmp += (1<<TAG_BITS)-1;
			 	pstate.d_period_fb = tmp;
			}
			pstate.d_tag_fb_d0 = tag_fb;
    	}
    	
    	freq_err = - pstate.d_period_fb + pstate.d_period_ref;

    	pstate.d_freq_error = freq_err;
    	
    	pstate.d_i += freq_err;
		dv = ((pstate.d_i * pll_cfg.dpll_f_ki + freq_err * pll_cfg.dpll_f_kp) >> PI_FRACBITS) + pll_cfg.dpll_dac_bias;
		SPLL->DAC_DMPLL = dv;

		if(abs(freq_err) <= pll_cfg.dpll_ld_threshold)
			pstate.d_lock_counter++;
		else
			pstate.d_lock_counter=0;

		/* frequency has been stable for quite a while? switch to phase branch */
		if(pstate.d_lock_counter == pll_cfg.dpll_ld_samples)
		{
			pstate.d_freq_mode = 0;
			pstate.d_dac_bias = dv;
			pstate.d_i = 0;
			pstate.d_lock_counter = 0;
			pstate.d_tag_ref_ready = 0;
			pstate.d_tag_fb_ready = 0;
			pstate.d_p_setpoint = 0;
		}
    }

	/* DMPLL phase-lock */
 	if(pstate.h_locked && !pstate.d_freq_mode)
 	{
 		int phase_err = 0;
 		 		
 		if(tag_ref_ready)
 			pstate.d_tag_ref_d0 = tag_ref;
 		
 		tag_ref = pstate.d_tag_ref_d0 ;

 		tag_fb += pstate.d_p_setpoint;  // was tag_fb
 		
 		tag_fb &= (1<<TAG_BITS)-1;
 		
 		//if(tag_ref > (1<<TAG_BITS)) tag_ref -= (1<<TAG_BITS);
 		//if(tag_ref < 0) tag_ref += (1<<TAG_BITS);
 				
 		if(tag_fb_ready)
		{
			while (tag_ref > 16384 ) tag_ref-=16384;
			while (tag_fb > 16384  ) tag_fb-=16384;
			
 		    phase_err = tag_ref-tag_fb;

    		pstate.d_i += phase_err;
			dv = ((pstate.d_i * pll_cfg.dpll_p_ki + phase_err * pll_cfg.dpll_p_kp) >> PI_FRACBITS) + pll_cfg.dpll_dac_bias;
			SPLL->DAC_DMPLL = dv;

 		    eee=phase_err;
		} 	
 	
 		if(pstate.d_p_setpoint < pstate.d_phase_shift)
	 		pstate.d_p_setpoint++;
 		else if(pstate.d_p_setpoint > pstate.d_phase_shift)
 			pstate.d_p_setpoint--;
		
		
		if(abs(phase_err) <= pll_cfg.dpll_ld_threshold && !pstate.d_locked)
            	pstate.d_lock_counter++;
			else
				pstate.d_lock_counter = 0;
				
			if(pstate.d_lock_counter == pll_cfg.dpll_ld_samples)
				pstate.d_locked = 1;
 	}

	gpio_out(GPIO_PIN_LED_STATUS, 0);
#endif

    clear_irq();
}

static int prev_lck = 0;

void softpll_enable()
{
 	SPLL->CSR = 0;
 	disable_irq();
 	
 	helper_init(&helper);

	SPLL->DAC_HPLL = 0;
	SPLL->DEGLITCH_THR = 2000; 
	
	SPLL->CSR = SPLL_CSR_TAG_EN_W(CHAN_PERIOD);
	SPLL->EIC_IER = 1;
	
	enable_irq();
	TRACE_DEV("[softpll]: enabled\n");
	prev_lck = 0;
}


int softpll_check_lock()
{
	TRACE_DEV("[softpll] Helper: lock %d freqmode %d x %d y %d\n", helper.ld_phase.locked, helper.freq_mode, helper.pi_freq.x, helper.pi_phase.y) ; 
	
/*nt lck = pstate.h_locked && pstate.d_locked;
	
	if(lck && !prev_lck)
		TRACE_DEV("[softpll]: got lock\n");
	else if (!lck && prev_lck)
		TRACE_DEV("[softpll]: lost lock\n");*/

//	prev_lck = lck;
// 	return lck;
}

int softpll_busy()
{
// 	return pstate.d_p_setpoint != pstate.d_phase_shift;
}

void softpll_set_phase(int ps)
{
//	pstate.d_phase_shift = -(int32_t) ((int64_t)ps * 16384LL / 8000LL);
//	TRACE_DEV("ADJdelta: phase %d [ps], %d units\n", ps, pstate.d_phase_shift);
}

void softpll_disable()
{
 	SPLL->CSR = 0;
 	disable_irq();
}

int softpll_get_setpoint()
{
//  return pstate.d_p_setpoint;
}

