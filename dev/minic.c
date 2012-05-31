#include <stdio.h>

#include "types.h"
#include "board.h"
#include "pps_gen.h" /* for pps_gen_get_time() */
#include "minic.h"

#include <hw/minic_regs.h>

#define MINIC_DMA_TX_BUF_SIZE 1024
#define MINIC_DMA_RX_BUF_SIZE 2048

#define MINIC_MTU 256

#define F_COUNTER_BITS 4
#define F_COUNTER_MASK ((1<<F_COUNTER_BITS)-1)

#define RX_DESC_VALID(d) ((d) & (1<<31) ? 1 : 0)
#define RX_DESC_ERROR(d) ((d) & (1<<30) ? 1 : 0)
#define RX_DESC_HAS_OOB(d)  ((d) & (1<<29) ? 1 : 0)
#define RX_DESC_SIZE(d)  (((d) & (1<<0) ? -1 : 0) + (d & 0xffe))

#define RXOOB_TS_INCORRECT (1<<11)

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


static volatile uint32_t dma_tx_buf[MINIC_DMA_TX_BUF_SIZE / 4];
static volatile uint32_t dma_rx_buf[MINIC_DMA_RX_BUF_SIZE / 4];

struct wr_minic {
  volatile uint32_t *rx_head, *rx_base; 
  uint32_t rx_avail, rx_size;
  volatile uint32_t *tx_head, *tx_base;
  uint32_t tx_avail, tx_size;

  int synced;
  int syncing_counters;
  int iface_up;
  
  int tx_count, rx_count;

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

  mprintf("minic: new rx buffer\n");
  minic_writel(MINIC_REG_MCR, 0);
  minic_writel(MINIC_REG_RX_ADDR, (uint32_t) minic.rx_base);
  minic_writel(MINIC_REG_RX_SIZE, minic.rx_size>>2); //(minic.rx_size - MINIC_MTU) >> 2);
 // TRACE_DEV("Sizeof: %d Size : %d Avail: %d\n", minic.rx_size, (minic.rx_size - MINIC_MTU) >> 2);
  //new buffer allocated, clear any old RX interrupts
  minic_writel(MINIC_REG_EIC_ISR, MINIC_EIC_ISR_RX);
  minic_writel(MINIC_REG_MCR, MINIC_MCR_RX_EN);
  
  //mprintf("Base : %x Avail: %d\n", minic_readl(MINIC_REG_RX_ADDR), minic_readl(MINIC_REG_RX_AVAIL));

}

static void minic_rxbuf_free(uint32_t words)
{
  minic_writel(MINIC_REG_RX_AVAIL, words);
}

static void minic_new_tx_buffer()
{
  minic.tx_head = minic.tx_base;
  minic.tx_avail = minic.tx_size>>2; //(minic.tx_size - MINIC_MTU) >> 2;
	
  minic_writel(MINIC_REG_TX_ADDR, (uint32_t) minic.tx_base);
}

static uint8_t * minic_rx_memcpy( uint8_t *dst, uint8_t *src, uint32_t size)
{
  uint32_t part;
  //if src is outside the circular buffer, bring it back to the beginning
  src = (uint32_t)minic.rx_base + ((uint32_t)src - (uint32_t)minic.rx_base) % minic.rx_size;

  if((uint32_t)src + size <= (uint32_t)minic.rx_base + minic.rx_size)
    return memcpy(dst, src, size);

  part = (uint32_t)minic.rx_base + minic.rx_size - (uint32_t)src;
  memcpy(dst, src, part);
  memcpy(dst+part, minic.rx_base, size - part);
  return dst;
}



