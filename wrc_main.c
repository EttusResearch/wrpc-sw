#include <stdio.h>
#include <inttypes.h>

#include <stdarg.h>

#include "gpio.h"
#include "uart.h"
#include "endpoint.h"
#include "minic.h"
#include "pps_gen.h"
#include "ptpd.h"



RunTimeOpts rtOpts = {
   .announceInterval = DEFAULT_ANNOUNCE_INTERVAL,
   .syncInterval = DEFAULT_SYNC_INTERVAL,
   .clockQuality.clockAccuracy = DEFAULT_CLOCK_ACCURACY,
   .clockQuality.clockClass = DEFAULT_CLOCK_CLASS,
   .clockQuality.offsetScaledLogVariance = DEFAULT_CLOCK_VARIANCE,
   .priority1 = DEFAULT_PRIORITY1,
   .priority2 = DEFAULT_PRIORITY2,
   .domainNumber = DEFAULT_DOMAIN_NUMBER,
   .currentUtcOffset = DEFAULT_UTC_OFFSET,
   .noResetClock = DEFAULT_NO_RESET_CLOCK,
   .noAdjust = NO_ADJUST,
   .inboundLatency.nanoseconds = DEFAULT_INBOUND_LATENCY,
   .outboundLatency.nanoseconds = DEFAULT_OUTBOUND_LATENCY,
   .s = DEFAULT_DELAY_S,
   .ap = DEFAULT_AP,
   .ai = DEFAULT_AI,
   .max_foreign_records = DEFAULT_MAX_FOREIGN_RECORDS,
#ifdef WRPC_SLAVE
   .slaveOnly = TRUE,
#else
   .slaveOnly = FALSE,
#endif

   /**************** White Rabbit *************************/
   .autoPortDiscovery = FALSE,     /*if TRUE: automagically discovers how many ports we have (and how many up-s); else takes from .portNumber*/
   .portNumber 		= 1,
   .calPeriod     = WR_DEFAULT_CAL_PERIOD,
   .E2E_mode 		  = TRUE,
   .wrStateRetry	= WR_DEFAULT_STATE_REPEAT,
   .wrStateTimeout= WR_DEFAULT_STATE_TIMEOUT_MS,
   .deltasKnown		= TRUE, //WR_DEFAULT_DELTAS_KNOWN,
   .knownDeltaTx	= 0,//WR_DEFAULT_DELTA_TX,
   .knownDeltaRx	= 0,//WR_DEFAULT_DELTA_RX,
/*SLAVE only or MASTER only*/
#ifdef WRPC_SLAVE
   .primarySource = FALSE,
   .wrConfig      = WR_S_ONLY,
   .masterOnly    = FALSE,
#endif
#ifdef WRPC_MASTER
   .primarySource = TRUE,
   .wrConfig      = WR_M_ONLY,
   .masterOnly    = TRUE,
#endif
   /********************************************************/
};
static   PtpPortDS *ptpPortDS;
static   PtpClockDS ptpClockDS;

	const uint8_t mac_addr[] = {0x00, 0x50, 0xde, 0xad, 0xba, 0xbe};
	const uint8_t dst_mac_addr[] = {0x00, 0x00, 0x12, 0x24, 0x46, 0x11};


volatile int count = 0;

uint32_t tag_prev;

uint32_t tics_last;

void silly_minic_test()
{
	uint8_t hdr[14];

    uint8_t buf_hdr[18], buf[256];
    for(;;)
    {
	memcpy(buf_hdr, dst_mac_addr, 6);
	memset(buf_hdr+6, 0, 6);
	buf_hdr[12] = 0xc0;
	buf_hdr[13] = 0xef;
	
	minic_tx_frame(buf_hdr, buf, 64, buf);
	mprintf("Send\n");
	timer_delay(1000);

    }
}

void test_transition()
{

    int phase = 0;
    
   softpll_enable();
   while(!softpll_check_lock()) timer_delay(1000);

    for(;;)
    {	struct hw_timestamp hwts;
        uint8_t buf_hdr[18], buf[128];
	
	if(minic_rx_frame(buf_hdr, buf, 128, &hwts) > 0)
	{
	    printf("phase %d ahead %d\n", phase, hwts.ahead);
	    phase+=100;
	    softpll_set_phase(phase);
	    timer_delay(10);
	}
    }
}

int last_btn0;

