#include <stdio.h>
#include <inttypes.h>

#include <stdarg.h>
#define INET_ADDRSTRLEN 16

#include "syscon.h"
#include "uart.h"
#include "endpoint.h"
#include "hw/endpoint_mdio.h"
#include "minic.h"
#include "pps_gen.h"
#include "softpll_ng.h"
#include "ptpd_netif.h"

#include "ptpd.h"

int wrc_extra_debug = 1;
int wrc_gui_mode = 0;

int get_bitslide(int ep)
{
	return (pcs_read(16) >> 4) & 0x1f;
}

#define MAX_BITSLIDES 20

static	struct {
		int occupied;
		int phase_min, phase_max, phase_dev;
		int rx_ahead;
		int delta;
		int hits;
} bslides[MAX_BITSLIDES];

int bslide_bins()
{
	int i, hits = 0;
	
	for(i=0;i<MAX_BITSLIDES;i++)
		if(bslides[i].occupied) hits++;
	
	return hits;
}

#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))

void bslide_update(int phase, int delta, int ahead, int bs)
{
	bslides[bs].occupied = 1;
	bslides[bs].phase_min = MIN(phase, bslides[bs].phase_min);
	bslides[bs].phase_max = MAX(phase, bslides[bs].phase_max);
	bslides[bs].phase_dev = bslides[bs].phase_max - bslides[bs].phase_min;
	bslides[bs].delta = delta;
	bslides[bs].rx_ahead = ahead;
	bslides[bs].hits++;
}

static int quit = 0;

static void print_cal_stats()
{
	int i,last_occupied = -1;
	printf("Calibration statistics: \n");
	printf("bitslide[UIs] | Delta[ns] | Ahead[bool] | phase_min[tics] | phase_max[tics] | phase_dev[tics] | hits | delta_prev[ps] \n");
	for(i=0;i<MAX_BITSLIDES;i++)
		if(bslides[i].occupied)
		{
			printf("%-15d %-11d %-13d %-17d %-17d %-17d %-6d %d\n", 
				i, 
				bslides[i].delta, 
				bslides[i].rx_ahead, 
				bslides[i].phase_min, 
				bslides[i].phase_max, 
				bslides[i].phase_dev, 
				bslides[i].hits, 
				(last_occupied >= 0) ? bslides[i].delta-bslides[last_occupied].delta:0
			);

			last_occupied = i;
		}
	printf("\n");
}


struct meas_entry {
  	int delta_ns;
  	int phase;
  	int phase_sync;
  	int ahead;
};

void purge_socket(wr_socket_t *sock)
{
	wr_sockaddr_t from;
	char buf[128];
	update_rx_queues();
	while(ptpd_netif_recvfrom(sock, &from, buf, 128, NULL) > 0) update_rx_queues();
}

int meas_phase_range(wr_socket_t *sock, int phase_min, int phase_max, int phase_step, struct meas_entry *results)
{
	char buf[128];
	wr_timestamp_t ts_tx, ts_rx, ts_sync;
	wr_sockaddr_t from;
	MsgHeader mhdr;
	int setpoint = phase_min, i = 0, phase;
	spll_set_phase_shift(SPLL_ALL_CHANNELS, phase_min);
	while(spll_shifter_busy(0));
	
	purge_socket(sock);

	i=0;
	while(setpoint <= phase_max)
	{
		ptpd_netif_get_dmtd_phase(sock, &phase);

		update_rx_queues();
		int n = ptpd_netif_recvfrom(sock, &from, buf, 128, &ts_rx);

		
		if(n>0)
		{			
			msgUnpackHeader(buf, &mhdr);
			if(mhdr.messageType == 0)
				ts_sync = ts_rx;
		    else if(mhdr.messageType == 8)
		    {
				MsgFollowUp fup;
				msgUnpackFollowUp(buf, &fup);
				
//				mprintf("FUP shift: %d TS_ahead : %d ,nsec :%d ,phase: %d, dmphase: %d delta %d\n",shift,ts_sync.raw_ahead, ts_sync.nsec, ts_sync.phase, phase,
//				fup.preciseOriginTimestamp.nanosecondsField - ts_sync.nsec);


            mprintf("Shift: %d/%dps [step %dps]        \r", setpoint,phase_max,phase_step);
            results[i].phase = phase;
            results[i].phase_sync = ts_sync.phase;
            results[i].ahead = ts_sync.raw_ahead;
            results[i].delta_ns = fup.preciseOriginTimestamp.nanosecondsField - ts_sync.nsec;
            results[i].delta_ns += (fup.preciseOriginTimestamp.secondsField.lsb - ts_sync.utc) * 1000000000;
            
			setpoint += phase_step;
			spll_set_phase_shift(0, setpoint);
			while(spll_shifter_busy(0));
			purge_socket(sock);

            i++;
			}
		}
	}
	mprintf("\n");
	return i;
}

