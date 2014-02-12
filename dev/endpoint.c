/*

WR Endpoint (WR-compatible Ethernet MAC driver

Tomasz Wlostowski/CERN 2011

LGPL 2.1

*/

#include <stdio.h>
#include <wrc.h>

#include "board.h"
#include "syscon.h"
#include <endpoint.h>
#include "eeprom.h"

#include <hw/endpoint_regs.h>
#include <hw/endpoint_mdio.h>

/* Length of a single bit on the gigabit serial link in picoseconds. Used for calculating deltaRx/deltaTx
   from the serdes bitslip value */
#define PICOS_PER_SERIAL_BIT 800

/* Number of raw phase samples averaged by the DMTD detector in the Endpoint during single phase measurement.
   The bigger, the better precision, but slower rate */
#define DMTD_AVG_SAMPLES 256

static int autoneg_enabled;
volatile struct EP_WB *EP;

/* functions for accessing PCS (MDIO) registers */
static uint16_t pcs_read(int location)
{
	EP->MDIO_CR = EP_MDIO_CR_ADDR_W(location >> 2);
	while ((EP->MDIO_ASR & EP_MDIO_ASR_READY) == 0) ;
	return EP_MDIO_ASR_RDATA_R(EP->MDIO_ASR) & 0xffff;
}

static void pcs_write(int location, int value)
{
	EP->MDIO_CR = EP_MDIO_CR_ADDR_W(location >> 2)
	    | EP_MDIO_CR_DATA_W(value)
	    | EP_MDIO_CR_RW;

	while ((EP->MDIO_ASR & EP_MDIO_ASR_READY) == 0) ;
}

/* MAC address setting */
void set_mac_addr(uint8_t dev_addr[])
{
	EP->MACL = ((uint32_t) dev_addr[2] << 24)
	    | ((uint32_t) dev_addr[3] << 16)
	    | ((uint32_t) dev_addr[4] << 8)
	    | ((uint32_t) dev_addr[5]);

	EP->MACH = ((uint32_t) dev_addr[0] << 8)
	    | ((uint32_t) dev_addr[1]);
}

void get_mac_addr(uint8_t dev_addr[])
{
	dev_addr[5] = (EP->MACL & 0x000000ff);
	dev_addr[4] = (EP->MACL & 0x0000ff00) >> 8;
	dev_addr[3] = (EP->MACL & 0x00ff0000) >> 16;
	dev_addr[2] = (EP->MACL & 0xff000000) >> 24;
	dev_addr[1] = (EP->MACH & 0x000000ff);
	dev_addr[0] = (EP->MACH & 0x0000ff00) >> 8;
}

/* Initializes the endpoint and sets its local MAC address */
void ep_init(uint8_t mac_addr[])
{
	EP = (volatile struct EP_WB *)BASE_EP;
	set_mac_addr(mac_addr);

	*(unsigned int *)(0x62000) = 0x2;	// reset network stuff (cleanup required!)
	*(unsigned int *)(0x62000) = 0;

	EP->ECR = 0;		/* disable Endpoint */
	EP->VCR0 = EP_VCR0_QMODE_W(3);	/* disable VLAN unit - not used by WRPC */
	EP->RFCR = EP_RFCR_MRU_W(1518);	/* Set the max RX packet size */
	EP->TSCR = EP_TSCR_EN_TXTS | EP_TSCR_EN_RXTS;	/* Enable timestamping */

/* Configure DMTD phase tracking */
	EP->DMCR = EP_DMCR_EN | EP_DMCR_N_AVG_W(DMTD_AVG_SAMPLES);
}

/* Enables/disables transmission and reception. When autoneg is set to 1,
   starts up 802.3 autonegotiation process */
int ep_enable(int enabled, int autoneg)
{
	uint16_t mcr;

	if (!enabled) {
		EP->ECR = 0;
		return 0;
	}

/* Disable the endpoint */
	EP->ECR = 0;

	mprintf("ID: %x\n", EP->IDCODE);

/* Load default packet classifier rules - see ep_pfilter.c for details */
	pfilter_init_default();

/* Enable TX/RX paths, reset RMON counters */
	EP->ECR = EP_ECR_TX_EN | EP_ECR_RX_EN | EP_ECR_RST_CNT;

	autoneg_enabled = autoneg;

/* Reset the GTP Transceiver - it's important to do the GTP phase alignment every time
   we start up the software, otherwise the calibration RX/TX deltas may not be correct */
	pcs_write(MDIO_REG_MCR, MDIO_MCR_PDOWN);	/* reset the PHY */
	timer_delay_ms(200);
	pcs_write(MDIO_REG_MCR, MDIO_MCR_RESET);	/* reset the PHY */
	pcs_write(MDIO_REG_MCR, 0);	/* reset the PHY */

/* Don't advertise anything - we don't want flow control */
	pcs_write(MDIO_REG_ADVERTISE, 0);

	mcr = MDIO_MCR_SPEED1000_MASK | MDIO_MCR_FULLDPLX_MASK;
	if (autoneg)
		mcr |= MDIO_MCR_ANENABLE | MDIO_MCR_ANRESTART;

	pcs_write(MDIO_REG_MCR, mcr);
	return 0;
}

/* Checks the link status. If the link is up, returns non-zero
   and stores the Link Partner Ability (LPA) autonegotiation register at *lpa */
int ep_link_up(uint16_t * lpa)
{
	uint16_t flags = MDIO_MSR_LSTATUS;
	volatile uint16_t msr;

	if (autoneg_enabled)
		flags |= MDIO_MSR_ANEGCOMPLETE;

	msr = pcs_read(MDIO_REG_MSR);
	msr = pcs_read(MDIO_REG_MSR);	/* Read this flag twice to make sure the status is updated */

	if (lpa)
		*lpa = pcs_read(MDIO_REG_LPA);

	return (msr & flags) == flags ? 1 : 0;
}

int ep_get_bitslide()
{
	return PICOS_PER_SERIAL_BIT *
	    MDIO_WR_SPEC_BSLIDE_R(pcs_read(MDIO_REG_WR_SPEC));
}

/* Returns the TX/RX latencies. They are valid only when the link is up. */
int ep_get_deltas(uint32_t * delta_tx, uint32_t * delta_rx)
{
	/* fixme: these values should be stored in calibration block in the EEPROM on the FMC. Also, the TX/RX delays of a particular SFP
	   should be added here */
	*delta_tx = sfp_deltaTx;
	*delta_rx =
	    sfp_deltaRx +
	    PICOS_PER_SERIAL_BIT *
	    MDIO_WR_SPEC_BSLIDE_R(pcs_read(MDIO_REG_WR_SPEC));
	return 0;
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

	return 0;
}

int ep_timestamper_cal_pulse()
{
	EP->TSCR |= EP_TSCR_RX_CAL_START;
	timer_delay_ms(1);
	return EP->TSCR & EP_TSCR_RX_CAL_RESULT ? 1 : 0;
}
