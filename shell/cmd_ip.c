#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "softpll_ng.h"
#include "shell.h"
#include "../lib/ipv4.h"

int cmd_ip(const char *args[])
{
	mprintf("My IP-address: %d.%d.%d.%d\n", 
	  myIP[0], myIP[1], myIP[2], myIP[3]);
}
