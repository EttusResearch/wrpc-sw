/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2011 CERN (www.cern.ch)
 * Copyright (C) 2012 GSI (www.gsi.de)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 * Author: Wesley W. Terpstra <w.terpstra@gsi.de>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

/* Endpoint Packet Filter/Classifier driver

   A little explanation: The WR core needs to classify the incoming
   packets into two (or more) categories. Current HDL (as of 2015)
   uses two actual classes even if the filter supports 8. Class bits
   0-3 go to the CPU (through the mini-nic) and class bits 4-7 go to
   the internal fabric.  The frame, in the fabric, is prefixed with
   a status word that includes the class bits. 

   The CPU is expected to receive PTP, ICMP, ARP and DHCP replies (so
   local "bootpc" port).

   The fabric should receive Etherbone (i.e. UDP port 0xebd0), the
   "streamer" protocol used by some CERN installation (ethtype 0xdbff)
   and everything else if the "NIC pfilter" feature by 7Solutions is used.

   The logic cells connected to the fabric do their own check on the
   frames, so it's not a problem if extra frames reach the fabric. Thus,
   even if earlier code bases had a build-time choice of filter rulese,
   we now have a catch-all rule set.   Please note that the CPU is
   not expected to receive a lot of undesired traffic, because it has
   very limited processing power.

   One special bit means "drop", and we'll use it when implementing vlans
   (it currently is not used, because the fabric can receive extra stuff,
   but it was activated by previous rule-sets.


  0. Introduction
  ------------------------------------------

  WR Endpoint (WR MAC) inside the WR Core therefore contains a simple
  microprogrammable packet filter/classifier. The classifier processes
  the incoming packet, and assigns it to one of 8 classes (an 8-bit
  word, where each bit corresponds to a particular class) or
  eventually drops it. Hardware implementation of the unit is a simple
  VLIW processor with 32 single-bit registers (0 - 31). The registers
  are organized as follows:

  - 0: don't touch (always 0)
  - 1 - 22: general purpose registers (but comparison can only write to 1-14).
  - 23: drop packet flag. It takes precedence to class assignment
  - 24..31: packet class (class 0 = reg 24, class 7 = reg 31). See "2." below.

  Program memory has 64 36-bit words, but you should use only 32,
  because the pfilter is synchronouse with frame ingress, and frames
  can be as short as 64 bytes long.

  The packet filtering program is restarted every time a new packet comes.

  The operations, <oper> and <oper2> below, are one of:
    AND, NAND,     OR, NOR,    XOR, XNOR,     MOV, NOT


  1. Instructions
  ------------------------------------------

  There are 5 possible instructions.

  1.1 CMP offset, value, mask, <oper>, Rd:
  ------------------------------------------

  * Rd = Rd <oper> ((((uint16_t *)packet) [offset] & mask) == value)

  Examples:

      * CMP 3, 0xcafe, 0xffff, MOV, Rd

      will compare the 4th word of the packet (offset 3: bytes 6, 7) against
      0xcafe and if the words are equal, 1 will be written to Rd register.

      * CMP 4, 0xbabe, 0xffff, AND, Rd

      will do the same with the 4th word and write to Rd its previous
      value ANDed with the result of the comparison. Effectively, Rd
      now will be 1 only if bytes [6..9] of the payload contain word
      0xcafebabe.

  Note that the mask value is nibble-granular. That means you can
  choose a particular set of nibbles within a word to be compared, but
  not an arbitrary set of bits (e.g. 0xf00f, 0xff00 and 0xf0f0 masks
  are ok, but 0x8001 is wrong.

  The target of a comparison can be register 1-15 alone.

  1.2. BTST offset, bit_number, <oper>, Rd
  ------------------------------------------

  * Rd = Rd <oper> (((uint16_t *)packet) [offset] & (1<<bit_number) ? 1 : 0)

  Examples:
 
      * BTST 3, 10, MOV, 11

      will write 1 to reg 11 if the 10th bit in the 3rd word of the
      packet is set (and 0 if it's clear)

  1.3. LOGIC2 Rd, Ra, <oper>, Rb
  -----------------------------------------

  * Rd = Ra <oper> Rb

  If the operation is MOV or NOT, Ra is taken as the source register
  and Rb is ignored.


  1.4. LOGIC3 Rd, Ra, <oper>, Rb, <oper2> Rc
  -----------------------------------------

  * Rd = (Ra <oper> Rb) <oper2> Rc

  1.4. Misc
  -----------------------------------------

  FIN instruction terminates the program.

  NOP executes a dummy instruction (LOGIC2 0, 0, AND, 0)


  IMPORTANT:

  - the program counter is advanved each time a 16-bit words of the
    packet arrives.

  - the CPU doesn't have any interlocks to simplify the HW, so you
    can't compare the 10th word when PC = 2. Max comparison offset is
    always equal to the address of the instruction.

  - Code may contain up to 64 operations, but it must classify shorter
    packets faster than in 32 instructions (there's no flow
    throttling)


  2. How the frame is routed after the pfilter
  -----------------------------------------

  After the input pipeline is over, the endpoint looks at the DROP bit.
  If the frame is not dropped, it is passed over to the MUX (xwrf_mux.vhd),
  with the CLASS bits attached (well, pre-pended).

  The MUX is configured with two classes in wr_code.vhd:
     mux_class(0)  <= x"0f";
     mux_class(1)  <= x"f0";

  This is the behaviour:

  - If at least one bit is set in the first class (here 0x0f), the frame
    goes to CPU and processing stops. Processing is in class order,
    not bit order.

  - If at least one bit is set in the second class (here 0xf0) the frame
    goes to fabric and processing stops.

  - If no class is set, the frame goes to the last class, the fabric.

  If, in the future or other implementations, the same pfilter is used
  with a different MUX configuration, ports are processed from lowest-number
  to highest-number, and if no bit is set it goes to the last.

  In the wr-nic gateware, a second MUX is connected, using two classes
  and bitmasks 0x20 and 0x80 as I write this note.  Such values must
  be changed for consistency with the other configurations.

*/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <endpoint.h> /* for operations and type pfilter_op_t */

