#ifndef __ENDPOINT_H
#define __ENDPOINT_H

void ep_init(uint8_t mac_addr[]);
int ep_enable(int enabled, int autoneg);
int ep_link_up();

#endif
