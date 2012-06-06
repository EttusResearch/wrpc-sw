/*  Command: sfp
    Arguments: subcommand [subcommand-specific args]
    
    Description: SFP detection/database manipulation.
		
		Subcommands:
			add vendor_type delta_tx delta_rx alpha - adds an SFP to the database, with given alpha/delta_rx/delta_rx values
			show - shows the SFP database
			detect - detects the transceiver type		
*/

#include "shell.h"

#include "sfp.h"

int cmd_sfp(const char *args[])
{
	if(args[0] && !strcasecmp(args[0], "detect"))
	{
		char pn[17];
		if(!sfp_present())
			mprintf("No SFP.\n");
		else
			sfp_read_part_id(pn);
		pn[16]=0;
		mprintf("%s\n",pn);
		return 0;
	} else if (args[3] && !strcasecmp(args[0], "add"))
	{
	
	}
			
	
	return 0;
}