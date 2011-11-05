#include "board.h"
#include "ptpd_exports.h"
#include "hal_exports.h"
#include "softpll.h"

extern ptpdexp_sync_state_t cur_servo_state;
extern int wrc_man_phase;
#define C_DIM 0x80                                                                                                    
#define C_WHITE 7

#define C_GREY (C_WHITE | C_DIM)                                                                                      

#define C_RED 1
#define C_GREEN 2                                                                                                     
#define C_BLUE 4 

int64_t abs64(int64_t t)
{
  return t<0?(-t):t;
}




int wrc_mon_gui(void)
{
  static char* slave_states[] = {"Uninitialized", "SYNC_UTC", "SYNC_NSEC", "SYNC_PHASE", "TRACK_PHASE"};
  static uint32_t last = 0;
  hexp_port_state_t ps;
  int tx, rx;
  int aux_stat;
    
  if(timer_get_tics() - last < 1000)
    return 0;

  last = timer_get_tics();

  m_term_clear();

  m_pcprintf(1, 1, C_BLUE, "WR PTP Core Sync Monitor v 0.2");
  m_pcprintf(2, 1, C_GREY, "g = exit, t = enable/disable phase tracking, +/- = increase/decrease phase by 100ps");

  /*show_ports*/
  halexp_get_port_state(&ps, NULL);
  m_pcprintf(4, 1, C_BLUE, "Link status:");

  m_pcprintf(6, 1, C_WHITE, "%s: ", "wru1");
  if(ps.up)  m_cprintf(C_GREEN, "Link up   "); else  m_cprintf(C_RED, "Link down ");

	if(ps.up)
	{
	 minic_get_stats(&tx, &rx);
	 m_cprintf(C_GREY, "(RX: %d, TX: %d), mode: ", rx, tx);

  switch(ps.mode)
  {   
    case HEXP_PORT_MODE_WR_MASTER:m_cprintf(C_WHITE, "WR Master  ");break;
    case HEXP_PORT_MODE_WR_SLAVE:m_cprintf(C_WHITE, "WR Slave   ");break;
  }   

  if(ps.is_locked) m_cprintf(C_GREEN, "Locked  "); else   m_cprintf(C_RED, "NoLock  ");
  if(ps.rx_calibrated  && ps.tx_calibrated) m_cprintf(C_GREEN, "Calibrated  "); else   m_cprintf(C_RED, "Uncalibrated  ");


  /*show_servo*/
  m_cprintf(C_BLUE, "\n\nSynchronization status:\n\n");

  if(!cur_servo_state.valid)
  {
    m_cprintf(C_RED, "Master mode or sync info not valid\n\n");
    return;
  }   

  m_cprintf(C_GREY, "Servo state:               "); m_cprintf(C_WHITE, "%s\n", cur_servo_state.slave_servo_state);
  m_cprintf(C_GREY, "Phase tracking:            "); if(cur_servo_state.tracking_enabled) m_cprintf(C_GREEN, "ON\n"); else m_cprintf(C_RED,"OFF\n");
  m_cprintf(C_GREY, "Synchronization source:    "); m_cprintf(C_WHITE, "%s\n", cur_servo_state.sync_source);

  m_cprintf(C_GREY, "Aux clock status:          "); 
  aux_stat = softpll_get_aux_status();

  if(aux_stat & SOFTPLL_AUX_ENABLED)
  	m_cprintf(C_GREEN,"enabled");

  if(aux_stat & SOFTPLL_AUX_LOCKED)
  	m_cprintf(C_GREEN,", locked");
  mprintf("\n");

  m_cprintf(C_BLUE, "\nTiming parameters:\n\n");

  m_cprintf(C_GREY, "Round-trip time (mu):      "); m_cprintf(C_WHITE, "%d ps\n", (int32_t)(cur_servo_state.mu));
  m_cprintf(C_GREY, "Master-slave delay:        "); m_cprintf(C_WHITE, "%d ps\n", (int32_t)(cur_servo_state.delay_ms));
  m_cprintf(C_GREY, "Master PHY delays:         "); m_cprintf(C_WHITE, "TX: %d ps, RX: %d ps\n", (int32_t)cur_servo_state.delta_tx_m, (int32_t)cur_servo_state.delta_rx_m);
  m_cprintf(C_GREY, "Slave PHY delays:          "); m_cprintf(C_WHITE, "TX: %d ps, RX: %d ps\n", (int32_t)cur_servo_state.delta_tx_s, (int32_t)cur_servo_state.delta_rx_s);
  m_cprintf(C_GREY, "Total link asymmetry:      "); m_cprintf(C_WHITE, "%d ps\n", (int32_t)(cur_servo_state.total_asymmetry));
  m_cprintf(C_GREY, "Clock offset:              "); m_cprintf(C_WHITE, "%d ps\n", (int32_t)(cur_servo_state.cur_offset));
  m_cprintf(C_GREY, "Phase setpoint:            "); m_cprintf(C_WHITE, "%d ps\n", (int32_t)(cur_servo_state.cur_setpoint));
  m_cprintf(C_GREY, "Skew:                      "); m_cprintf(C_WHITE, "%d ps\n", (int32_t)(cur_servo_state.cur_skew));
  m_cprintf(C_GREY, "Manual phase adjustment:   "); m_cprintf(C_WHITE, "%d ps\n", (int32_t)(wrc_man_phase));


    }

 m_cprintf(C_GREY, "--");

  return 0;
}
