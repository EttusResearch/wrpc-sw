/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2011d CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 * Author: Alessandro Rubini <rubini@gnudd.com>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

/*
 * Trivial pll programmer using an spi controller.
 * PLL is AD9516, SPI is opencores
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <wrc.h>

#include "board.h"
#include "syscon.h"
#include "gpio-wrs.h"

#include "rt_ipc.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a)/sizeof(a[0]))
#endif

static inline void writel(uint32_t data, void *where)
{
	* (volatile uint32_t *)where = data;
}

static inline uint32_t readl(void *where)
{
	return * (volatile uint32_t *)where;
}

struct ad9516_reg {
	uint16_t reg;
	uint8_t val;
};

#include "ad9516_config.h"

/*
 * SPI stuff, used by later code
 */

#define SPI_REG_RX0	0
#define SPI_REG_TX0	0
#define SPI_REG_RX1	4
#define SPI_REG_TX1	4
#define SPI_REG_RX2	8
#define SPI_REG_TX2	8
#define SPI_REG_RX3	12
#define SPI_REG_TX3	12

#define SPI_REG_CTRL	16
#define SPI_REG_DIVIDER	20
#define SPI_REG_SS	24

#define SPI_CTRL_ASS		(1<<13)
#define SPI_CTRL_IE		(1<<12)
#define SPI_CTRL_LSB		(1<<11)
#define SPI_CTRL_TXNEG		(1<<10)
#define SPI_CTRL_RXNEG		(1<<9)
#define SPI_CTRL_GO_BSY		(1<<8)
#define SPI_CTRL_CHAR_LEN(x)	((x) & 0x7f)

#define GPIO_PLL_RESET_N 1
#define GPIO_SYS_CLK_SEL 0
#define GPIO_PERIPH_RESET_N 3

#define CS_PLL	0 /* AD9516 on SPI CS0 */

static void *oc_spi_base;

static int oc_spi_init(void *base_addr)
{
	oc_spi_base = base_addr;

	writel(100, oc_spi_base + SPI_REG_DIVIDER);
	return 0;
}

static int oc_spi_txrx(int ss, int nbits, uint32_t in, uint32_t *out)
{
	uint32_t rval;

	if (!out)
		out = &rval;

	writel(SPI_CTRL_ASS | SPI_CTRL_CHAR_LEN(nbits)
		     | SPI_CTRL_TXNEG,
		     oc_spi_base + SPI_REG_CTRL);

	writel(in, oc_spi_base + SPI_REG_TX0);
	writel((1 << ss), oc_spi_base + SPI_REG_SS);
	writel(SPI_CTRL_ASS | SPI_CTRL_CHAR_LEN(nbits)
		     | SPI_CTRL_TXNEG | SPI_CTRL_GO_BSY,
		     oc_spi_base + SPI_REG_CTRL);

	while(readl(oc_spi_base + SPI_REG_CTRL) & SPI_CTRL_GO_BSY)
		;
	*out = readl(oc_spi_base + SPI_REG_RX0);
	return 0;
}

/*
 * AD9516 stuff, using SPI, used by later code.
 * "reg" is 12 bits, "val" is 8 bits, but both are better used as int
 */

static void ad9516_write_reg(int reg, int val)
{
	oc_spi_txrx(CS_PLL, 24, (reg << 8) | val, NULL);
}

static int ad9516_read_reg(int reg)
{
	uint32_t rval;
	oc_spi_txrx(CS_PLL, 24, (reg << 8) | (1 << 23), &rval);
	return rval & 0xff;
}



static void ad9516_load_regset(const struct ad9516_reg *regs, int n_regs, int commit)
{
	int i;
	for(i=0; i<n_regs; i++)
		ad9516_write_reg(regs[i].reg, regs[i].val);
		
	if(commit)
		ad9516_write_reg(0x232, 1);
}


static void ad9516_wait_lock(void)
{
	while ((ad9516_read_reg(0x1f) & 1) == 0);
}

#define SECONDARY_DIVIDER 0x100

static int ad9516_set_output_divider(int output, int ratio, int phase_offset)
{
	uint8_t lcycles = (ratio/2) - 1;
	uint8_t hcycles = (ratio - (ratio / 2)) - 1;
	int secondary = (output & SECONDARY_DIVIDER) ? 1 : 0;
	output &= 0xf;

	if(output >= 0 && output < 6) /* LVPECL outputs */
	{
		uint16_t base = (output / 2) * 0x3 + 0x190;

		if(ratio == 1)  /* bypass the divider */
		{
			uint8_t div_ctl = ad9516_read_reg(base + 1);
			ad9516_write_reg(base + 1, div_ctl | (1<<7) | (phase_offset & 0xf)); 
		} else {
			uint8_t div_ctl = ad9516_read_reg(base + 1);
			ad9516_write_reg(base + 1, (div_ctl & (~(1<<7))) | (phase_offset & 0xf));  /* disable bypass bit */
			ad9516_write_reg(base, (lcycles << 4) | hcycles);
		}
	} else { /* LVDS/CMOS outputs */
			
		uint16_t base = ((output - 6) / 2) * 0x5 + 0x199;

		pp_printf("Output [divider %d]: %d ratio: %d base %x lc %d hc %d\n", secondary, output, ratio, base, lcycles ,hcycles);

		if(!secondary)
		{
			if(ratio == 1)  /* bypass the divider 1 */
				ad9516_write_reg(base + 3, ad9516_read_reg(base + 3) | 0x10); 
			else {
				ad9516_write_reg(base, (lcycles << 4) | hcycles); 
				ad9516_write_reg(base + 1, phase_offset & 0xf);
			}
		} else {
			if(ratio == 1)  /* bypass the divider 2 */
				ad9516_write_reg(base + 3, ad9516_read_reg(base + 3) | 0x20); 
			else {
				ad9516_write_reg(base + 2, (lcycles << 4) | hcycles); 
//				ad9516_write_reg(base + 1, phase_offset & 0xf);
				
		}
		}		
	}

	/* update */
	ad9516_write_reg(0x232, 0x0);
	ad9516_write_reg(0x232, 0x1);
	ad9516_write_reg(0x232, 0x0);
	return 0;
}

