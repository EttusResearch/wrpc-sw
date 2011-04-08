#ifndef __MINIC_H
#define __MINIC_H

#include "types.h"

void minic_init();
int minic_poll_rx();

int minic_rx_frame(uint8_t *hdr, uint8_t *payload, uint32_t buf_size, struct hw_timestamp *hwts);
int minic_tx_frame(uint8_t *hdr, uint8_t *payload, uint32_t size, struct hw_timestamp *hwts);



#endif
