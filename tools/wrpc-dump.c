#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <arpa/inet.h> /* ntohl */
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>

#include <ppsi/ppsi.h>
#include <softpll_ng.h>

/* We have a problem: ppsi is built for wrpc, so it has ntoh[sl] wrong */
#undef ntohl
#undef ntohs
#undef ntohll
#define ntohs(x) __do_not_use
#define ntohl(x) __do_not_use
#define ntohll(x) __do_not_use

/*  be safe, in case some other header had them slightly differently */
#undef container_of
#undef offsetof
#undef ARRAY_SIZE

#define container_of(ptr, type, member) ({			\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type,member) );})

#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))


/*
 * This picks items from memory, converting as needed. No ntohl any more.
 * Next, we'll detect the byte order from the code itself.
 */
static long long wrpc_get_64(void *p)
{
	uint64_t *p64 = p;
	uint64_t result;

	result = __bswap_32((uint32_t)(*p64 >> 32));
	result <<= 32;
	result |= __bswap_32((uint32_t)*p64);
	return result;
}

/* printf complains for i/l mismatch, so get i32 and l32 separately */
static long wrpc_get_l32(void *p)
{
	uint32_t *p32 = p;

	return __bswap_32(*p32);
}
static int wrpc_get_i32(void *p)
{
	return wrpc_get_l32(p);
}

static int wrpc_get_16(void *p)
{
	uint16_t *p16 = p;

	return __bswap_16(*p16);
}

/*
 * To ease copying from header files, allow int, char and other known types.
 * Please add more type as more structures are included here
 */
enum dump_type {
	dump_type_char, /* for zero-terminated strings */
	dump_type_bina, /* for binary stull in MAC format */
	/* normal types follow */
	dump_type_uint32_t,
	dump_type_uint16_t,
	dump_type_int,
	dump_type_unsigned_long,
	dump_type_unsigned_char,
	dump_type_unsigned_short,
	dump_type_double,
	dump_type_float,
	dump_type_pointer,
	/* and strange ones, from IEEE */
	dump_type_UInteger64,
	dump_type_Integer64,
	dump_type_UInteger32,
	dump_type_Integer32,
	dump_type_UInteger16,
	dump_type_Integer16,
	dump_type_UInteger8,
	dump_type_Integer8,
	dump_type_Enumeration8,
	dump_type_Boolean,
	dump_type_ClockIdentity,
	dump_type_PortIdentity,
	dump_type_ClockQuality,
	/* and this is ours */
	dump_type_TimeInternal,
	dump_type_ip_address,
	dump_type_sensor_temp,
};

/*
 * A structure to dump fields. This is meant to simplify things, see use here
 */
struct dump_info {
	char *name;
	enum dump_type type;   /* see above */
	int offset;
	int size;  /* only for strings or binary strings */
};



