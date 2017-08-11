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
#include <temperature.h>
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
char wrc_hw_name[HW_NAME_LENGTH];

uint32_t cal_phase_transition = 2389;

int wrc_vlan_number = CONFIG_VLAN_NR;

static uint32_t prev_nanos_for_profile;
static uint32_t prev_ticks_for_profile;
#define DEFAULT_PRINT_TASK_TIME_THRESHOLD 0
uint32_t print_task_time_threshold = DEFAULT_PRINT_TASK_TIME_THRESHOLD;

static void wrc_initialize(void)
{
	uint8_t mac_addr[6];

	sdb_find_devices();
	uart_init_hw();

	pp_printf("WR Core: starting up...\n");

	timer_init(1);
	get_hw_name(wrc_hw_name);
	storage_read_hdl_cfg();
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
		mac_addr[0] = 0x22;	/*
		mac_addr[1] = 0x33;	*
		mac_addr[2] = 0x44;	* fallback MAC if get_persistent_mac fails
		mac_addr[3] = 0x55;	*
		mac_addr[4] = 0x66;	*
		mac_addr[5] = 0x77;	*/
	}

	pp_printf("Local MAC address: %02x:%02x:%02x:%02x:%02x:%02x\n",
		mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3],
		mac_addr[4], mac_addr[5]);

	net_rst();
	ep_init(mac_addr);
	/* Sleep for 1s to make sure WRS v4.2 always realizes that
	 * the link is down */
	timer_delay_ms(200);
	ep_enable(1, 1);

	minic_init();
	shw_pps_gen_init();
	wrc_ptp_init();
	/* try reading t24 phase transition from EEPROM */
	calib_t24p(WRC_MODE_MASTER, &cal_phase_transition);
	spll_very_init();
	usleep_init();
	shell_init();

	wrc_ui_mode = UI_SHELL_MODE;
	_endram = ENDRAM_MAGIC;

	wrc_ptp_set_mode(WRC_MODE_SLAVE);
	wrc_ptp_start();
	shw_pps_gen_get_time(NULL, &prev_nanos_for_profile);
	/* get tics */
	prev_ticks_for_profile = timer_get_tics();
}

DEFINE_WRC_TASK0(idle) = {
	.name = "idle",
	.init = wrc_initialize,
};

int link_status;

static int wrc_check_link(void)
{
	static int prev_state = 0;
	int state = ep_link_up(NULL);
	int rv = 0;

	if (!prev_state && state) {
		wrc_verbose("Link up.\n");
		gpio_out(GPIO_LED_LINK, 1);
		sfp_match();
		wrc_ptp_start();
		link_status = LINK_WENT_UP;
		rv = 1;
	} else if (prev_state && !state) {
		wrc_verbose("Link down.\n");
		gpio_out(GPIO_LED_LINK, 0);
		link_status = LINK_WENT_DOWN;
		wrc_ptp_stop();
		rv = 1;
		/* special case */
		spll_init(SPLL_MODE_FREE_RUNNING_MASTER, 0, 1);
		shw_pps_gen_enable_output(0);

	} else
		link_status = (state ? LINK_UP : LINK_DOWN);
	prev_state = state;

	return rv;
}
DEFINE_WRC_TASK(link) = {
	.name = "check-link",
	.job = wrc_check_link,
};

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
	uart_init_hw();
	timer_init(1);
}

/* count uptime, in seconds, for remote polling */
static uint32_t uptime_lastj;
static void init_uptime(void)
{
	uptime_lastj = timer_get_tics();
}
static int update_uptime(void)
{
	extern uint32_t uptime_sec;
	uint32_t j;
	static uint32_t fraction = 0;

	j = timer_get_tics();
	fraction += j - uptime_lastj;
	uptime_lastj = j;
	if (fraction > TICS_PER_SECOND) {
		fraction -= TICS_PER_SECOND;
		uptime_sec++;
		return 1;
	}
	return 0;
}
DEFINE_WRC_TASK(uptime) = {
	.name = "uptime",
	.init = init_uptime,
	.job = update_uptime,
};

DEFINE_WRC_TASK(ptp) = {
	.name = "ptp",
	.job = wrc_ptp_update,
};
DEFINE_WRC_TASK(shell) = {
	.name = "shell+gui",
	.init = shell_boot_script,
	.job = ui_update,
};
DEFINE_WRC_TASK(spll) = {
	.name = "spll-bh",
	.job = spll_update,
};

static void task_time_normalize(struct wrc_task *t)
{
	if (t->nanos > 1000 * 1000 * 1000) {
		t->nanos -= 1000 * 1000 * 1000;
		t->seconds++;
	}
}

/* Account the time to either this task or task 0 */
static void account_task(struct wrc_task *t, int done_sth)
{
	uint32_t nanos;
	signed int delta;
	uint32_t ticks;
	signed int delta_ticks;

	if (!done_sth)
		t = __task_begin; /* task 0 is special */
	shw_pps_gen_get_time(NULL, &nanos);
	/* get monotonic number of ticks */
	ticks = timer_get_tics();

	delta = nanos - prev_nanos_for_profile;
	if (delta < 0)
		delta += 1000 * 1000 * 1000;

	t->nanos += delta;
	task_time_normalize(t);
	prev_nanos_for_profile = nanos;

	delta_ticks = ticks - prev_ticks_for_profile;
	if (delta_ticks < 0)
		delta_ticks += TICS_PER_SECOND;

	if (t->max_run_ticks < delta_ticks) {/* update max_run_ticks */
		if (print_task_time_threshold) {
			/* Print only if threshold is set */
			pp_printf("New max run time for a task %s, old %ld, "
				  "new %d\n",
				  t->name, t->max_run_ticks, delta_ticks);
		}
		t->max_run_ticks = delta_ticks;
	}
	if (print_task_time_threshold
            && delta_ticks > print_task_time_threshold)
		pp_printf("task %s, run for %d ms\n", t->name, delta_ticks);

	prev_ticks_for_profile = ticks;
}

/* Run a task with profiling */
static void wrc_run_task(struct wrc_task *t)
{
	int done_sth = 0;

	if (!t->job) /* idle task, just count iterations */
		t->nrun++;
	else if (!t->enable || *t->enable) {
		/* either enabled or without a check variable */
		done_sth = t->job();
		t->nrun += done_sth;
	}
	account_task(t, done_sth);
}

int main(void) __attribute__ ((weak));
int main(void)
{
	struct wrc_task *t;

	check_reset();

	/* initialization of individual tasks */
	for_each_task(t)
		if (t->init)
			t->init();

	for (;;) {
		for_each_task(t)
			wrc_run_task(t);

		/* better safe than sorry */
		check_stack();
	}
}
