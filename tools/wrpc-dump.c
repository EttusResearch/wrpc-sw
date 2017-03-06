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
#include <revision.h>
#include <arch/lm32/crt0.h>

#include <dump-info.h>
/* We have a problem: ppsi is built for wrpc, so it has ntoh[sl] wrong */
#undef ntohl
#undef ntohs
#undef ntohll
#define ntohs(x) __do_not_use
#define ntohl(x) __do_not_use
#define ntohll(x) __do_not_use

uint32_t endian_flag; /* from dump_info[0], lazily */
/*
 * This picks items from memory, converting as needed. No ntohl any more.
 * Next, we'll detect the byte order from the code itself.
 */
static long long wrpc_get_64(void *p)
{
	uint64_t *p64 = p;
	uint64_t result;

	if (endian_flag == DUMP_ENDIAN_FLAG) {
		return *p64;
	}
	result = __bswap_32((uint32_t)*p64);
	result <<= 32;
	result |= __bswap_32((uint32_t)(*p64 >> 32));
	return result;
}

/* printf complains for i/l mismatch, so get i32 and l32 separately */
static long wrpc_get_l32(void *p)
{
	uint32_t *p32 = p;

	if (endian_flag == DUMP_ENDIAN_FLAG)
		return *p32;
	return __bswap_32(*p32);
}

static int wrpc_get_i32(void *p)
{
	return wrpc_get_l32(p);
}

static int wrpc_get_16(void *p)
{
	uint16_t *p16 = p;

	if (endian_flag == DUMP_ENDIAN_FLAG)
		return *p16;
	return __bswap_16(*p16);
}

static uint8_t wrpc_get_8(void *p)
{
	uint8_t *p8 = p;

	return *p8;
}

void dump_one_field(void *addr, struct dump_info *info)
{
	void *p = addr + wrpc_get_i32(&info->offset);
	struct pp_time *t = p;
	struct PortIdentity *pi = p;
	struct ClockQuality *cq = p;
	char format[16];
	char localname[64];
	int i, type, size;

	/* now, info may be in wrong-endian. so fix it */
	type = wrpc_get_i32(&info->type);
	size = wrpc_get_i32(&info->size);
	sprintf(localname, "%s:", info->name);
	printf("        %-30s ", localname);
	switch(type) {
	case dump_type_char:
		sprintf(format,"\"%%.%is\"\n", size);
		printf(format, (char *)p);
		break;
	case dump_type_bina:
		for (i = 0; i < size; i++)
			printf("%02x%c", ((unsigned char *)p)[i],
			       i == size - 1 ? '\n' : ':');
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
	case dump_type_uint8_t:
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
		if (size == 4)
			printf("%08lx\n", wrpc_get_l32(p));
		else
			printf("%016llx\n", wrpc_get_64(p));
		break;
	case dump_type_Integer16:
		printf("%i\n", wrpc_get_16(p));
		break;
	case dump_type_pp_time:
	{
		struct pp_time localt;
		localt.secs = wrpc_get_64(&t->secs);
		localt.scaled_nsecs = wrpc_get_64(&t->scaled_nsecs);

		printf("correct %i: %10lli.%09li:0x%04x\n",
		       !is_incorrect(&localt),
		       (long long)localt.secs,
		       (long)(localt.scaled_nsecs >> 16),
		       (int)(localt.scaled_nsecs & 0xffff));
		break;
	}

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
	}
}
void dump_many_fields(void *addr, char *name)
{
	struct dump_info *p = dump_info;

	/* Look for name */
	for (; strcmp(p->name, "end"); p++)
		if (!strcmp(p->name, name))
			break;

	if (!strcmp(p->name, "end")) {
		fprintf(stderr, "structure \"%s\" not described\n", name);
		return;
	}

	endian_flag = p->endian_flag;
	for (p++; p->endian_flag == 0; p++)
		dump_one_field(addr, p);
}
unsigned long wrpc_get_pointer(void *base, char *s_name, char *f_name)
{
	struct dump_info *p = dump_info;
	int offset;

	for (; strcmp(p->name, "end"); p++)
		if (!strcmp(p->name, s_name))
			break;

	if (!strcmp(p->name, "end")) {
		fprintf(stderr, "structure \"%s\" not described\n", s_name);
		return 0;
	}
	endian_flag = p->endian_flag;
	/* Look for the field: we find the offset,  */
	for (p++; p->endian_flag == 0; p++) {
		if (!strcmp(p->name, f_name)) {
			offset = wrpc_get_i32(&p->offset);
			return wrpc_get_l32(base + offset);
		}
	}
	fprintf(stderr, "can't find \"%s\" in \"%s\"\n", f_name, s_name);
	return 0;
}

void print_version(void)
{
	fprintf(stderr, "Built in wrpc-sw repo ver:%s, by %s on %s %s\n",
		__GIT_VER__, __GIT_USR__, __TIME__, __DATE__);
	fprintf(stderr, "Supported WRPC structures version %d\n",
		WRPC_SHMEM_VERSION);
	fprintf(stderr, "Supported PPSI structures version %d\n",
		WRS_PPSI_SHMEM_VERSION);
}
/* all of these are 0 by default */
unsigned long spll_off, fifo_off, ppi_off, ppg_off, servo_off, ds_off,
	      stats_off;

