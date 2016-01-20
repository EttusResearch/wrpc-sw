/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012,2015 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 * Author: Adam Wujek <adam.wujek@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <wrc.h>
#include "uart.h"
#include "softpll_ng.h"
#include "minipc.h"
#include "revision.h"
#include "system_checks.h"


int scb_ver = 33;		/* SCB version */

extern struct spll_stats stats;

/* initialize functions to be called after reset in check_reset function */
void init_hw_after_reset(void)
{
	/* Ok, now init the devices so we can printf and delay */
	uart_init_hw();
}

int main(void)
{
	uint32_t start_tics = timer_get_tics();

	check_reset();
	stats.start_cnt++;
	_endram = ENDRAM_MAGIC;
	uart_init_hw();
	pp_printf("\n");
	pp_printf("WR Switch Real Time Subsystem (c) CERN 2011 - 2014\n");
	pp_printf("Revision: %s, built: %s %s.\n",
	      build_revision, build_date, build_time);
	pp_printf("SCB version: %d. %s\n", scb_ver,(scb_ver>=34)?"10 MHz SMC Output.":"" );
	pp_printf("Start counter %d\n", stats.start_cnt);
	pp_printf("--\n");

	if (stats.start_cnt > 1) {
		pp_printf("!!spll does not work after restart!!\n");
		/* for sure problem is in calling second time ad9516_init,
		 * but not only */
	}
	ad9516_init(scb_ver);
	rts_init();
	rtipc_init();
	spll_very_init();

	for(;;)
	{
		uint32_t tics = timer_get_tics();

		if (time_after(tics, start_tics + TICS_PER_SECOND/5)) {
			spll_show_stats();
			start_tics = tics;
		}

		rts_update();
		rtipc_action();
		spll_update();
		check_stack();
	}

	return 0;
}
