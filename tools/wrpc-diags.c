#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <time.h>

#include <libdevmap.h>
#include <extest.h>

#include <hw/wrc_diags_regs.h>

static struct mapping_desc *wrcdiag = NULL;

static void unlock_diag(volatile struct WRC_DIAGS_WB *ptr)
{
	//reset snapshot bit & keep valid bit as it is
	ptr->CTRL &= iomemw32(wrcdiag->is_be, 0x1);
}

static int lock_diag(volatile struct WRC_DIAGS_WB *ptr)
{
	int loop = 10;

	//snapshot diag ( just raise snapshot bit)
	ptr->CTRL |= iomemw32(wrcdiag->is_be, WRC_DIAGS_CTRL_DATA_SNAPSHOT);
	//wait max 10ms that valid bit becomes true.
	do {
		if (ptr->CTRL & iomemw32(wrcdiag->is_be,
					 WRC_DIAGS_CTRL_DATA_VALID))
			return 0;
		usleep(1000);
		--loop;
	} while(loop);
	fprintf(stderr, "timeout(10ms) expired while waiting for the valid bit "
		"for snapshot\n");
	return 1;
}

static void print_servo_status(uint32_t val)
{
	static char *sstat_str[] = {
		"Not initialized",
		"Sync ns",
		"Sync TAI",
		"Sync phase",
		"Track phase",
		"Wait offset stable",
	};

	fprintf(stderr, "servo status:\t\t%s\n",
		sstat_str[val >> WRC_DIAGS_WDIAG_SSTAT_SERVOSTATE_SHIFT]);
}

static void print_port_status(uint32_t val)
{
	static int nbits = 2;
	static char *pstat_str[][2] = {
		//bit = 0     	 	bit = 1
		{"Link down", 		"Link up",},
		{"PLL not locked",	"PLL locked",},
	};
	int i, idx;

	fprintf(stderr, "Port status:\t\t");
	for (i = 0; i < nbits; ++i) {
		idx = (val & (1 << i)) ? 1 : 0;
		fprintf(stderr, "%s, ", pstat_str[i][idx]);
	}
	fprintf(stderr, "\n");
}

static void print_ptp_state(uint32_t val)
{
	static char *ptpstat_str[] = {
		"None",
		"PPS initializing",
		"PPS faulty",
		"disabled",
		"PPS listening",
		"PPS pre-master",
		"PPS master",
		"PPS passive",
		"PPS uncalibrated",
		"PPS slave",
	};

	fprintf(stderr, "PTP state:\t\t");
	if (val <= 9)
		fprintf(stderr, "%s", ptpstat_str[val]);
	else if (val >= 100 && val <= 116)
		fprintf(stderr, "WR STATES(see ppsi/ieee1588_types.h): %d", val);
	else
		fprintf(stderr, "Unknown");
	fprintf(stderr, "\n");
}

static void print_aux_state(uint32_t val)
{
	int nch = 8; //should be retrieved from a register
	int i;

	fprintf(stderr, "Aux state:\t\t");
	for (i = 0; i < nch; i++) {
		if (val & (1 << i))
			fprintf(stderr, "ch%d:enabled ", i);
	}
	fprintf(stderr, "\n");
}

static void print_tx_frame_count(uint32_t val)
{
	fprintf(stderr, "TX frame count:\t\t%d\n", val);
}

static void print_rx_frame_count(uint32_t val)
{
	fprintf(stderr, "RX frame count:\t\t%d\n", val);
}

static void print_local_time(uint32_t sec_msw, uint32_t sec_lsw, uint32_t ns)
{
	uint64_t sec = (uint64_t)(sec_msw) << 32 | sec_lsw;
//	fprintf(stderr, "TAI time:\t\t %" PRIu64 "sec %d nsec\n",
//		sec, ns);
	fprintf(stderr, "TAI time:\t\t%s", ctime((time_t *)&sec));
}

static void print_roundtrip_time(uint32_t msw, uint32_t lsw)
{
	uint64_t val = (uint64_t)(msw) << 32 | lsw;
	fprintf(stderr, "Round trip time:\t%" PRIu64 " ps\n", val);
}

