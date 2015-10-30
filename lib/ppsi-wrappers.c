/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2013 - 2015 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 * Author: Adam Wujek <adam.wujek@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <stdint.h>
#include <libwr/hal_shmem.h>
#include <libwr/shmem.h>
#include <wrc_ptp.h>
#include <syscon.h>
#include <endpoint.h>
#include <softpll_ng.h>
#include <ptpd_netif.h>

struct wrs_shm_head *ppsi_head;

/* Following code from ptp-noposix/libposix/freestanding-wrapper.c */

static int read_phase_val(struct hal_port_state *port)
{
	int32_t dmtd_phase;

	if (spll_read_ptracker(0, &dmtd_phase, NULL)) {
		port->phase_val = dmtd_phase;
		port->phase_val_valid = 1;
	} else {
		port->phase_val = 0;
		port->phase_val_valid = 0;
	}

	return 0;
}

extern uint32_t cal_phase_transition;

int wrpc_get_port_state(struct hal_port_state *port, const char *port_name)
{
	if (wrc_ptp_get_mode() == WRC_MODE_SLAVE)
		port->mode = HEXP_PORT_MODE_WR_SLAVE;
	else
		port->mode = HEXP_PORT_MODE_WR_MASTER;

	/* all deltas are added anyway */
	ep_get_deltas(&port->calib.delta_tx_board,
		      &port->calib.delta_rx_board);
	port->calib.delta_tx_phy = 0;
	port->calib.delta_rx_phy = 0;
	port->calib.sfp.delta_tx_ps = 0;
	port->calib.sfp.delta_rx_ps = 0;
	read_phase_val(port);
	port->state = ep_link_up(NULL);
	port->calib.tx_calibrated = 1;
	port->calib.rx_calibrated = 1;
	port->locked = spll_check_lock(0);
	/*  port->lock_priority = 0;*/
	/*spll_get_phase_shift(0, NULL, (int32_t *)&port->phase_setpoint);*/
	port->clock_period  = REF_CLOCK_PERIOD_PS;
	port->t2_phase_transition = cal_phase_transition;
	port->t4_phase_transition = cal_phase_transition;
	get_mac_addr(port->hw_addr);
	port->hw_index      = 0;

	return 0;
}

/* dummy function, no shmem locks (no even shmem) are implemented in wrpc */
void wrs_shm_write(void *headptr, int flags)
{
	return;
}