void minic_init()
{
	uint32_t lo , hi;
	
    minic_writel(MINIC_REG_EIC_IDR, MINIC_EIC_IDR_RX);
    minic_writel(MINIC_REG_EIC_ISR, MINIC_EIC_ISR_RX);
	minic.rx_base = dma_rx_buf;
	minic.rx_size = sizeof(dma_rx_buf);

/* FIXME: now we have a temporary HW protection against accidentally overwriting the memory - there's some
   very well hidden bug in Minic's RX logic which sometimes causes an overwrite of the memory outside
   the buffer. */
   
	lo = (uint32_t)minic.rx_base >> 2;
	hi = ((uint32_t)minic.rx_base >> 2) + (sizeof(dma_rx_buf)>>2) - 1;

	minic_writel(MINIC_REG_MPROT, MINIC_MPROT_LO_W(lo) | MINIC_MPROT_HI_W(hi));

	minic.tx_base = dma_tx_buf;
	minic.tx_size = sizeof(dma_tx_buf);

	minic.tx_count = 0;
	minic.rx_count = 0;

	minic_new_rx_buffer();
	minic_writel(MINIC_REG_EIC_IER, MINIC_EIC_IER_RX);
}

void minic_disable()
{
  minic_writel(MINIC_REG_MCR, 0);
}

int minic_poll_rx()
{
  uint32_t isr;

  isr = minic_readl(MINIC_REG_EIC_ISR);

  return (isr & MINIC_EIC_ISR_RX) ? 1 : 0;//>>1;
}

int minic_rx_frame(uint8_t *hdr, uint8_t *payload, uint32_t buf_size, struct hw_timestamp *hwts)
{
  uint32_t payload_size, num_words;
  uint32_t desc_hdr;
  uint32_t raw_ts;
  uint32_t rx_addr_cur, cur_avail;
  int n_recvd;
  //	mprintf("erxf\n");

  if(! (minic_readl(MINIC_REG_EIC_ISR) & MINIC_EIC_ISR_RX))
    return 0;

  //TRACE_DEV("minic: got sthx \n");

  desc_hdr = *minic.rx_head;

  //mprintf("%s: rx_base = %x\n", __FUNCTION__, (uint32_t)minic.rx_base>>2);
  //mprintf("%s: rx_head= %x\n", __FUNCTION__, (uint32_t)minic.rx_head>>2);
  //mprintf("%s: rx_size=%x\n", __FUNCTION__, (uint32_t)minic.rx_size>>2);
  //mprintf("%s: rx_end = %x\n", __FUNCTION__, (((uint32_t)minic.rx_base)>>2) + (minic.rx_size>>2));

  if(!RX_DESC_VALID(desc_hdr)) /* invalid descriptor? Weird, the RX_ADDR seems to be saying something different. Ignore the packet and purge the RX buffer. */
  {
    mprintf("invalid descriptor\n");
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
      uint16_t dhdr;

      payload_size -= RX_OOB_SIZE;

      minic_rx_memcpy(&raw_ts, (uint8_t *)minic.rx_head + payload_size + 6, 4); /* fixme: ugly way of doing unaligned read */
      minic_rx_memcpy(&dhdr, (uint8_t *)minic.rx_head + payload_size + 4, 2);
      EXPLODE_WR_TIMESTAMP(raw_ts, counter_r, counter_f);

      pps_gen_get_time(&utc, &counter_ppsg);

      if(counter_r > 3*125000000/4 && counter_ppsg < 125000000/4)
        utc--; 

      hwts->utc = utc & 0x7fffffff ;

      cntr_diff = (counter_r & F_COUNTER_MASK) - counter_f;

      if(cntr_diff == 1 || cntr_diff == (-F_COUNTER_MASK))
        hwts->ahead = 1;
      else
        hwts->ahead = 0;

      hwts->nsec = counter_r * 8;
      hwts->valid = (dhdr & RXOOB_TS_INCORRECT) ? 0 : 1;
    }

    n_recvd = (buf_size < payload_size ? buf_size : payload_size);
    minic.rx_count++;

    minic_rx_memcpy(hdr, (void*)minic.rx_head + 4, ETH_HEADER_SIZE);
    minic_rx_memcpy(payload, (void*)minic.rx_head + 4 + ETH_HEADER_SIZE, n_recvd - ETH_HEADER_SIZE);


    //mprintf(" packet OK\n");
  } else { 
    //mprintf(" error in packet\n");
    n_recvd = -1;
  }
  minic_rxbuf_free(num_words);
  minic.rx_head = (uint32_t)minic.rx_base + ((uint32_t)minic.rx_head+(num_words<<2) - (uint32_t)minic.rx_base) % minic.rx_size;

  //rx_addr_cur = minic_readl(MINIC_REG_RX_ADDR) & 0xffff;
  cur_avail = minic_readl(MINIC_REG_RX_AVAIL);

  //mprintf("minic: cur_avail=%d, minic.rx_size=%d\n", cur_avail, minic.rx_size>>2);
  if( cur_avail == (minic.rx_size>>2) )  /*empty buffer->no more received packets*/
  {
    //mprintf("minic: nothing new in the buffer\n");
    if(minic_readl(MINIC_REG_MCR) & MINIC_MCR_RX_FULL)
      minic_new_rx_buffer();

    minic_writel(MINIC_REG_EIC_ISR, MINIC_EIC_ISR_RX);
  }

  //	TRACE_DEV("minic: num_rx %d\n", n_recvd);
  return n_recvd;
}