static void print_master_slave_delay(uint32_t msw, uint32_t lsw)
{
	uint64_t val = (uint64_t)(msw) << 32 | lsw;
	fprintf(stderr, "Master slave delay:\t%" PRIu64 " ps\n", val);
}

static void print_link_asym(uint32_t val)
{
	fprintf(stderr, "Total Link asymmetry:\t%d ps\n", val);
}

static void print_clock_offset(uint32_t val)
{
	fprintf(stderr, "Clock offset:\t\t%d ps\n", val);
}

static void print_phase_setpoint(uint32_t val)
{
	fprintf(stderr, "Phase setpoint:\t\t%d ps\n", val);
}

static void print_update_counter(uint32_t val)
{
	fprintf(stderr, "Update counter:\t\t%d\n", val);
}

static void print_board_temp(uint32_t val)
{
	 fprintf(stderr, "temp:\t\t\t%d.%04d C\n", val >> 16,
	 	   (int)((val & 0xffff) * 10 * 1000 >> 16));
}

enum wrcdiag_cmd_id{
	WRCDIAG_CMD_DIAGS = CMD_USR,
	WRCDIAG_CMD_SSTAT,
	WRCDIAG_CMD_PTPSTAT,
	WRCDIAG_CMD_PSTAT,
	WRCDIAG_CMD_ASTAT,
	WRCDIAG_CMD_TXFCNT,
	WRCDIAG_CMD_RXFCNT,
	WRCDIAG_CMD_LTIME,
	WRCDIAG_CMD_RTTIME,
	WRCDIAG_CMD_MSTRSLAVEDELAY,
	WRCDIAG_CMD_LINKASYM,
	WRCDIAG_CMD_CKOFFSET,
	WRCDIAG_CMD_PHASESETPOINT,
	WRCDIAG_CMD_UPDATECNT,
	WRCDIAG_CMD_TEMP,
	WRCDIAG_CMD_LAST,
};

