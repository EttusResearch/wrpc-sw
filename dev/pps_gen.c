/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Copyright (C) 2012 GSI (www.gsi.de)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 * Author: Wesley W. Terpstra <w.terpstra@gsi.de>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <wrc.h>
#include "board.h"
#include "pps_gen.h"

#include "hw/pps_gen_regs.h"

/* PPS Generator driver */

/* Warning: references to "UTC" in the registers DO NOT MEAN actual UTC time, it's just a plain second counter
	 It doesn't care about leap seconds. */

#define ppsg_write(reg, val) \
	*(volatile uint32_t *) (BASE_PPS_GEN + (offsetof(struct PPSG_WB, reg))) = (val)

#define ppsg_read(reg) \
	*(volatile uint32_t *) (BASE_PPS_GEN + (offsetof(struct PPSG_WB, reg)))

void shw_pps_gen_init()
{
	uint32_t cr;

	cr = PPSG_CR_CNT_EN | PPSG_CR_PWIDTH_W(PPS_WIDTH);

	ppsg_write(CR, cr);

	ppsg_write(ADJ_UTCLO, 0);
	ppsg_write(ADJ_UTCHI, 0);
	ppsg_write(ADJ_NSEC, 0);

	ppsg_write(CR, cr | PPSG_CR_CNT_SET);
	ppsg_write(CR, cr);
	ppsg_write(ESCR, 0);	/* disable PPS output */
}

/* Adjusts the nanosecond (refclk cycle) counter by atomically adding (how_much) cycles. */
int shw_pps_gen_adjust(int counter, int64_t how_much)
{
	TRACE_DEV("Adjust: counter = %s [%c%d]\n",
		  counter == PPSG_ADJUST_SEC ? "seconds" : "nanoseconds",
		  how_much < 0 ? '-' : '+', (int32_t) abs(how_much));

	if (counter == PPSG_ADJUST_NSEC) {
		ppsg_write(ADJ_UTCLO, 0);
		ppsg_write(ADJ_UTCHI, 0);
		ppsg_write(ADJ_NSEC,
			   (int32_t) ((int64_t) how_much * 1000LL /
				      (int64_t) REF_CLOCK_PERIOD_PS));
	} else {
		ppsg_write(ADJ_UTCLO, (uint32_t) (how_much & 0xffffffffLL));
		ppsg_write(ADJ_UTCHI, (uint32_t) (how_much >> 32) & 0xff);
		ppsg_write(ADJ_NSEC, 0);
	}

	ppsg_write(CR, ppsg_read(CR) | PPSG_CR_CNT_ADJ);
	return 0;
}

/* Sets the current time */
void shw_pps_gen_set_time(uint64_t seconds, uint32_t nanoseconds, int counter)
{
	ppsg_write(ADJ_UTCLO, (uint32_t) (seconds & 0xffffffffLL));
	ppsg_write(ADJ_UTCHI, (uint32_t) (seconds >> 32) & 0xff);
	ppsg_write(ADJ_NSEC,
		   (int32_t) ((int64_t) nanoseconds * 1000LL /
			      (int64_t) REF_CLOCK_PERIOD_PS));

	if (counter == PPSG_SET_ALL)
		ppsg_write(CR, (ppsg_read(CR) & 0xfffffffb) | PPSG_CR_CNT_SET);
	else if (counter == PPSG_SET_SEC)
		ppsg_write(ESCR, (ppsg_read(ESCR) & 0xffffffe7) | PPSG_ESCR_SEC_SET);
	else if (counter == PPSG_SET_NSEC)
		ppsg_write(ESCR, (ppsg_read(ESCR) & 0xffffffe7) | PPSG_ESCR_NSEC_SET);
}

uint64_t pps_get_utc(void)
{
	uint64_t out;
	uint32_t low, high;

	low = ppsg_read(CNTR_UTCLO);
	high = ppsg_read(CNTR_UTCHI);

	high &= 0xFF;		/* CNTR_UTCHI has only 8 bits defined -- rest are HDL don't care */

	out = (uint64_t) low | (uint64_t) high << 32;
	return out;
}

void shw_pps_gen_get_time(uint64_t * seconds, uint32_t * nanoseconds)
{
	uint32_t ns_cnt;
	uint64_t sec1, sec2;

	do {
		sec1 = pps_get_utc();
		ns_cnt = ppsg_read(CNTR_NSEC) & 0xFFFFFFFUL;	/* 28-bit wide register */
		sec2 = pps_get_utc();
	} while (sec2 != sec1);

	if (seconds)
		*seconds = sec2;
	if (nanoseconds)
		*nanoseconds =
		    (uint32_t) ((int64_t) ns_cnt *
				(int64_t) REF_CLOCK_PERIOD_PS / 1000LL);
}

/* Returns 1 when the adjustment operation is not yet finished */
int shw_pps_gen_busy()
{
	uint32_t cr = ppsg_read(CR);
	return cr & PPSG_CR_CNT_ADJ ? 0 : 1;
}

/* Enables/disables PPS output */
int shw_pps_gen_enable_output(int enable)
{
	uint32_t escr = ppsg_read(ESCR);
	if (enable)
		ppsg_write(ESCR,
			   escr | PPSG_ESCR_PPS_VALID | PPSG_ESCR_TM_VALID);
	else
		ppsg_write(ESCR,
			   escr & ~(PPSG_ESCR_PPS_VALID | PPSG_ESCR_TM_VALID));

	return 0;
}