void dump_one_field(void *addr, struct dump_info *info)
{
	void *p = addr + info->offset;
	struct TimeInternal *ti = p;
	struct PortIdentity *pi = p;
	struct ClockQuality *cq = p;
	char format[16];
	int i;

	printf("        %-30s ", info->name); /* name includes trailing ':' */
	switch(info->type) {
	case dump_type_char:
		sprintf(format,"\"%%.%is\"\n", info->size);
		printf(format, (char *)p);
		break;
	case dump_type_bina:
		for (i = 0; i < info->size; i++)
			printf("%02x%c", ((unsigned char *)p)[i],
			       i == info->size - 1 ? '\n' : ':');
		break;
	case dump_type_UInteger64:
		printf("%lld\n", wrpc_get_64(p));
		break;
	case dump_type_Integer64:
		printf("%lld\n", wrpc_get_64(p));
		break;
	case dump_type_uint32_t:
		printf("0x%08lx\n", wrpc_get_l32(p));
		break;
	case dump_type_Integer32:
	case dump_type_int:
		printf("%i\n", wrpc_get_i32(p));
		break;
	case dump_type_UInteger32:
	case dump_type_unsigned_long:
		printf("%li\n", wrpc_get_l32(p));
		break;
	case dump_type_unsigned_char:
	case dump_type_UInteger8:
	case dump_type_Integer8:
	case dump_type_Enumeration8:
	case dump_type_Boolean:
		printf("%i\n", *(unsigned char *)p);
		break;
	case dump_type_UInteger16:
	case dump_type_uint16_t:
	case dump_type_unsigned_short:
		printf("%i\n", wrpc_get_16(p));
		break;
	case dump_type_double:
		printf("%lf\n", *(double *)p);
		break;
	case dump_type_float:
		printf("%f\n", *(float *)p);
		break;
	case dump_type_pointer:
		printf("%08lx\n", wrpc_get_l32(p));
		break;
	case dump_type_Integer16:
		printf("%i\n", wrpc_get_16(p));
		break;
	case dump_type_TimeInternal:
		printf("correct %i: %10i.%09i:%04i\n",
		       wrpc_get_i32(&ti->correct),
		       wrpc_get_i32(&ti->seconds),
		       wrpc_get_i32(&ti->nanoseconds),
		       wrpc_get_i32(&ti->phase));
		break;

	case dump_type_ip_address:
		for (i = 0; i < 4; i++)
			printf("%02x%c", ((unsigned char *)p)[i],
			       i == 3 ? '\n' : ':');
		break;

	case dump_type_ClockIdentity: /* Same as binary */
		for (i = 0; i < sizeof(ClockIdentity); i++)
			printf("%02x%c", ((unsigned char *)p)[i],
			       i == sizeof(ClockIdentity) - 1 ? '\n' : ':');
		break;

	case dump_type_PortIdentity: /* Same as above plus port */
		for (i = 0; i < sizeof(ClockIdentity); i++)
			printf("%02x%c", ((unsigned char *)p)[i],
			       i == sizeof(ClockIdentity) - 1 ? '.' : ':');
		printf("%04x (%i)\n", pi->portNumber, pi->portNumber);
		break;

	case dump_type_ClockQuality:
		printf("class %i, accuracy %02x (%i), logvariance %i\n",
		       cq->clockClass, cq->clockAccuracy, cq->clockAccuracy,
		       wrpc_get_16(&cq->offsetScaledLogVariance));
		break;
	case dump_type_sensor_temp:
		printf("%f\n", ((float)(wrpc_get_i32(p) >> 4)) / 16.0);
		break;
	}
}
void dump_many_fields(void *addr, struct dump_info *info, int ninfo)
{
	int i;

	if (!addr) {
		fprintf(stderr, "dump: pointer not valid\n");
		return;
	}
	for (i = 0; i < ninfo; i++)
		dump_one_field(addr, info + i);
}

/* the macro below relies on an externally-defined structure type */
#define DUMP_FIELD(_type, _fname) { \
	.name = #_fname ":",  \
	.type = dump_type_ ## _type, \
	.offset = offsetof(DUMP_STRUCT, _fname), \
}
#define DUMP_FIELD_SIZE(_type, _fname, _size) { \
	.name = #_fname ":",		\
	.type = dump_type_ ## _type, \
	.offset = offsetof(DUMP_STRUCT, _fname), \
	.size = _size, \
}


/* map for fields of ppsi structures */
#undef DUMP_STRUCT
#define DUMP_STRUCT struct pp_globals
static struct dump_info ppg_info [] = {
	DUMP_FIELD(pointer, pp_instances),	/* FIXME: follow this */
	DUMP_FIELD(pointer, servo),		/* FIXME: follow this */
	DUMP_FIELD(pointer, rt_opts),
	DUMP_FIELD(pointer, defaultDS),
	DUMP_FIELD(pointer, currentDS),
	DUMP_FIELD(pointer, parentDS),
	DUMP_FIELD(pointer, timePropertiesDS),
	DUMP_FIELD(int, ebest_idx),
	DUMP_FIELD(int, ebest_updated),
	DUMP_FIELD(int, nlinks),
	DUMP_FIELD(int, max_links),
	//DUMP_FIELD(struct pp_globals_cfg cfg),
	DUMP_FIELD(int, rxdrop),
	DUMP_FIELD(int, txdrop),
	DUMP_FIELD(pointer, arch_data),
	DUMP_FIELD(pointer, global_ext_data),
};

#undef DUMP_STRUCT
#define DUMP_STRUCT DSDefault /* Horrible typedef */
static struct dump_info dsd_info [] = {
	DUMP_FIELD(Boolean, twoStepFlag),
	DUMP_FIELD(ClockIdentity, clockIdentity),
	DUMP_FIELD(UInteger16, numberPorts),
	DUMP_FIELD(ClockQuality, clockQuality),
	DUMP_FIELD(UInteger8, priority1),
	DUMP_FIELD(UInteger8, priority2),
	DUMP_FIELD(UInteger8, domainNumber),
	DUMP_FIELD(Boolean, slaveOnly),
};

