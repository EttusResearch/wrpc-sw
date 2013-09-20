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
#include <w1.h>
#include <ppsi/ppsi.h>
#include <wrpc.h>
#include <wr-api.h>
#include <minic.h>
#include <softpll_ng.h>
#include <syscon.h>
#include <pps_gen.h>
#include <onewire.h>
#include <util.h>
#include "wrc_ptp.h"
#include "hal_exports.h"
#include "lib/ipv4.h"

#define UI_REFRESH_PERIOD TICS_PER_SECOND	/* 1 sec */

struct ptpdexp_sync_state_t;
extern ptpdexp_sync_state_t cur_servo_state;
extern int wrc_man_phase;

extern struct pp_servo servo;
extern struct pp_instance ppi_static;
struct pp_instance *ppi = &ppi_static;

static void wrc_mon_std_servo(void);

#define PRINT64_FACTOR	1000000000LL
char* print64(uint64_t x)
{
	uint32_t h_half, l_half;
	static char buf[2*10+1];	//2x 32-bit value + \0
	if (x < PRINT64_FACTOR)
		sprintf(buf, "%u", (uint32_t)x);
	else{
		l_half = __div64_32(&x, PRINT64_FACTOR);
		h_half = (uint32_t) x;
		sprintf(buf, "%u%u", h_half, l_half);
	}
	return buf;

}

int wrc_mon_status()
{
	struct pp_state_table_item *ip = NULL;
	for (ip = pp_state_table; ip->state != PPS_END_OF_TABLE; ip++) {
		if (ip->state == ppi->state)
			break;
	}

	cprintf(C_BLUE, "\n\nPTP status: ");
	cprintf(C_WHITE, "%s", ip ? ip->name : "unknown");

	if ((!cur_servo_state.valid) || (ppi->state != PPS_SLAVE)) {
		cprintf(C_RED,
			"\n\nSync info not valid\n\n");
		return 0;
	}

	/* show_servo */
	cprintf(C_BLUE, "\n\nSynchronization status:\n\n");

	return 1;
}

