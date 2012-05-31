/* 	Command: gui 
		Arguments: none
		
		Description: launches the WR Core monitor GUI */
		
#include "shell.h"

extern int wrc_gui_mode;

int cmd_gui(const char *args[])
{
	wrc_gui_mode = 1;
	return 0;
}