#define PFILTER_MAX_CODE_SIZE      32

#define pfilter_dbg(x, ...) /* nothing */

char *prgname;

extern volatile struct EP_WB *EP;

static const uint64_t PF_MODE_LOGIC = (1ULL << 34);
static const uint64_t PF_MODE_CMP = 0ULL;

static int code_pos;
static uint64_t code_buf[PFILTER_MAX_CODE_SIZE];

enum pf_regs {
	R_ZERO = 1024,
	R_1, R_2, R_3, R_4, R_5, R_6, R_7, R_8, R_9, R_10,
	R_11, R_12, R_13, R_14, R_15, R_16, R_17, R_18, R_19, R_20,
	R_21, R_22,
	R_DROP,
	R_C0, R_C1, R_C2, R_C3, R_C4, R_C5, R_C6, R_C7
};
#define R_CLASS(x) (R_C0 + x)

/* Give also "symbolic" names, to the roles of each register. */
enum pf_symbolic_regs {

	/* The first set is used for straight comparisons */
	FRAME_BROADCAST = R_1,
	FRAME_MAC_OK,
	FRAME_MAC_PTP,
	FRAME_OUR_VLAN,
	FRAME_TYPE_IPV4,
	FRAME_TYPE_PTP2,
	FRAME_TYPE_LATENCY,
	FRAME_TYPE_ARP,
	FRAME_ICMP,
	FRAME_UDP,
	PORT_UDP_HOST,
	PORT_UDP_ETHERBONE,
	R_TMP,

	/* These are results of logic over the previous bits  */
	FRAME_IP_UNI,
	FRAME_IP_OK, /* unicast or broadcast */

	/* A temporary register, and the CPU target */
	FRAME_FOR_CPU, /* must be last */
};

int v1[R_C7 - (R_ZERO + 31)]; /* fails if we lost  a register */
int v2[(R_ZERO + 31) - R_C7]; /* fails if we added a register */

int v3[0 - ((int)FRAME_FOR_CPU > R_22)];



/* begins assembling a new packet filter program */
static void pfilter_new(void)
{
	code_pos = 0;
}

static void check_size(void)
{
	if (code_pos == PFILTER_MAX_CODE_SIZE - 1) {
		fprintf(stderr, "%s: microcode too big (max size: %d)\n",
			prgname, PFILTER_MAX_CODE_SIZE);
		exit(1);
	}
}

static void check_reg_range(int val, int minval, int maxval, char *name)
{
	if (val < minval || val > maxval) {
		fprintf(stderr, "%s: register \"%s\" out of range (%d to %d)",
			prgname, name, minval, maxval);
		exit(1);
	}
}

