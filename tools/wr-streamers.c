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

#include <hw/wr-streamer.h>

static struct mapping_desc *wrstm = NULL;

static char *stats_mesg[][2] = {
	{"Number of sent wr streamer frames since reset", "tx_cnt"},
        {"Number of received wr streamer frames since reset", "rx_cnt"},
        {"Number of lost wr streamer frames since reset", "rx_cnt_lost_fr"},
        {"Maximum latency of received frames since reset", "max_lat_raw"},
        {"Minimum latency of received frames since reset", "min_lat_raw"},
        {"Accumulated latency of received frames since reset", "acc_lat"},
        {"Counter of the accumulated frequency", "acc_freq_cnt"},
        {"Number of indications that one or more blocks in a"
	 "frame were lost (probably CRC error) since reset", "rx_cnt_lost_blk"},
};

int read_stats(struct cmd_desc *cmdd, struct atom *atoms)
{
	volatile struct WR_STREAMERS_WB *ptr =
		(volatile struct WR_STREAMERS_WB *)wrstm->base;
	int val, overflow;
	double max_lat, min_lat, avg_lat;
	uint64_t acc_lat;

	if (atoms == (struct atom *)VERBOSE_HELP) {
		printf("%s - options\n"
		       "\t0: %s\n\t1: %s\n\t2: %s\n\t3: %s\n\t4: %s\n\t5: %s\n"
		       "\t6: %s\n\t7: %s\n", cmdd->name, stats_mesg[0][0],
		       stats_mesg[1][0], stats_mesg[2][0], stats_mesg[3][0],
		       stats_mesg[4][0], stats_mesg[5][0], stats_mesg[6][0],
		       stats_mesg[7][0]);
		return 1;
	}

	++atoms;
	if (atoms->type != Terminator && atoms->type != Numeric)
		return -TST_ERR_WRONG_ARG;
	val = -1; // all stats
	if (atoms->type == Numeric)
		val = atoms->val; // specific stats
	//snapshot stats
	ptr->SSCR1 = WR_STREAMERS_SSCR1_SNAPSHOT_STATS;

	switch(val) {
	case -1:
		max_lat = WR_STREAMERS_RX_STAT3_RX_LATENCY_MAX_R(ptr->RX_STAT3);
		max_lat = (max_lat * 8) / 1000.0;
		min_lat = WR_STREAMERS_RX_STAT3_RX_LATENCY_MAX_R(ptr->RX_STAT4);
		min_lat = (min_lat * 8) / 1000.0;
		val = WR_STREAMERS_SSCR1_RX_LATENCY_ACC_OVERFLOW &
		      ptr->RX_STAT7;
		overflow = val != 0;
		//put it all together
		acc_lat = (((uint64_t)ptr->RX_STAT6) << 32) | ptr->RX_STAT5;
		avg_lat = (((double)acc_lat) * 8 / 1000) / (double)ptr->RX_STAT7;
		fprintf(stderr, "Latency [us]    : min=%10g max=%10g avg =%10g "
			"(0x%x, 0x%x, %lld=%u << 32 | %u)*8/1000 us, cnt=%u)\n",
			min_lat, max_lat, avg_lat, ptr->RX_STAT4,
			ptr->RX_STAT3, (long long)acc_lat, ptr->RX_STAT6,
			ptr->RX_STAT5, ptr->RX_STAT7);
		fprintf(stderr, "Frames  [number]: tx =%10u rx =%10u lost=%10u "
			"(lost blocks%5u)\n",
			ptr->TX_STAT, ptr->RX_STAT1, ptr->RX_STAT2,
			ptr->RX_STAT8);
		break;
	case 0:
		fprintf(stderr, "%s:%10u\n", stats_mesg[0][1], ptr->RX_STAT1);
		break;
	case 1:
		fprintf(stderr, "%s:%10u\n", stats_mesg[1][1], ptr->RX_STAT2);
		break;
	case 2:
		fprintf(stderr, "%s: 0x%x\n", stats_mesg[2][1], ptr->RX_STAT3);
		break;
	case 3:
		fprintf(stderr, "%s: 0x%x\n", stats_mesg[3][1], ptr->RX_STAT4);
		break;
	case 4:
		fprintf(stderr, "%s:%10u\n", stats_mesg[4][1], ptr->RX_STAT5);
		break;
	case 5:
		fprintf(stderr, "%s:%10u\n", stats_mesg[5][1], ptr->RX_STAT6);
		break;
	case 6:
		fprintf(stderr, "%s:%10u\n", stats_mesg[6][1], ptr->RX_STAT7);
		break;
	case 7:
		fprintf(stderr, "%s: 0x%x\n", stats_mesg[7][1], ptr->RX_STAT8);
		break;
	}

	//release snapshot
	ptr->SSCR1 = 0;

	return 1;
}

