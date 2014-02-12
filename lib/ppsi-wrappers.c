#include <stdint.h>
#include <hal_exports.h>
#include <wrc_ptp.h>
#include <syscon.h>
#include <endpoint.h>
#include <softpll_ng.h>
#include <ptpd_netif.h>

/* Following code from ptp-noposix/libposix/freestanding-wrapper.c */

uint64_t ptpd_netif_get_msec_tics(void)
{
#if TICS_PER_SECOND != 1000
#error "This code assumes 1kHz timer"
#endif
  return timer_get_tics();
}

static int read_phase_val(hexp_port_state_t *state)
{
  int32_t dmtd_phase;

  if(spll_read_ptracker(0, &dmtd_phase, NULL))
  {
    state->phase_val = dmtd_phase;
    state->phase_val_valid = 1;
  }
  else
  {
    state->phase_val = 0;
    state->phase_val_valid = 0;
  }

  return 0;
}

extern uint32_t cal_phase_transition;
extern int32_t sfp_alpha;

int halexp_get_port_state(hexp_port_state_t *state, const char *port_name)
{
  state->valid         = 1;

  if(wrc_ptp_get_mode()==WRC_MODE_SLAVE)
    state->mode = HEXP_PORT_MODE_WR_SLAVE;
  else
    state->mode = HEXP_PORT_MODE_WR_MASTER;

  ep_get_deltas( &state->delta_tx, &state->delta_rx);
  read_phase_val(state);
  state->up            = ep_link_up(NULL);
  state->tx_calibrated = 1;
  state->rx_calibrated = 1;
  state->is_locked     = spll_check_lock(0);
  state->lock_priority = 0;
  spll_get_phase_shift(0, NULL, (int32_t *)&state->phase_setpoint);
  state->clock_period  = 8000;
  state->t2_phase_transition = cal_phase_transition;
  state->t4_phase_transition = cal_phase_transition;
  get_mac_addr(state->hw_addr);
  state->hw_index      = 0;
  state->fiber_fix_alpha = sfp_alpha;

  return 0;
}

int ptpd_netif_get_dmtd_phase(wr_socket_t *sock, int32_t *phase)
{
        if(phase)
                return spll_read_ptracker(0, phase, NULL);
        return 0;
}