int find_transition(struct meas_entry *results, int n, int positive)
{
	int i;
	for(i=0;i<n;i++)
	{
	 	if ((results[i].ahead ^ results[(i+1)%n].ahead) && (results[(i+1)%n].ahead == positive))
	 		return i;
	}
}

extern void ptpd_netif_set_phase_transition(wr_socket_t *sock, int phase);


void calc_trans()
{
	wr_socket_t *sock;
	wr_sockaddr_t sock_addr;
	int i, nr;
	struct meas_entry results[128];
	
	spll_enable_ptracker(0, 1);

	sock_addr.family = PTPD_SOCK_RAW_ETHERNET; // socket type
	sock_addr.ethertype = 0x88f7;
	sock_addr.mac[0] = 0x1;
	sock_addr.mac[1] = 0x1b;
	sock_addr.mac[2] = 0x19;
	sock_addr.mac[3] = 0;
	sock_addr.mac[4] = 0;
	sock_addr.mac[5] = 0;

	mprintf("---------------------------------\nWR PTP Core phase_transition parameter calibration program\n\
Make sure your SPEC is connected to a WR switch (running PTP in master mode)\n---------------------------------\n");

	mprintf("Waiting for link to go up");
	while( !ep_link_up(NULL) )
	{
		mprintf(".");
		timer_delay(1000);
    }
    mprintf("\n");

	spll_init(SPLL_MODE_SLAVE, 0, 1);

	mprintf("Locking the PLL...");
	while(!spll_check_lock(0));
	mprintf("locked\n");

	if(ptpd_netif_init() != 0)
	{
	 	mprintf("ptpd-netif initialization failed\n");
	 	return;
	}
    
	sock = ptpd_netif_create_socket(PTPD_SOCK_RAW_ETHERNET, 0, &sock_addr);
	nr=meas_phase_range(sock, 0, 8000, 1000, results);
	int tplus = find_transition(results, nr, 1);
	int tminus = find_transition(results, nr, 0);

	int approx_plus = results[tplus].phase;
	int approx_minus = results[tminus].phase;
	

	nr=meas_phase_range(sock, approx_plus-1000, approx_plus+1000, 100, results);
	tplus = find_transition(results, nr, 1);
    int phase_plus = results[tplus].phase;
    
	nr=meas_phase_range(sock, approx_minus-1000, approx_minus+1000, 100, results);
	tminus = find_transition(results, nr, 0);
    int phase_minus = results[tminus].phase;
    
	mprintf("Transitions found: positive @ %d ps, negative @ %d ps.\n", phase_plus, phase_minus);
	int ttrans = phase_minus-1000;
	if(ttrans < 0) ttrans+=8000;
	
	mprintf("(t2/t4)_phase_transition = %dps\n", ttrans);
	ptpd_netif_set_phase_transition(sock, ttrans);

	mprintf("Verification... \n");
	nr =meas_phase_range(sock, 0, 16000, 500, results);
	
	for(i=0;i<nr;i++) mprintf("phase_dmtd: %d delta_ns: %d, phase_sync: %d\n", results[i].phase, results[i].delta_ns, results[i].phase_sync);
	

	for(;;);
//	print_cal_stats();
}



