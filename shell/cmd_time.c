/*  Command: time
    Arguments: 
    	set UTC NSEC - sets time
    	raw - dumps raw time
    	<none> - dumps pretty time
    
    Description: (re)starts/stops the PTP session. */

#include <errno.h>
#include <string.h>

#include "shell.h"
#include "wrc_ptp.h"
#include "pps_gen.h"

extern char *format_time(uint32_t utc);

int cmd_time(const char *args[])
{
	uint32_t utc, nsec;

	pps_gen_get_time(&utc, &nsec);
	
	if(args[2] && !strcasecmp(args[0], "set")) {
		if(wrc_ptp_get_mode() != WRC_MODE_SLAVE)
		{
			pps_gen_set_time(atoi(args[1]), atoi(args[2]) / 8);
			return 0;
		} else
			return -EBUSY;
	} else if(args[0] && !strcasecmp(args[0], "raw"))
	{
			mprintf("%d %d\n", utc, nsec);
			return 0;
	}

	mprintf("%s +%d nanoseconds.\n", format_time(utc), nsec*8); /* fixme: clock freq is not always 125 MHz */
	
	return 0;	
}