static int read_diags(struct cmd_desc *cmdd, struct atom *atoms)
{
	volatile struct WRC_DIAGS_WB *ptr =
		(volatile struct WRC_DIAGS_WB *)wrcdiag->base;
	int res;

	if (atoms == (struct atom *)VERBOSE_HELP) {
		printf("%s - %s\n", cmdd->name, cmdd->help);
		return 1;
	}

	res = lock_diag(ptr);
	if (res) {
		fprintf(stderr, "Cmd is not executed\n");
		return 1;
	}

	switch (cmdd->id) {
	case WRCDIAG_CMD_DIAGS:
		print_servo_status(iomemr32(wrcdiag->is_be, ptr->WDIAG_SSTAT));
		print_port_status(iomemr32(wrcdiag->is_be, ptr->WDIAG_PSTAT));
		print_ptp_state(iomemr32(wrcdiag->is_be, ptr->WDIAG_PTPSTAT));
		print_aux_state(iomemr32(wrcdiag->is_be, ptr->WDIAG_ASTAT));
		print_tx_frame_count(iomemr32(wrcdiag->is_be, ptr->WDIAG_TXFCNT));
		print_rx_frame_count(iomemr32(wrcdiag->is_be, ptr->WDIAG_RXFCNT));
		print_local_time(iomemr32(wrcdiag->is_be, ptr->WDIAG_SEC_MSB),
				 iomemr32(wrcdiag->is_be, ptr->WDIAG_SEC_LSB),
				 iomemr32(wrcdiag->is_be, ptr->WDIAG_NS));
		print_roundtrip_time(iomemr32(wrcdiag->is_be, ptr->WDIAG_MU_MSB),
				     iomemr32(wrcdiag->is_be, ptr->WDIAG_MU_LSB));
		print_master_slave_delay(iomemr32(wrcdiag->is_be,
						  ptr->WDIAG_DMS_MSB),
					 iomemr32(wrcdiag->is_be,
					 	  ptr->WDIAG_DMS_LSB));
		print_link_asym(iomemr32(wrcdiag->is_be, ptr->WDIAG_ASYM));
		print_clock_offset(iomemr32(wrcdiag->is_be, ptr->WDIAG_CKO));
		print_phase_setpoint(iomemr32(wrcdiag->is_be, ptr->WDIAG_SETP));
		print_update_counter(iomemr32(wrcdiag->is_be, ptr->WDIAG_UCNT));
		print_board_temp(iomemr32(wrcdiag->is_be, ptr->WDIAG_TEMP));
		break;
	case WRCDIAG_CMD_SSTAT:
		print_servo_status(iomemr32(wrcdiag->is_be, ptr->WDIAG_SSTAT));
		break;
	case WRCDIAG_CMD_PSTAT:
		print_port_status(iomemr32(wrcdiag->is_be, ptr->WDIAG_PSTAT));
		break;
	case WRCDIAG_CMD_PTPSTAT:
		print_ptp_state(iomemr32(wrcdiag->is_be, ptr->WDIAG_PTPSTAT));
		break;
	case WRCDIAG_CMD_ASTAT:
		print_aux_state(iomemr32(wrcdiag->is_be, ptr->WDIAG_ASTAT));
		break;
	case WRCDIAG_CMD_TXFCNT:
		print_tx_frame_count(iomemr32(wrcdiag->is_be,
					      ptr->WDIAG_TXFCNT));
		break;
	case WRCDIAG_CMD_RXFCNT:
		print_rx_frame_count(iomemr32(wrcdiag->is_be,
					      ptr->WDIAG_RXFCNT));
		break;
	case WRCDIAG_CMD_LTIME:
		print_local_time(iomemr32(wrcdiag->is_be, ptr->WDIAG_SEC_MSB),
				 iomemr32(wrcdiag->is_be, ptr->WDIAG_SEC_LSB),
				 iomemr32(wrcdiag->is_be, ptr->WDIAG_NS));
		break;
	case WRCDIAG_CMD_RTTIME:
		print_roundtrip_time(iomemr32(wrcdiag->is_be,
					      ptr->WDIAG_MU_MSB),
			             iomemr32(wrcdiag->is_be,
				     	      ptr->WDIAG_MU_LSB));
		break;
	case WRCDIAG_CMD_MSTRSLAVEDELAY:
		print_master_slave_delay(iomemr32(wrcdiag->is_be,
						  ptr->WDIAG_DMS_MSB),
			                 iomemr32(wrcdiag->is_be,
					   	  ptr->WDIAG_DMS_LSB));
		break;
	case WRCDIAG_CMD_LINKASYM:
		print_link_asym(iomemr32(wrcdiag->is_be, ptr->WDIAG_ASYM));
		break;
	case WRCDIAG_CMD_CKOFFSET:
		print_clock_offset(iomemr32(wrcdiag->is_be, ptr->WDIAG_CKO));
		break;
	case WRCDIAG_CMD_PHASESETPOINT:
		print_phase_setpoint(iomemr32(wrcdiag->is_be, ptr->WDIAG_SETP));
		break;
	case WRCDIAG_CMD_UPDATECNT:
		print_update_counter(iomemr32(wrcdiag->is_be, ptr->WDIAG_UCNT));
		break;
	case WRCDIAG_CMD_TEMP:
		print_board_temp(iomemr32(wrcdiag->is_be, ptr->WDIAG_TEMP));
		break;
	}

	unlock_diag(ptr);

	return 1;
}

void print_version(void)
{
	fprintf(stderr, "Built in wrpc-sw repo ver:%s, by %s on %s %s\n",
		__GIT_VER__, __GIT_USR__, __TIME__, __DATE__);
}

static void wrcdiag_help(char *prog)
{
	fprintf(stderr, "%s [options]\n", prog);
	fprintf(stderr, "%s\n", dev_mapping_help());
	print_version();
}

static void sig_hndl()
{
	volatile struct WRC_DIAGS_WB *ptr =
		(volatile struct WRC_DIAGS_WB *)wrcdiag->base;

	// Signal occured: free resource and exit
	fprintf(stderr, "Handle signal: free resource and exit.\n");
	unlock_diag(ptr); // clean snapshot
	dev_unmap(wrcdiag);
	exit(1);
}

