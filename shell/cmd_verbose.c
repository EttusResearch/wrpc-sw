/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012 GSI (www.gsi.de)
 * Author: Wesley W. Terpstra <w.terpstra@gsi.de>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

#include <wrc.h>

extern int pp_diag_verbosity;

int cmd_verbose(const char *args[])
{
	int v;
	v = args[0][0] - '0';

	if (v < 0)
		v = 0;
	else if (v > 2)
		v = 2;

	pp_printf("PPSI verbosity set to %d\n", v);
	pp_diag_verbosity = v;
	return 0;
}