#undef DUMP_STRUCT
#define DUMP_STRUCT DSCurrent /* Horrible typedef */
static struct dump_info dsc_info [] = {
	DUMP_FIELD(UInteger16, stepsRemoved),
	DUMP_FIELD(TimeInternal, offsetFromMaster),
	DUMP_FIELD(TimeInternal, meanPathDelay), /* oneWayDelay */
	DUMP_FIELD(UInteger16, primarySlavePortNumber),
};

#undef DUMP_STRUCT
#define DUMP_STRUCT DSParent /* Horrible typedef */
static struct dump_info dsp_info [] = {
	DUMP_FIELD(PortIdentity, parentPortIdentity),
	DUMP_FIELD(UInteger16, observedParentOffsetScaledLogVariance),
	DUMP_FIELD(Integer32, observedParentClockPhaseChangeRate),
	DUMP_FIELD(ClockIdentity, grandmasterIdentity),
	DUMP_FIELD(ClockQuality, grandmasterClockQuality),
	DUMP_FIELD(UInteger8, grandmasterPriority1),
	DUMP_FIELD(UInteger8, grandmasterPriority2),
};

#undef DUMP_STRUCT
#define DUMP_STRUCT DSTimeProperties /* Horrible typedef */
static struct dump_info dstp_info [] = {
	DUMP_FIELD(Integer16, currentUtcOffset),
	DUMP_FIELD(Boolean, currentUtcOffsetValid),
	DUMP_FIELD(Boolean, leap59),
	DUMP_FIELD(Boolean, leap61),
	DUMP_FIELD(Boolean, timeTraceable),
	DUMP_FIELD(Boolean, frequencyTraceable),
	DUMP_FIELD(Boolean, ptpTimescale),
	DUMP_FIELD(Enumeration8, timeSource),
};

#undef DUMP_STRUCT
#define DUMP_STRUCT struct wr_servo_state
static struct dump_info servo_state_info [] = {
	DUMP_FIELD_SIZE(char, if_name, 16),
	DUMP_FIELD(unsigned_long, flags),
	DUMP_FIELD(int, state),
	DUMP_FIELD(Integer32, delta_tx_m),
	DUMP_FIELD(Integer32, delta_rx_m),
	DUMP_FIELD(Integer32, delta_tx_s),
	DUMP_FIELD(Integer32, delta_rx_s),
	DUMP_FIELD(Integer32, fiber_fix_alpha),
	DUMP_FIELD(Integer32, clock_period_ps),
	DUMP_FIELD(TimeInternal, t1),
	DUMP_FIELD(TimeInternal, t2),
	DUMP_FIELD(TimeInternal, t3),
	DUMP_FIELD(TimeInternal, t4),
	DUMP_FIELD(Integer32, delta_ms_prev),
	DUMP_FIELD(int, missed_iters),
	DUMP_FIELD(TimeInternal, mu),		/* half of the RTT */
	DUMP_FIELD(Integer64, picos_mu),
	DUMP_FIELD(Integer32, cur_setpoint),
	DUMP_FIELD(Integer32, delta_ms),
	DUMP_FIELD(UInteger32, update_count),
	DUMP_FIELD(int, tracking_enabled),
	DUMP_FIELD_SIZE(char, servo_state_name, 32),
	DUMP_FIELD(Integer64, skew),
	DUMP_FIELD(Integer64, offset),
	DUMP_FIELD(UInteger32, n_err_state),
	DUMP_FIELD(UInteger32, n_err_offset),
	DUMP_FIELD(UInteger32, n_err_delta_rtt),
};

