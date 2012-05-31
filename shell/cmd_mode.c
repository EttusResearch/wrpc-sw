/*  Command: mode
    Arguments: PTP mode: gm = grandmaster, master = free-running master, slave = slave
    
    Description: (re)starts/stops the PTP session. */

#include <errno.h>
#include <string.h>

#include "shell.h"
#include "wrc_ptp.h"

int cmd_mode(const char *args[])
{
	int mode;
		
	if(!strcasecmp(args[0], "gm"))
		mode = WRC_MODE_GM;
	else if(!strcasecmp(args[0], "master"))
		mode = WRC_MODE_MASTER;
	else if(!strcasecmp(args[0], "slave"))
		mode = WRC_MODE_SLAVE;
	else
		return -EINVAL;

	return wrc_ptp_set_mode(mode);
}