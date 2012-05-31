#include <stdio.h>
#include <time.h>

#include "board.h"
#include "ptpd_exports.h"
#include "hal_exports.h"
#include "softpll_ng.h"
#include "pps_gen.h"

extern ptpdexp_sync_state_t cur_servo_state;
extern int wrc_man_phase;

#define C_DIM 0x80
#define C_WHITE 7
#define C_GREY (C_WHITE | C_DIM)                                                                                  
#define C_RED 1
#define C_GREEN 2                                                                                                 
#define C_BLUE 4 

#define 	YEAR0   1900
#define 	EPOCH_YR   1970
#define 	SECS_DAY   (24L * 60L * 60L)
#define 	LEAPYEAR(year)   (!((year) % 4) && (((year) % 100) || !((year) % 400)))
#define 	YEARSIZE(year)   (LEAPYEAR(year) ? 366 : 365)
#define 	FIRSTSUNDAY(timp)   (((timp)->tm_yday - (timp)->tm_wday + 420) % 7)
#define 	FIRSTDAYOF(timp)   (((timp)->tm_wday - (timp)->tm_yday + 420) % 7)
#define 	TIME_MAX   ULONG_MAX
#define 	ABB_LEN   3

static const char *_days[] = {
                      "Sunday", "Monday", "Tuesday", "Wednesday",
                      "Thursday", "Friday", "Saturday"
};

static const char *_months[] = {
                      "January", "February", "March",
                      "April", "May", "June",
                      "July", "August", "September",
                      "October", "November", "December"
};