#undef DUMP_STRUCT
#define DUMP_STRUCT struct pp_instance
static struct dump_info ppi_info [] = {
	DUMP_FIELD(int, state),
	DUMP_FIELD(int, next_state),
	DUMP_FIELD(int, next_delay),
	DUMP_FIELD(int, is_new_state),
	DUMP_FIELD(pointer, arch_data),
	DUMP_FIELD(pointer, ext_data),
	DUMP_FIELD(unsigned_long, d_flags),
	DUMP_FIELD(unsigned_char, flags),
	DUMP_FIELD(unsigned_char, role),
	DUMP_FIELD(unsigned_char, proto),
	DUMP_FIELD(pointer, glbs),
	DUMP_FIELD(pointer, n_ops),
	DUMP_FIELD(pointer, t_ops),
	DUMP_FIELD(pointer, __tx_buffer),
	DUMP_FIELD(pointer, __rx_buffer),
	DUMP_FIELD(pointer, tx_frame),
	DUMP_FIELD(pointer, rx_frame),
	DUMP_FIELD(pointer, tx_ptp),
	DUMP_FIELD(pointer, rx_ptp),

	/* This is a sub-structure */
	DUMP_FIELD(int, ch[0].fd),
	DUMP_FIELD(pointer, ch[0].custom),
	DUMP_FIELD(pointer, ch[0].arch_data),
	DUMP_FIELD_SIZE(bina, ch[0].addr, 6),
	DUMP_FIELD(int, ch[0].pkt_present),
	DUMP_FIELD(int, ch[1].fd),
	DUMP_FIELD(pointer, ch[1].custom),
	DUMP_FIELD(pointer, ch[1].arch_data),
	DUMP_FIELD_SIZE(bina, ch[1].addr, 6),
	DUMP_FIELD(int, ch[1].pkt_present),

	DUMP_FIELD(ip_address, mcast_addr),
	DUMP_FIELD(int, tx_offset),
	DUMP_FIELD(int, rx_offset),
	DUMP_FIELD_SIZE(bina, peer, 6),
	DUMP_FIELD(uint16_t, peer_vid),

	DUMP_FIELD(TimeInternal, t1),
	DUMP_FIELD(TimeInternal, t2),
	DUMP_FIELD(TimeInternal, t3),
	DUMP_FIELD(TimeInternal, t4),
	DUMP_FIELD(TimeInternal, cField),
	DUMP_FIELD(TimeInternal, last_rcv_time),
	DUMP_FIELD(TimeInternal, last_snt_time),
	DUMP_FIELD(UInteger16, frgn_rec_num),
	DUMP_FIELD(Integer16,  frgn_rec_best),
	//DUMP_FIELD(struct pp_frgn_master frgn_master[PP_NR_FOREIGN_RECORDS]),
	DUMP_FIELD(pointer, portDS),
	//DUMP_FIELD(unsigned long timeouts[__PP_TO_ARRAY_SIZE]),
	DUMP_FIELD(UInteger16, recv_sync_sequence_id),
	DUMP_FIELD(Integer8, log_min_delay_req_interval),
	//DUMP_FIELD(UInteger16 sent_seq[__PP_NR_MESSAGES_TYPES]),
	DUMP_FIELD_SIZE(bina, received_ptp_header, sizeof(MsgHeader)),
	//DUMP_FIELD(pointer, iface_name),
	//DUMP_FIELD(pointer, port_name),
	DUMP_FIELD(int, port_idx),
	DUMP_FIELD(int, vlans_array_len),
	/* FIXME: array */
	DUMP_FIELD(int, nvlans),

	/* sub structure */
	DUMP_FIELD_SIZE(char, cfg.port_name, 16),
	DUMP_FIELD_SIZE(char, cfg.iface_name, 16),
	DUMP_FIELD(int, cfg.ext),
	DUMP_FIELD(int, cfg.ext),

	DUMP_FIELD(unsigned_long, ptp_tx_count),
	DUMP_FIELD(unsigned_long, ptp_rx_count),
};


