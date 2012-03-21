#include <stdio.h>
#include <stdlib.h>

#include "board.h"
#include "timer.h"
#include "trace.h"
#include "hw/softpll_regs.h"

#include "irq.h"

volatile int irq_count = 0,eee,yyy,py;

static volatile struct SPLL_WB *SPLL = (volatile struct SPLL_WB *) BASE_SOFTPLL;

/* The includes below contain code (not only declarations) to enable the compiler
   to inline functions where necessary and save some CPU cycles */


#include "spll_defs.h"
#include "spll_common.h"
#include "spll_debug.h"
#include "spll_helper.h"
#include "spll_main.h"
#include "spll_ptracker.h"

static volatile struct spll_helper_state helper;
static volatile struct spll_main_state mpll;

void _irq_entry()
{
	volatile uint32_t trr;
	int src = -1, tag;

	if(! (SPLL->CSR & SPLL_TRR_CSR_EMPTY))
	{
		trr = SPLL->TRR_R0;
		src = SPLL_TRR_R0_CHAN_ID_R(trr);
		tag = SPLL_TRR_R0_VALUE_R(trr);

		helper_update(&helper, tag, src);
		mpll_update(&mpll, tag, src);
	}

		irq_count++;
    //TRACE_DEV("%u\n", irq_count);
		clear_irq();
}

void spll_init()
{
	volatile int dummy;
	disable_irq();

	
	n_chan_ref = SPLL_CSR_N_REF_R(SPLL->CSR);
	n_chan_out = SPLL_CSR_N_OUT_R(SPLL->CSR);

	TRACE_DEV("SPLL_Init: %d ref channels, %d out channels\n", n_chan_ref, n_chan_out);
	
	SPLL->CSR= 0 ;
	SPLL->OCER = 0;
	SPLL->RCER = 0;
	SPLL->RCGER = 0;
	SPLL->DCCR = 0;
	SPLL->DEGLITCH_THR = 1000;
	SPLL->DAC_MAIN = 32000;
	while(! (SPLL->TRR_CSR & SPLL_TRR_CSR_EMPTY)) dummy = SPLL->TRR_R0;
	dummy = SPLL->PER_HPLL;
	SPLL->EIC_IER = 1;
}

int spll_check_lock()
{
	return helper.ld.locked ? 1 : 0;
}

#define CHAN_RX 0
#define CHAN_TCXO 1

void spll_test()
{
	int i = 0;
	volatile	int dummy;

  TRACE_DEV("running spll_init\n");
//  TRACE_DEV("enable irq\n");

//	mpll_init(&mpll, 0, CHAN_TCXO);
	while(!helper.ld.locked) ;//TRACE("%d\n", helper.phase.ld.locked);
	TRACE_DEV("Helper locked, starting main\n");
//	mpll_start(&mpll);

}

static int spll_master = 0;
void softpll_set_mode(int master)
{
    spll_master = master;
}

/* Enables SoftPLL in Slave mode.*/
void softpll_enable()
{
  if(spll_master)
    mprintf("Softpll: running in MASTER mode\n");
  else
    mprintf("Softpll: running in SLAVE mode\n");

  spll_init();
  
  helper_init(&helper, spll_master? CHAN_TCXO : CHAN_RX); /* Change CHAN_RX to CHAN_TCXO if master mode */
  helper_start(&helper);
  enable_irq();

  while(!helper.ld.locked) timer_delay(1); 
    TRACE_DEV("Helper locked\n");

  if(spll_master) return;  
  /* comment the lines below if Master mode */
  mpll_init(&mpll, CHAN_RX, CHAN_TCXO);
  mpll_start(&mpll);

  while(!mpll.ld.locked) timer_delay(1);
    TRACE_DEV("Softpll locked\n");
}

 
int softpll_check_lock()
{
	static int prev_lck = 0;
	int lck = mpll.ld.locked && helper.ld.locked;

	if(lck && !prev_lck) {
		TRACE_DEV("[softpll]: got lock\n");
	}else if (!lck && prev_lck)
		TRACE_DEV("[softpll]: lost lock\n");

	prev_lck = lck;
 	return lck;
}

int softpll_get_aux_status()
{
    return 0;
}

int softpll_busy(int channel)
{
 	return mpll.phase_shift_target != mpll.phase_shift_current;
}

void softpll_set_phase( int ps)
{
    mpll_set_phase_shift(&mpll, ((int32_t) ((int64_t)ps * (long long)(1<<HPLL_N) / 8000LL)));
}

void softpll_disable()
{
 	disable_irq();
}

int softpll_get_setpoint()
{
  return mpll.phase_shift_target;
}


void softpll_test()
{
    softpll_enable();
    for(;;)
    {
        mprintf("L!\n\n");
        softpll_set_phase(1000000);
        while(softpll_busy(0)) mprintf("%d %d %d%d %d %d\n", mpll.phase_shift_current, mpll.phase_shift_target, mpll.ld.locked, helper.ld.locked, lerr, ly);
        mprintf("R!\n\n");
        softpll_set_phase(10000);
        while(softpll_busy(0)) mprintf("%d %d %d%d %d\n", mpll.phase_shift_current, mpll.phase_shift_target, mpll.ld.locked, helper.ld.locked, lerr);
        
    }
}