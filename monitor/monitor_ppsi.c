/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2011,2012 CERN (www.cern.ch)
 * Author: Aurelio Colosimo <aurelio@aureliocolosimo.it>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

#include <inttypes.h>
#include <wrc.h>
#include <ppsi/ppsi.h>
#include <spec.h>
#include <wr-api.h>
#include <minic.h>
#include <softpll_ng.h>
#include <syscon.h>
#include <pps_gen.h>
#include <onewire.h>
#include <util.h>

#define UI_REFRESH_PERIOD TICS_PER_SECOND	/* 1 sec */

struct ptpdexp_sync_state_t;
extern ptpdexp_sync_state_t cur_servo_state;
extern int wrc_man_phase;

void wrc_mon_gui(void)
{
	static uint32_t last = 0;
	hexp_port_state_t ps;
	int tx, rx;
	int aux_stat;
	uint64_t sec;
	uint32_t nsec;

	if (timer_get_tics() - last < UI_REFRESH_PERIOD)
		return;

	last = timer_get_tics();

	term_clear();
	pp_printf("WR PTP Core Sync Monitor v 1.0");
	pp_printf("Esc = exit");

	shw_pps_gen_get_time(&sec, &nsec);

	pp_printf("\n\nTAI Time:                  ");
	pp_printf("%s", format_time(sec));

	/*show_ports */
	halexp_get_port_state(&ps, NULL);
	pp_printf("\n\nLink status:");

	pp_printf("%s: ", "wru1");
	if (ps.up)
		pp_printf("Link up   ");
	else
		pp_printf("Link down ");

	if (ps.up) {
		minic_get_stats(&tx, &rx);
		pp_printf("(RX: %d, TX: %d), mode: ", rx, tx);

		/* FIXME: define HEXP_PORT_MODE_WR_MASTER/SLAVE somewhere
		switch (ps.mode) {
		case HEXP_PORT_MODE_WR_MASTER:
			pp_printf("WR Master  ");
			break;
		case HEXP_PORT_MODE_WR_SLAVE:
			pp_printf("WR Slave   ");
			break;
		}
		*/

		if (ps.is_locked)
			pp_printf("Locked  ");
		else
			pp_printf("NoLock  ");
		if (ps.rx_calibrated && ps.tx_calibrated)
			pp_printf("Calibrated  ");
		else
			pp_printf("Uncalibrated  ");

		/* show_servo */
		pp_printf("\n\nSynchronization status:\n\n");

		if (!cur_servo_state.valid) {
			pp_printf("Master mode or sync info not valid\n\n");
			return;
		}

		pp_printf("Servo state:               ");
		pp_printf("%s\n", cur_servo_state.slave_servo_state);
		pp_printf("Phase tracking:            ");
		if (cur_servo_state.tracking_enabled)
			pp_printf("ON\n");
		else
			pp_printf("OFF\n");
		pp_printf("Synchronization source:    ");
		pp_printf("%s\n", cur_servo_state.sync_source);

		pp_printf("Aux clock status:          ");

		aux_stat = spll_get_aux_status(0);

		if (aux_stat & SPLL_AUX_ENABLED)
			pp_printf("enabled");

		if (aux_stat & SPLL_AUX_LOCKED)
			pp_printf(", locked");
		pp_printf("\n");

		pp_printf("\nTiming parameters:\n\n");

		pp_printf("Round-trip time (mu):      ");
		pp_printf("%d ps\n", (int32_t) (cur_servo_state.mu));
		pp_printf("Master-slave delay:        ");
		pp_printf("%d ps\n",
			(int32_t) (cur_servo_state.delay_ms));
		pp_printf("Master PHY delays:         ");
		pp_printf("TX: %d ps, RX: %d ps\n",
			(int32_t) cur_servo_state.delta_tx_m,
			(int32_t) cur_servo_state.delta_rx_m);
		pp_printf("Slave PHY delays:          ");
		pp_printf("TX: %d ps, RX: %d ps\n",
			(int32_t) cur_servo_state.delta_tx_s,
			(int32_t) cur_servo_state.delta_rx_s);
		pp_printf("Total link asymmetry:      ");
		pp_printf("%d ps\n",
			(int32_t) (cur_servo_state.total_asymmetry));
		pp_printf("Cable rtt delay:           ");
		pp_printf("%d ps\n",
			(int32_t) (cur_servo_state.mu) -
			(int32_t) cur_servo_state.delta_tx_m -
			(int32_t) cur_servo_state.delta_rx_m -
			(int32_t) cur_servo_state.delta_tx_s -
			(int32_t) cur_servo_state.delta_rx_s);
		pp_printf("Clock offset:              ");
		pp_printf("%d ps\n",
			(int32_t) (cur_servo_state.cur_offset));
		pp_printf("Phase setpoint:            ");
		pp_printf("%d ps\n",
			(int32_t) (cur_servo_state.cur_setpoint));
		pp_printf("Skew:                      ");
		pp_printf("%d ps\n",
			(int32_t) (cur_servo_state.cur_skew));
		pp_printf("Manual phase adjustment:   ");
		pp_printf("%d ps\n", (int32_t) (wrc_man_phase));

		pp_printf("Update counter:            ");
		pp_printf("%d \n",
			(int32_t) (cur_servo_state.update_count));

	}

	pp_printf("--");

	return;
}

