/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <stdio.h>
#include <inttypes.h>
#include <errno.h>
#include <wrc.h>

#include "ptpd.h"
#include "ptpd_netif.h"
#include "syscon.h"
#include "softpll_ng.h"
#include "wrc_ptp.h"
#include "pps_gen.h"
#include "uart.h"

static RunTimeOpts rtOpts = {
	.ifaceName = {"wr1"},
	.announceInterval = DEFAULT_ANNOUNCE_INTERVAL,
	.syncInterval = DEFAULT_SYNC_INTERVAL,
	.clockQuality.clockAccuracy = DEFAULT_CLOCK_ACCURACY,
	.clockQuality.clockClass = DEFAULT_CLOCK_CLASS,
	.clockQuality.offsetScaledLogVariance = DEFAULT_CLOCK_VARIANCE,
	.priority1 = DEFAULT_PRIORITY1,
	.priority2 = DEFAULT_PRIORITY2,
	.domainNumber = DEFAULT_DOMAIN_NUMBER,
	.currentUtcOffset = DEFAULT_UTC_OFFSET,
	.noResetClock = DEFAULT_NO_RESET_CLOCK,
	.noAdjust = NO_ADJUST,
	.inboundLatency.nanoseconds = DEFAULT_INBOUND_LATENCY,
	.outboundLatency.nanoseconds = DEFAULT_OUTBOUND_LATENCY,
	.s = DEFAULT_DELAY_S,
	.ap = DEFAULT_AP,
	.ai = DEFAULT_AI,
	.max_foreign_records = DEFAULT_MAX_FOREIGN_RECORDS,

   /**************** White Rabbit *************************/
	.autoPortDiscovery = FALSE,	/*if TRUE: automagically discovers how many ports we have (and how many up-s); else takes from .portNumber */
	.portNumber = 1,
	.calPeriod = WR_DEFAULT_CAL_PERIOD,
	.E2E_mode = TRUE,
	.wrStateRetry = WR_DEFAULT_STATE_REPEAT,
	.wrStateTimeout = WR_DEFAULT_STATE_TIMEOUT_MS,
	.phyCalibrationRequired = FALSE,
	.disableFallbackIfWRFails = TRUE,

	.primarySource = FALSE,
	.wrConfig = WR_S_ONLY,
	.masterOnly = FALSE,

   /********************************************************/
};

static PtpPortDS *ptpPortDS;
static PtpClockDS ptpClockDS;
static int ptp_enabled = 0, ptp_mode = WRC_MODE_UNKNOWN;

int wrc_ptp_init()
{
	Integer16 ret;

	netStartup();

	ptpPortDS = ptpdStartup(0, NULL, &ret, &rtOpts, &ptpClockDS);
	initDataClock(&rtOpts, &ptpClockDS);

	//initialize sockets
	if (!netInit(&ptpPortDS->netPath, &rtOpts, ptpPortDS)) {
		PTPD_TRACE(TRACE_WRPC, NULL, "failed to initialize network\n");
		return -1;
	}

	ptpPortDS->linkUP = FALSE;
	ptp_enabled = 0;
	return 0;
}

#define LOCK_TIMEOUT_FM (4 * TICS_PER_SECOND)
#define LOCK_TIMEOUT_GM (60 * TICS_PER_SECOND)

int wrc_ptp_set_mode(int mode)
{
	uint32_t start_tics, lock_timeout = 0;

	ptp_mode = 0;

	wrc_ptp_stop();

	switch (mode) {
	case WRC_MODE_GM:
		rtOpts.primarySource = TRUE;
		rtOpts.wrConfig = WR_M_ONLY;
		rtOpts.masterOnly = TRUE;
		spll_init(SPLL_MODE_GRAND_MASTER, 0, 1);
		lock_timeout = LOCK_TIMEOUT_GM;
		break;

	case WRC_MODE_MASTER:
		rtOpts.primarySource = FALSE;
		rtOpts.wrConfig = WR_M_ONLY;
		rtOpts.masterOnly = TRUE;
		spll_init(SPLL_MODE_FREE_RUNNING_MASTER, 0, 1);
		lock_timeout = LOCK_TIMEOUT_FM;
		break;

	case WRC_MODE_SLAVE:
		rtOpts.primarySource = FALSE;
		rtOpts.wrConfig = WR_S_ONLY;
		rtOpts.masterOnly = FALSE;
		spll_init(SPLL_MODE_SLAVE, 0, 1);
		break;
	}

	initDataClock(&rtOpts, &ptpClockDS);

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
	ptpPortDS->linkUP = FALSE;
	wr_servo_reset();
	initDataClock(&rtOpts, &ptpClockDS);

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
		singlePortLoop(&rtOpts, ptpPortDS, 0);
		sharedPortsLoop(ptpPortDS);
	}
	return 0;
}
