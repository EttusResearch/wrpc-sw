#include <stdio.h>

#include "types.h"
#include "board.h"
#include "pps_gen.h" /* for pps_gen_get_time() */
#include "minic.h"

#include <hw/minic_regs.h>

#define MINIC_DMA_TX_BUF_SIZE 1024
#define MINIC_DMA_RX_BUF_SIZE 2048

#define MINIC_MTU 1540

#define F_COUNTER_BITS 4
#define F_COUNTER_MASK ((1<<F_COUNTER_BITS)-1)

#define RX_DESC_VALID(d) ((d) & (1<<31) ? 1 : 0)
#define RX_DESC_ERROR(d) ((d) & (1<<30) ? 1 : 0)
#define RX_DESC_HAS_OOB(d)  ((d) & (1<<29) ? 1 : 0)
#define RX_DESC_SIZE(d)  (((d) & (1<<0) ? -1 : 0) + (d & 0xfffe))

#define TX_DESC_VALID (1<<31)
#define TX_DESC_WITH_OOB (1<<30)
#define TX_DESC_HAS_OWN_MAC (1<<28)

#define RX_OOB_SIZE 6
#define REFCLK_FREQ 125000000


#define ETH_HEADER_SIZE 14

// extracts the values of TS rising and falling edge counters from the descriptor header

#define EXPLODE_WR_TIMESTAMP(raw, rc, fc) \
  rc = (raw) & 0xfffffff;		  \
  fc = (raw >> 28) & 0xf;


static volatile uint32_t dma_tx_buf[MINIC_DMA_RX_BUF_SIZE / 4];
static volatile uint32_t dma_rx_buf[MINIC_DMA_TX_BUF_SIZE / 4];

struct wr_minic {
  volatile uint32_t *rx_head, *rx_base; 
  uint32_t rx_avail, rx_size;
  volatile uint32_t *tx_head, *tx_base;
  uint32_t tx_avail, tx_size;

  int synced;
  int syncing_counters;
  int iface_up;

  uint32_t cur_rx_desc;
};

static struct wr_minic minic;

static inline void minic_writel(uint32_t reg,uint32_t data)
{
  *(volatile uint32_t *) (BASE_MINIC + reg) = data;
}

static inline uint32_t minic_readl(uint32_t reg)
{
  return *(volatile uint32_t *)(BASE_MINIC + reg);
}

static void minic_new_rx_buffer()
{
  minic.rx_head = minic.rx_base;

  minic_writel(MINIC_REG_MCR, 0);
  minic_writel(MINIC_REG_RX_ADDR, (uint32_t) minic.rx_base);
  minic_writel(MINIC_REG_RX_AVAIL, (minic.rx_size - MINIC_MTU) >> 2);
  minic_writel(MINIC_REG_MCR, MINIC_MCR_RX_EN);

}

static void minic_new_tx_buffer()
{
  minic.tx_head = minic.tx_base;
  minic.tx_avail = (minic.tx_size - MINIC_MTU) >> 2;
	
  minic_writel(MINIC_REG_TX_ADDR, (uint32_t) minic.tx_base);
}



void minic_init()
{
  minic.rx_base = dma_rx_buf;
  minic.rx_size = sizeof(dma_rx_buf);

  minic.tx_base = dma_tx_buf;
  minic.tx_size = sizeof(dma_tx_buf);


  minic_new_rx_buffer();
  minic_writel(MINIC_REG_EIC_IER, MINIC_EIC_IER_RX);
}

int minic_poll_rx()
{
  uint32_t isr;

  isr = minic_readl(MINIC_REG_EIC_ISR);

  return isr;
}