#undef DUMP_STRUCT
#define DUMP_STRUCT struct softpll_state
static struct dump_info pll_info [] = {
	DUMP_FIELD(int, mode),
	DUMP_FIELD(int, seq_state),
	DUMP_FIELD(int, dac_timeout),
	DUMP_FIELD(int, default_dac_main),
	DUMP_FIELD(int, delock_count),
	DUMP_FIELD(uint32_t, irq_count),
	DUMP_FIELD(int, mpll_shift_ps),
	DUMP_FIELD(int, helper.p_adder),
	DUMP_FIELD(int, helper.p_setpoint),
	DUMP_FIELD(int, helper.tag_d0),
	DUMP_FIELD(int, helper.ref_src),
	DUMP_FIELD(int, helper.sample_n),
	DUMP_FIELD(int, helper.delock_count),
	/* FIXME: missing helper.pi etc.. */
	DUMP_FIELD(int, ext.enabled),
	DUMP_FIELD(int, ext.align_state),
	DUMP_FIELD(int, ext.align_timer),
	DUMP_FIELD(int, ext.align_target),
	DUMP_FIELD(int, ext.align_step),
	DUMP_FIELD(int, ext.align_shift),
	DUMP_FIELD(int, mpll.state),
	/* FIXME: mpll.pi etc */
	DUMP_FIELD(int, mpll.adder_ref),
	DUMP_FIELD(int, mpll.adder_out),
	DUMP_FIELD(int, mpll.tag_ref),
	DUMP_FIELD(int, mpll.tag_out),
	DUMP_FIELD(int, mpll.tag_ref_d),
	DUMP_FIELD(int, mpll.tag_out_d),
	DUMP_FIELD(uint32_t, mpll.seq_ref),
	DUMP_FIELD(int, mpll.seq_out),
	DUMP_FIELD(int, mpll.match_state),
	DUMP_FIELD(int, mpll.match_seq),
	DUMP_FIELD(int, mpll.phase_shift_target),
	DUMP_FIELD(int, mpll.phase_shift_current),
	DUMP_FIELD(int, mpll.id_ref),
	DUMP_FIELD(int, mpll.id_out),
	DUMP_FIELD(int, mpll.sample_n),
	DUMP_FIELD(int, mpll.delock_count),
	DUMP_FIELD(int, mpll.dac_index),
	DUMP_FIELD(int, mpll.enabled),
	/* FIXME: aux_state and ptracker_state -- variable-len arrays */
};

#undef DUMP_STRUCT
#define DUMP_STRUCT struct spll_fifo_log
static struct dump_info fifo_info [] = {
	DUMP_FIELD(uint32_t, trr),
	DUMP_FIELD(uint16_t, irq_count),
	DUMP_FIELD(uint16_t, tag_count),
};

/* all of these are 0 by default */
unsigned long spll_off, fifo_off, ppi_off, ppg_off, servo_off, ds_off;

