/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2011,2012 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 * Author: Grzegorz Daniluk <grzegorz.daniluk@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <stdio.h>
#include <inttypes.h>

#include <stdarg.h>

#include <wrc.h>
#include <w1.h>
#include "syscon.h"
#include "uart.h"
#include "endpoint.h"
#include "minic.h"
#include "pps_gen.h"
#include "ptpd_netif.h"
#include "i2c.h"
#include "storage.h"
#include "softpll_ng.h"
#include "onewire.h"
#include "pps_gen.h"
#include "shell.h"
#include "lib/ipv4.h"
#include "rxts_calibrator.h"
#include "flash.h"

#include "wrc_ptp.h"
#include "system_checks.h"

int wrc_ui_mode = UI_SHELL_MODE;
int wrc_ui_refperiod = TICS_PER_SECOND; /* 1 sec */
int wrc_phase_tracking = 1;

///////////////////////////////////
//Calibration data (from EEPROM if available)
int32_t sfp_alpha = 73622176;	//default values if could not read EEPROM
int32_t sfp_deltaTx = 0;
int32_t sfp_deltaRx = 0;
uint32_t cal_phase_transition = 2389;

static void wrc_initialize(void)
{
	uint8_t mac_addr[6];

	sdb_find_devices();
	uart_init_sw();
	uart_init_hw();

	pp_printf("WR Core: starting up...\n");

	timer_init(1);
	wrpc_w1_init();
	wrpc_w1_bus.detail = ONEWIRE_PORT;
	w1_scan_bus(&wrpc_w1_bus);

	/*initialize flash*/
	flash_init();
	/*initialize I2C bus*/
	mi2c_init(WRPC_FMC_I2C);
	/*init storage (Flash / W1 EEPROM / I2C EEPROM*/
	storage_init(WRPC_FMC_I2C, FMC_EEPROM_ADR);

	if (get_persistent_mac(ONEWIRE_PORT, mac_addr) == -1) {
		pp_printf("Unable to determine MAC address\n");
		mac_addr[0] = 0x22;	//
		mac_addr[1] = 0x33;	//
		mac_addr[2] = 0x44;	// fallback MAC if get_persistent_mac fails
		mac_addr[3] = 0x55;	//
		mac_addr[4] = 0x66;	//
		mac_addr[5] = 0x77;	//
	}

	pp_printf("Local MAC address: %02x:%02x:%02x:%02x:%02x:%02x\n",
		mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3],
		mac_addr[4], mac_addr[5]);

	ep_init(mac_addr);
	ep_enable(1, 1);

	minic_init();
	shw_pps_gen_init();
	wrc_ptp_init();
	//try reading t24 phase transition from EEPROM
	calib_t24p(WRC_MODE_MASTER, &cal_phase_transition);
	spll_very_init();
	usleep_init();
	shell_init();

	wrc_ui_mode = UI_SHELL_MODE;
	_endram = ENDRAM_MAGIC;

	wrc_ptp_set_mode(WRC_MODE_SLAVE);
	wrc_ptp_start();
}

int link_status;

static int wrc_check_link(void)
{
	static int prev_state = -1;
	int state = ep_link_up(NULL);
	int rv = 0;

	if (!prev_state && state) {
		wrc_verbose("Link up.\n");
		gpio_out(GPIO_LED_LINK, 1);
		link_status = LINK_WENT_UP;
		rv = 1;
	} else if (prev_state && !state) {
		wrc_verbose("Link down.\n");
		gpio_out(GPIO_LED_LINK, 0);
		link_status = LINK_WENT_DOWN;
		rv = 1;
		/* special case */
		spll_init(SPLL_MODE_FREE_RUNNING_MASTER, 0, 1);
		shw_pps_gen_enable_output(0);

	} else
		link_status = (state ? LINK_UP : LINK_DOWN);
	prev_state = state;

	return rv;
}

int wrc_man_phase = 0;

static int ui_update(void)
{
	int ret;

	if (wrc_ui_mode == UI_GUI_MODE) {
		ret = wrc_mon_gui();
		if (uart_read_byte() == 27 || wrc_ui_refperiod == 0) {
			shell_init();
			wrc_ui_mode = UI_SHELL_MODE;
		}
	} else {
		ret = shell_interactive();
	}
	return ret;
}

/* initialize functions to be called after reset in check_reset function */
void init_hw_after_reset(void)
{
	/* Ok, now init the devices so we can printf and delay */
	sdb_find_devices();
	uart_init_sw();
	uart_init_hw();
	timer_init(1);
}

struct wrc_task wrc_tasks[] = {
	{
		.name = "idle",
		.init = wrc_initialize,
	}, {
		.name = "check-link",
		.job = wrc_check_link,
	}, {
		.name = "net-bh",
		.enable = &link_status,
		.job = update_rx_queues,
	}, {
		.name = "ptp",
		.job = wrc_ptp_update,
	}, {
		.name = "shell+gui",
		.init = shell_boot_script,
		.job = ui_update,
	}, {
		.name = "stats",
		.job = wrc_log_stats,
	}, {
		.name = "spll-bh",
		.job = spll_update,
#ifdef CONFIG_IP
	}, {
		.name = "ipv4",
		.enable = &link_status,
		.init = ipv4_init,
		.job = ipv4_poll,
	}, {
		.name = "arp",
		.enable = &link_status,
		.init = arp_init,
		.job = arp_poll,
#endif
	}
};

int wrc_n_tasks = ARRAY_SIZE(wrc_tasks);

static void wrc_run_task(struct wrc_task *t)
{
	if (t->enable && !*t->enable)
		return;
	if (t->job)
	    t->nrun += t->job();
	else t->nrun++;
}

int main(void)
{
	extern uint32_t uptime_sec;
	uint32_t j, lastj, fraction = 0;
	int i;

	check_reset();

	/* initialization of individual tasks */
	for (i = 0; i < wrc_n_tasks; i++)
		if (wrc_tasks[i].init)
			wrc_tasks[i].init();

	lastj = timer_get_tics();
	for (;;) {
		/* count uptime, in seconds, for remote polling */
		j = timer_get_tics();
		fraction += j -lastj;
		lastj = j;
		while (fraction > TICS_PER_SECOND) {
			fraction -= TICS_PER_SECOND;
			uptime_sec++;
		}

		/* run your tasks */
		for (i = 0; i < wrc_n_tasks; i++)
			wrc_run_task(wrc_tasks + i);

		/* better safe than sorry */
		check_stack();
	}
}
