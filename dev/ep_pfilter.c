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

#include <wrc.h>
#include <shell.h>
#include <endpoint.h>
#include <hw/endpoint_regs.h>

extern uint32_t _binary_rules_novlan_bin_start[];
extern uint32_t _binary_rules_novlan_bin_end[];
extern uint32_t _binary_rules_vlan_bin_start[];
extern uint32_t _binary_rules_vlan_bin_end[];

struct rule_set {
	uint32_t *ini;
	uint32_t *end;
} rule_sets[2] = {
	{
		_binary_rules_novlan_bin_start,
		_binary_rules_novlan_bin_end,
	}, {
		_binary_rules_vlan_bin_start,
		_binary_rules_vlan_bin_end,
	}
};


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
	struct rule_set *s;
	uint8_t mac[6];
	char buf[20];
	uint32_t m, *vini, *vend, *v, *v_vlan = NULL;
	uint64_t cmd_word;
	int i;
	static int inited;
	uint32_t latency_ethtype = CONFIG_LATENCY_ETHTYPE;

	/* If vlan, use rule-set 1, else rule-set 0 */
	s = rule_sets + (wrc_vlan_number != 0);
	if (!s->ini) {
		pp_printf("no pfilter rule-set!\n");
		return;
	}
	vini = s->ini;
	vend = s->end;

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
	 * in the first three instructions after NOP
	 */
	get_mac_addr(mac);
	v[2] &= ~(0xffff << 13);
	v[4] &= ~(0xffff << 13);
	v[6] &= ~(0xffff << 13);
	v[2] |= ((mac[0] << 8) | mac[1]) << 13;
	v[4] |= ((mac[2] << 8) | mac[3]) << 13;
	v[6] |= ((mac[4] << 8) | mac[5]) << 13;
	pfilter_verbose("fixing MAC adress in rule: use %s\n",
			format_mac(buf, mac));

	/*
	 * Patch in the "latency" ethtype too. This is set at build time
	 * so there's not need to remember the place or the value.
	 */
	if (latency_ethtype == 0)
		latency_ethtype = 0x88f7; /* reuse PTPv2 type: turn into NOP */
	for (v = vini + 1; v < vend; v += 2) {
		if (((*v >> 13) & 0xffff) == 0xcafe
		    && (*v & 0x7) == OR) {
			pfilter_verbose("fixing latency eth_type: use 0x%x\n",
					latency_ethtype);
			*v &= ~(0xffff << 13);
			*v |= latency_ethtype << 13;
		}
	}

	/* If this is the VLAN rule-set, patch the vlan number too */
	for (v = vini + 1; v < vend; v += 2) {
		if (((*v >> 13) & 0xffff) == 0x0aaa
		    && ((*v >> 7) & 0x1f) == 7) {
			pfilter_verbose("fixing VLAN number in rule: use %i\n",
					wrc_vlan_number);
			v_vlan = v;
			*v &= ~(0xffff << 13);
			*v |= wrc_vlan_number << 13;
		}
	}

	EP->PFCR0 = 0;		// disable pfilter

	for (i = 0, v = vini + 1; v < vend; v += 2, i++) {
		uint32_t cr0, cr1;

		cmd_word = v[0] | ((uint64_t)v[1] << 32);
		pfilter_verbose("pfilter rule %02i: %x.%08x\n", i,
				(uint32_t)(cmd_word >> 32),
				(uint32_t)(cmd_word));

		cr1 = EP_PFCR1_MM_DATA_LSB_W(cmd_word & 0xfff);
		cr0 = EP_PFCR0_MM_ADDR_W(i) | EP_PFCR0_MM_DATA_MSB_W(cmd_word >> 12) |
		    EP_PFCR0_MM_WRITE_MASK;

		EP->PFCR1 = cr1;
		EP->PFCR0 = cr0;
	}

	/* Restore the 0xaaa vlan number, so we can re-patch next time */
	if (v_vlan) {
		*v_vlan &= ~(0xffff << 13);
		*v_vlan |= 0x0aaa << 13;
	}

	/* Restore default MAC, so that the rule-set check doesn't fail on LM32
	 * restart */
	v = vini + 1; /* rewind v to the beginning, skipping the first magic number */
	v[2] &= ~(0xffff << 13);
	v[4] &= ~(0xffff << 13);
	v[6] &= ~(0xffff << 13);
	v[2] |= 0x1234 << 13;
	v[4] |= 0x5678 << 13;
	v[6] |= 0x9abc << 13;

	EP->PFCR0 = EP_PFCR0_ENABLE;
}
