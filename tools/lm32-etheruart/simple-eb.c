/*
 * main.c
 *
 *  Created on: Aug 10, 2012
 *      Author: bbielawski
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include <unistd.h>

#include "etherbone.h"

static 	eb_socket_t socket;

static inline void process_result(eb_status_t result)
{
	if (result != EB_OK)
	{
		printf("Error: %s\n", eb_status(result));
//		exit(1);
	}
	return;
}


eb_status_t ebs_block_write(eb_device_t device, eb_address_t address, eb_data_t* data, int count, int autoincrement_address)
{

	int write_done = 0;

	void wr_callback(eb_user_data_t data, eb_device_t device, eb_operation_t operation, eb_status_t status)
	{
		write_done = 1;
	}


	eb_cycle_t cycle;
	eb_status_t status = eb_cycle_open(device, NULL, wr_callback, &cycle);

	if (status != EB_OK)
		return status;

	int i;
	for (i = 0; i < count; i++)
	{
		eb_cycle_write(cycle, address, EB_DATA32 | EB_LITTLE_ENDIAN, data[i]);
		if (autoincrement_address)
			address += 4;
	}

	eb_cycle_close(cycle);
	status = eb_device_flush(device);

	while (!write_done) eb_socket_run(socket, 10);		//wait forever

	return status;
}

eb_status_t ebs_block_read(eb_device_t device, eb_address_t read_address, eb_data_t *rdata, int count, int autoincrement_address)
{
	int read_done = 0;
	void rd_callback(eb_user_data_t data, eb_device_t device, eb_operation_t op, eb_status_t status)
	{
		static int c = 0;

		if (status != EB_OK)
		{
			fprintf(stderr, "read failed: %s\n", eb_status(status));
			exit(-1);
		}

		for(; op != EB_NULL; op = eb_operation_next(op)) {
      /* We read low bits first */
	     *rdata++ = eb_operation_data(op);
		}
			
		read_done = 1;
	}

	eb_cycle_t cycle;
	eb_status_t status = eb_cycle_open(device, NULL, rd_callback, &cycle);
	if (status != EB_OK)
			return status;

	int i;
	for (i = 0; i < count; i++)
	{
		eb_cycle_read(cycle, read_address, EB_DATA32 | EB_LITTLE_ENDIAN, NULL);
		if(autoincrement_address)
			read_address += 4;
	}

	eb_cycle_close(cycle);
	status = eb_device_flush(device);

	while (!read_done) eb_socket_run(socket, -1);		//wait forever

	return status;
}

uint32_t ebs_read(eb_device_t device, eb_address_t addr)
{
	eb_data_t rdata;
	ebs_block_read(device, addr, &rdata, 1, 0);
	return rdata;
}

void ebs_write(eb_device_t device, eb_address_t addr, eb_data_t data)
{
	ebs_block_write(device, addr, &data, 1, 0);
}

eb_status_t ebs_init()
{
	eb_status_t status = EB_OK;

	status = eb_socket_open(EB_ABI_CODE, 0, EB_DATA32 | EB_ADDR32, &socket);
	process_result(status);
}

eb_status_t ebs_shutdown()
{
	return eb_socket_close(socket);
}

eb_status_t ebs_open(eb_device_t *dev, const char *network_address)
{
	eb_status_t status = EB_OK;

	status = eb_device_open(socket, network_address, EB_DATA32 | EB_ADDR32, 5, dev);
	process_result(status);
	return status;
}

eb_status_t ebs_close(eb_device_t dev)
{
	return eb_device_close(dev);
}

struct bus_record {
  int i;
  int stop;
  eb_address_t addr_first, addr_last;
  struct bus_record* parent;
};

int ebs_sdb_find_device(eb_device_t dev, uint32_t vendor, uint32_t device, int seq, uint32_t *base_addr)
{
	struct bus_record br;
	int done = 0, found = 0, n = 0;
	
		
	void  scan_cb(eb_user_data_t user, eb_device_t dev, const struct sdb_table * sdb, eb_status_t status) {
	  struct bus_record br;
  		int devices;
  
  		br.parent = (struct bus_record*)user;
  		br.parent->stop = 1;
  
  		if (status != EB_OK) {
    		fprintf(stderr, "failed to retrieve SDB: %s\n", eb_status(status));
    		exit(1);
  		} 
  
  		devices = sdb->interconnect.sdb_records - 1;
  		for (br.i = 0; br.i < devices; ++br.i) {
  			const union sdb_record *des;
		    des = &sdb->record[br.i];
 		
 				if(des->empty.record_type == sdb_record_device)
	 			{
 					if(des->device.sdb_component.product.vendor_id == vendor &&
						 des->device.sdb_component.product.device_id == device)
						 {
						 		if(n == seq) 
						 		{ 
					 				*base_addr = des->device.sdb_component.addr_first; 
					 				found = 1;
						 			return;
						 		}
						 		n ++;
	 					}
	  		}
	  	}
		done = 1;
	}

	br.parent = 0;
 	br.i = -1;
 	br.stop = 0;
 	br.addr_first = 0;
	br.addr_last = ~(eb_address_t)0;

	br.addr_last >>= (sizeof(eb_address_t) - (eb_device_width(dev) >> 4))*8;

	if(eb_sdb_scan_root(dev, &br, &scan_cb) != EB_OK)
		return -1;

	while(!done && !found) // fixme: crap code
		eb_socket_run(eb_device_socket(dev), -1);

	return found;
}

#if 0
main()
{
	eb_device_t dev;
	uint32_t base;
	ebs_init();
	ebs_open(&dev,  "udp/192.168.10.11");
	ebs_sdb_find_device(dev, 0xce42, 0xf19ede1a, 0, &base);
	printf("base addr: %x\n", base);
}
#endif
