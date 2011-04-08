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
#define CHAN_HPLL 1

#define READY_FB (8<<4)
#define READY_REF (4<<4)
#define READY_PERIOD (2<<4)
#define READY_HPLL (1<<4)
             
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
};	
	
	
const struct softpll_config pll_cfg =
{
/* Helper PLL */
28*32*16, 			// f_kp
20*32*16,			// f_ki
16,					// setpoint
1000, 				// lock detect freq samples
2, 					// lock detect freq threshold
1.0*32*16,			// p_kp
0.03*32*16,			// p_ki
1000,				// lock detect phase samples
220,				// lock detect phase threshold
500,				// delock threshold
32000,				// HPLL dac bias

/* DMTD PLL */

0,					// f_kp
0,                  // f_ki

0,					// p_kp
0,					// p_ki

0,					// lock detect samples
0					// lock detect threshold
};

struct softpll_state {
 	int h_lock_counter;
 	int h_i;
 	int h_dac_bias;
 	int h_p_setpoint;
 	int h_freq_mode;
 	int h_locked;
};


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
			}
           
		}
	}
 
    /* DMPLL */
	if ((tag_ref_ready || tag_fb_ready) && pstate.h_locked)
	{
	
	}
	
	clear_irq();
}

void softpll_enable()
{
	pstate.h_dac_bias = HPLL_DAC_BIAS;
	SPLL->DAC_HPLL = pstate.h_dac_bias;

	pstate.h_p_setpoint = -1;
	pstate.h_i = 0;
	pstate.h_freq_mode = 1;
	pstate.h_lock_counter = 0;
	pstate.h_locked = 0;
	  	
	SPLL->CSR = SPLL_CSR_TAG_EN_W(CHAN_PERIOD);
	SPLL->EIC_IER = 1;
	
	enable_irq();
}

int softpll_check_lock()
{
 	return pstate.h_locked;
}

void softpll_disable()
{
 	SPLL->CSR = 0;
 	disable_irq();
}
