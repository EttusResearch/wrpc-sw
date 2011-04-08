#include "board.h"
#include "irq.h"

#include <hw/softpll_regs.h>

#define TAG_BITS 17

#define HPLL_DAC_BIAS 30000
#define PI_FRACBITS 12

#define CHAN_REF 4
#define CHAN_PERIOD 2
#define CHAN_HPLL 1

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

	int dpll_f_kp;
	int dpll_f_ki;

	int dpll_p_kp;
	int dpll_p_ki;

	int dpll_ld_samples;
	int dpll_ld_threshold;
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
}

static int hpll_lock_count = 0;
static int hpll_i = 0;
static int hpll_dac_bias = 0;
static int hpll_perr_setpoint;
int hpll_freq_mode = 1;

volatile int freq_err, eee;
volatile int phase_err;
volatile int hpll_tag;
int hpll_prev_tag;

volatile int dv;

void _irq_entry()
{
	
	if(hpll_freq_mode)
	{
    	freq_err =	SPLL->PER_HPLL;// >> 1;

	if(freq_err & 0x100) freq_err |= 0xffffff00;
//		freq_err = - freq_err;

		freq_err += hpll_ferr_setpoint;

		hpll_i += freq_err;

		eee=freq_err;

		dv = ((hpll_i * hpll_ki + freq_err * hpll_kp) >> PI_FRACBITS) + hpll_dac_bias;
		SPLL->DAC_HPLL = dv;

		if(freq_err >= -hpll_ld_threshold && freq_err <= hpll_ld_threshold)
			hpll_lock_count++;
		else
			hpll_lock_count=0;

		if(hpll_lock_count == hpll_ld_samples)
		{
			hpll_freq_mode = 0;
			hpll_dac_bias = dv;
			hpll_i = 0;
			SPLL->CSR = SPLL_CSR_TAG_EN_W(CHAN_REF);
		}
	} else {
	    if(hpll_perr_setpoint < 0)
	    {
	     	hpll_perr_setpoint = SPLL->TAG_REF & 0x3fff;
	    } else {
         	phase_err = (SPLL->TAG_REF & 0x3fff) - hpll_perr_setpoint;
			hpll_i += phase_err;
			dv = ((hpll_i * hpll_pki + phase_err * hpll_pkp) >> PI_FRACBITS) + hpll_dac_bias;
            SPLL->DAC_HPLL = dv;
		}
	}
 
	clear_irq();
}

void softpll_init()
{
	hpll_dac_bias = HPLL_DAC_BIAS;
	SPLL->DAC_HPLL = HPLL_DAC_BIAS;
	hpll_perr_setpoint = -1;
	hpll_i = 0;
  	hpll_freq_mode = 1;
	hpll_lock_count = 0;
	  	
	SPLL->CSR = SPLL_CSR_TAG_EN_W(CHAN_PERIOD);
	SPLL->EIC_IER = 1;
	
	
	enable_irq();

	
	for(;;)
	{		
		mprintf("err %07d ldc %07d tag %07d dv %07d\n", eee, hpll_lock_count, phase_err, dv);
	}
}