static int ad9516_set_vco_divider(int ratio) /* Sets the VCO divider (2..6) or 0 to enable static output */
{
	if(ratio == 0)
		ad9516_write_reg(0x1e0, 0x5); /* static mode */
	else
		ad9516_write_reg(0x1e0, (ratio-2));
	ad9516_write_reg(0x232, 0x1);
	return 0;
}

static void ad9516_sync_outputs(void)
{
	/* VCO divider: static mode */
	ad9516_write_reg(0x1E0, 0x7);
	ad9516_write_reg(0x232, 0x1);

	/* Sync the outputs when they're inactive to avoid +-1 cycle uncertainity */
	ad9516_write_reg(0x230, 1);
	ad9516_write_reg(0x232, 1);
	ad9516_write_reg(0x230, 0);
	ad9516_write_reg(0x232, 1);
}

int ad9516_init(int scb_version)
{
	pp_printf("Initializing AD9516 PLL...\n");

	oc_spi_init((void *)BASE_SPI);

	gpio_out(GPIO_SYS_CLK_SEL, 0); /* switch to the standby reference clock, since the PLL is off after reset */

	/* reset the PLL */
	gpio_out(GPIO_PLL_RESET_N, 0);
	timer_delay(10);
	gpio_out(GPIO_PLL_RESET_N, 1);
	timer_delay(10);
	
	/* Use unidirectional SPI mode */
	ad9516_write_reg(0x000, 0x99);

	/* Check the presence of the chip */
	if (ad9516_read_reg(0x3) != 0xc3) {
		pp_printf("Error: AD9516 PLL not responding.\n");
		return -1;
	}

	if( scb_version >= 34)	//New SCB v3.4. 10MHz Output.
		ad9516_load_regset(ad9516_base_config_34, ARRAY_SIZE(ad9516_base_config_34), 0);
	else 				//Old one
		ad9516_load_regset(ad9516_base_config_33, ARRAY_SIZE(ad9516_base_config_33), 0);

	ad9516_load_regset(ad9516_ref_tcxo, ARRAY_SIZE(ad9516_ref_tcxo), 1);
	ad9516_wait_lock();

	ad9516_sync_outputs();

	if( scb_version >= 34) {	//New SCB v3.4. 10MHz Output.

		ad9516_set_output_divider(2, 4, 0);  	// OUT2. 187.5 MHz. - not anymore
		ad9516_set_output_divider(3, 4, 0);  	// OUT3. 187.5 MHz. - not anymore

		ad9516_set_output_divider(4, 1, 0);  	// OUT4. 500 MHz.

		/*The following PLL outputs have been configured through the ad9516_base_config_34 register,
		 * so it doesn't need to replicate the configuration:
		 *
		 * Output 6 => 62.5 MHz
		 * Output 7	=> 62.5 MHz
		 * Output 8	=> 10 MHz
		 * Output 9	=> 10 MHz
		 */

	} else {	//Old one

		ad9516_set_output_divider(9, 4, 0);  /* AUX/SWCore = 187.5 MHz */ //not needed anymore
		ad9516_set_output_divider(7, 8, 0); /* REF = 62.5 MHz */
		ad9516_set_output_divider(4, 8, 0);  /* GTX = 62.5 MHz */
	}

	ad9516_sync_outputs();
	ad9516_set_vco_divider(3); 
	
	pp_printf("AD9516 locked.\n");

	gpio_out(GPIO_SYS_CLK_SEL, 1); /* switch the system clock to the PLL reference */
	gpio_out(GPIO_PERIPH_RESET_N, 0); /* reset all peripherals which use AD9516-provided clocks */
	gpio_out(GPIO_PERIPH_RESET_N, 1);

	return 0;
}


int rts_debug_command(int command, int value)
{
	switch(command)
	{
		case RTS_DEBUG_ENABLE_SERDES_CLOCKS:
			if(value)
			{
				ad9516_write_reg(0xf4, 0x08); // OUT4 enabled
				ad9516_write_reg(0x232, 0x0);
				ad9516_write_reg(0x232, 0x1);
			} else {
				ad9516_write_reg(0xf4, 0x0a); // OUT4 power-down, no serdes clock
				ad9516_write_reg(0x232, 0x0);
				ad9516_write_reg(0x232, 0x1);
			}
			break;
	}
	return 0;
}
