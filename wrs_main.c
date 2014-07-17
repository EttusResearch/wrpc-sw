#include <wrc.h>
#include "uart.h"

#include "softpll_ng.h"

#include "minipc.h"

const char *build_revision;
const char *build_date;

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
    TRACE("SoftPLL VCO Frequency/APR test:\n");

    spll_set_dac(-1, 0);
    f_min = spll_measure_frequency(SPLL_OSC_DMTD);
    spll_set_dac(-1, 65535);
    f_max = spll_measure_frequency(SPLL_OSC_DMTD);
    TRACE("DMTD VCO:  Low=%d Hz Hi=%d Hz, APR = %d ppm.\n", f_min, f_max, calc_apr(f_min, f_max, 62500000));

    spll_set_dac(0, 0);
    f_min = spll_measure_frequency(SPLL_OSC_REF);
    spll_set_dac(0, 65535);
    f_max = spll_measure_frequency(SPLL_OSC_REF);
    TRACE("REF VCO:   Low=%d Hz Hi=%d Hz, APR = %d ppm.\n", f_min, f_max, calc_apr(f_min, f_max, 62500000));

    f_min = spll_measure_frequency(SPLL_OSC_EXT);
    TRACE("EXT clock: Freq=%d Hz\n", f_min);
}

int main(void)
{
	uint32_t start_tics = timer_get_tics();

	uart_init_hw();
	
	TRACE("WR Switch Real Time Subsystem (c) CERN 2011 - 2014\n");
	TRACE("Revision: %s, built %s.\n", build_revision, build_date);
	TRACE("--");

	ad9516_init();
	rts_init();
	rtipc_init();

	for(;;)
	{
			uint32_t tics = timer_get_tics();

			if(time_after(tics, start_tics + TICS_PER_SECOND/5))
			{
				spll_show_stats();
				start_tics = tics;
			}
	    rts_update();
	    rtipc_action();
		spll_update();
	}

	return 0;
}
