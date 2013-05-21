#ifndef __SIMPLE_EB_H
#define __SIMPLE_EB_H

#include <stdint.h>

#include "etherbone.h"

eb_status_t ebs_block_write(eb_device_t device, eb_address_t address, eb_data_t* data, int count, int autoincrement_address);
eb_status_t ebs_block_read(eb_device_t device, eb_address_t read_address, eb_data_t *rdata, int count, int autoincrement_address);
uint32_t ebs_read(eb_device_t device, eb_address_t addr);
void ebs_write(eb_device_t device, eb_address_t addr, eb_data_t data);
eb_status_t ebs_init();
eb_status_t ebs_shutdown();
eb_status_t ebs_open(eb_device_t *dev, const char *network_address);
eb_status_t ebs_close(eb_device_t dev);
int ebs_sdb_find_device(eb_device_t dev, uint32_t vendor, uint32_t device, int seq, uint32_t *base_addr);


#endif
