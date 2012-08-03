/* 	Command: gui 
		Arguments: none
		
		Description: launches the WR Core monitor GUI */
		
#include "shell.h"


extern int measure_t24p(int *value);

int cmd_calib(const char *args[])
{
	return measure_t24p(NULL);
}