static const int _ytab[2][12] = {
     { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
     { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
};

static char *format_time(uint32_t utc)
{
  struct tm t;
  static char buf[64];
  unsigned long dayclock, dayno;
  int year = EPOCH_YR;
 
  dayclock = (unsigned long)utc % SECS_DAY;
  dayno = (unsigned long)utc / SECS_DAY;
 
  t.tm_sec = dayclock % 60;
  t.tm_min = (dayclock % 3600) / 60;
  t.tm_hour = dayclock / 3600;
  t.tm_wday = (dayno + 4) % 7;       /* day 0 was a thursday */
  while (dayno >= YEARSIZE(year)) {
    dayno -= YEARSIZE(year);
    year++;
  }
  t.tm_year = year - YEAR0;
  t.tm_yday = dayno;
  t.tm_mon = 0;
  while (dayno >= _ytab[LEAPYEAR(year)][t.tm_mon]) {
    dayno -= _ytab[LEAPYEAR(year)][t.tm_mon];
    t.tm_mon++;
  }
  t.tm_mday = dayno + 1;
  t.tm_isdst = 0;

  sprintf(buf, "%s, %s %d, %d, %2d:%2d:%2d", _days[t.tm_wday], _months[t.tm_mon],
          t.tm_mday, t.tm_year + YEAR0, t.tm_hour, t.tm_min, t.tm_sec);

  return buf;
}

int wrc_mon_gui(void)
{
  static char* slave_states[] = {"Uninitialized", "SYNC_UTC", "SYNC_NSEC", "SYNC_PHASE", "TRACK_PHASE"};
  static uint32_t last = 0;
  hexp_port_state_t ps;
  int tx, rx;
  int aux_stat;
  int32_t utc,nsec;
    
  if(timer_get_tics() - last < 1000)
    return 0;

  last = timer_get_tics();

  m_term_clear();

  m_pcprintf(1, 1, C_BLUE, "WR PTP Core Sync Monitor v 1.0");
  m_pcprintf(2, 1, C_GREY, "Esc = exit");

  pps_gen_get_time(&utc, &nsec);

  m_cprintf(C_BLUE, "\n\nUTC Time:                  ");
  m_cprintf(C_WHITE, "%s", format_time(utc));

  /*show_ports*/
  halexp_get_port_state(&ps, NULL);
  m_pcprintf(4, 1, C_BLUE, "\n\nLink status:");

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

      


      
      /* show_servo */
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

      aux_stat = spll_get_aux_status(0);

      if(aux_stat & SPLL_AUX_ENABLED)
  	m_cprintf(C_GREEN,"enabled");

      if(aux_stat & SPLL_AUX_LOCKED)
  	m_cprintf(C_GREEN,", locked");
      mprintf("\n");

      m_cprintf(C_BLUE, "\nTiming parameters:\n\n");

      m_cprintf(C_GREY, "Round-trip time (mu):      "); m_cprintf(C_WHITE, "%d ps\n", (int32_t)(cur_servo_state.mu));
      m_cprintf(C_GREY, "Master-slave delay:        "); m_cprintf(C_WHITE, "%d ps\n", (int32_t)(cur_servo_state.delay_ms));
      m_cprintf(C_GREY, "Master PHY delays:         "); m_cprintf(C_WHITE, "TX: %d ps, RX: %d ps\n", (int32_t)cur_servo_state.delta_tx_m, (int32_t)cur_servo_state.delta_rx_m);
      m_cprintf(C_GREY, "Slave PHY delays:          "); m_cprintf(C_WHITE, "TX: %d ps, RX: %d ps\n", (int32_t)cur_servo_state.delta_tx_s, (int32_t)cur_servo_state.delta_rx_s);
      m_cprintf(C_GREY, "Total link asymmetry:      "); m_cprintf(C_WHITE, "%d ps\n", (int32_t)(cur_servo_state.total_asymmetry));
      m_cprintf(C_GREY, "Cable rtt delay:           "); m_cprintf(C_WHITE, "%d ps\n", 
                                                                  (int32_t)(cur_servo_state.mu) - (int32_t)cur_servo_state.delta_tx_m - (int32_t)cur_servo_state.delta_rx_m - 
                                                                  (int32_t)cur_servo_state.delta_tx_s - (int32_t)cur_servo_state.delta_rx_s);
      m_cprintf(C_GREY, "Clock offset:              "); m_cprintf(C_WHITE, "%d ps\n", (int32_t)(cur_servo_state.cur_offset));
      m_cprintf(C_GREY, "Phase setpoint:            "); m_cprintf(C_WHITE, "%d ps\n", (int32_t)(cur_servo_state.cur_setpoint));
      m_cprintf(C_GREY, "Skew:                      "); m_cprintf(C_WHITE, "%d ps\n", (int32_t)(cur_servo_state.cur_skew));
      m_cprintf(C_GREY, "Manual phase adjustment:   "); m_cprintf(C_WHITE, "%d ps\n", (int32_t)(wrc_man_phase));

      m_cprintf(C_GREY, "Update counter:            "); m_cprintf(C_WHITE, "%d \n", (int32_t)(cur_servo_state.update_count));


    }

  m_cprintf(C_GREY, "--");

  return 0;
}

int wrc_log_stats(void)
{
  static char* slave_states[] = {"Uninitialized", "SYNC_UTC", "SYNC_NSEC", "SYNC_PHASE", "TRACK_PHASE"};
  static uint32_t last = 0;
  hexp_port_state_t ps;
  int tx, rx;
  int aux_stat;
    
  halexp_get_port_state(&ps, NULL);
  minic_get_stats(&tx, &rx);
  mprintf("lnk:%d rx:%d tx:%d ", ps.up, rx, tx);
  mprintf("lock:%d ", ps.is_locked?1:0);
  mprintf("sv:%d ", cur_servo_state.valid?1:0);
  mprintf("ss:'%s' ", cur_servo_state.slave_servo_state);
  aux_stat = spll_get_aux_status(0);
  mprintf("aux:%x ", aux_stat);
  mprintf("mu:%d ", (int32_t)cur_servo_state.mu);
  mprintf("dms:%d ", (int32_t)cur_servo_state.delay_ms);
  mprintf("dtxm:%d drxm:%d ", (int32_t)cur_servo_state.delta_tx_m, (int32_t)cur_servo_state.delta_rx_m);
  mprintf("dtxs:%d drxs:%d ", (int32_t)cur_servo_state.delta_tx_s, (int32_t)cur_servo_state.delta_rx_s);
  mprintf("asym:%d ", (int32_t)(cur_servo_state.total_asymmetry));
  mprintf("crtt:%d ", 
          (int32_t)(cur_servo_state.mu) - (int32_t)cur_servo_state.delta_tx_m - (int32_t)cur_servo_state.delta_rx_m - 
          (int32_t)cur_servo_state.delta_tx_s - (int32_t)cur_servo_state.delta_rx_s);
  mprintf("cko:%d ", (int32_t)(cur_servo_state.cur_offset));
  mprintf("setp:%d ", (int32_t)(cur_servo_state.cur_setpoint));
  mprintf("hd:%d md:%d ad:%d ",
          spll_get_dac(-1),
          spll_get_dac(0),
          spll_get_dac(1)
          );
  mprintf("ucnt:%d ", (int32_t)cur_servo_state.update_count);

  mprintf("\n");
  return 0;
}
