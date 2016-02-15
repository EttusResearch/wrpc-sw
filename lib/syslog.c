#include <string.h>
#include <wrc.h>
#include <wrpc.h>
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

	if (args[0] && !strcmp(args[0], "off")) {
		syslog_addr.daddr = 0;
		return 0;
	}
	if (!args[1]) {
		pp_printf("use: syslog <ipaddr> <macaddr> (or just \"off\"\n");
		return -1;
	}
	decode_ip(args[0], (void *)&syslog_addr.daddr);
	decode_mac(args[1], syslog_mac);
	pp_printf("Syslog parameters: %s, %s\n",
		  format_ip(b1, (void *)&syslog_addr.daddr),
		  format_mac(b2, syslog_mac));
	tics = 0; /* send the first frame immediately to the new host */
	return 0;
}

DEFINE_WRC_COMMAND(mac) = {
	.name = "syslog",
	.exec = cmd_syslog,
};


int syslog_poll(int l_status)
{
	struct wr_sockaddr addr;
	char buf[256];
	char b1[32], b2[32];
	unsigned char mac[6];
	unsigned char ip[4];
	uint64_t secs;
	static uint32_t down_tics;
	int len = 0;

	if (ip_status == IP_TRAINING)
		return 0;
	if (!syslog_addr.daddr)
		return 0;

	if (!tics) {
		/* first time ever, or new syslog server */
		tics = timer_get_tics() - 1;
		shw_pps_gen_get_time(&secs, NULL);
		get_mac_addr(mac);
		getIP(ip);
		len = pp_sprintf(buf + UDP_END, /* 8 == user + 6 == info */
				 "<14> %s %s (%s) Node up "
				 "since %i seconds\n",
				 format_time(secs, TIME_FORMAT_SYSLOG),
				 format_ip(b1, ip), format_mac(b2, mac),
				 (tics - tics_zero) / 1000);
		goto send;
	}

	if (l_status == LINK_WENT_DOWN)
		down_tics = timer_get_tics();
	if (l_status == LINK_UP && down_tics) {
		down_tics = timer_get_tics() - down_tics;
		shw_pps_gen_get_time(&secs, NULL);
		getIP(ip);
		len = pp_sprintf(buf + UDP_END, /* 8 == user + 6 == info */
				 "<14> %s %s Link up after %i.%03i s\n",
				 format_time(secs, TIME_FORMAT_SYSLOG),
				 format_ip(b1, ip),
				 down_tics / 1000, down_tics % 1000);
		down_tics = 0;
		goto send;
	}

	return 0;

send:
	len += UDP_END;
	memcpy(&syslog_addr.saddr, ip, 4);
	fill_udp((void *)buf, len, &syslog_addr);
	memcpy(&addr.mac, syslog_mac, 6);
	ptpd_netif_sendto(syslog_socket, &addr, buf, len, 0);
	return 1;
}