/* Use:  wrs_dump_memory <file> <hex-offset> <name> */
int main(int argc, char **argv)
{
	int fd;
	void *mapaddr;
	unsigned long offset;
	struct stat st;
	char *dumpname = "";
	char c;
	uint8_t version_wrpc, version_ppsi;

	if (argc != 4 && argc != 2) {
		fprintf(stderr, "%s: use \"%s <file> <offset> <name>\n",
			argv[0], argv[0]);
		fprintf(stderr,
			"\"name\" is one of pll, fifo, ppg, ppi, servo_state"
			" or ds for data-sets. \"ds\" gets a ppg offset\n");
		fprintf(stderr, "But with a new binary, just pass <file>\n\n");
		print_version();
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
	if (!strncmp(mapaddr + WRPC_MARK, "CPRW", 4))
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

	/* If we have a new binary file, pick the pointers
	 * Magic numbers are taken from crt0.S or disassembly of wrc.bin */
	if (!strncmp(mapaddr + WRPC_MARK, "WRPC----", 8)) {

		spll_off = wrpc_get_l32(mapaddr + SOFTPLL_PADDR);
		fifo_off = wrpc_get_l32(mapaddr + FIFO_LOG_PADDR);
		ppi_off = wrpc_get_l32(mapaddr + PPI_STATIC_PADDR);
		stats_off = wrpc_get_l32(mapaddr + STATS_PADDR);
		if (ppi_off) { /* This is 0 for wrs */
			ppg_off = wrpc_get_pointer(mapaddr + ppi_off,
				   "pp_instance", "glbs");
			servo_off = wrpc_get_pointer(mapaddr + ppg_off,
				    "pp_globals", "global_ext_data");
			ds_off = ppg_off;
		}
	}

	/* Check the version of wrpc and ppsi structures */
	version_wrpc = wrpc_get_8(mapaddr + VERSION_WRPC_ADDR);
	version_ppsi = wrpc_get_8(mapaddr + VERSION_PPSI_ADDR);
	if (version_wrpc != WRPC_SHMEM_VERSION) {
		printf("Unsupported version of WRPC structures! Expected %d, "
		       "but read %d\n", WRPC_SHMEM_VERSION, version_wrpc);
		exit(1);
	}
	if (version_ppsi != WRS_PPSI_SHMEM_VERSION) {
		printf("Unsupported version of PPSI structures! Expected %d, "
		       "but read %d\n", WRS_PPSI_SHMEM_VERSION, version_ppsi);
		exit(1);
	}

	#define ARRAY_AND_SIZE(x) (x), ARRAY_SIZE(x)

	/* Now check the "name" to be dumped  */
	if (!strcmp(dumpname, "pll"))
		spll_off = offset;
	if (spll_off) {
		printf("pll at 0x%lx\n", spll_off);
		dump_many_fields(mapaddr + spll_off, "softpll");
	}
	if (!strcmp(dumpname, "fifo"))
		fifo_off = offset;
	if (fifo_off) {
		int i;

		printf("fifo log at 0x%lx\n", fifo_off);
		for (i = 0; i < FIFO_LOG_LEN; i++)
			dump_many_fields(mapaddr + fifo_off
					 + i * sizeof(struct spll_fifo_log),
					 "pll_fifo");
	}
	if (!strcmp(dumpname, "ppg"))
		ppg_off = offset;
	if (ppg_off) {
		printf("ppg at 0x%lx\n", ppg_off);
		dump_many_fields(mapaddr + ppg_off, "pp_globals");
	}
	if (!strcmp(dumpname, "ppi"))
		ppi_off = offset;
	if (ppi_off) {
		printf("ppi at 0x%lx\n", ppi_off);
		dump_many_fields(mapaddr + ppi_off, "pp_instance");
	}
	if (!strcmp(dumpname, "servo_state"))
		servo_off = offset;
	if (servo_off) {
		printf("servo_state at 0x%lx\n", servo_off);
		dump_many_fields(mapaddr + servo_off, "servo_state");
	}

	/* This "all" gets the ppg pointer. It's not really all: no pll */
	if (!strcmp(dumpname, "ds"))
		ds_off = offset;
	if (ds_off) {
		unsigned long newoffset;

		ppg_off = ds_off;
		newoffset = wrpc_get_pointer(mapaddr + ppg_off,
					     "pp_globals", "defaultDS");
		printf("DSDefault at 0x%lx\n", newoffset);
		dump_many_fields(mapaddr + newoffset, "DSDefault");

		newoffset = wrpc_get_pointer(mapaddr + ppg_off,
					     "pp_globals", "currentDS");
		printf("DSCurrent at 0x%lx\n", newoffset);
		dump_many_fields(mapaddr + newoffset, "DSCurrent");

		newoffset = wrpc_get_pointer(mapaddr + ppg_off,
					     "pp_globals", "parentDS");
		dump_many_fields(mapaddr + newoffset, "DSParent");

		newoffset = wrpc_get_pointer(mapaddr + ppg_off,
					     "pp_globals", "timePropertiesDS");
		printf("DSTimeProperties at 0x%lx\n", newoffset);
		dump_many_fields(mapaddr + newoffset, "DSTimeProperties");
	}

	if (!strcmp(dumpname, "stats"))
		stats_off = offset;
	if (stats_off) {
		printf("stats at 0x%lx\n", stats_off);
		dump_many_fields(mapaddr + stats_off, "stats");
	}

	exit(0);
}
