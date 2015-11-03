/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2011,2014 CERN (www.cern.ch)
 * Copyright (C) 2012 GSI (www.gsi.de)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 * Author: Wesley W. Terpstra <w.terpstra@gsi.de>
 * Author: Alessandro rubini (for the build-time creation of the array)
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

/*
 * The packet filter documentation is moved to tools/pfilter-builder.c, where
 * the actual packet filter rules are created
 */

#include <stdio.h>

#include <endpoint.h>
#include <hw/endpoint_regs.h>

extern uint32_t _binary_rules_pfilter_bin_start[];
extern uint32_t _binary_rules_pfilter_bin_end[];

#define pfilter_dbg(fmt, ...) /* nothing */

extern volatile struct EP_WB *EP;

static uint32_t swap32(uint32_t v)
{
	uint32_t res;

	res  = (v & 0xff000000) >> 24;
	res |= (v & 0x00ff0000) >>  8;
	res |= (v & 0x0000ff00) <<  8;
	res |= (v & 0x000000ff) << 24;
	return res;
}

void pfilter_init_default(void)
{
	/* Use shorter names to avoid getting mad */
	uint32_t *vini = _binary_rules_pfilter_bin_start;
	uint32_t *vend = _binary_rules_pfilter_bin_end;
	uint32_t m, *v;
	uint64_t cmd_word;
	int i;
	static int inited;

	/*
	 * The array of words starts with 0x11223344 so we
	 * can fix endianness. Do it.
	 */
	v = vini;
	m = v[0];
	if (m != 0x11223344)
		for (v = vini; v < vend; v++)
			*v = swap32(*v);
	v = vini;
	if (v[0] != 0x11223344) {
		pp_printf("pfilter: wrong magic number (got 0x%x)\n", m);
		return;
	}
	v++;

	/*
	 * First time: be extra-careful that the rule-set is ok. But if
	 * we change MAC address, this is re-called, and v[] is already changed
	 */
	if (!inited) {
		if (   (((v[2] >> 13) & 0xffff) != 0x1234)
		    || (((v[4] >> 13) & 0xffff) != 0x5678)
		    || (((v[6] >> 13) & 0xffff) != 0x9abc)) {
			pp_printf("pfilter: wrong rule-set, can't apply\n");
			return;
		}
		inited++;
	}
	/*
	 * Patch the local MAC address in place,
	 * in the first three instructions after NOP */
	v[2] &= ~(0xffff << 13);
	v[4] &= ~(0xffff << 13);
	v[6] &= ~(0xffff << 13);
	v[2] |= ((EP->MACH >>  0) & 0xffff) << 13;
	v[4] |= ((EP->MACL >> 16) & 0xffff) << 13;
	v[6] |= ((EP->MACL >>  0) & 0xffff) << 13;

	EP->PFCR0 = 0;		// disable pfilter

	for (i = 0;v < vend; v += 2, i++) {
		uint32_t cr0, cr1;

		cmd_word = v[0] | ((uint64_t)v[1] << 32);
		pfilter_dbg("pos %02i: %x.%08x\n", i, (uint32_t)(cmd_word >> 32), (uint32_t)(cmd_word));

		cr1 = EP_PFCR1_MM_DATA_LSB_W(cmd_word & 0xfff);
		cr0 = EP_PFCR0_MM_ADDR_W(i) | EP_PFCR0_MM_DATA_MSB_W(cmd_word >> 12) |
		    EP_PFCR0_MM_WRITE_MASK;

		EP->PFCR1 = cr1;
		EP->PFCR0 = cr0;
	}

	EP->PFCR0 = EP_PFCR0_ENABLE;
}