#define LEAP_SECONDS 37
int read_reset_time(struct cmd_desc *cmdd, struct atom *atoms)
{
	uint32_t val=0;
	int days=0, hours=0, minutes=0, seconds;
	double reset_time_elapsed=0;
	time_t cur_time;
	time_t res_time_sec;
	volatile struct WR_STREAMERS_WB *ptr =
		(volatile struct WR_STREAMERS_WB *)wrstm->base;

	if (atoms == (struct atom *)VERBOSE_HELP) {
		printf("%s - %s\n", cmdd->name, cmdd->help);
		return 1;
	}

	val = ptr->SSCR2;
	res_time_sec = (time_t)(WR_STREAMERS_SSCR2_RST_TS_TAI_LSB_R(val) +
		       LEAP_SECONDS);//to UTC

	cur_time           = time(NULL);
	reset_time_elapsed = difftime(cur_time,res_time_sec);
	days               =  reset_time_elapsed/(60*60*24);
	hours              = (reset_time_elapsed-days*60*60*24)/(60*60);
	minutes            = (reset_time_elapsed-days*60*60*24-hours*60*60)/(60);
	seconds            = (reset_time_elapsed-days*60*60*24-hours*60*60-minutes*60);
	fprintf(stderr, "Time elapsed from reset: %d days, %d h, %d m, %d s; Reseted on %s\n",
		days, hours, minutes, seconds, asctime(localtime(&res_time_sec)));
	return 1;
}

int reset_counters(struct cmd_desc *cmdd, struct atom *atoms)
{
	volatile struct WR_STREAMERS_WB *ptr =
		(volatile struct WR_STREAMERS_WB *)wrstm->base;

	if (atoms == (struct atom *)VERBOSE_HELP) {
		printf("%s - %s\n", cmdd->name, cmdd->help);
		return 1;
	}

	ptr->SSCR1 = WR_STREAMERS_SSCR1_RST_STATS;
	fprintf(stderr, "Reseted statistics counters\n");
	return 1;
}

int reset_seqid(struct cmd_desc *cmdd, struct atom *atoms)
{
	volatile struct WR_STREAMERS_WB *ptr =
		(volatile struct WR_STREAMERS_WB *)wrstm->base;

	if (atoms == (struct atom *)VERBOSE_HELP) {
		printf("%s - %s\n", cmdd->name, cmdd->help);
		return 1;
	}

	ptr->SSCR1 = WR_STREAMERS_SSCR1_RST_SEQ_ID;
	return 1;
}

int set_tx_ethertype(struct cmd_desc *cmdd, struct atom *atoms)
{
	fprintf(stderr, "Not impleented\n");
	return 1;
}

int set_tx_local_mac(struct cmd_desc *cmdd, struct atom *atoms)
{
	fprintf(stderr, "Not impleented\n");
	return 1;
}

int set_tx_remote_mac(struct cmd_desc *cmdd, struct atom *atoms)
{
	fprintf(stderr, "Not impleented\n");
	return 1;
}

int set_rx_ethertype(struct cmd_desc *cmdd, struct atom *atoms)
{
	fprintf(stderr, "Not impleented\n");
	return 1;
}

int set_rx_local_mac(struct cmd_desc *cmdd, struct atom *atoms)
{
	fprintf(stderr, "Not impleented\n");
	return 1;
}

int set_rx_remote_mac(struct cmd_desc *cmdd, struct atom *atoms)
{
	fprintf(stderr, "Not impleented\n");
	return 1;
}

int get_set_latency(struct cmd_desc *cmdd, struct atom *atoms)
{
	volatile struct WR_STREAMERS_WB *ptr =
		(volatile struct WR_STREAMERS_WB *)wrstm->base;
	int lat;
	uint32_t val;

	if (atoms == (struct atom *)VERBOSE_HELP) {
		printf("%s - %s\n", cmdd->name, cmdd->help);
		return 1;
	}

	++atoms;
	if (atoms->type == Terminator) {
		// Get Latency
		fprintf(stderr, "Fixed latency configured: %d [us]\n",
			ptr->RX_CFG5);
	}
	else {
		if (atoms->type != Numeric)
			return -TST_ERR_WRONG_ARG;
		lat = atoms->val;
		if (lat < 0) {
			val = ~WR_STREAMERS_CFG_OR_RX_FIX_LAT & ptr->CFG;
			ptr->CFG = val;
			fprintf(stderr, "Disabled overriding of default fixed "
				"latency value (it is now the default/set "
				"by application)\n");
		}
		else {
			val = (lat * 1000) / 8;
			ptr->RX_CFG5 = WR_STREAMERS_RX_CFG5_FIXED_LATENCY_W(val);
			val = ptr->CFG;
			val |= WR_STREAMERS_CFG_OR_RX_FIX_LAT;
			ptr->CFG = val;
			val = WR_STREAMERS_RX_CFG5_FIXED_LATENCY_R(ptr->RX_CFG5);
			fprintf(stderr, "Fixed latency set: %d [us] "
				"(set %d | read : %d [cycles])\n",
				lat, ((lat * 1000) / 8), val);
		}
	}

	return 1;
}

