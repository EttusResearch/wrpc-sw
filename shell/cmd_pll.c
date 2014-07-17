/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <wrc.h>

#include "softpll_ng.h"
#include "shell.h"

static int calc_apr(int meas_min, int meas_max, int f_center )
{
	// apr_min is in PPM

    int64_t delta_low =  meas_min - f_center;
    int64_t delta_hi = meas_max - f_center;

    if(delta_low >= 0)
	return -1;
    if(delta_hi <= 0)
	return -1;

    int ppm_lo = -(int64_t)delta_low * 1000000LL / f_center;
    int ppm_hi = (int64_t)delta_hi * 1000000LL / f_center;

    return ppm_lo < ppm_hi ? ppm_lo : ppm_hi;
}

extern void disable_irq();

static void check_vco_frequencies()
{
    disable_irq();

    int f_min, f_max;
    pp_printf("SoftPLL VCO Frequency/APR test:\n");

    spll_set_dac(-1, 0);
    f_min = spll_measure_frequency(SPLL_OSC_DMTD);
    spll_set_dac(-1, 65535);
    f_max = spll_measure_frequency(SPLL_OSC_DMTD);
    pp_printf("DMTD VCO:  Low=%d Hz Hi=%d Hz, APR = %d ppm.\n", f_min, f_max, calc_apr(f_min, f_max, 62500000));

    spll_set_dac(0, 0);
    f_min = spll_measure_frequency(SPLL_OSC_REF);
    spll_set_dac(0, 65535);
    f_max = spll_measure_frequency(SPLL_OSC_REF);
    pp_printf("REF VCO:   Low=%d Hz Hi=%d Hz, APR = %d ppm.\n", f_min, f_max, calc_apr(f_min, f_max, 125000000));

    f_min = spll_measure_frequency(SPLL_OSC_EXT);
    pp_printf("EXT clock: Freq=%d Hz\n", f_min);
}

static int cmd_pll(const char *args[])
{
	int cur, tgt;

	if (!strcasecmp(args[0], "init")) {
		if (!args[3])
			return -EINVAL;
		spll_init(atoi(args[1]), atoi(args[2]), atoi(args[3]));
	} else if (!strcasecmp(args[0], "cl")) {
		if (!args[1])
			return -EINVAL;
		mprintf("%d\n", spll_check_lock(atoi(args[1])));
	} else if (!strcasecmp(args[0], "stat")) {
		spll_show_stats();
	} else if (!strcasecmp(args[0], "sps")) {
		if (!args[2])
			return -EINVAL;
		spll_set_phase_shift(atoi(args[1]), atoi(args[2]));
	} else if (!strcasecmp(args[0], "gps")) {
		if (!args[1])
			return -EINVAL;
		spll_get_phase_shift(atoi(args[1]), &cur, &tgt);
		mprintf("%d %d\n", cur, tgt);
	} else if (!strcasecmp(args[0], "start")) {
		if (!args[1])
			return -EINVAL;
		spll_start_channel(atoi(args[1]));
	} else if (!strcasecmp(args[0], "stop")) {
		if (!args[1])
			return -EINVAL;
		spll_stop_channel(atoi(args[1]));
	} else if (!strcasecmp(args[0], "sdac")) {
		if (!args[2])
			return -EINVAL;
		spll_set_dac(atoi(args[1]), atoi(args[2]));
	} else if (!strcasecmp(args[0], "gdac")) {
		if (!args[1])
			return -EINVAL;
		mprintf("%d\n", spll_get_dac(atoi(args[1])));
	} else if(!strcasecmp(args[0], "checkvco"))
		check_vco_frequencies();
	else
		return -EINVAL;

	return 0;
}

DEFINE_WRC_COMMAND(pll) = {
	.name = "pll",
	.exec = cmd_pll,
};
