#ifndef __ENDPOINT_H
#define __ENDPOINT_H

#define DMTD_AVG_SAMPLES 256
#define DMTD_MAX_PHASE 16384

void ep_init(uint8_t mac_addr[]);
void get_mac_addr(uint8_t dev_addr[]);
int ep_enable(int enabled, int autoneg);
int ep_link_up();
int ep_get_deltas(uint32_t *delta_tx, uint32_t *delta_rx);
int ep_get_psval(int32_t *psval);
int ep_cal_pattern_enable();
int ep_cal_pattern_disable();
#endif
