/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2011,2012,2016 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 * Author: Grzegorz Daniluk <grzegorz.daniluk@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <stdio.h>
#include <string.h>
#include <wrc.h>
#include <wrpc.h>

#include "types.h"
#include "board.h"
#include "pps_gen.h"		/* for pps_gen_get_time() */
#include "minic.h"
#include <syscon.h>

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

#define RX_OOB_SIZE 6

#define ETH_HEADER_SIZE 14

// extracts the values of TS rising and falling edge counters from the descriptor header

#define EXPLODE_WR_TIMESTAMP(raw, rc, fc) \
  rc = (raw) & 0xfffffff;		  \
  fc = (raw >> 28) & 0xf;

static volatile uint32_t dma_rx_buf[MINIC_DMA_RX_BUF_SIZE / 4];

struct wr_minic minic;

static inline void minic_writel(uint32_t reg, uint32_t data)
{
	*(volatile uint32_t *)(BASE_MINIC + reg) = data;
}

static inline uint32_t minic_readl(uint32_t reg)
{
	return *(volatile uint32_t *)(BASE_MINIC + reg);
}

static inline void minic_txword(uint8_t type, uint16_t word)
{
	minic_writel(MINIC_REG_TX_FIFO,
			MINIC_TX_FIFO_TYPE_W(type) | MINIC_TX_FIFO_DAT_W(word));
}

void minic_init()
{
	uint32_t mcr;

	/* disable interrupts, driver does polling */
	minic_writel(MINIC_REG_EIC_IDR, MINIC_EIC_IDR_TX |
			MINIC_EIC_IDR_RX | MINIC_EIC_IDR_TXTS);

	/* enable RX path */
	mcr = minic_readl(MINIC_REG_MCR);
	minic_writel(MINIC_REG_MCR, mcr | MINIC_MCR_RX_EN);
}

void minic_disable()
{
	minic_writel(MINIC_REG_MCR, 0);
}

int minic_poll_rx()
{
	uint32_t isr;

	isr = minic_readl(MINIC_REG_EIC_ISR);

	return (isr & MINIC_EIC_ISR_RX) ? 1 : 0;
}

int minic_rx_frame(struct wr_ethhdr *hdr, uint8_t * payload, uint32_t buf_size,
		   struct hw_timestamp *hwts)
{
	uint32_t frame_size, payload_size, num_words;
	uint32_t desc_hdr;
	uint32_t raw_ts;
	uint32_t cur_avail;
	int n_recvd;

	if (!(minic_readl(MINIC_REG_EIC_ISR) & MINIC_EIC_ISR_RX))
		return 0;

	if (!RX_DESC_VALID(desc_hdr)) {	/* invalid descriptor? Weird, the RX_ADDR seems to be saying something different. Ignore the packet and purge the RX buffer. */
		//invalid descriptor ? then probably the interrupt was generated by full rx buffer
		if (minic_readl(MINIC_REG_MCR) & MINIC_MCR_RX_FULL) {
			//minic_new_rx_buffer();
		}
		return 0;
	}

	num_words = ((frame_size + 3) >> 2) + 1;

	/* valid packet */
	if (!RX_DESC_ERROR(desc_hdr)) {

		//if (RX_DESC_HAS_OOB(desc_hdr) && hwts != NULL) {
		//	uint32_t counter_r, counter_f, counter_ppsg;
		//	uint64_t sec;
		//	int cntr_diff;
		//	uint16_t dhdr;

		//	frame_size -= RX_OOB_SIZE;

		//	/* fixme: ugly way of doing unaligned read */
		//	minic_rx_memcpy((uint8_t *) & raw_ts,
		//			(uint8_t *) minic.rx_head
		//			+ frame_size + 6, 4);
		//	minic_rx_memcpy((uint8_t *) & dhdr,
		//			(uint8_t *) minic.rx_head +
		//			frame_size + 4, 2);
		//	EXPLODE_WR_TIMESTAMP(raw_ts, counter_r, counter_f);

		//	shw_pps_gen_get_time(&sec, &counter_ppsg);

		//	if (counter_r > 3 * REF_CLOCK_FREQ_HZ / 4
		//	    && counter_ppsg < 250000000)
		//		sec--;

		//	hwts->sec = sec & 0x7fffffff;

		//	cntr_diff = (counter_r & F_COUNTER_MASK) - counter_f;

		//	if (cntr_diff == 1 || cntr_diff == (-F_COUNTER_MASK))
		//		hwts->ahead = 1;
		//	else
		//		hwts->ahead = 0;

		//	hwts->nsec = counter_r * (REF_CLOCK_PERIOD_PS / 1000);
		//	hwts->valid = (dhdr & RXOOB_TS_INCORRECT) ? 0 : 1;
		//}
		payload_size = frame_size - ETH_HEADER_SIZE;
		n_recvd = (buf_size < payload_size ? buf_size : payload_size);
		minic.rx_count++;

	} else {
		n_recvd = -1;
	}

