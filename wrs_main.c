#include "uart.h"
#include "syscon.h"

#include "softpll_ng.h"

#include "minipc.h"

const char *build_revision;
const char *build_date;

main()
{
	uint32_t start_tics = 0;

	uart_init_hw();
	
	TRACE("WR Switch Real Time Subsystem (c) CERN 2011 - 2013\n");
	TRACE("Revision: %s, built %s.\n", build_revision, build_date);
	TRACE("--");

	ad9516_init();
	rts_init();
	rtipc_init();

	for(;;)
	{
			uint32_t tics = timer_get_tics();
			
			if(tics - start_tics > TICS_PER_SECOND/5)
			{
//				TRACE("tick!\n");
				spll_show_stats();
				start_tics = tics;
			}
	    rts_update();
	    rtipc_action();
	}

	return 0;
}
