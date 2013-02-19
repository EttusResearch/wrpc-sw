/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Aurelio Colosimo <aurelio@aureliocolosimo.it>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <stdio.h>
#include <inttypes.h>
#include <errno.h>
#include <wrc.h>

#include <ppsi/ppsi.h>
#include <wr-constants.h>
#include "syscon.h"
#include "softpll_ng.h"
#include "wrc_ptp.h"
#include "pps_gen.h"
#include "uart.h"

extern struct pp_runtime_opts default_rt_opts;

static int ptp_enabled = 0, ptp_mode = WRC_MODE_UNKNOWN;
static struct pp_instance ppi_static;
CONST_VERBOSITY int pp_diag_verbosity = 0;

/*ppi fields*/
static UInteger16 sent_seq_id[16];
static DSDefault  defaultDS;
static DSCurrent  currentDS;
static DSParent   parentDS;
static DSPort     portDS;
static DSTimeProperties timePropertiesDS;
static struct pp_net_path net_path;
static struct pp_servo servo;
static struct pp_frgn_master frgn_master;

int wrc_ptp_init()
{
	struct pp_instance *ppi = &ppi_static; /* no malloc, one instance */
	sdb_find_devices();
	uart_init();

	/* FIXME printf? pp_puts("Spec: starting. Compiled on " __DATE__ "\n"); */

	ppi->sent_seq_id = sent_seq_id;
	ppi->defaultDS   = &defaultDS;
	ppi->currentDS   = &currentDS;
	ppi->parentDS    = &parentDS;
	ppi->portDS      = &portDS;
	ppi->timePropertiesDS = &timePropertiesDS;
	ppi->net_path    = &net_path;
	ppi->servo       = &servo;
	ppi->frgn_master = &frgn_master;
	ppi->arch_data   = NULL;

#ifdef FIXME_NET_INIT	
	if (spec_open_ch(ppi)) {
		/* FIXME spec_errno pp_diag_error(ppi, spec_errno); */
		/* FIXME pp_diag_fatal(ppi, "open_ch", ""); */
	}
#endif
	pp_open_instance(ppi, 0 /* no opts */);

#ifdef PPSI_SLAVE
	/* FIXME slave_only */
	OPTS(ppi)->slave_only = 1;
#endif

	/* FIXME main spec_main_loop(ppi); */
	return 0;
}

#define LOCK_TIMEOUT_FM (4 * TICS_PER_SECOND)
#define LOCK_TIMEOUT_GM (60 * TICS_PER_SECOND)

int wrc_ptp_set_mode(int mode)
{
	uint32_t start_tics, lock_timeout = 0;
	struct pp_instance *ppi = &ppi_static;

	ptp_mode = 0;

	wrc_ptp_stop();

	switch (mode) {
	case WRC_MODE_GM:
		/* FIXME multiport rtOpts.primarySource = TRUE; */
		DSPOR(ppi)->wrConfig = WR_M_ONLY;
		/* FIXME multiport? rtOpts.masterOnly = TRUE; */
		spll_init(SPLL_MODE_GRAND_MASTER, 0, 1);
		lock_timeout = LOCK_TIMEOUT_GM;
		break;

	case WRC_MODE_MASTER:
		/* FIXME multiport rtOpts.primarySource = FALSE; */
		DSPOR(ppi)->wrConfig = WR_M_ONLY;
		/* FIXME multiport? rtOpts.masterOnly = TRUE; */
		spll_init(SPLL_MODE_FREE_RUNNING_MASTER, 0, 1);
		lock_timeout = LOCK_TIMEOUT_FM;
		break;

	case WRC_MODE_SLAVE:
		/* FIXME multiport rtOpts.primarySource = FALSE; */
		DSPOR(ppi)->wrConfig = WR_S_ONLY;
		/* FIXME multiport? rtOpts.masterOnly = FALSE; */
		spll_init(SPLL_MODE_SLAVE, 0, 1);
		break;
	}

	start_tics = timer_get_tics();

	mprintf("Locking PLL");

	shw_pps_gen_enable_output(0);

	while (!spll_check_lock(0) && lock_timeout) {
		timer_delay(TICS_PER_SECOND);
		mprintf(".");
		if (timer_get_tics() - start_tics > lock_timeout) {
			mprintf("\nLock timeout.\n");
			return -ETIMEDOUT;
		} else if (uart_read_byte() == 27) {
			mprintf("\n");
			return -EINTR;
		}
	}

	if (mode == WRC_MODE_MASTER || mode == WRC_MODE_GM)
		shw_pps_gen_enable_output(1);

	mprintf("\n");
	ptp_mode = mode;
	return 0;
}

int wrc_ptp_get_mode()
{
	return ptp_mode;
}

int wrc_ptp_start()
{
	struct pp_instance *ppi = &ppi_static;
	DSPOR(ppi)->linkUP = FALSE;
	wr_servo_reset();

	ptp_enabled = 1;
	return 0;
}

int wrc_ptp_stop()
{
	ptp_enabled = 0;
	wr_servo_reset();
	return 0;
}

int wrc_ptp_update()
{
	if (ptp_enabled) {
		/* FIXME singlePortLoop(&rtOpts,  ptpPortDS, 0); */
		/* sharedPortsLoop(ptpPortDS); Questo mi sa che a me ora non serve */
	}
	return 0;
}
