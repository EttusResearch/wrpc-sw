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

void syslog_init(void)
{
	syslog_socket = ptpd_netif_create_socket(&__static_syslog_socket, NULL,
					       PTPD_SOCK_UDP, 514 /* time */);
	syslog_addr.sport = syslog_addr.dport = htons(514);
}

static uint32_t tics;

static int cmd_syslog(const char *args[])
{
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
	print_ip("Syslog parameters: ", (void *)&syslog_addr.daddr, ", ");
	print_mac("", syslog_mac, "\n");
	tics = 0; /* send the first frame immediately to the new host */
	return 0;
}

DEFINE_WRC_COMMAND(mac) = {
	.name = "syslog",
	.exec = cmd_syslog,
};


void syslog_poll(void)
{
	struct wr_sockaddr addr;
	char buf[128];
	uint64_t secs;
	int len;

	if (needIP)
		return;
	if (!syslog_addr.daddr)
		return;

	if (!tics) /* first time ever */
		tics = timer_get_tics() - 1;

	if (time_before(timer_get_tics(), tics))
		return;

	getIP((void *)&syslog_addr.saddr);

	shw_pps_gen_get_time(&secs, NULL);
	len = pp_sprintf(buf + UDP_END, /* 8 == user + 6 == info */
			 "<14> %s wr-node: Alive and Well", format_time(secs));
	len += UDP_END;
	memcpy(&syslog_addr.saddr, ip, 4);
	fill_udp((void *)buf, len, &syslog_addr);
	memcpy(&addr.mac, syslog_mac, 6);
	ptpd_netif_sendto(syslog_socket, &addr, buf, len, 0);

	tics += 10 * TICS_PER_SECOND;

}

