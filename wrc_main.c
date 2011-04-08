#include <stdio.h>
#include <inttypes.h>

#include "gpio.h"
#include "uart.h"
#include "endpoint.h"
#include "minic.h"
#include "pps_gen.h"

volatile int count = 0;

uint32_t tag_prev;

uint32_t tics_last;
void _irq_entry()
{
	volatile uint32_t dummy_tag;
	dummy_tag= *(volatile uint32_t*) 0x40004;
//	mprintf("tag %d\n", dummy_tag-tag_prev);
    count++;
    
    if(timer_get_tics() - tics_last > 1000)
    {
     	tics_last = timer_get_tics();
     	mprintf("cnt: %d delta %d\n", count, tag_prev-dummy_tag);
     	count = 0;
    }
	tag_prev=dummy_tag;
    	
}

int main(void)
{
	int rx, tx;
	const uint8_t mac_addr[] = {0x00, 0x00, 0xde, 0xad, 0xba, 0xbe};
	const uint8_t dst_mac_addr[] = {0x00, 0x00, 0x12, 0x24, 0x46, 0x11};
	uint8_t hdr[14];

	uart_init();
  
	mprintf("Starting up\n");

	gpio_dir(GPIO_PIN_LED, 1);

	ep_init(mac_addr);
	ep_enable(1, 1);


	while(!ep_link_up());

	ep_get_deltas(&tx, &rx);
	mprintf("delta: tx = %d, rx=%d\n", tx, rx);

	minic_init();
	pps_gen_init();

//(unsigned int *)(0x40000) = 0x1;
//	*(unsigned int *)(0x40024) = 0x1;

	for(;;)
	  {
	    struct hw_timestamp hwts;
	    uint32_t utc, nsec;
	    uint8_t buf[1024];


	    memcpy(hdr, dst_mac_addr, 6);
	    hdr[12] = 0x88;
	    hdr[13] = 0xf7;
		
	    minic_tx_frame(hdr, buf, 500, &hwts);

	    mprintf("TxTs: utc %d nsec %d\n", hwts.utc, hwts.nsec);
	    
	   	delay(1000000);
 
		mprintf("cnt:%d\n", count);
	  }
}


