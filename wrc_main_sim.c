/* ****************************************************************************
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2017 CERN (www.cern.ch)
 * Author: Grzegorz Daniluk <grzegorz.daniluk@cern.ch>
 * Author: Maciej Lipinski <maciej.lipinski@cern.ch>
 * Author: Adam Wujek <adam.wujek@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 *
 * ****************************************************************************
 * Description:
 * This file provides the main() function that replaces the main function of
 * wrc_main.c file when WRPC is compiled for testbench. This is done by
 * configuring WRPC using make menuconfig:
 * a) by using the default wrpc_sim_defconfig, i.e. "make wrpc_sim_defconfig",or
 * b) by chosing in make menuconfig the following opton:
 *    "Build simple software for test of WR PTP Core in simulatin"
 *
 * When compiled for testbench simulation, the main() function of this file is
 * used. This file includes all the tests needed for different testbenches
 * (so far only one...)
 * ****************************************************************************
 */

#include <stdio.h>
#include <inttypes.h>

#include "system_checks.h"
#include "endpoint.h"
#include "minic.h"
#include "pps_gen.h"
#include "softpll_ng.h"
#include <wrpc.h> /*needed for htons()*/

#define TESTBENCH_MAGIC 0x4d433ebc
#define TESTBENCH_VERSION 1
#define TESTBENCH_RET_NO_TEST 0
#define TESTBENCH_RET_OK 1
#define TESTBENCH_RET_ERROR 2


/*
 * This is a structure to pass information from the testbench to lm32's
 * software. hdl_testbench structure is meant to be set by testbench through
 * memory-manipulation.
 *
 * At the address HDL_TESTBENCH_PADDR is a pointer to hdl_testbench structure.
 * hdl_testbench_t structure can be expanded to carry information with any
 * size.
 *
 * The idea behind is that different tests for different testbenches are
 * compiled in this file, and the proper test is loaded based on variable 
 * hdl_testbench.test_num.
 * Each HDL testbench sets the proper testbench number at the startup.
 */

struct hdl_testbench_t {
	uint32_t magic;
	uint32_t version;
	uint32_t test_num;
	uint32_t return_val;
};

struct hdl_testbench_t hdl_testbench = {
	.magic = TESTBENCH_MAGIC,
	.version = TESTBENCH_VERSION,
	.test_num = 0,
};

int wrpc_test_1(void);

/*
 * Basic initialization required for simulation
 */
static void wrc_sim_initialize(void)
{
	uint8_t mac_addr[6];

	// Search SDB for devices takes too long, hard-coded offsets
	// should work for most WR-PTP core implementations
	// (unless you add/remove/move peripherals), in which case
	// you should comment out all the hard-coded offsets and
	// uncomment the following line to perform a dynamic search
	// at runtime.
	//sdb_find_devices();
	BASE_MINIC         = (void *)0x20000;
	BASE_EP            = (void *)0x20100;
	BASE_SOFTPLL       = (void *)0x20200;
	BASE_PPS_GEN       = (void *)0x20300;
	BASE_SYSCON        = (void *)0x20400;
	BASE_UART          = (void *)0x20500;
	BASE_ONEWIRE       = (void *)0x20600;
	BASE_ETHERBONE_CFG = (void *)0x20700;

	timer_init(1);

	/* Source MAC used by WRPC's Endpoint */
	mac_addr[0] = 0xDE;
	mac_addr[1] = 0xAD;
	mac_addr[2] = 0xBE;
	mac_addr[3] = 0xEF;
	mac_addr[4] = 0xBA;
	mac_addr[5] = 0xBE;

	ep_init(mac_addr);
	ep_enable(1, 1);

	minic_init();
	shw_pps_gen_init();
	spll_very_init();
	shw_pps_gen_enable_output(1);
}
/*
 * This is a test used by:
 * - WRPC testbench located in "testbench/wrc_core" folder of the wr-cores
 *   repository (git://ohwr.org/hdl-core-lib/wr-cores.git)
 *
 * This test:
 * - sends min-size frames of PTP EtherType (0x88f7) and Dst MAC
 *   (01:1B:19:00:00:00) with embedded sequence number and flags providing
 *   information about the previously received frame
 * - checkes whether the transmitted frames are successfully received back in a
 *   proper sequence (it is supposed that the testbench loops back the frames)
 * - embeds in the next frame flag and return value regarding the previously
 *   received frames, i.e.:
 *   # the flags say:
 *     0xAA: this is the first frame (no previous frames)
 *     0xBB: previous frame was successfully received
 *     0xE*: something was wrong with the previously received frame
 *   # return value - it is the value returned by the reception funcation
 *
 */