void pps_adjustment_test()
{
  wr_timestamp_t ts_tx, ts_rx;
  wr_socket_t *sock;
  wr_sockaddr_t sock_addr, to, from;
  int adjust_count = 0;


  sock_addr.family = PTPD_SOCK_RAW_ETHERNET; // socket type
  sock_addr.ethertype = 0x88f7;
		sock_addr.mac[0] = 0x1;
		sock_addr.mac[1] = 0x1b;
		sock_addr.mac[2] = 0x19;
		sock_addr.mac[3] = 0;
		sock_addr.mac[4] = 0;
		sock_addr.mac[5] = 0;

	ptpd_netif_init();
    
  sock = ptpd_netif_create_socket(PTPD_SOCK_RAW_ETHERNET, 0, &sock_addr);


	while(!quit)
	{
		char buf[128];
		wr_sockaddr_t to;

//	  memset(to.mac, 0xff, 6);
		to.mac[0] = 0x1;
		to.mac[1] = 0x1b;
		to.mac[2] = 0x19;
		to.mac[3] = 0;
		to.mac[4] = 0;
		to.mac[5] = 0;
		
 	  to.ethertype = 0x88f7;
	  to.family = PTPD_SOCK_RAW_ETHERNET; // socket type

		if(adjust_count == 0)
		{
			ptpd_netif_adjust_counters(1,0);// 500000000);
			adjust_count = 8;
		}

//		if(!ptpd_netif_adjust_in_progress())
		{	
			ptpd_netif_sendto(sock, &to, buf, 64, &ts_tx);
			update_rx_queues();
			int n = ptpd_netif_recvfrom(sock, &from, buf, 128, &ts_rx);
			mprintf("TX timestamp: correct %d %d:%d\n", ts_tx.correct, (int) ts_tx.utc, ts_tx.nsec);
			mprintf("RX timestamp: correct %d %d:%d\n", ts_rx.correct, (int) ts_rx.utc, ts_rx.nsec);
			adjust_count --;
		}// else printf("AdjustInProgress\n");
			
		timer_delay(1000);
	}
}


void wrc_debug_printf(int subsys, const char *fmt, ...)
{
	va_list ap;
	
	if(wrc_gui_mode) return;
	
	va_start(ap, fmt);
	
	if(wrc_extra_debug || (!wrc_extra_debug))
	 	vprintf(fmt, ap);
	
	va_end(ap);
}


void wrc_initialize()
{
	int ret, i;
	uint8_t mac_addr[6], ds18_id[8] = {0,0,0,0,0,0,0,0};
  char sfp_pn[17];
	
	uart_init();

	uart_write_string(__FILE__ " is up (compiled on "
		__DATE__ " " __TIME__ ")\n");

	mac_addr[0] = 0x08;   //
	mac_addr[1] = 0x00;   // CERN OUI
	mac_addr[2] = 0x30;   //  
	mac_addr[3] = 0xca;
	mac_addr[4] = 0xfe;
	mac_addr[5] = 0xba;

	timer_init(1);  
	ep_init(mac_addr);
	ep_enable(1, 1);

	minic_init();
	pps_gen_init();
}

#define LINK_UP 0
#define LINK_WENT_UP 1
#define LINK_DOWN 2
#define LINK_WENT_DOWN 3


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
	} else if(prev_link_state && !link_state)
	{
		TRACE_DEV("Link down.\n");
		gpio_out(GPIO_LED_LINK, 0);
		rv = LINK_WENT_DOWN;
	}  else rv = (link_state ? LINK_UP : LINK_DOWN);
	prev_link_state = link_state;
	
	return rv;
}

int main(void)
{
  wrc_initialize();

  calc_trans();
  
	for(;;);
	for(;;)
	{
		int l_status = wrc_check_link();
		switch (l_status)
		{
			case LINK_WENT_UP:
				mprintf("Link up.\n");
				spll_init(SPLL_MODE_SLAVE, 0, 1);
            
			case LINK_UP:
				break;

			case LINK_WENT_DOWN:
				mprintf("Link down.\n");
				break;
		}        
	    spll_show_stats();
//    	timer_delay(1000);

	}
}

