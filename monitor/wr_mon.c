#include <stdio.h>
#include <stdlib.h>

#include <wr_ipc.h>

#include "term.h"

#include "ptpd_exports.h"
#include "hal_client.h"

hexp_port_list_t port_list;

wripc_handle_t h_ptp;

void init()
{

	halexp_client_init();

	if( (h_ptp = wripc_connect("ptpd")) < 0)
	{
		fprintf(stderr,"Can't establish WRIPC connection to the PTP daemon!\n");
		exit(-1);
	}
	
	term_init();		
	halexp_query_ports(&port_list);
}

void show_ports()
{
	int i;
	term_pcprintf(3, 1, C_BLUE, "Switch ports:");

	for(i=0; i<port_list.num_ports;i++)
	{
		hexp_port_state_t state;
		halexp_get_port_state(&state, port_list.port_names[i]);
			
		term_pcprintf(5+i, 1, C_WHITE, "%s: ", port_list.port_names[i]);
		if(state.up)	term_cprintf(C_GREEN, "Link up    "); else 	term_cprintf(C_RED, "Link down  ");
	
		term_cprintf(C_GREY, "mode: ");
	
		switch(state.mode)
		{
			case HEXP_PORT_MODE_WR_MASTER:term_cprintf(C_WHITE, "WR Master  ");break;
			case HEXP_PORT_MODE_WR_SLAVE:term_cprintf(C_WHITE, "WR Slave   ");break;
		}

		if(state.is_locked)	term_cprintf(C_GREEN, "Locked  "); else 	term_cprintf(C_RED, "NoLock  ");
		if(state.rx_calibrated  && state.tx_calibrated)	term_cprintf(C_GREEN, "Calibrated  "); else 	term_cprintf(C_RED, "Uncalibrated  ");
	}

	

	
}

	int ph_adjust = 0;

void show_servo()
{
	ptpdexp_sync_state_t ss;
  wripc_call(h_ptp, "ptpdexp_get_sync_state", &ss, 0);
	  
	term_cprintf(C_BLUE, "\n\nSynchronization status:\n\n");

	if(!ss.valid)
	{
		term_cprintf(C_RED, "Master mode or sync info not valid\n\n");
		return;
	} 
	
	term_cprintf(C_GREY, "Servo state:               "); term_cprintf(C_WHITE, "%s\n", ss.slave_servo_state);
	term_cprintf(C_GREY, "Phase tracking:            "); if(ss.tracking_enabled) term_cprintf(C_GREEN, "ON\n"); else term_cprintf(C_RED,"OFF\n");
	term_cprintf(C_GREY, "Synchronization source:    "); term_cprintf(C_WHITE, "%s\n", ss.sync_source);

	term_cprintf(C_BLUE, "\nTiming parameters:\n\n");

	term_cprintf(C_GREY, "Round-trip time (mu):      "); term_cprintf(C_WHITE, "%.2f nsec\n", ss.mu/1000.0);
	term_cprintf(C_GREY, "Master-slave delay:        "); term_cprintf(C_WHITE, "%.2f nsec\n", ss.delay_ms/1000.0);
	term_cprintf(C_GREY, "Link length:               "); term_cprintf(C_WHITE, "%.0f meters \n", ss.delay_ms/1e12 * 300e6 / 1.55);
	term_cprintf(C_GREY, "Master PHY delays:         "); term_cprintf(C_WHITE, "TX: %.2f nsec, RX: %.2f nsec\n", ss.delta_tx_m/1000.0, ss.delta_rx_m/1000.0);
	term_cprintf(C_GREY, "Slave PHY delays:          "); term_cprintf(C_WHITE, "TX: %.2f nsec, RX: %.2f nsec\n", ss.delta_tx_s/1000.0, ss.delta_rx_s/1000.0);
	term_cprintf(C_GREY, "Total link asymmetry:      "); term_cprintf(C_WHITE, "%.2f nsec\n", ss.total_asymmetry/1000.0);
//	term_cprintf(C_GREY, "Fiber asymmetry:           "); term_cprintf(C_WHITE, "%.2f nsec\n", ss.fiber_asymmetry/1000.0);

	term_cprintf(C_GREY, "Clock offset:              "); term_cprintf(C_WHITE, "%.2f nsec\n", ss.cur_offset/1000.0);
	term_cprintf(C_GREY, "Phase setpoint:            "); term_cprintf(C_WHITE, "%.2f nsec\n", ss.cur_setpoint/1000.0);
	term_cprintf(C_GREY, "Skew:                      "); term_cprintf(C_WHITE, "%.2f nsec\n", ss.cur_skew/1000.0);
	term_cprintf(C_GREY, "Phase extra adjust:                      "); term_cprintf(C_WHITE, "%.2f nsec\n", (double) ph_adjust/1000.0);
	
	

}

int ki=20, kp=1000;

void show_pll()
{
	term_cprintf(C_BLUE, "\n\nDMPLL:\n\n");
	term_cprintf(C_GREY, "KI: %d KP: %d", ki, kp);

}

void show_menu()
{
	term_pcprintf(30, 1, C_BLUE, "q = quit, t = toggle tracking");
}

int track_onoff = 1;

void show_screen()
{
	term_clear();
	term_pcprintf(1, 1, C_BLUE, "WR Switch Sync Monitor v 0.1");
	
	show_ports();
	show_servo();	
	show_menu();
	
//	handle_toggle();
}

void pll_set_gain(int kp, int ki)
{
	hexp_pll_cmd_t cmd;
	cmd.pll = HEXP_DMPLL;
	cmd.branch = HEXP_PHASE;
	cmd.ki = ki;
	cmd.kp = kp;
//	halexp_pll_cmd(0, &cmd);
}

main()
{
  int rval;
	init();
	
	
	for(;;)
	{
		if(term_poll())
		{
			int c = term_get();
			
			if(c=='q') break;
			else if(c=='t') {
				track_onoff = 1-track_onoff;
				wripc_call(h_ptp, "ptpdexp_cmd", &rval, 2, A_INT32(PTPDEXP_COMMAND_TRACKING), A_INT32(track_onoff));
			} else if (c=='a')
			{
				ph_adjust += 500;
				wripc_call(h_ptp, "ptpdexp_cmd", &rval, 2, A_INT32(PTPDEXP_COMMAND_MAN_ADJUST_PHASE), A_INT32(ph_adjust));
				
			} else if (c=='z')
			{
				ph_adjust -= 500;
				wripc_call(h_ptp, "ptpdexp_cmd", &rval, 2, A_INT32(PTPDEXP_COMMAND_MAN_ADJUST_PHASE), A_INT32(ph_adjust));
				
			}
			
			else if (c=='s')
			{
				ki += 10;
				pll_set_gain(kp, ki);
			} else if (c=='x')
			{
				ki -= 10;
				pll_set_gain(kp, ki);
			}
			else if (c=='d')
			{
				kp += 10;
				pll_set_gain(kp, ki);
			} else if (c=='c')
			{
				kp -= 10;
				pll_set_gain(kp, ki);
			}



		}
	
		show_screen();
		usleep(500000);
	}
	
	term_cprintf(C_GREY,"bye...\n\n");
	term_clear();	
	term_restore();
	
}