#include "board.h"
#include "irq.h"

#include <hw/softpll_regs.h>

#define TAG_BITS 17

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
	
	
const struct softpll_config pll_cfg =
{
/* Helper PLL */
28*32*16, 			// f_kp
20*32*16,			// f_ki
16,					// setpoint
1000, 				// lock detect freq samples
2, 					// lock detect freq threshold
2.0*32*16,			// p_kp
0.05*32*16,			// p_ki
1000,				// lock detect phase samples
300,				// lock detect phase threshold
500,				// delock threshold
32000,				// HPLL dac bias

/* DMTD PLL */

100,  				// f_kp
600,                // f_ki

1304,					// p_kp
10,					// p_ki

1000,					// lock detect samples
500,					// lock detect threshold
500,				// delock threshold
32000,				// DPLL dac bias
1000  				// deglitcher threshold
};

struct softpll_state {
 	int h_lock_counter;
 	int h_i;
 	int h_dac_bias;
 	int h_p_setpoint;
 	int h_freq_mode;
 	int h_locked;
 	

	int d_dac_bias;
 	int d_tag_ref_d0;
 	int d_tag_fb_d0;
 	int d_tag_ref_ready;
 	int d_tag_fb_ready;
 	
 	int d_period_ref;
 	int d_period_fb;
 	int d_freq_mode;
 	int d_lock_counter;
 	int d_freq_error;
 	
 	int d_i;
 	
 	int d_p_setpoint;
 	int d_phase_shift;
 	int d_locked;
};


int eee, dve;

static volatile struct softpll_state pstate;

void _irq_entry()
{
	int dv;
	int tag_ref_ready = 0;
	int tag_fb_ready = 0;
	int tag_ref;
	int tag_fb;
	
	if(SPLL->CSR & READY_REF)
	{
	 	tag_ref = SPLL->TAG_REF;
	 	tag_ref_ready = 1;
	}
	
	if(SPLL->CSR & READY_FB)
	{
	 	tag_fb = SPLL->TAG_FB;
	 	tag_fb_ready = 1;
	}
	
	/* HPLL: active frequency branch */
	if(pstate.h_freq_mode)
	{
    	int freq_err =	SPLL->PER_HPLL;
		if(freq_err & 0x100) freq_err |= 0xffffff00; /* sign-extend */
		freq_err += pll_cfg.hpll_f_setpoint;

		/* PI control */
   		pstate.h_i += freq_err;
		dv = ((pstate.h_i * pll_cfg.hpll_f_ki + freq_err * pll_cfg.hpll_f_kp) >> PI_FRACBITS) + pll_cfg.hpll_dac_bias;

		SPLL->DAC_HPLL = dv; /* update DAC */

		/* lock detection */
		if(freq_err >= -pll_cfg.hpll_ld_f_threshold && freq_err <= pll_cfg.hpll_ld_f_threshold)
			pstate.h_lock_counter++;
		else
			pstate.h_lock_counter=0;

		/* frequency has been stable for quite a while? switch to phase branch */
		if(pstate.h_lock_counter == pll_cfg.hpll_ld_f_samples)
		{
			pstate.h_freq_mode = 0;
			pstate.h_dac_bias = dv;
			pstate.h_i = 0;
			pstate.h_lock_counter = 0;
			SPLL->CSR = SPLL_CSR_TAG_EN_W(CHAN_REF);
		}

	/* HPLL: active phase branch */
	} else if (tag_ref_ready) {
	    if(pstate.h_p_setpoint < 0) 		/* we don't have yet any phase samples? */
	     	pstate.h_p_setpoint = tag_ref & 0x3fff;
	 	else {
	 		int phase_err;
	 		
         	phase_err = (tag_ref & 0x3fff) - pstate.h_p_setpoint;
			pstate.h_i += phase_err;
			dv = ((pstate.h_i * pll_cfg.hpll_p_ki + phase_err * pll_cfg.hpll_p_kp) >> PI_FRACBITS) + pstate.h_dac_bias;
         
            SPLL->DAC_HPLL = dv; /* Update DAC */
            
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
				SPLL->CSR |= SPLL_CSR_TAG_EN_W(CHAN_FB); /* enable feedback channel and start DMPLL */
				pstate.h_locked = 1;
				pstate.d_tag_ref_d0 = -1;
				pstate.d_tag_fb_d0 = -1;
				pstate.d_freq_mode = 1;
				pstate.d_p_setpoint = 0;
				pstate.d_lock_counter = 0;
				pstate.d_i = 0;
				pstate.d_locked = 0;
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

    clear_irq();
}

void softpll_enable()
{
 	SPLL->CSR = 0;
 	disable_irq();

	SPLL->DAC_HPLL = pll_cfg.hpll_dac_bias;
	SPLL->DAC_DMPLL = pll_cfg.dpll_dac_bias;
	SPLL->DEGLITCH_THR = pll_cfg.dpll_deglitcher_threshold;
	
	pstate.h_p_setpoint = -1;
	pstate.h_i = 0;
	pstate.h_freq_mode = 1;
	pstate.h_lock_counter = 0;
	pstate.h_locked = 0;
	pstate.d_p_setpoint = 0;
	pstate.d_phase_shift = 0;
	
		  	
	SPLL->CSR = SPLL_CSR_TAG_EN_W(CHAN_PERIOD);
	SPLL->EIC_IER = 1;
	
	enable_irq();
//	softpll_test();
}

int softpll_check_lock()
{
	TRACE_DEV("LCK h:f%d l%d d: f%d l%d err %d\n", 
	pstate.h_freq_mode ,pstate.h_locked,
	pstate.d_freq_mode, pstate.d_locked,
	pstate.d_freq_error);

 	return pstate.h_locked && pstate.d_locked;
}

int softpll_busy()
{
 	return pstate.d_p_setpoint != pstate.d_phase_shift;
}

void softpll_set_phase(int ps)
{
	pstate.d_phase_shift = -(int32_t) ((int64_t)ps * 16384LL / 8000LL);
	TRACE_DEV("ADJdelta: phase %d [ps], %d units\n", ps, pstate.d_phase_shift);
}

void softpll_disable()
{
 	SPLL->CSR = 0;
 	disable_irq();
}

int softpll_get_setpoint()
{
  return pstate.d_p_setpoint;
}
