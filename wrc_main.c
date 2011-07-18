#include <stdio.h>
#include <inttypes.h>

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

int main(void)
{
	int rx, tx;
	int link_went_up, link_went_down;
	int prev_link_state= 0, link_state;
	
  int16_t ret;

	uart_init();

	ep_init(mac_addr);
	ep_enable(1, 1);


	minic_init();
	pps_gen_init();

	netStartup();

//    mi2c_init();
//    mi2c_scan();

  gpio_dir(GPIO_PIN_BTN1, 0);

//	softpll_enable();
//	for(;;) softpll_check_lock();

  wr_servo_man_adjust_phase(-11600 + 1700);

  displayConfigINFO(&rtOpts);

	ptpPortDS = ptpdStartup(0, NULL, &ret, &rtOpts, &ptpClockDS);
	initDataClock(&rtOpts, &ptpClockDS);
    	
  for(;;)
	{
		link_state = ep_link_up();
		
		link_went_up = !prev_link_state && link_state;
		prev_link_state = link_state;
		
		if(link_went_up)
		{
			uint32_t dtxm, drxm;
			TRACE_DEV("LINK UP\n");
//			toState(PTP_INITIALIZING, &rtOpts, ptpPortDS);
	
		}

			//wr_mon_debug();
			if(button_pressed())
			{
			 	enable_tracking = 1-enable_tracking;
			 	wr_servo_enable_tracking(enable_tracking);
			}	
	
    //mprintf("before state=%d, wrState=%d\n", ptpPortDS->portState, ptpPortDS->wrPortState);
		  protocol_nonblock(&rtOpts, ptpPortDS);
    //mprintf("after state=%d, wrState=%d\n", ptpPortDS->portState, ptpPortDS->wrPortState);
		  update_rx_queues();
		  timer_delay(10);
	}
  
}



