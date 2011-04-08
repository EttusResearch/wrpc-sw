#include "board.h"

#include <inttypes.h>

#include <hw/pps_gen_regs.h>

#define PPS_PULSE_WIDTH 100000

static inline void ppsg_writel(uint32_t reg,uint32_t data)
{
  *(volatile uint32_t *) (BASE_PPSGEN + reg) = data;
}
 
static inline uint32_t ppsg_readl(uint32_t reg)
{
  return *(volatile uint32_t *)(BASE_PPSGEN + reg);
}
    
void pps_gen_init()
{
	uint32_t cr;
	
	cr = PPSG_CR_CNT_EN | PPSG_CR_PWIDTH_W(PPS_PULSE_WIDTH);
  
  ppsg_writel( PPSG_REG_CR, cr);

  ppsg_writel( PPSG_REG_ADJ_UTCLO, 1285700840);
  ppsg_writel( PPSG_REG_ADJ_UTCHI, 0);
  ppsg_writel( PPSG_REG_ADJ_NSEC, 0);

  ppsg_writel( PPSG_REG_CR, cr | PPSG_CR_CNT_SET);
  ppsg_writel( PPSG_REG_CR, cr);
}

void pps_gen_adjust_nsec(int32_t how_much)
{
  uint32_t cr;

  mprintf("AdjustPPS: %d nanoseconds\n", how_much);
  ppsg_writel( PPSG_REG_ADJ_UTCLO, 0);
  ppsg_writel( PPSG_REG_ADJ_UTCHI, 0);

  ppsg_writel( PPSG_REG_ADJ_NSEC, ( how_much / 8 ));

  ppsg_writel( PPSG_REG_CR,   PPSG_CR_CNT_EN | PPSG_CR_PWIDTH_W(PPS_PULSE_WIDTH) | PPSG_CR_CNT_ADJ);
}

void shw_pps_gen_adjust_utc(int32_t how_much)
{
  uint32_t cr;

  mprintf("AdjustUTC: %d seconds\n", how_much);
  ppsg_writel( PPSG_REG_ADJ_UTCLO, how_much);
  ppsg_writel( PPSG_REG_ADJ_UTCHI, 0);
  ppsg_writel( PPSG_REG_ADJ_NSEC, 0);
  ppsg_writel( PPSG_REG_CR,   PPSG_CR_CNT_EN | PPSG_CR_PWIDTH_W(PPS_PULSE_WIDTH) | PPSG_CR_CNT_ADJ);
}

int pps_gen_busy()
{
  return ppsg_readl(PPSG_REG_CR) & PPSG_CR_CNT_ADJ ? 0 : 1;
}

void pps_gen_get_time(uint32_t *utc, uint32_t *cntr_nsec)
{
  uint32_t cyc_before, cyc_after;
  uint32_t utc_lo;

  do {
  cyc_before =ppsg_readl(PPSG_REG_CNTR_NSEC) & 0xfffffff;
  utc_lo = ppsg_readl(PPSG_REG_CNTR_UTCLO) ;
  cyc_after = ppsg_readl(PPSG_REG_CNTR_NSEC) & 0xfffffff;
  } while (cyc_after < cyc_before);

  if(utc) *utc = utc_lo;
  if(cntr_nsec) *cntr_nsec = cyc_after;
}