	/*empty buffer->no more received packets, or packet reception in progress but not done */
	//if (!RX_DESC_VALID(*minic.rx_head)) {
		if (minic_readl(MINIC_REG_MCR) & MINIC_MCR_RX_FULL)
			//minic_new_rx_buffer();

		minic_writel(MINIC_REG_EIC_ISR, MINIC_EIC_ISR_RX);
	//}

	return n_recvd;
}

int minic_tx_frame(struct wr_ethhdr_vlan *hdr, uint8_t *payload, uint32_t size,
		   struct hw_timestamp *hwts)
{
	uint32_t d_hdr, mcr, pwords, hwords;
	uint8_t ts_valid;
	int i, hsize;
	uint16_t *ptr;

	if (hdr->ethtype == htons(0x8100))
		hsize = sizeof(struct wr_ethhdr_vlan);
	else
		hsize = sizeof(struct wr_ethhdr);
	hwords = hsize >> 1;

	if (size + hsize < 60)
		size = 60 - hsize;
	pwords = ((size + 1) >> 1);

	d_hdr = 0;

	/* First we write status word (empty status for Tx) */
	minic_txword(WRF_STATUS, 0);

	/* Write the header of the frame */
	ptr = (uint16_t *)hdr;
	for (i = 0; i < hwords; ++i)
		minic_txword(WRF_DATA, ptr[i]);

	/* Write the payload without the last word (which can be one byte) */
	ptr = (uint16_t *)payload;
	for (i = 0; i < pwords-1; ++i)
		minic_txword(WRF_DATA, ptr[i]);

	/* Write last word of the payload (which can be one byte) */
	if (size % 2 == 0)
		minic_txword(WRF_DATA, ptr[i]);
	else
		minic_txword(WRF_BYTESEL, ptr[i]);

	/* Write also OOB if needed */
	if (hwts) {
		minic_txword(WRF_OOB, TX_OOB);
		minic_txword(WRF_OOB, WRPC_FID);
	}

	/* Start sending the frame */
	mcr = minic_readl(MINIC_REG_MCR);
	minic_writel(MINIC_REG_MCR, mcr | MINIC_MCR_TX_START);

	/* wait for the DMA to finish */
	for (i = 0; i < 1000; ++i) {
		mcr = minic_readl(MINIC_REG_MCR);
		if ((mcr & MINIC_MCR_TX_IDLE) != 0) break;
		timer_delay_ms(1);
	}

	if (i == 1000)
		pp_printf("Warning: tx not terminated infinite mcr=0x%x\n",mcr);

	if (hwts) {
		uint32_t raw_ts;
		uint16_t fid;
		uint32_t counter_r, counter_f;
		uint64_t sec;
		uint32_t nsec;

		/* wait for the timestamp */
		for (i = 0; i < 100; ++i) {
			mcr = minic_readl(MINIC_REG_MCR);
			if ((mcr & MINIC_MCR_TX_TS_READY) != 0) break;
			timer_delay_ms(1);
		}

		ts_valid = 1;

		if (i == 100)
		{
			pp_printf("Warning: tx timestamp never became available\n");
			ts_valid = 0;
		}


		if(ts_valid)
			ts_valid = (uint8_t)(minic_readl(MINIC_REG_TSR0)
					     & MINIC_TSR0_VALID);

		raw_ts = minic_readl(MINIC_REG_TSR1);
		fid = MINIC_TSR0_FID_R(minic_readl(MINIC_REG_TSR0));

		if (fid != WRPC_FID) {
			wrc_verbose("minic_tx_frame: unmatched fid %d vs %d\n",
				  fid, WRPC_FID);
		}

		EXPLODE_WR_TIMESTAMP(raw_ts, counter_r, counter_f);
		shw_pps_gen_get_time(&sec, &nsec);

		if (counter_r > 3 * REF_CLOCK_FREQ_HZ / 4 && nsec < 250000000)
			sec--;

		hwts->valid = ts_valid;
		hwts->sec = sec;
		hwts->ahead = 0;
		hwts->nsec = counter_r * (REF_CLOCK_PERIOD_PS / 1000);
		
		minic.tx_count++;
        }
        
	return size;
}

void minic_get_stats(int *tx_frames, int *rx_frames)
{
	*tx_frames = minic.tx_count;
	*rx_frames = minic.rx_count;
}