/* Use:  wrs_dump_memory <file> <hex-offset> <name> */
int main(int argc, char **argv)
{
	int fd;
	void *mapaddr;
	unsigned long offset;
	struct stat st;
	char *dumpname = "";
	char c;

	if (argc != 4 && argc != 2) {
		fprintf(stderr, "%s: use \"%s <file> <offset> <name>\n",
			argv[0], argv[0]);
		fprintf(stderr,
			"\"name\" is one of pll, fifo, ppg, ppi, servo_state"
			" or ds for data-sets. \"ds\" gets a ppg offset\n");
		fprintf(stderr, "But with a new binary, just pass <file>\n");
		exit(1);
	}

	fd = open(argv[1], O_RDONLY | O_SYNC);
	if (fd < 0) {
		fprintf(stderr, "%s: %s: %s\n",
			argv[0], argv[1], strerror(errno));
		exit(1);
	}
	if (fstat(fd, &st) < 0) {
		fprintf(stderr, "%s: stat(%s): %s\n",
			argv[0], argv[1], strerror(errno));
		exit(1);
	}
	if (!S_ISREG(st.st_mode)) { /* FIXME: support memory */
		fprintf(stderr, "%s: %s not a regular file\n",
			argv[0], argv[1]);
		exit(1);
	}
	if (st.st_size > 128 * 1024) /* support /sys/..../resource0 */
		st.st_size = 128 * 1024;

	if (argc == 4 && sscanf(argv[2], "%lx%c", &offset, &c) != 1) {
		fprintf(stderr, "%s: \"%s\" not a hex offset\n", argv[0],
			argv[2]);
		exit(1);
	}
	mapaddr = mmap(0, st.st_size, PROT_READ | PROT_WRITE,
		       MAP_FILE | MAP_PRIVATE, fd, 0);
	if (mapaddr == MAP_FAILED) {
		fprintf(stderr, "%s: mmap(%s): %s\n",
			argv[0], argv[1], strerror(errno));
		exit(1);
	}

	/* In case we have a "new" binary file, use such information */
	if (!strncmp(mapaddr + 0x80, "CPRW", 4))
		setenv("WRPC_SPEC", "yes", 1);

	/* If the dump file needs "spec" byte order, fix it all */
	if (getenv("WRPC_SPEC")) {
		uint32_t *p = mapaddr;
		int i;

		for (i = 0; i < st.st_size / 4; i++, p++)
			*p = __bswap_32(*p);
	}

	if (argc == 4)
		dumpname = argv[3];

	/* If we have a new binary file, pick the pointers */
	if (!strncmp(mapaddr + 0x80, "WRPC----", 8)) {
		struct pp_globals *ppg;
		struct pp_instance *ppi;

		spll_off = wrpc_get_l32(mapaddr + 0x90);
		fifo_off = wrpc_get_l32(mapaddr + 0x94);
		ppi_off = wrpc_get_l32(mapaddr + 0x98);
		if (ppi_off) { /* This is 0 for wrs */
			ppi = mapaddr + ppi_off;
			ppg_off = wrpc_get_l32(&ppi->glbs);
			ppg = mapaddr + ppg_off;
			servo_off = wrpc_get_l32(&ppg->global_ext_data);
			ds_off = ppg_off;
		}
	}

	#define ARRAY_AND_SIZE(x) (x), ARRAY_SIZE(x)

	/* Now check the "name" to be dumped  */
	if (!strcmp(dumpname, "pll"))
		spll_off = offset;
	if (spll_off) {
		printf("pll at 0x%lx\n", spll_off);
		dump_many_fields(mapaddr + spll_off,
				 ARRAY_AND_SIZE(pll_info));
	}
	if (!strcmp(dumpname, "fifo"))
		fifo_off = offset;
	if (fifo_off) {
		int i;

		printf("fifo log at 0x%lx\n", fifo_off);
		for (i = 0; i < FIFO_LOG_LEN; i++)
			dump_many_fields(mapaddr + fifo_off
					 + i * sizeof(struct spll_fifo_log),
					 ARRAY_AND_SIZE(fifo_info));
	}
	if (!strcmp(dumpname, "ppg"))
		ppg_off = offset;
	if (ppg_off) {
		printf("ppg at 0x%lx\n", ppg_off);
		dump_many_fields(mapaddr + ppg_off,
				 ARRAY_AND_SIZE(ppg_info));
	}
	if (!strcmp(dumpname, "ppi"))
		ppi_off = offset;
	if (ppi_off) {
		printf("ppi at 0x%lx\n", ppi_off);
		dump_many_fields(mapaddr + ppi_off,
				 ARRAY_AND_SIZE(ppi_info));
	}
	if (!strcmp(dumpname, "servo_state"))
		servo_off = offset;
	if (servo_off) {
		printf("servo_state at 0x%lx\n", servo_off);
		dump_many_fields(mapaddr + servo_off,
				 ARRAY_AND_SIZE(servo_state_info));
	}

	/* This "all" gets the ppg pointer. It's not really all: no pll */
	if (!strcmp(dumpname, "ds"))
		ds_off = offset;
	if (ds_off) {
		unsigned long newoffset;
		struct pp_globals *ppg;

		ppg = mapaddr + ds_off;

		newoffset = wrpc_get_l32(&ppg->defaultDS);
		printf("defaultDS at 0x%lx\n", newoffset);
		dump_many_fields(mapaddr + newoffset,
				 ARRAY_AND_SIZE(dsd_info));

		newoffset = wrpc_get_l32(&ppg->currentDS);
		printf("currentDS at 0x%lx\n", newoffset);
		dump_many_fields(mapaddr + newoffset,
				 ARRAY_AND_SIZE(dsc_info));

		newoffset = wrpc_get_l32(&ppg->parentDS);
		printf("parentDS at 0x%lx\n", newoffset);
		dump_many_fields(mapaddr + newoffset,
				 ARRAY_AND_SIZE(dsp_info));

		newoffset = wrpc_get_l32(&ppg->timePropertiesDS);
		printf("timePropertiesDS at 0x%lx\n", newoffset);
		dump_many_fields(mapaddr + newoffset,
				 ARRAY_AND_SIZE(dstp_info));

		newoffset = wrpc_get_l32(&ppg->global_ext_data);
		printf("servo_state at 0x%lx\n", newoffset);
		dump_many_fields(mapaddr + newoffset,
				 ARRAY_AND_SIZE(servo_state_info));

	}


	exit(0);
}