static void pfilter_cmp(int offset, int value, int mask, pfilter_op_t op,
			int rd)
{
	uint64_t ir;

	check_size();

	if (offset > code_pos) {
		fprintf(stderr, "%s: comparison offset is bigger than current PC. Insert some nops before comparing",
			prgname);
		exit(1);
	}

	check_reg_range(rd, R_1, R_15, "ra/rd");
	rd -= R_ZERO;

	ir = (PF_MODE_CMP | ((uint64_t) offset << 7)
	      | ((mask & 0x1) ? (1ULL << 29) : 0)
	      | ((mask & 0x10) ? (1ULL << 30) : 0)
	      | ((mask & 0x100) ? (1ULL << 31) : 0)
	      | ((mask & 0x1000) ? (1ULL << 32) : 0))
	    | op | (rd << 3);

	ir = ir | ((uint64_t) value & 0xffffULL) << 13;

	code_buf[code_pos++] = ir;
}

static void pfilter_nop(void)
{
	uint64_t ir;
	check_size();
	ir = PF_MODE_LOGIC;
	code_buf[code_pos++] = ir;
}

  // rd  = ra op rb
static void pfilter_logic2(int rd, int ra, pfilter_op_t op, int rb)
{
	uint64_t ir;
	check_size();
	check_reg_range(ra, R_ZERO, R_C7, "ra"); ra -= R_ZERO;
	check_reg_range(rb, R_ZERO, R_C7, "rb"); rb -= R_ZERO;
	check_reg_range(rd, R_1,    R_C7, "rd"); rd -= R_ZERO;

	ir = ((uint64_t) ra << 8) | ((uint64_t) rb << 13) |
	    (((uint64_t) rd & 0xf) << 3) | (((uint64_t) rd & 0x10) ? (1ULL << 7)
					    : 0) | (uint64_t) op;
	ir = ir | PF_MODE_LOGIC | (3ULL << 23);
	code_buf[code_pos++] = ir;
}

static void pfilter_logic3(int rd, int ra, pfilter_op_t op, int rb,
			   pfilter_op_t op2, int rc)
{
	uint64_t ir;
	check_size();
	check_reg_range(ra, R_ZERO, R_C7, "ra"); ra -= R_ZERO;
	check_reg_range(rb, R_ZERO, R_C7, "rb"); rb -= R_ZERO;
	check_reg_range(rc, R_ZERO, R_C7, "rc"); rc -= R_ZERO;
	check_reg_range(rd, R_1,    R_C7, "rd"); rd -= R_ZERO;

	ir = (ra << 8) | (rb << 13) | (rc << 18) | ((rd & 0xf) << 3) |
	    ((rd & 0x10) ? (1 << 7) : 0) | op;
	ir = ir | PF_MODE_LOGIC | (op2 << 23);
	code_buf[code_pos++] = ir;
}

/* Terminates the microcode, creating the output file */
static void pfilter_output(char *fname)
{
	uint32_t v1, v2;
	int i;
	FILE *f;

	code_buf[code_pos++] = (1ULL << 35);	// insert FIN instruction

	f = fopen(fname, "w");
	if (!f) {
		fprintf(stderr, "%s: %s: %s\n", prgname, fname, strerror(errno));
		exit(1);
	}

	/* First write a magic word, so the target can check endianness */
	v1 = 0x11223344;
	fwrite(&v1, sizeof(v1), 1, f);

	pfilter_dbg("Writing \"%s\"\n", fname);
	for (i = 0; i < code_pos; i++) {
		pfilter_dbg("   pos %02i: %x.%08x\n", i,
			    (uint32_t)(code_buf[i] >> 32),
			    (uint32_t)(code_buf[i]));
		/* Explicitly write the LSB first */
		v1 = code_buf[i];
		v2 = code_buf[i] >> 32;
		fwrite(&v1, sizeof(v1), 1, f);
		fwrite(&v2, sizeof(v2), 1, f);
	}
	fclose(f);
}