void wrc_mon_gui(void)
{
	static uint32_t last = 0;
	hexp_port_state_t ps;
	int tx, rx;
	int aux_stat;
	uint64_t sec;
	uint32_t nsec;
#ifdef CONFIG_ETHERBONE
	uint8_t ip[4];
#endif

	if (timer_get_tics() - last < UI_REFRESH_PERIOD)
		return;

	last = timer_get_tics();

	term_clear();

	pcprintf(1, 1, C_BLUE, "WR PTP Core Sync Monitor v 1.0");
	pcprintf(2, 1, C_GREY, "Esc = exit");

	shw_pps_gen_get_time(&sec, &nsec);

	cprintf(C_BLUE, "\n\nTAI Time:                  ");
	cprintf(C_WHITE, "%s", format_time(sec));

	/*show_ports */
	halexp_get_port_state(&ps, NULL);
	pcprintf(4, 1, C_BLUE, "\n\nLink status:");

	pcprintf(6, 1, C_WHITE, "%s: ", "wru1");
	if (ps.up)
		cprintf(C_GREEN, "Link up   ");
	else
		cprintf(C_RED, "Link down ");

	if (ps.up) {
		minic_get_stats(&tx, &rx);
		cprintf(C_GREY, "(RX: %d, TX: %d), mode: ", rx, tx);

		if (!WR_DSPOR(ppi)->wrModeOn) {
			wrc_mon_std_servo();
			return;
		}

		switch (ptp_mode) {
		case WRC_MODE_GM:
		case WRC_MODE_MASTER:
			cprintf(C_WHITE, "WR Master  ");
			break;
		case WRC_MODE_SLAVE:
			cprintf(C_WHITE, "WR Slave   ");
			break;
		default:
			cprintf(C_RED, "WR Unknown   ");
		}

		if (ps.is_locked)
			cprintf(C_GREEN, "Locked  ");
		else
			cprintf(C_RED, "NoLock  ");
		if (ps.rx_calibrated && ps.tx_calibrated)
			cprintf(C_GREEN, "Calibrated  ");
		else
			cprintf(C_RED, "Uncalibrated  ");
#ifdef CONFIG_ETHERBONE
		cprintf(C_WHITE, "\nIPv4: ");
		getIP(ip);
		if (needIP)
			cprintf(C_RED, "BOOTP running");
		else
			cprintf(C_GREEN, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
#endif

		if (wrc_mon_status() == 0)
			return;

		cprintf(C_GREY, "Servo state:               ");
		cprintf(C_WHITE, "%s\n", cur_servo_state.slave_servo_state);
		cprintf(C_GREY, "Phase tracking:            ");
		if (cur_servo_state.tracking_enabled)
			cprintf(C_GREEN, "ON\n");
		else
			cprintf(C_RED, "OFF\n");
		cprintf(C_GREY, "Synchronization source:    ");
		cprintf(C_WHITE, "%s\n", cur_servo_state.sync_source);

		cprintf(C_GREY, "Aux clock status:          ");

		aux_stat = spll_get_aux_status(0);

		if (aux_stat & SPLL_AUX_ENABLED)
			cprintf(C_GREEN, "enabled");

		if (aux_stat & SPLL_AUX_LOCKED)
			cprintf(C_GREEN, ", locked");
		mprintf("\n");

		cprintf(C_BLUE, "\nTiming parameters:\n\n");

		cprintf(C_GREY, "Round-trip time (mu):    ");
		cprintf(C_WHITE, "%s ps\n", print64(cur_servo_state.mu));
		cprintf(C_GREY, "Master-slave delay:      ");
		cprintf(C_WHITE, "%s ps\n", print64(cur_servo_state.delay_ms));
		cprintf(C_GREY, "Master PHY delays:       ");
		cprintf(C_WHITE, "TX: %d ps, RX: %d ps\n",
			(int32_t) cur_servo_state.delta_tx_m,
			(int32_t) cur_servo_state.delta_rx_m);
		cprintf(C_GREY, "Slave PHY delays:        ");
		cprintf(C_WHITE, "TX: %d ps, RX: %d ps\n",
			(int32_t) cur_servo_state.delta_tx_s,
			(int32_t) cur_servo_state.delta_rx_s);
		cprintf(C_GREY, "Total link asymmetry:    ");
		cprintf(C_WHITE, "%9d ps\n",
			(int32_t) (cur_servo_state.total_asymmetry));
		cprintf(C_GREY, "Cable rtt delay:         ");
		cprintf(C_WHITE, "%9d ps\n", print64(cur_servo_state.mu -
					cur_servo_state.delta_tx_m -
					cur_servo_state.delta_rx_m -
					cur_servo_state.delta_tx_s -
					cur_servo_state.delta_rx_s));
		cprintf(C_GREY, "Clock offset:            ");
		cprintf(C_WHITE, "%9d ps\n",
			(int32_t) (cur_servo_state.cur_offset));
		cprintf(C_GREY, "Phase setpoint:          ");
		cprintf(C_WHITE, "%9d ps\n",
			(int32_t) (cur_servo_state.cur_setpoint));
		cprintf(C_GREY, "Skew:                    ");
		cprintf(C_WHITE, "%9d ps\n",
			(int32_t) (cur_servo_state.cur_skew));
		cprintf(C_GREY, "Manual phase adjustment: ");
		cprintf(C_WHITE, "%9d ps\n", (int32_t) (wrc_man_phase));

		cprintf(C_GREY, "Update counter:          ");
		cprintf(C_WHITE, "%9d \n",
			(int32_t) (cur_servo_state.update_count));

	}

	pp_printf("--");

	return;
}

static inline void cprintf_ti(int color, struct TimeInternal *ti)
{
	if ((ti->seconds > 0) ||
		((ti->seconds == 0) && (ti->nanoseconds >= 0)))
		cprintf(color, "%2i.%09i s", ti->seconds, ti->nanoseconds);
	else {
		if (ti->seconds == 0)
			cprintf(color, "-%i.%09i s", ti->seconds, -ti->nanoseconds);
		else
			cprintf(color, "%2i.%09i s", ti->seconds, -ti->nanoseconds);
	}

}

static void wrc_mon_std_servo(void)
{
	cprintf(C_RED, "WR Off");

	if (wrc_mon_status() == 0)
		return;

	cprintf(C_GREY, "Clock offset:                 ");

	if (DSCUR(ppi)->offsetFromMaster.seconds)
		cprintf_ti(C_WHITE, &DSCUR(ppi)->offsetFromMaster);
	else {
		cprintf(C_WHITE, "%9i ns", DSCUR(ppi)->offsetFromMaster.nanoseconds);

		cprintf(C_GREY, "\nOne-way delay averaged:       ");
		cprintf(C_WHITE, "%9i ns", DSCUR(ppi)->oneWayDelay.nanoseconds);

		cprintf(C_GREY, "\nObserved drift:               ");
		cprintf(C_WHITE, "%9i ns", SRV(ppi)->obs_drift);
	}
}


int wrc_log_stats(uint8_t onetime)
{
	static uint32_t last = 0;
	hexp_port_state_t ps;
	int tx, rx;
	int aux_stat;
	uint64_t sec;
	uint32_t nsec;

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
	pp_printf("mu:%s ", print64(cur_servo_state.mu));
	pp_printf("dms:%s ", print64(cur_servo_state.delay_ms));
	pp_printf("dtxm:%d drxm:%d ", (int32_t) cur_servo_state.delta_tx_m,
		(int32_t) cur_servo_state.delta_rx_m);
	pp_printf("dtxs:%d drxs:%d ", (int32_t) cur_servo_state.delta_tx_s,
		(int32_t) cur_servo_state.delta_rx_s);
	pp_printf("asym:%d ", (int32_t) (cur_servo_state.total_asymmetry));
	pp_printf("crtt:%s ", print64(cur_servo_state.mu -
				cur_servo_state.delta_tx_m -
				cur_servo_state.delta_rx_m -
				cur_servo_state.delta_tx_s -
				cur_servo_state.delta_rx_s));
	pp_printf("cko:%d ", (int32_t) (cur_servo_state.cur_offset));
	pp_printf("setp:%d ", (int32_t) (cur_servo_state.cur_setpoint));
	pp_printf("hd:%d md:%d ad:%d ", spll_get_dac(-1), spll_get_dac(0),
		spll_get_dac(1));
	pp_printf("ucnt:%d ", (int32_t) cur_servo_state.update_count);

#ifdef CONFIG_SOCKITOWM
	{
		int16_t brd_temp = 0;
		int16_t brd_temp_frac = 0;

		own_readtemp(ONEWIRE_PORT, &brd_temp, &brd_temp_frac);
		pp_printf("temp:%d.%02d C", brd_temp, brd_temp_frac);
	}
#else
	{
		int32_t temp;

		temp = w1_read_temp_bus(&wrpc_w1_bus, 0);
		pp_printf("temp: %d.%04d C", temp >> 16,
			  (int)((temp & 0xffff) * 10 * 1000 >> 16));
	}
#endif

	pp_printf("\n");
	return 0;
}
