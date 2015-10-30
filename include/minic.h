/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#ifndef __MINIC_H
#define __MINIC_H

#include "types.h"

#define ETH_HEADER_SIZE 14
#define ETH_ALEN 6
#define ETH_P_1588     0x88F7          /* IEEE 1588 Timesync */

#define WRPC_FID				0

void minic_init(void);
void minic_disable(void);
int minic_poll_rx(void);
void minic_get_stats(int *tx_frames, int *rx_frames);

int minic_rx_frame(uint8_t * hdr, uint8_t * payload, uint32_t buf_size,
		   struct hw_timestamp *hwts);
int minic_tx_frame(uint8_t * hdr, uint8_t * payload, uint32_t size,
		   struct hw_timestamp *hwts);

#endif
