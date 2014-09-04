#include <wrc.h>
#include "uart.h"

#include "softpll_ng.h"

#include "minipc.h"

const char *build_revision;
const char *build_date;

int scb_ver = 33;		//SCB version.

int main(void)
{
	uint32_t start_tics = timer_get_tics();

	uart_init_hw();
	
	TRACE("");
	TRACE("WR Switch Real Time Subsystem (c) CERN 2011 - 2014\n");
	TRACE("Revision: %s, built %s.\n", build_revision, build_date);
	TRACE("SCB version: %d. %s\n", scb_ver,(scb_ver>=34)?"10 MHz SMC Output.":"" );
	TRACE("--");

	ad9516_init( scb_ver );
	rts_init();
	rtipc_init();

	for(;;)
	{
			uint32_t tics = timer_get_tics();

			if(time_after(tics, start_tics + TICS_PER_SECOND/5))
			{
				spll_show_stats();
				start_tics = tics;
			}
	    rts_update();
	    rtipc_action();
		spll_update();
	}

	return 0;
}