#define WRCDIAG_CMD_NB WRCDIAG_CMD_LAST - CMD_USR
struct cmd_desc wrcdiag_cmd[WRCDIAG_CMD_NB + 1] = {
	{ 1, WRCDIAG_CMD_DIAGS, "diags", "show all wrc diags",
	  "", 0, read_diags},
	{ 1, WRCDIAG_CMD_SSTAT, "sstat",
	  "get servo status", "", 0, read_diags},
	{ 1, WRCDIAG_CMD_PTPSTAT, "ptpstat",
	  "get PTP state", "", 0, read_diags},
	{ 1, WRCDIAG_CMD_PSTAT, "pstat",
	  "get port status", "", 0, read_diags},
	{ 1, WRCDIAG_CMD_ASTAT, "astat",
	  "get auxiliare state", "", 1, read_diags},
	{ 1, WRCDIAG_CMD_TXFCNT, "txfcnt",
	  "get TX PTP frame count", "", 1, read_diags},
	{ 1, WRCDIAG_CMD_RXFCNT, "rxfcnt",
	  "get RX frame count", "", 1, read_diags},
	{ 1, WRCDIAG_CMD_LTIME, "ltime",
	  "Local Time expressed in sec since epoch", "", 1, read_diags},
	{ 1, WRCDIAG_CMD_RTTIME, "rttime",
	  "Round trip time in picoseconds", "", 1, read_diags},
	{ 1, WRCDIAG_CMD_MSTRSLAVEDELAY, "msdelay",
	  "Master slave link delay in picoseconds", "", 1, read_diags},
	{ 1, WRCDIAG_CMD_LINKASYM, "asym",
	  "Total link asymmetry in picoseonds", "", 1, read_diags},
	{ 1, WRCDIAG_CMD_CKOFFSET, "cko",
	  "Clock offset in picoseonds", "", 1, read_diags},
	{ 1, WRCDIAG_CMD_PHASESETPOINT, "setp",
	  "Current slave's clock phase shift value", "", 1, read_diags},
	{ 1, WRCDIAG_CMD_UPDATECNT, "ucnt",
	  "Update counter", "", 1, read_diags},
	{ 1, WRCDIAG_CMD_TEMP, "temp",
	  "get board temperature", "", 1, read_diags},
	{0, },
};

static int verify_reg_version()
{
	volatile struct WRC_DIAGS_WB *ptr =
		(volatile struct WRC_DIAGS_WB *)wrcdiag->base;
	uint32_t ver = 0;
	ver = iomemr32(wrcdiag->is_be, ptr->VER);
	fprintf(stderr, "Wishbone register version: in FPGA = 0x%x |"
		" in SW = 0x%x\n", ver, WBGEN2_WRC_DIAGS_VERSION);
	if(ver != WBGEN2_WRC_DIAGS_VERSION)
		return -1;
	else
		return 0;
}

int main(int argc, char *argv[])
{
	int ret;
	struct mapping_args *map_args;

	map_args = dev_parse_mapping_args(argc, argv);
	if (!map_args) {
		wrcdiag_help(argv[0]);
		return -1;
	}

	wrcdiag = dev_map(map_args, sizeof(struct WRC_DIAGS_WB));
	if (!wrcdiag) {
		fprintf(stderr, "%s: wrcdiag mmap() failed: %s\n", argv[0],
			strerror(errno));
		free(map_args);
		return -1;
	}

	ret = verify_reg_version();
	if (ret) {
		fprintf(stderr, "Register version in FPGA and SW does not match\n");
		dev_unmap(wrcdiag);
		return -1;
	}
	
	ret = extest_register_user_cmd(wrcdiag_cmd, WRCDIAG_CMD_NB);
	if (ret) {
		dev_unmap(wrcdiag);
		return -1;
	}

	/* execute command loop */
	ret = extest_run("wrcdiag", sig_hndl);

	dev_unmap(wrcdiag);
	return (ret) ? -1 : 0;
}
