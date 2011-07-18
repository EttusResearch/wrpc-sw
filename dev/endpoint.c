#include <stdio.h>

#include "board.h"
#include<endpoint.h>

#include <hw/endpoint_regs.h>
#include <hw/endpoint_mdio.h>

#define UIS_PER_SERIAL_BIT 800

static int autoneg_enabled;

static volatile struct EP_WB *EP = (volatile struct EP_WB *) BASE_EP;

/* functions for accessing PCS registers */

static uint16_t pcs_read(int location)
{
  EP->MDIO_CR = EP_MDIO_CR_ADDR_W(location >> 2);
  while ((EP->MDIO_SR & EP_MDIO_SR_READY) == 0);
  return EP_MDIO_SR_RDATA_R(EP->MDIO_SR) & 0xffff;
}

static void pcs_write(int location,
		      int value)
{
  EP->MDIO_CR = EP_MDIO_CR_ADDR_W(location >> 2)
		| EP_MDIO_CR_DATA_W(value)
		| EP_MDIO_CR_RW;

  while ((EP->MDIO_SR & EP_MDIO_SR_READY) == 0);
}

static void set_mac_addr(uint8_t dev_addr[])
{                                
  EP->MACL = ((uint32_t)dev_addr[2] << 24)
    | ((uint32_t)dev_addr[3] << 16)
    | ((uint32_t)dev_addr[4] << 8)
    | ((uint32_t)dev_addr[5]);

  EP->MACH = ((uint32_t)dev_addr[0] << 8)
    | ((uint32_t)dev_addr[1]);
}

void get_mac_addr(uint8_t dev_addr[])
{
  dev_addr[5] = (uint8_t)(EP->MACL & 0x000000ff);
  dev_addr[4] = (uint8_t)(EP->MACL & 0x0000ff00) >> 8;
  dev_addr[3] = (uint8_t)(EP->MACL & 0x00ff0000) >> 16; 
  dev_addr[2] = (uint8_t)(EP->MACL & 0xff000000) >> 24; 
  dev_addr[1] = (uint8_t)(EP->MACH & 0x000000ff);
  dev_addr[0] = (uint8_t)(EP->MACH & 0x0000ff00) >> 8;
}


void ep_init(uint8_t mac_addr[])
{
  int i;
  set_mac_addr(mac_addr);

  EP->ECR = 0;
  EP->DMCR =  EP_DMCR_EN | EP_DMCR_N_AVG_W(DMTD_AVG_SAMPLES);
  EP->RFCR =  3 << EP_RFCR_QMODE_SHIFT;
  EP->TSCR = EP_TSCR_EN_TXTS | EP_TSCR_EN_RXTS;
  EP->FCR = 0;
}
  

int ep_enable(int enabled, int autoneg)
{
  uint16_t mcr;

  if(!enabled)
    {
      EP->ECR = 0;
      return;
    }

  EP->ECR =  EP_ECR_TX_EN_FRA | EP_ECR_RX_EN_FRA | EP_ECR_RST_CNT;
  
  autoneg_enabled = autoneg;
 #if 1
  pcs_write(MDIO_REG_MCR, MDIO_MCR_PDOWN); /* reset the PHY */
  timer_delay(2000);
  pcs_write(MDIO_REG_MCR, 0);  /* reset the PHY */
 // pcs_write(MDIO_REG_MCR, MDIO_MCR_RESET);  /* reset the PHY */
   #endif
   
  pcs_write(MDIO_REG_ADVERTISE, 0);

  mcr = MDIO_MCR_SPEED1000_MASK | MDIO_MCR_FULLDPLX_MASK;
  if(autoneg)
    mcr |= MDIO_MCR_ANENABLE | MDIO_MCR_ANRESTART;


  pcs_write(MDIO_REG_MCR, mcr);
}

int ep_link_up()
{
  uint16_t flags = MDIO_MSR_LSTATUS;
	volatile  uint16_t msr;

  if(autoneg_enabled)
    flags |= MDIO_MSR_ANEGCOMPLETE;


	msr = pcs_read(MDIO_REG_MSR);
	msr = pcs_read(MDIO_REG_MSR);

  return (msr & flags) == flags ? 1 : 0;
}


int ep_get_deltas(uint32_t *delta_tx, uint32_t *delta_rx)
{
//	mprintf("called ep_get_deltas()\n");
	*delta_tx = 0; 
	*delta_rx = 15000 - 7000 + 195000 + 32000 + UIS_PER_SERIAL_BIT * MDIO_WR_SPEC_BSLIDE_R(pcs_read(MDIO_REG_WR_SPEC)) + 2800;
}

void ep_show_counters()
{
  int i;
  for(i=0;i<16;i++)
    TRACE_DEV("cntr%d = %d\n", i, EP->RMON_RAM[i]);
}

int ep_get_psval(int32_t *psval)
{
  uint32_t val;
  val = EP->DMSR;
  
  if(val & EP_DMSR_PS_RDY)
    *psval =  EP_DMSR_PS_VAL_R(val);
  else
    *psval = 0;

  return val & EP_DMSR_PS_RDY;
}

int ep_cal_pattern_enable()
{
  uint32_t val;
  val = pcs_read(MDIO_REG_WR_SPEC);   
  val |= MDIO_WR_SPEC_TX_CAL;
  pcs_write(MDIO_REG_WR_SPEC, val);

  return 0;
}

int ep_cal_pattern_disable()
{
  uint32_t val;
  val = pcs_read(MDIO_REG_WR_SPEC);
  val &= (~MDIO_WR_SPEC_TX_CAL);
  pcs_write(MDIO_REG_WR_SPEC, val);
}