int wrc_log_stats(uint8_t onetime)
{
	static uint32_t last = 0;
	hexp_port_state_t ps;
	int tx, rx;
	int aux_stat;
	uint64_t sec;
	uint32_t nsec;
	int16_t brd_temp = 0;
	int16_t brd_temp_frac = 0;

	if (!onetime && timer_get_tics() - last < UI_REFRESH_PERIOD)
		return 0;

	last = timer_get_tics();

	shw_pps_gen_get_time(&sec, &nsec);
	halexp_get_port_state(&ps, NULL);
	minic_get_stats(&tx, &rx);
	pp_printf("lnk:%d rx:%d tx:%d ", ps.up, rx, tx);
	pp_printf("lock:%d ", ps.is_locked ? 1 : 0);
	pp_printf("sv:%d ", cur_servo_state.valid ? 1 : 0);
	pp_printf("ss:'%s' ", cur_servo_state.slave_servo_state);
	aux_stat = spll_get_aux_status(0);
	pp_printf("aux:%x ", aux_stat);
	pp_printf("sec:%d nsec:%d ", (uint32_t) sec, nsec);	/* fixme: clock is not always 125 MHz */
	pp_printf("mu:%d ", (int32_t) cur_servo_state.mu);
	pp_printf("dms:%d ", (int32_t) cur_servo_state.delay_ms);
	pp_printf("dtxm:%d drxm:%d ", (int32_t) cur_servo_state.delta_tx_m,
		(int32_t) cur_servo_state.delta_rx_m);
	pp_printf("dtxs:%d drxs:%d ", (int32_t) cur_servo_state.delta_tx_s,
		(int32_t) cur_servo_state.delta_rx_s);
	pp_printf("asym:%d ", (int32_t) (cur_servo_state.total_asymmetry));
	pp_printf("crtt:%d ",
		(int32_t) (cur_servo_state.mu) -
		(int32_t) cur_servo_state.delta_tx_m -
		(int32_t) cur_servo_state.delta_rx_m -
		(int32_t) cur_servo_state.delta_tx_s -
		(int32_t) cur_servo_state.delta_rx_s);
	pp_printf("cko:%d ", (int32_t) (cur_servo_state.cur_offset));
	pp_printf("setp:%d ", (int32_t) (cur_servo_state.cur_setpoint));
	pp_printf("hd:%d md:%d ad:%d ", spll_get_dac(-1), spll_get_dac(0),
		spll_get_dac(1));
	pp_printf("ucnt:%d ", (int32_t) cur_servo_state.update_count);

	own_readtemp(ONEWIRE_PORT, &brd_temp, &brd_temp_frac);
	pp_printf("temp:%d.%02d C", brd_temp, brd_temp_frac);

	pp_printf("\n");
	return 0;
}
