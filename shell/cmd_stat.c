#include "shell.h"

int cmd_stat(const char *args[])
{
  if(!strcasecmp(args[0], "cont") )
  {
    wrc_ui_mode = UI_STAT_MODE;
  }
  else
	  wrc_log_stats(1);

	return 0;
}