void pfilter_init_novlan(char *fname)
{
	pfilter_new();
	pfilter_nop();

	/*
	 * Make three sets of comparisons over the destination address.
	 * After these instructions, the whole Eth header is there
	 */

	/* Local frame, using fake MAC: 12:34:56:78:9a:bc */
	pfilter_cmp(0, 0x1234, 0xffff, MOV, FRAME_MAC_OK);
	pfilter_cmp(1, 0x5678, 0xffff, AND, FRAME_MAC_OK);
	pfilter_cmp(2, 0x9abc, 0xffff, AND, FRAME_MAC_OK);

	/* Broadcast frame */
	pfilter_cmp(0, 0xffff, 0xffff, MOV, FRAME_BROADCAST);
	pfilter_cmp(1, 0xffff, 0xffff, AND, FRAME_BROADCAST);
	pfilter_cmp(2, 0xffff, 0xffff, AND, FRAME_BROADCAST);

	/* PTP UDP (end to end: 01:00:5e:00:01:81    224.0.1.129) */
	pfilter_cmp(0, 0x0100, 0xffff, MOV, FRAME_MAC_PTP);
	pfilter_cmp(1, 0x5e00, 0xffff, AND, FRAME_MAC_PTP);
	pfilter_cmp(2, 0x0181, 0xffff, MOV, R_TMP);

	/* PTP UDP (peer-to-p:  01:00:5e:00:00:6b    224.0.0.197) */
	pfilter_cmp(2, 0x006b, 0xffff, OR, R_TMP);

	pfilter_logic3(FRAME_MAC_OK, FRAME_MAC_PTP, AND, R_TMP, OR, FRAME_MAC_OK);

	/* Tagged is dropped. We'll invert the check in the vlan rule-set */
	pfilter_cmp(6, 0x8100, 0xffff, MOV, R_TMP);
	pfilter_logic2(R_DROP, R_TMP, MOV, R_ZERO);

	/* Identify some Ethertypes used later -- type latency is 0xcafe */
	pfilter_cmp(6, 0x88f7, 0xffff, MOV, FRAME_TYPE_PTP2);
	pfilter_cmp(6, 0xcafe, 0xffff, OR, FRAME_TYPE_PTP2);
	pfilter_cmp(6, 0x0800, 0xffff, MOV, FRAME_TYPE_IPV4);
	pfilter_cmp(6, 0x0806, 0xffff, MOV, FRAME_TYPE_ARP);

	/* Mark one bits for ip-valid (unicast or broadcast) */
	pfilter_logic3(FRAME_IP_OK, FRAME_BROADCAST, OR, FRAME_MAC_OK, AND, FRAME_TYPE_IPV4);

	/* Ethernet = 14 bytes, Offset to type in IP: 8 bytes = 22/2 = 11 */
	pfilter_cmp(11, 0x0001, 0x00ff, MOV, FRAME_ICMP);
	pfilter_cmp(11, 0x0011, 0x00ff, MOV, FRAME_UDP);
	pfilter_logic2(FRAME_UDP, FRAME_UDP, AND, FRAME_IP_OK);

	/* For CPU: arp or icmp unicast or ptp (or latency) */
	pfilter_logic2(FRAME_FOR_CPU, FRAME_TYPE_ARP, OR, FRAME_TYPE_PTP2);
	pfilter_logic3(FRAME_FOR_CPU, FRAME_IP_OK, AND, FRAME_ICMP, OR, FRAME_FOR_CPU);

	/* Now look in UDP ports: at offset 18 (14 + 20 + 8 = 36) */
	pfilter_cmp(18, 0x0000, 0xff00, MOV, PORT_UDP_HOST);	/* ports 0-255 */
	pfilter_cmp(18, 0x0100, 0xff00, OR, PORT_UDP_HOST);	/* ports 256-511 */

	/* The CPU gets those ports in a proper UDP frame, plus the previous selections */
	pfilter_logic3(R_CLASS(0), FRAME_UDP, AND, PORT_UDP_HOST, OR, FRAME_FOR_CPU);

	/* Etherbone is UDP at port 0xebd0, let's "or" in the last move */
	pfilter_cmp(18, 0xebd0, 0xffff, MOV, PORT_UDP_ETHERBONE);

	/* and now copy out fabric selections: 7 etherbone, 6 for anything else */
	pfilter_logic2(R_CLASS(7), FRAME_UDP, AND, PORT_UDP_ETHERBONE);
	pfilter_logic2(R_CLASS(6), FRAME_UDP, NAND, PORT_UDP_ETHERBONE);

	/*
	 * Note that earlier we used to be more strict in ptp ethtype (only proper multicast),
	 * but since we want to accept peer-delay sooner than later, we'd better avoid the checks
	 */

	/*
	 * Also, please note that "streamer" ethtype 0xdbff and "etherbone" udp port 
	 * 0xebd0 go to the fabric by being part of the "anything else" choice".
	 */

	pfilter_output(fname);

}

