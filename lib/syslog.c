#include <string.h>
#include <wrc.h>
#include <wrpc.h>
#include <temperature.h>
#include "endpoint.h"
#include "minic.h"
#include "shell.h"
#include "pps_gen.h"

#include "ipv4.h"

/* syslog: a tx-only socket: no queue is there */
static struct wrpc_socket __static_syslog_socket = {
	.queue.buff = NULL,
	.queue.size = 0,
};
static struct wrpc_socket *syslog_socket;

static struct wr_udp_addr syslog_addr;
unsigned char syslog_mac[6];

static uint32_t tics, tics_zero;

void syslog_init(void)
{
	syslog_socket = ptpd_netif_create_socket(&__static_syslog_socket, NULL,
					       PTPD_SOCK_UDP, 514 /* time */);
	syslog_addr.sport = syslog_addr.dport = htons(514);
	tics_zero = timer_get_tics();
}

static int cmd_syslog(const char *args[])
{
	char b1[32], b2[32];
	char *p;
	int i;

	if (args[0] && !strcmp(args[0], "off")) {
		syslog_addr.daddr = 0;
		return 0;
	}
	if (!args[1]) {
		pp_printf("use: syslog <ipaddr> <macaddr> (or just \"off\"\n");
		pp_printf(" or  syslog msg  ....\n");
		return -1;
	}
	if (!strcmp(args[0], "msg")) {
		for (i = 0; args[i+1]; i++)
			;
		/* now, args[i] is the last argument; undo previous nulls */
		for (p = (void *)args[i]; p > args[1]; p--) /* not "const"! */
			if (*p == '\0')
				*p = ' ';
		syslog_report(args[1]);
		return 0;
	}
	decode_ip(args[0], (void *)&syslog_addr.daddr);
	decode_mac(args[1], syslog_mac);
	pp_printf("Syslog parameters: %s, %s\n",
		  format_ip(b1, (void *)&syslog_addr.daddr),
		  format_mac(b2, syslog_mac));
	tics = 0; /* send the first frame immediately to the new host */
	return 0;
}

DEFINE_WRC_COMMAND(syslog) = {
	.name = "syslog",
	.exec = cmd_syslog,
};

#define SYSLOG_DEFAULT_LEVEL 14 /* 8 == user + 6 ==info */
static int syslog_header(char *buf, int level, unsigned char ip[4])
{
	uint64_t secs;
	char b[32];
	int len;

	shw_pps_gen_get_time(&secs, NULL);
	getIP(ip);
	len = pp_sprintf(buf + UDP_END, "<%i> %s %s ", level,
			 format_time(secs, TIME_FORMAT_SYSLOG),
			 format_ip(b, ip));
	return len + UDP_END;
}

static void syslog_send(void *buf, unsigned char *ip, int len)
{
	struct wr_sockaddr addr;

	memcpy(&syslog_addr.saddr, ip, 4);
	fill_udp((void *)buf, len, &syslog_addr);
	memcpy(&addr.mac, syslog_mac, 6);
	ptpd_netif_sendto(syslog_socket, &addr, buf, len, 0);
	return;
}