int button_pressed()
{
	int p;
	int btn0 = gpio_in(GPIO_PIN_BTN1);
	p=!btn0 && last_btn0;
	last_btn0 = btn0;
	return p;
}

int enable_tracking = 1;

void wrc_initialize()
{
	int ret;
	uart_init();

	uart_write_string(__FILE__ " is up (compiled on "
			  __DATE__ " " __TIME__ ")\n");

	mprintf("wr_core: starting up (press G to launch the GUI and D for extra debug messages)....\n");
	ep_init(mac_addr);
	ep_enable(1, 1);

	minic_init();
	pps_gen_init();

	netStartup();

	gpio_dir(GPIO_PIN_BTN1, 0);
	gpio_dir(GPIO_PIN_LED_LINK, 1);
	gpio_out(GPIO_PIN_LED_LINK, 0);
	gpio_dir(GPIO_PIN_LED_STATUS, 1);

	wr_servo_man_adjust_phase(-11600 + 1700);

	displayConfigINFO(&rtOpts);

	ptpPortDS = ptpdStartup(0, NULL, &ret, &rtOpts, &ptpClockDS);
	initDataClock(&rtOpts, &ptpClockDS);
}

#define LINK_UP 1
#define LINK_DOWN 2

int wrc_check_link()
{
	static int prev_link_state = -1;
	int link_state = ep_link_up();
	int rv = 0;

	if(!prev_link_state && link_state)
	{
		TRACE_DEV("Link up.\n");
		gpio_out(GPIO_PIN_LED_LINK, 1);
		rv = LINK_UP;
	} else if(prev_link_state && !link_state)
	{
		TRACE_DEV("Link down.\n");
		gpio_out(GPIO_PIN_LED_LINK, 0);
		rv = LINK_DOWN;
	} 
	prev_link_state = link_state;
	
	return rv;
}

int wrc_extra_debug = 0;
int wrc_gui_mode = 0;

void wrc_debug_printf(int subsys, const char *fmt, ...)
{
	va_list ap;
	
	if(wrc_gui_mode) return;
	
	va_start(ap, fmt);
	
	if(wrc_extra_debug || (!wrc_extra_debug && (subsys & TRACE_SERVO)))
	 	vprintf(fmt, ap);
	
	va_end(ap);
}

static int wrc_enable_tracking = 1;
int wrc_man_phase = 0;

void wrc_handle_input()
{
 	if(uart_poll())
 	{
 		int x = uart_read_byte();
 		
 		switch(x)
 		{
 		 	case 'g':
 		 		wrc_gui_mode = 1 - wrc_gui_mode;
 		 		if(!wrc_gui_mode)
 		 		{
 		 			m_term_clear();
 		 			wrc_debug_printf(0, "Exiting GUI mode\n");
 		 		}
 		 	break;
 		 	
 		 	case 'd':
 		 		wrc_extra_debug =  1 - wrc_extra_debug;

				wrc_debug_printf(0,"Verbose debug %s.\n", wrc_extra_debug ? "enabled" : "disabled");
				break;
 		 		
 		 	
 		 	case 't':
 		 		wrc_enable_tracking = 1 - wrc_enable_tracking;
			 	wr_servo_enable_tracking(wrc_enable_tracking);

				wrc_debug_printf(0,"Phase tracking %s.\n", wrc_enable_tracking ? "enabled" : "disabled");
				break;

			case '+':
			case '-':
				wrc_man_phase += (x=='+' ? 100 : -100);
				wrc_debug_printf(0,"Manual phase adjust: %d\n", wrc_man_phase);
				wr_servo_man_adjust_phase(wrc_man_phase);
				break;

                      
 		 		
 		}
 	}
}

extern volatile int irq_cnt;

int main(void)
{
	int rx, tx;
	int link_went_up, link_went_down;
	int prev_link_state= 0, link_state;
	
	int16_t ret;

	wrc_initialize();
	softpll_enable();
	for(;;)
	{
		wrc_check_link();
		wrc_handle_input();
		
		if(wrc_gui_mode)
			wrc_mon_gui();
			
/*		if(button_pressed())
		{
		 	enable_tracking = 1-enable_tracking;
		 	wr_servo_enable_tracking(enable_tracking);
		}	
	                                          */
		protocol_nonblock(&rtOpts, ptpPortDS);
		update_rx_queues();
//		softpll_check_lock();
		timer_delay(10);                      

	}
}



