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
	FRAME_PTP_MCAST,
	FRAME_OUR_MAC,
	FRAME_TYPE_IPV4,
	FRAME_TYPE_PTP2,
	FRAME_TYPE_ARP,
	FRAME_ICMP,
	FRAME_UDP,
	FRAME_TYPE_STREAMER, /* An ethtype by Tom, used in gateware */
	FRAME_PORT_ETHERBONE,

	/* These are results of logic over the previous bits  */
	FRAME_IP_UNI,
	FRAME_IP_OK, /* unicast or broadcast */
	FRAME_PTP_OK,
	FRAME_STREAMER_BCAST,

	/* A temporary register, and the CPU target */
	R_TMP,
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

/* We generate all supported rule-sets, those that used to be ifdef'd */
#define MODE_ETHERBONE   1
#define MODE_NIC_PFILTER 2


void pfilter_init(int mode, char *fname)
{
	pfilter_new();
	pfilter_nop();

	/*
	 * Make three sets of comparisons over the destination address.
	 * After these 9 instructions, the whole Eth header is available.
	 */
	pfilter_cmp(0, 0x1234, 0xffff, MOV, FRAME_OUR_MAC);	/* Use fake MAC: 12:34:56:78:9a:bc */
	pfilter_cmp(1, 0x5678, 0xffff, AND, FRAME_OUR_MAC);
	pfilter_cmp(2, 0x9abc, 0xffff, AND, FRAME_OUR_MAC);	/* set when our MAC */

	pfilter_cmp(0, 0xffff, 0xffff, MOV, FRAME_BROADCAST);
	pfilter_cmp(1, 0xffff, 0xffff, AND, FRAME_BROADCAST);
	pfilter_cmp(2, 0xffff, 0xffff, AND, FRAME_BROADCAST);	/* set when dst mac is broadcast */

	pfilter_cmp(0, 0x011b, 0xffff, MOV, FRAME_PTP_MCAST);
	pfilter_cmp(1, 0x1900, 0xffff, AND, FRAME_PTP_MCAST);
	pfilter_cmp(2, 0x0000, 0xffff, AND, FRAME_PTP_MCAST);	/* set when dst mac is PTP multicast (01:1b:19:00:00:00) */

	/* Identify some Ethertypes used later */
	pfilter_cmp(6, 0x0800, 0xffff, MOV, FRAME_TYPE_IPV4);
	pfilter_cmp(6, 0x88f7, 0xffff, MOV, FRAME_TYPE_PTP2);
	pfilter_cmp(6, 0x0806, 0xffff, MOV, FRAME_TYPE_ARP);
	pfilter_cmp(6, 0xdbff, 0xffff, MOV, FRAME_TYPE_STREAMER);

	/* Ethernet = 14 bytes, Offset to type in IP: 8 bytes = 22/2 = 11 */
	pfilter_cmp(11, 0x0001, 0x00ff, MOV, FRAME_ICMP);
	pfilter_cmp(11, 0x0011, 0x00ff, MOV, FRAME_UDP);

	if (mode & MODE_ETHERBONE) {

		/* Mark bits for unicast to us, and for unicast-to-us-or-broadcast */
		pfilter_logic3(FRAME_IP_UNI, FRAME_OUR_MAC, OR, R_ZERO, AND, FRAME_TYPE_IPV4);
		pfilter_logic3(FRAME_IP_OK, FRAME_BROADCAST, OR, FRAME_OUR_MAC, AND, FRAME_TYPE_IPV4);

		/* Make a selection for the CPU, that is later still added-to */
		pfilter_logic3(R_TMP, FRAME_BROADCAST, AND, FRAME_TYPE_ARP, OR, FRAME_TYPE_PTP2);
		pfilter_logic3(FRAME_FOR_CPU, FRAME_IP_UNI, AND, FRAME_ICMP, OR, R_TMP);

		/* Ethernet = 14 bytes, IPv4 = 20 bytes, offset to dport: 2 = 36/2 = 18 */
		pfilter_cmp(18, 0x0044, 0xffff, MOV, R_TMP);	/* R_TMP now means dport = BOOTPC */

		pfilter_logic3(R_TMP, R_TMP, AND, FRAME_UDP, AND, FRAME_IP_OK);	/* BOOTPC and UDP and IP(unicast|broadcast) */
		pfilter_logic2(FRAME_FOR_CPU, R_TMP, OR, FRAME_FOR_CPU);

		if (mode & MODE_NIC_PFILTER) {

			pfilter_cmp(18,0xebd0,0xffff,MOV, FRAME_PORT_ETHERBONE);

			/* Here we had a commented-out check for magic (offset 21, value 0x4e6f) */

			pfilter_logic2(R_CLASS(0), FRAME_FOR_CPU, MOV, R_ZERO);
			pfilter_logic2(R_CLASS(5), FRAME_PORT_ETHERBONE, OR, R_ZERO); /* class 5: Etherbone packet => Etherbone Core */
			pfilter_logic3(R_CLASS(7), FRAME_FOR_CPU, OR, FRAME_PORT_ETHERBONE, NOT, R_ZERO); /* class 7: Rest => NIC Core */
		} else {

			pfilter_logic3(R_TMP, FRAME_IP_OK, AND, FRAME_UDP, OR, FRAME_FOR_CPU);	/* Something we accept: cpu+udp or streamer */

			pfilter_logic3(R_DROP, R_TMP, OR, FRAME_TYPE_STREAMER, NOT, R_ZERO);	/* None match? drop */

			pfilter_logic2(R_CLASS(7), FRAME_IP_OK, AND, FRAME_UDP);	/* class 7: UDP/IP(unicast|broadcast) => external fabric */
			pfilter_logic2(R_CLASS(6), FRAME_BROADCAST, AND, FRAME_TYPE_STREAMER);	/* class 6: streamer broadcasts => external fabric */
			pfilter_logic2(R_CLASS(0), FRAME_FOR_CPU, MOV, R_ZERO);	/* class 0: all selected for CPU earlier */

		}
	} else { /* not etherbone */

		pfilter_logic3(FRAME_PTP_OK, FRAME_OUR_MAC, OR, FRAME_PTP_MCAST, AND, FRAME_TYPE_PTP2);
		pfilter_logic2(FRAME_STREAMER_BCAST, FRAME_BROADCAST, AND, FRAME_TYPE_STREAMER);
		pfilter_logic3(R_TMP, FRAME_PTP_OK, OR, FRAME_STREAMER_BCAST, NOT, R_ZERO); /* R_TMP = everything else */

 * Released according to the GNU GPL
		pfilter_logic2(R_CLASS(7), R_TMP, MOV, R_ZERO);	/* class 7: all non PTP and non-streamer traffic => external fabric */
		pfilter_logic2(R_CLASS(6), FRAME_STREAMER_BCAST, MOV, R_ZERO); /* class 6: streamer broadcasts => external fabric */
		pfilter_logic2(R_CLASS(0), FRAME_PTP_OK, MOV, R_ZERO); /* class 0: PTP frames => LM32 */

	}

	pfilter_output(fname);

}

int main(int argc, char **argv) /* no arguments used currently */
{
	prgname = argv[0];

	pfilter_init(0, "rules-plain.bin");
	pfilter_init(MODE_ETHERBONE, "rules-ebone.bin");
	pfilter_init(MODE_ETHERBONE | MODE_NIC_PFILTER, "rules-e+nic.bin");
	exit(0);
}
