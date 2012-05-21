#include <stdio.h>
#include <inttypes.h>

#include <stdarg.h>

#include "syscon.h"
#include "uart.h"
#include "endpoint.h"
#include "minic.h"
#include "pps_gen.h"
#include "ptpd.h"
#include "ptpd_netif.h"
#include "i2c.h"
//#include "eeprom.h"
#include "onewire.h"
#include "softpll_ng.h"



RunTimeOpts rtOpts = {
    .ifaceName = { "wr0" },
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

   /**************** White Rabbit *************************/
   .autoPortDiscovery = FALSE,     /*if TRUE: automagically discovers how many ports we have (and how many up-s); else takes from .portNumber*/
   .portNumber 		= 1,
   .calPeriod     = WR_DEFAULT_CAL_PERIOD,
   .E2E_mode 		  = TRUE,
   .wrStateRetry	= WR_DEFAULT_STATE_REPEAT,
   .wrStateTimeout= WR_DEFAULT_STATE_TIMEOUT_MS,
   .phyCalibrationRequired = FALSE,
	.disableFallbackIfWRFails = TRUE,
         
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



int32_t sfp_alpha = 0;
int32_t sfp_deltaTx = 0;
int32_t sfp_deltaRx = 0;

#include "ptp-noposix/libptpnetif/ptpd_netif.h"


//void test_transition()
//{
//    wr_socket_t *sock;
//    wr_sockaddr_t bindaddr;
//    const mac_addr_t PTP_MULTICAST_ADDR = {0x01, 0x1b, 0x19, 0, 0, 0};
//    
//    int phase = 0;
//    
//   softpll_enable();
//   
//   while(!softpll_check_lock()) timer_delay(1000);
//
//
//    bindaddr.family = PTPD_SOCK_RAW_ETHERNET;// socket type
//    bindaddr.ethertype = 0x88f7;         // PTPv2
//    memcpy(bindaddr.mac, PTP_MULTICAST_ADDR, sizeof(mac_addr_t));
//     
//    // Create one socket for event and general messages (WR lower level layer requires that
//    sock = ptpd_netif_create_socket(PTPD_SOCK_RAW_ETHERNET, 0, &bindaddr);
//         
//    for(;;)
//    {	struct hw_timestamp hwts;
//        wr_timestamp_t t_rx;
//        uint8_t buf_hdr[18], buf[128];
//       	update_rx_queues();
//	
//	    if(ptpd_netif_recvfrom(sock, &bindaddr, buf, 128, &t_rx) > 0)
//	
////	if(minic_rx_frame(buf_hdr, buf, 128, &hwts) > 0)
//	{
//	    printf("phase %d ahead %d TS %d:%d\n", phase,0,t_rx.nsec, t_rx.phase);
//	    phase+=100;
//	    softpll_set_phase(phase);
//	    timer_delay(10);
//	}
////    	mprintf(".");
//    }
//}

//int last_btn0;
//int button_pressed()
//{
//	int p;
//	int btn0 = gpio_in(GPIO_BTN1);
//	p=!btn0 && last_btn0;
//	last_btn0 = btn0;
//	return p;
//}

//void rx_test()
//{
//  uint8_t mac[]= {0x1, 0x1b, 0x19, 0,0,0};
//  uint16_t buf[100];
//  wr_socket_t *sock;
//  wr_sockaddr_t addr;
//
//  memcpy(addr.mac, mac, 6);
//  addr.ethertype = 0x88f7;
//
//  ptpd_netif_init();
//  sock = ptpd_netif_create_socket(PTPD_SOCK_RAW_ETHERNET, 0, &addr);
//  mprintf("Sock @ %x\n", sock);
//  wrc_extra_debug = 0;
//  for(;;)
//  {
//   	update_rx_queues();
//  	int n = ptpd_netif_recvfrom(sock, &addr, buf, sizeof(buf), NULL);
//  	if(n>0) 
//  	{
//  		uint16_t sum = 0 ,i, rx;
//  		rx=n;
//  		n= buf[0];
//	  	for(i=1;i<n;i++) sum+=buf[i]; 
//	  	mprintf("%x %x\n", sum, buf[n]);
//	  	if(sum != buf[n])
//	  	{
//	  		mprintf("****************** ERR: rxed %d size %d\n", rx, n);
//	  	}
//	  	
//    }
//    timer_delay(10);
//    mprintf(".");
//  }
//}

#if 1
int get_sfp_id(char *sfp_pn)
{
  uint8_t data, sum=0;
  uint8_t i;

//  wait until SFP signalizes its presence
  while( gpio_in(GPIO_SFP_DET) );

//  mprintf("wr_core: SFP present\n");
  mi2c_init(WRPC_SFP_I2C);

  mi2c_start(WRPC_SFP_I2C);
  mi2c_put_byte(WRPC_SFP_I2C, 0xA0);
  mi2c_put_byte(WRPC_SFP_I2C, 0x00);
  mi2c_repeat_start(WRPC_SFP_I2C); 
  mi2c_put_byte(WRPC_SFP_I2C, 0xA1);
  mi2c_get_byte(WRPC_SFP_I2C, &data, 1);
  mi2c_stop(WRPC_SFP_I2C);

  sum = data;

  mi2c_start(WRPC_SFP_I2C);
  mi2c_put_byte(WRPC_SFP_I2C, 0xA1);
  for(i=1; i<63; ++i)
  {
    mi2c_get_byte(WRPC_SFP_I2C, &data, 0);
    sum = (uint8_t) ((uint16_t)sum + data) & 0xff;
    if(i>=40 && i<=55)    //Part Number
      sfp_pn[i-40] = data;
  }
  mi2c_get_byte(WRPC_SFP_I2C, &data, 1);  //final word, checksum
  mi2c_stop(WRPC_SFP_I2C);

  if(sum == data) 
      return 0;
  
  return -1;
}
#endif

void wrc_initialize()
{
  int ret, i;
  uint8_t mac_addr[6], ds18_id[8] = {0,0,0,0,0,0,0,0};
  char sfp_pn[17];
	
	uart_init();
	timer_init(1);

	uart_write_string(__FILE__ " is up (compiled on "
			  __DATE__ " " __TIME__ ")\n");
    
	mprintf("wr_core: starting up (press G to launch the GUI and D for extra debug messages)....\n");
  //SFP
#if 1
  if( get_sfp_id(sfp_pn) >= 0)
  {
    //mprintf("Found SFP transceiver ID: ");
    for(i=0;i<16;i++)
      mprintf("%c", sfp_pn[i]);
    mprintf("\n");
    /* 
     * if( !access_eeprom(sfp_pn, &sfp_alpha, &sfp_deltaTx, &sfp_deltaRx) )
     * {
     *   mprintf("SFP: alpha=%d, deltaTx=%d, deltaRx=%d\n", sfp_alpha, sfp_deltaTx, sfp_deltaRx);
     * }
     */
  }
#endif

#if 1
  //Generate MAC address
  ow_init();
  if( ds18x_read_serial(ds18_id) == 0 ) 
    TRACE_DEV("Found DS18xx sensor: %x:%x:%x:%x:%x:%x:%x:%x\n",
        ds18_id[7], ds18_id[6], ds18_id[5], ds18_id[4], 
        ds18_id[3], ds18_id[2], ds18_id[1], ds18_id[0]);
  else
    TRACE_DEV("DS18B20 not found\n");
#endif

  mac_addr[0] = 0x08;   //
  mac_addr[1] = 0x00;   // CERN OUI
  mac_addr[2] = 0x30;   //  
  mac_addr[3] = ds18_id[3]; // www.maxim-ic.com
  mac_addr[4] = ds18_id[2]; // APPLICATION NOTE 186  
  mac_addr[5] = ds18_id[1]; // Creating Global Identifiers Using 1-Wire® Devices

  TRACE_DEV("wr_core: local MAC address: %x:%x:%x:%x:%x:%x\n", mac_addr[0],mac_addr[1],mac_addr[2],mac_addr[3],mac_addr[4],mac_addr[5]);
  ep_init(mac_addr);
  ep_enable(1, 1);

  minic_init();
  pps_gen_init();

  netStartup();

  ptpPortDS = ptpdStartup(0, NULL, &ret, &rtOpts, &ptpClockDS);
  initDataClock(&rtOpts, &ptpClockDS);

  displayConfigINFO(&rtOpts);

  //initialize sockets
  if(!netInit(&ptpPortDS->netPath, &rtOpts, ptpPortDS))
  {
    PTPD_TRACE(TRACE_WRPC, NULL,"failed to initialize network\n");
    return;
  }
  ptpPortDS->linkUP = FALSE;
}

#define LINK_WENT_UP 1
#define LINK_WENT_DOWN 2
#define LINK_UP 3
#define LINK_DOWN 4

int wrc_check_link()
{
	static int prev_link_state = -1;
	int link_state = ep_link_up(NULL);
	int rv = 0;

  if(!prev_link_state && link_state)
  {
    TRACE_DEV("Link up.\n");
    gpio_out(GPIO_LED_LINK, 1);
    rv = LINK_WENT_UP;
  } 
  else if(prev_link_state && !link_state)
  {
    TRACE_DEV("Link down.\n");
    gpio_out(GPIO_LED_LINK, 0);
    rv = LINK_WENT_DOWN;
  }  else rv = (link_state ? LINK_UP : LINK_DOWN);
  prev_link_state = link_state;

  return rv;
}

int wrc_extra_debug = 0;
int wrc_gui_mode = 1;

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

//				wrc_debug_printf(0,"Verbose debug %s.\n", wrc_extra_debug ? "enabled" : "disabled");
				break;
 		 		
 		 	
 		 	case 't':
 		 		wrc_enable_tracking = 1 - wrc_enable_tracking;
			 	wr_servo_enable_tracking(wrc_enable_tracking);

//				wrc_debug_printf(0,"Phase tracking %s.\n", wrc_enable_tracking ? "enabled" : "disabled");
				break;

			case '+':
			case '-':
				wrc_man_phase += (x=='+' ? 100 : -100);
//				wrc_debug_printf(0,"Manual phase adjust: %d\n", wrc_man_phase);
				wr_servo_man_adjust_phase(wrc_man_phase);
				break;
 		}
 	}
}

extern volatile int irq_cnt;

int main(void)
{
  wrc_initialize();

  wrc_extra_debug = 1;
  wrc_gui_mode = 0;

  spll_init(SPLL_MODE_FREE_RUNNING_MASTER, 0, 1);

	for(;;)
	{
		wrc_handle_input();
		if(wrc_gui_mode)
			wrc_mon_gui();
		else
			wrc_log_stats();

    int l_status = wrc_check_link();

    switch (l_status)
    {
      case LINK_UP:
        update_rx_queues();
        break;

      case LINK_WENT_DOWN:
        spll_init(SPLL_MODE_FREE_RUNNING_MASTER, 0, 1);
        break;
    }        

    singlePortLoop(&rtOpts, ptpPortDS, 0);// RunTimeOpts *rtOpts, PtpPortDS *ptpPortDS, int portIndex)
    sharedPortsLoop(ptpPortDS);

    spll_update_aux_clocks();
    // 		delay(1000);
  }
}