void pfilter_init_vlan(char *fname)
{
	pfilter_new();
	pfilter_nop();

	/*
	 * This is a simplified set, for development,
	 * to allow me write vlan support in software.
	 * Mostly a subset of above set
	 */

	/* Local frame, using fake MAC: 12:34:56:78:9a:bc */
	pfilter_cmp(0, 0x1234, 0xffff, MOV, FRAME_MAC_OK);
	pfilter_cmp(1, 0x5678, 0xffff, AND, FRAME_MAC_OK);
	pfilter_cmp(2, 0x9abc, 0xffff, AND, FRAME_MAC_OK);

	/* Broadcast frame */
	pfilter_cmp(0, 0xffff, 0xffff, MOV, FRAME_BROADCAST);
	pfilter_cmp(1, 0xffff, 0xffff, AND, FRAME_BROADCAST);
	pfilter_cmp(2, 0xffff, 0xffff, AND, FRAME_BROADCAST);

	/* PTP UDP (end to end: 01:00:5e:00:01:81    224.0.1.129) */
	pfilter_cmp(0, 0x0100, 0xffff, MOV, FRAME_MAC_PTP);
	pfilter_cmp(1, 0x5e00, 0xffff, AND, FRAME_MAC_PTP);
	pfilter_cmp(2, 0x0181, 0xffff, MOV, R_TMP);

	/* PTP UDP (peer-to-p:  01:00:5e:00:00:6b    224.0.0.197) */
	pfilter_cmp(2, 0x006b, 0xffff, OR, R_TMP);

	pfilter_logic3(FRAME_MAC_OK, FRAME_MAC_PTP, AND, R_TMP, OR, FRAME_MAC_OK);

	/* Untagged is dropped. */
	pfilter_cmp(6, 0x8100, 0xffff, MOV, R_TMP);
	pfilter_logic2(R_DROP, R_TMP, NOT, R_ZERO);

	/* Compare with our vlan (fake number 0xaaa) */
	pfilter_cmp(7, 0x0aaa, 0x0fff, MOV, FRAME_OUR_VLAN);

	/* Identify some Ethertypes used later -- type latency is 0xcafe */
	pfilter_cmp(8, 0x88f7, 0xffff, MOV, FRAME_TYPE_PTP2);
	pfilter_cmp(8, 0xcafe, 0xffff, OR, FRAME_TYPE_PTP2);
	pfilter_cmp(8, 0x0800, 0xffff, MOV, FRAME_TYPE_IPV4);
	pfilter_cmp(8, 0x0806, 0xffff, MOV, FRAME_TYPE_ARP);

	/* Loose match: keep all bcast, all our mac and all ptp ... */
	pfilter_logic3(FRAME_FOR_CPU,
		FRAME_MAC_OK, OR, FRAME_BROADCAST, OR, FRAME_TYPE_PTP2);

	/* .... but only for our current VLAN */
	pfilter_logic2(R_CLASS(0), FRAME_FOR_CPU, AND, FRAME_OUR_VLAN);

	/*
	 * And route these build-time selected vlans to fabric 7 and 6.
	 * Class 7 is etherbone and class 6 is streamer or nic (or whatever).
	 * We get all broadcast and all frames for our mac.
	 *
	 * We reuse FRAME_OUR_VLAN, even if it's not OUR as in "this CPU"
	 */
	pfilter_logic2(R_TMP, FRAME_MAC_OK, OR, FRAME_BROADCAST);

	pfilter_cmp(7, CONFIG_VLAN_1_FOR_CLASS7, 0x0fff, MOV, FRAME_OUR_VLAN);
	pfilter_cmp(7, CONFIG_VLAN_2_FOR_CLASS7, 0x0fff, OR, FRAME_OUR_VLAN);
	pfilter_logic2(R_CLASS(7), R_TMP, AND, FRAME_OUR_VLAN);

	pfilter_cmp(7, CONFIG_VLAN_FOR_CLASS6, 0x0fff, MOV, FRAME_OUR_VLAN);
	pfilter_logic2(R_CLASS(6), R_TMP, AND, FRAME_OUR_VLAN);

	pfilter_output(fname);
}

int main(int argc, char **argv) /* no arguments used currently */
{
	prgname = argv[0];

	pfilter_init_novlan("rules-novlan.bin");
	pfilter_init_vlan("rules-vlan.bin");
	exit(0);
}