int wrpc_test_1(void)
{
	struct hw_timestamp hwts;
	struct wr_ethhdr_vlan tx_hdr;
	struct wr_ethhdr rx_hdr;
	int j;
	uint8_t tx_payload[NET_MAX_SKBUF_SIZE - 32];
	uint8_t rx_payload[NET_MAX_SKBUF_SIZE - 32];
	int ret = 0;
	int tx_cnt = 0, rx_cnt = 0, pl_cnt;
	/* error code:
	 * 0xAA - first
	 * 0xBB - normal
	 * 0xE* - error code:
	 *    0 - Error: returned zero value
	 *    1 - Error: wrong seqID
	 *    2 - Error: error of rx */
	int code = 0xAA;

	/** prepare dummy frame */
	/* payload */
	for (j = 0; j < 80; ++j)
		tx_payload[j] = 0xC7;/* j; */

	/* MAC address and EtherType of PTP */
	memcpy(tx_hdr.dstmac, "\x01\x1B\x19\x00\x00\x00", 6);
	tx_hdr.ethtype = htons(0x88f7);

	hdl_testbench.return_val = TESTBENCH_RET_OK;
	/** main loop, send test frames */
	for (;;) {
		/* seqID */
		tx_payload[0] = 0xFF & (tx_cnt >> 8);
		tx_payload[1] = 0xFF & tx_cnt;

		tx_payload[2] = 0xFF & (code >> 8);
		tx_payload[3] = 0xFF & code;

		/* rx return value */
		tx_payload[4] = 0xFF & (ret >> 8);
		tx_payload[5] = 0xFF & ret;

		/* A frame is sent out with sequenceID (firt octet) and awaited
		 * reception. */
		minic_tx_frame(&tx_hdr, tx_payload, 62, &hwts);
		tx_cnt++;
		ret = minic_rx_frame(&rx_hdr, rx_payload, NET_MAX_SKBUF_SIZE,
				&hwts);

		/** check whether the received value is OK */
		if (ret == 0) {
			code = 0xE0; /* Error: returned zero value */
			hdl_testbench.return_val = TESTBENCH_RET_ERROR;
		}
		else if (ret > 0) {
			pl_cnt = 0xFFFF & ((tx_payload[0] << 8) | tx_payload[1]);
			if (pl_cnt == rx_cnt) {
				rx_cnt++;
				code = 0xBB; /* OK */
				hdl_testbench.return_val = TESTBENCH_RET_OK;
			} else {
				rx_cnt = pl_cnt+1;
				code = 0xE1; /* Error: wrong seqID */
				hdl_testbench.return_val = TESTBENCH_RET_ERROR;
			}
		} else {
			code = 0xE2; /* Error: error of rx */
			hdl_testbench.return_val = TESTBENCH_RET_ERROR;
		}
	}

}

void main(void)
{
	wrc_sim_initialize();

	if (hdl_testbench.magic != TESTBENCH_MAGIC
	    || hdl_testbench.magic != TESTBENCH_VERSION) {
		/* Wrong testbench structure */
		hdl_testbench.return_val = TESTBENCH_RET_ERROR;
		while (1)
			;
	}
	switch (hdl_testbench.test_num) {
	case 0:
		/* for simulations that just need link-up */
		hdl_testbench.return_val = TESTBENCH_RET_NO_TEST;
		while (1)
			;
	case 1:
		wrpc_test_1();
		break;
	default:
		/* Wrong test number */
		hdl_testbench.return_val = TESTBENCH_RET_ERROR;
		while (1)
			;
	}
}