int minic_rx_frame(uint8_t *hdr, uint8_t *payload, uint32_t buf_size, struct hw_timestamp *hwts)
{
  uint32_t payload_size, num_words;
  uint32_t desc_hdr;
  uint32_t raw_ts;
  uint32_t rx_addr_cur;
  int i;
  int n_recvd;

  if(! (minic_readl(MINIC_REG_EIC_ISR) & MINIC_EIC_ISR_RX))
    return 0;

  desc_hdr = *minic.rx_head;

  if(!RX_DESC_VALID(desc_hdr)) /* invalid descriptor? Weird, the RX_ADDR seems to be saying something different. Ignore the packet and purge the RX buffer. */
    {
      //      mprintf("weird, invalid RX descriptor (%x, head %x)\n", desc_hdr, rx_head);
      minic_new_rx_buffer();
      return 0;
    }
  
  payload_size = RX_DESC_SIZE(desc_hdr);
  num_words = ((payload_size + 3) >> 2) + 1;

  /* valid packet */	
  if(!RX_DESC_ERROR(desc_hdr))
    {   
      
      if(RX_DESC_HAS_OOB(desc_hdr) && hwts != NULL)
	{
	  uint32_t counter_r, counter_f, counter_ppsg, utc;
	  int cntr_diff;

	  payload_size -= RX_OOB_SIZE;

	  memcpy(&raw_ts, (uint8_t *)minic.rx_head + payload_size + 6, 4); /* fixme: ugly way of doing unaligned read */

	  EXPLODE_WR_TIMESTAMP(raw_ts, counter_r, counter_f);

	  pps_gen_get_time(&utc, &counter_ppsg);

	  if(counter_r > (3*REFCLK_FREQ/4) && counter_ppsg < REFCLK_FREQ/4)
	    utc--; 

	  hwts->utc = utc & 0x7fffffff ;

	  cntr_diff = (counter_r & F_COUNTER_MASK) - counter_f;

	  if(cntr_diff == 1 || cntr_diff == (-F_COUNTER_MASK))
	    hwts->ahead = 1;
	  else
	    hwts->ahead = 0;
  
	  hwts->nsec = counter_r * 8;
	}

      n_recvd = (buf_size < payload_size ? buf_size : payload_size);
      
      /* FIXME: VLAN support */

      memcpy(hdr, (void*)minic.rx_head + 4, ETH_HEADER_SIZE);
      memcpy(payload, (void*)minic.rx_head + 4 + ETH_HEADER_SIZE, n_recvd - ETH_HEADER_SIZE);

      /*      for(i=0;i<n_recvd;i++)
	mprintf("%x ", buf[i]);
	mprintf("---\n");*/

      minic.rx_head += num_words;
    } else    { // RX_DESC_ERROR
    minic.rx_head += num_words;  
  }

  rx_addr_cur = minic_readl(MINIC_REG_RX_ADDR) & 0xffff;

  if(rx_addr_cur < (uint32_t)minic.rx_head)  /* nothing new in the buffer? */
    {

      if(minic_readl(MINIC_REG_MCR) & MINIC_MCR_RX_FULL)
	minic_new_rx_buffer();

      minic_writel(MINIC_REG_EIC_ISR, MINIC_EIC_ISR_RX);
    }

  return n_recvd;
}


static uint16_t tx_oob_val = 0;

int minic_tx_frame(uint8_t *hdr, uint8_t *payload, uint32_t size, struct hw_timestamp *hwts)
{
  uint32_t d_hdr, mcr, nwords;
  minic_new_tx_buffer();
  nwords = ((size + 1) >> 1) - 1;

  memset(minic.tx_head, 0x0, size + 16);
  memcpy((void*)minic.tx_head + 4, hdr, ETH_HEADER_SIZE);
  memcpy((void*)minic.tx_head + 4 + ETH_HEADER_SIZE, payload, size - ETH_HEADER_SIZE);

  if(hwts)
    {

      memcpy((void *) minic.tx_head + 4 + size, &tx_oob_val, sizeof(uint16_t));
      nwords++;
      d_hdr = TX_DESC_WITH_OOB;
    } else
    d_hdr = 0;

  d_hdr |= TX_DESC_VALID | nwords;
  
  *(volatile uint32_t *)(minic.tx_head) = d_hdr;
  *(volatile uint32_t *)(minic.tx_head + nwords) = 0;

  mcr = minic_readl(MINIC_REG_MCR);
  minic_writel(MINIC_REG_MCR, mcr | MINIC_MCR_TX_START);


  if(hwts) /* wait for the timestamp */
    {
      uint32_t raw_ts;
      uint16_t fid;
      uint32_t counter_r, counter_f;
      uint32_t utc;
      uint32_t nsec;


      while(minic_readl(MINIC_REG_TSFIFO_CSR) & MINIC_TSFIFO_CSR_EMPTY);


      raw_ts = minic_readl(MINIC_REG_TSFIFO_R0);
      fid = (minic_readl(MINIC_REG_TSFIFO_R1) >> 5) & 0xffff;

      EXPLODE_WR_TIMESTAMP(raw_ts, counter_r, counter_f);
      pps_gen_get_time(&utc, &nsec);

      if(counter_r > (3*REFCLK_FREQ/4) && nsec < REFCLK_FREQ/4)
	utc--;

      hwts->utc = utc;
      hwts->ahead = 0;
      hwts->nsec = counter_r * 8;
    }

  tx_oob_val++;

  return size;
}