int syslog_poll(void)
{
	char buf[256];
	char b[32];
	unsigned char mac[6];
	unsigned char ip[4];
	static uint32_t down_tics;
	int len = 0;
	uint32_t now;

	/* for temperature state */
	static uint32_t next_temp_report, next_temp_check;

	/* for servo-state (accesses ppsi  internal variables */
	extern struct pp_instance *ppi;
	struct wr_servo_state *s;
	static uint32_t prev_tics;
	static int last_setp, worst_delta, bad_track_lost;
	static int track_ok_count, prev_servo_state = -1;

	if (IS_HOST_PROCESS)
		s = NULL;
	else
		s = &((struct wr_data *)ppi->ext_data)->servo_state;

	if (ip_status == IP_TRAINING)
		return 0;
	if (!syslog_addr.daddr)
		return 0;

	now = timer_get_tics();

	if (!tics) {
		/* first time ever, or new syslog server */
		tics = now - 1;
		get_mac_addr(mac);
		len = syslog_header(buf, SYSLOG_DEFAULT_LEVEL, ip);
		len += pp_sprintf(buf + len, "(%s) Node up "
				 "since %i seconds", format_mac(b, mac),
				 (tics - tics_zero) / 1000);
		goto send;
	}

	if (link_status == LINK_WENT_DOWN)
		down_tics = now;
	if (link_status == LINK_UP && down_tics) {
		down_tics = now - down_tics;
		len = syslog_header(buf, SYSLOG_DEFAULT_LEVEL, ip);
		len += pp_sprintf(buf + len, "Link up after %i.%03i s",
				 down_tics / 1000, down_tics % 1000);
		down_tics = 0;
		goto send;
	}

	/*
	 * The following section is all about track-lost events
	 */
	if (!prev_tics)
		prev_tics = now;

	if (s && s->state == WR_TRACK_PHASE) /* monitor setpoint while ok */
		last_setp = s->cur_setpoint;

	if (s && s->state == WR_TRACK_PHASE &&
	    prev_servo_state != WR_TRACK_PHASE) {
		/* we reached sync: log it */
		track_ok_count++;

		prev_servo_state = s->state;
		prev_tics = now - prev_tics;
		if (track_ok_count == 1) {
			len = syslog_header(buf, SYSLOG_DEFAULT_LEVEL, ip);
			len += pp_sprintf(buf + len,
				   "Tracking after %i.%03i s",
				   prev_tics / 1000, prev_tics % 1000);
			goto send;
		}
		len = syslog_header(buf, SYSLOG_DEFAULT_LEVEL, ip);
		len += pp_sprintf(buf + len,
				  "%i-th re-rtrack after %i.%03i s",
				  track_ok_count,
				  prev_tics / 1000, prev_tics % 1000);
		/* Report if we didn't really loose time */
		if (!bad_track_lost)
			len += pp_sprintf(buf + len, " (max delta %i ps)",
					  worst_delta);

		bad_track_lost = worst_delta = 0;
		goto send;
	}

	if (s && s->state == WR_SYNC_PHASE && (s->flags & WR_FLAG_WAIT_HW)) {
		/* 
		 * Passing through SYNC_NSEC is a glimpse, and we won't notice.
		 * Check, rather if we are waiting beofre sync_phase.
		 */
		bad_track_lost = 1;
	}

	if (s && s->state != WR_TRACK_PHASE) {
		int delta = s->cur_setpoint - last_setp;

		/* "abs(x - y)" is not working, unexpectedly) */
		if (delta < 0)
			delta = - delta;
		if (delta > worst_delta)
			worst_delta = delta;
	}

	if (s && s->state != WR_TRACK_PHASE &&
	    prev_servo_state == WR_TRACK_PHASE) {
		prev_servo_state = s->state;
		prev_tics = now;
		len = syslog_header(buf, SYSLOG_DEFAULT_LEVEL, ip);
		len += pp_sprintf(buf + len, "Lost track");
		goto send;
	}

	/*
	 * A section about temperature monitoring
	 */

	if (!next_temp_check) {
		next_temp_check = now + 1000;
		next_temp_report = 0;
	}

	if (time_after_eq(now, next_temp_check)) {
		struct wrc_onetemp *t = NULL;
		int over_t = 0;

		next_temp_check += 1000;
		while ( (t = wrc_temp_getnext(t)) )
			over_t += (t->t > (CONFIG_TEMP_HIGH_THRESHOLD << 16));
		/* report if over temp, and first report or rappel time */
		if (over_t && (!next_temp_report
			       || time_after_eq(now, next_temp_report))) {
			next_temp_report = now + 1000 * CONFIG_TEMP_HIGH_RAPPEL;
			len = syslog_header(buf, SYSLOG_DEFAULT_LEVEL, ip);
			len += pp_sprintf(buf + len, "Temperature high: ");
			len += wrc_temp_format(buf + len, sizeof(buf) - len);
			goto send;
		}
		if (!over_t && next_temp_report) {
			next_temp_report = 0;
			len = syslog_header(buf, SYSLOG_DEFAULT_LEVEL, ip);
			len += pp_sprintf(buf + len, "Temperature ok: ");
			len += wrc_temp_format(buf + len, sizeof(buf) - len);
			goto send;
		}
	}
	return 0;

send:
	syslog_send(buf, ip, len);
	return 1;
}

/* A report tool for others to call (used by ltest at least) */
void syslog_report(const char *msg)
{
	char buf[256];
	unsigned char ip[4];
	int len;

	if (ip_status == IP_TRAINING)
		return;
	if (!syslog_addr.daddr)
		return;

	len = syslog_header(buf, SYSLOG_DEFAULT_LEVEL, ip);
	strcpy(buf + len, msg);
	len += strlen(msg);
	syslog_send(buf, ip, len);
}
