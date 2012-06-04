#include "board.h"

#include <inttypes.h>

#include <hw/pps_gen_regs.h>

#define PPS_PULSE_WIDTH 100000

static volatile struct PPSG_WB *PPSG = (volatile struct PPS_GEN_WB *) BASE_PPS_GEN;

void pps_gen_init()
{
	pps_gen_set_time(0, 0);
}

void pps_gen_adjust_nsec(int32_t how_much)
{
  TRACE_DEV("ADJ: nsec %d nanoseconds\n", how_much);
  PPSG->ADJ_UTCLO = 0;
  PPSG->ADJ_UTCHI = 0;
  PPSG->ADJ_NSEC = ( how_much / 8 );
  PPSG->CR =  PPSG_CR_CNT_EN | PPSG_CR_PWIDTH_W(PPS_PULSE_WIDTH) | PPSG_CR_CNT_ADJ;
}

void pps_gen_adjust_utc(int32_t how_much)
{
   TRACE_DEV("ADJ: utc %d seconds\n", how_much);
   PPSG->ADJ_UTCLO = how_much;
   PPSG->ADJ_UTCHI = 0;
   PPSG->ADJ_NSEC = 0;
   PPSG->CR = PPSG_CR_CNT_EN | PPSG_CR_PWIDTH_W(PPS_PULSE_WIDTH) | PPSG_CR_CNT_ADJ;
}

int pps_gen_busy()
{
  return PPSG->CR & PPSG_CR_CNT_ADJ ? 0 : 1;
}

void pps_gen_set_time(uint32_t utc, uint32_t nsec)
{
	 uint32_t cr = PPSG_CR_CNT_EN | PPSG_CR_PWIDTH_W(PPS_PULSE_WIDTH);
  
   PPSG->CR = cr;
   PPSG->ESCR = 0;

   PPSG->ADJ_UTCLO = utc;
   PPSG->ADJ_UTCHI = 0;
   PPSG->ADJ_NSEC = nsec;

   PPSG->CR = cr | PPSG_CR_CNT_SET;
   PPSG->CR = cr;
}

void pps_gen_get_time(uint32_t *utc, uint32_t *cntr_nsec)
{
  uint32_t cyc_before, cyc_after;
  uint32_t utc_lo;

  do {
  cyc_before = PPSG->CNTR_NSEC & 0xfffffff;
  utc_lo = PPSG->CNTR_UTCLO ;
  cyc_after = PPSG->CNTR_NSEC & 0xfffffff;
  } while (cyc_after < cyc_before);

//	delay(100000);

  if(utc) *utc = utc_lo;
  if(cntr_nsec) *cntr_nsec = cyc_after;
}

void pps_gen_enable_output(int enable)
{
 	if(enable)
		PPSG->ESCR = PPSG_ESCR_PPS_VALID  | PPSG_ESCR_TM_VALID;
	else
		PPSG->ESCR = 0;
}

