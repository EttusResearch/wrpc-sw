#include <stdint.h>
#include <stddef.h>

#include <hw/pps_gen_regs.h>
#include <hw/endpoint_regs.h>
#include <hw/softpll_regs.h>
#include <hw/endpoint_mdio.h>

#include <hw/wb_uart.h>

#define BASE_ENDPOINT 0x20100
#define BASE_PPS_GEN 0x20300
#define BASE_UART 0x20500
#define BASE_SOFTPLL 0x20200


#define ppsg_write(reg, val) \
	*(volatile uint32_t *) (BASE_PPS_GEN + (offsetof(struct PPSG_WB, reg))) = (val)

#define ppsg_read(reg) \
	*(volatile uint32_t *) (BASE_PPS_GEN + (offsetof(struct PPSG_WB, reg)))

#define CALC_BAUD(baudrate) \
    ( ((( (unsigned long long)baudrate * 8ULL) << (16 - 7)) + \
      (CPU_CLOCK >> 8)) / (CPU_CLOCK >> 7) )


#define CPU_CLOCK 62500000
#define UART_BAUDRATE 6000000

void uart_init()
{
	volatile struct UART_WB *uart;
	uart = (volatile struct UART_WB *)BASE_UART;
	uart->BCR = CALC_BAUD(UART_BAUDRATE);
}

void uart_write_byte(int b)
{
	volatile struct UART_WB *uart;
	uart = (volatile struct UART_WB *)BASE_UART;
	while (uart->SR & UART_SR_TX_BUSY)
		;
	uart->TDR = b;
}

void uart_write_string(char *s)
{
	char c;
	while(c=*s++)
		uart_write_byte(c);
}

volatile struct EP_WB *EP = (volatile struct EP_WB *)BASE_ENDPOINT;
volatile struct SPLL_WB *SPLL = (volatile struct EP_WB *)BASE_SOFTPLL;

static void pcs_write(int location, int value)
{
  EP->MDIO_CR = EP_MDIO_CR_ADDR_W(location >> 2)
      | EP_MDIO_CR_DATA_W(value)
      | EP_MDIO_CR_RW;

  while ((EP->MDIO_ASR & EP_MDIO_ASR_READY) == 0) ;
}

void endpoint_init()
{

  EP->PFCR0 = 0;    // disable pfilter
	#include "pf-microcode.h"  
  EP->PFCR0 = EP_PFCR0_ENABLE;


  EP->ECR = 0;    /* disable Endpoint */
  EP->VCR0 = EP_VCR0_QMODE_W(3);  /* disable VLAN unit - not used by WRPC */
  EP->RFCR = EP_RFCR_MRU_W(1518); /* Set the max RX packet size */
  EP->TSCR = EP_TSCR_EN_TXTS | EP_TSCR_EN_RXTS; /* Enable timestamping */
  EP->MACL = 0x33445566;
  EP->MACH = 0x1122;
	EP->ECR = EP_ECR_TX_EN | EP_ECR_RX_EN | EP_ECR_RST_CNT;
	pcs_write(MDIO_REG_MCR, MDIO_MCR_PDOWN);  /* reset the PHY */
  pcs_write(MDIO_REG_MCR, 0); /* reset th */
}


main()
{

	uint32_t cr;

	cr = PPSG_CR_CNT_EN | PPSG_CR_PWIDTH_W(1);
	ppsg_write(CR, cr);
	ppsg_write(ADJ_UTCLO, 0);
	ppsg_write(ADJ_UTCHI, 0);
	ppsg_write(ADJ_NSEC, 0);
	ppsg_write(CR, cr | PPSG_CR_CNT_SET);
	ppsg_write(CR, cr);
	ppsg_write(ESCR, PPSG_ESCR_TM_VALID);

	endpoint_init();

	uart_init();
	int i=0;
	for(i=0;i<50;i++)
		uart_write_string("Hello!\n");
	for(;;)
	{
		SPLL->DAC_MAIN = (1<<16) | (i++);
		uart_write_string(" ");
	
	}


	for(;;);
}