static uint16_t tx_oob_val = 0;

int minic_tx_frame(uint8_t *hdr, uint8_t *payload, uint32_t size, struct hw_timestamp *hwts)
{
  uint32_t d_hdr, mcr, nwords;
  uint8_t ts_valid;
  minic_new_tx_buffer();


//  TRACE_DEV("minic_tx_frame: head %x size %d\n", minic.tx_head, size);
  memset(minic.tx_head, 0x0, size + 16);
  memset((void*)minic.tx_head + 4, 0, size < 60 ? 60 : size);
  memcpy((void*)minic.tx_head + 4, hdr, ETH_HEADER_SIZE);
  memcpy((void*)minic.tx_head + 4 + ETH_HEADER_SIZE, payload, size - ETH_HEADER_SIZE);

  if(size < 60)
	size = 60;

  nwords = ((size + 1) >> 1);

	d_hdr = 0;

  if(hwts)
    d_hdr = TX_DESC_WITH_OOB | (tx_oob_val << 12);

  d_hdr |= TX_DESC_VALID | nwords;
  
  *(volatile uint32_t *)(minic.tx_head) = d_hdr;
  *(volatile uint32_t *)(minic.tx_head + nwords) = 0;

  mcr = minic_readl(MINIC_REG_MCR);
  minic_writel(MINIC_REG_MCR, mcr | MINIC_MCR_TX_START);

  while((minic_readl(MINIC_REG_MCR) & MINIC_MCR_TX_IDLE) == 0);

 #if 1
  if(hwts) /* wait for the timestamp */
    {
      uint32_t raw_ts;
      uint16_t fid;
      uint32_t counter_r, counter_f;
      uint32_t utc;
      uint32_t nsec;

	  //while((minic_readl(MINIC_REG_TSR0) & MINIC_TSR0_VALID) == 0);
      ts_valid = (uint8_t) (minic_readl(MINIC_REG_TSR0) & MINIC_TSR0_VALID);

      raw_ts = minic_readl(MINIC_REG_TSR1);
      fid = MINIC_TSR0_FID_R(minic_readl(MINIC_REG_TSR0));

		if(fid != tx_oob_val)
		{
		 	TRACE_DEV("minic_tx_frame: unmatched fid %d vs %d\n", fid, tx_oob_val);
		}
      EXPLODE_WR_TIMESTAMP(raw_ts, counter_r, counter_f);
      pps_gen_get_time(&utc, &nsec);

      if(counter_r > 3*125000000/4 && nsec < 125000000/4)
		utc--;

      hwts->valid = ts_valid;
      hwts->utc = utc;
      hwts->ahead = 0;
      hwts->nsec = counter_r * 8;

//	  TRACE_DEV("minic_tx_frame [%d bytes] TS: %d.%d valid %d\n", size, hwts->utc, hwts->nsec, hwts->valid);
	  minic.tx_count++;

    }
#endif
  tx_oob_val++;

  return size;
}

void minic_get_stats(int *tx_frames, int *rx_frames)
{
 	*tx_frames = minic.tx_count;
 	*rx_frames = minic.rx_count;
}