enum wrstm_cmd_id{
	WRSTM_CMD_STATS = CMD_USR,
	WRSTM_CMD_RESET,
	WRSTM_CMD_RESET_CNTS,
	WRSTM_CMD_RESET_SEQID,
	WRSTM_CMD_TX_ETHERTYPE,
	WRSTM_CMD_TX_LOC_MAC,
	WRSTM_CMD_TX_REM_MAC,
	WRSTM_CMD_RX_ETHERTYPE,
	WRSTM_CMD_RX_LOC_MAC,
	WRSTM_CMD_RX_REM_MAC,
	WRSTM_CMD_LATENCY,
//	WRSTM_CMD_DBG_BYTE,
//	WRSTM_CMD_DBG_MUX,
//	WRSTM_CMD_DBG_VAL,
	WRSTM_CMD_LAST,
};

#define WRSTM_CMD_NB WRSTM_CMD_LAST - CMD_USR
struct cmd_desc wrstm_cmd[WRSTM_CMD_NB + 1] = {
	{ 1, WRSTM_CMD_STATS, "stats", "show streamers statistics",
	  "[0/1/2/3/4/5/6/7]", 0, read_stats},
	{ 1, WRSTM_CMD_RESET, "reset",
	  "show time of the latest reset / time elapsed since then", "", 0,
	  read_reset_time},
	{ 1, WRSTM_CMD_RESET_CNTS, "resetcnt",
	  "reset tx/rx/lost counters and avg/min/max latency values", "", 0,
	  reset_counters},
	{ 1, WRSTM_CMD_RESET_SEQID, "resetseqid",
	  "reset sequence ID of the tx streamer", "", 0, reset_seqid},
	{ 1, WRSTM_CMD_TX_ETHERTYPE, "txether",
	  "set TX ethertype", "ethertype", 1, set_tx_ethertype},
	{ 1, WRSTM_CMD_TX_LOC_MAC, "txlocmac",
	  "set TX Local  MAC addres", "mac", 1, set_tx_local_mac},
	{ 1, WRSTM_CMD_TX_REM_MAC, "txremmac",
	  "set TX Target MAC address", "mac", 1, set_tx_remote_mac},
	{ 1, WRSTM_CMD_RX_ETHERTYPE, "rxether",
	  "set RX ethertype", "ethertype", 1, set_rx_ethertype},
	{ 1, WRSTM_CMD_RX_LOC_MAC, "rxlocmac",
	  "set RX Local  MAC addres", "mac", 1, set_rx_local_mac},
	{ 1, WRSTM_CMD_RX_REM_MAC, "rxremmac",
	  "set RX Remote MAC address", "mac", 1, set_rx_remote_mac},
	{ 1, WRSTM_CMD_LATENCY, "lat",
	  "get/set config of fixed latency in integer [us] (-1 to disable)",
	  "[latency]", 0, get_set_latency},
//	{ 1, WRSTM_CMD_DBG_BYTE, "dbgbyte",
//	  "set which byte of the rx or tx frame should be snooped", "byte", 1,},
//	{ 1, WRSTM_CMD_DBG_MUX, "dbgdir",
//	  "set whether tx or rx frames should be snooped", "dir", 1,},
//	{ 1, WRSTM_CMD_DBG_VAL, "dbgword",
//	  "read the snooped 32-bit value", "", 0,},
	{0, },
};

static void wrstm_help(char *prog)
{
	fprintf(stderr, "%s [options]\n", prog);
	fprintf(stderr, "%s\n", dev_mapping_help());
}

static void sig_hndl()
{
	// Signal occured: free resource and exit
	fprintf(stderr, "Handle signal: free resource and exit.\n");
	dev_unmap(wrstm);
	exit(1);
}

int main(int argc, char *argv[])
{
	int ret;
	struct mapping_args *map_args;

	map_args = dev_parse_mapping_args(argc, argv);
	if (!map_args) {
		wrstm_help(argv[0]);
		return -1;
	}

	wrstm = dev_map(map_args, sizeof(struct WR_STREAMERS_WB));
	if (!wrstm) {
		fprintf(stderr, "%s: wrstm mmap() failed: %s\n", argv[0],
			strerror(errno));
		free(map_args);
		return -1;
	}

	ret = extest_register_user_cmd(wrstm_cmd, WRSTM_CMD_NB);
	if (ret) {
		dev_unmap(wrstm);
		return -1;
	}

	/* execute command loop */
	ret = extest_run("wrtsm", sig_hndl);

	dev_unmap(wrstm);
	return (ret) ? -1 : 0;
}
