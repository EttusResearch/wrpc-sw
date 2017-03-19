#include <wrc.h>
#include <wrpc.h>
#include <string.h>
#include <shell.h>
#include <lib/ipv4.h>


/* a tx-only socket: no queue is there */
static struct wrpc_socket __static_daclog_socket = {
	.queue.buff = NULL,
	.queue.size = 0,
};
static struct wrpc_socket *daclog_socket;
static struct wr_udp_addr daclog_addr;
unsigned char daclog_mac[6];


/* alternate between two buffers */
#define BSIZE 512
struct daclog_buf {
	unsigned char hdr[UDP_END];
	uint16_t data[BSIZE];
};

static struct daclog_buf buffers[2];
static int  ready[2];

void spll_log_dac(int y);
void spll_log_dac(int y)
{
	static int bindex, bcount;

	buffers[bindex].data[bcount++] = y;
	if (bcount == BSIZE) {
		ready[bindex] = 1;
		bindex = !bindex;
		bcount = 0;
	}
}


static void daclog_init(void)
{
	daclog_socket = ptpd_netif_create_socket(&__static_daclog_socket, NULL,
						 PTPD_SOCK_UDP, 1050);
	daclog_addr.sport = daclog_addr.dport = htons(1050);
}

static int configured;

static int daclog_poll(void)
{
	struct wr_sockaddr addr;
	int len = sizeof(struct daclog_buf);
	struct daclog_buf *b = NULL;

	if (!configured)
		return 0;

	if (ready[0]) {
		b = buffers + 0;
		ready[0] = 0;
	} else if (ready[1]) {
		b = buffers + 1;
		ready[1] = 0;
	}
	if (!b)
		return 0;

	/* format and send */
	getIP((void *)&daclog_addr.saddr); /* if we have no ip yet, 0 is ok */
	fill_udp((void *)b, len, &daclog_addr);
	memcpy(&addr.mac, daclog_mac, 6);
	ptpd_netif_sendto(daclog_socket, &addr, b, len, 0);
	return 1;
}

DEFINE_WRC_TASK(daclog) = {
	.name = "daclog",
	.init = daclog_init,
	.job = daclog_poll,
};

static int cmd_daclog(const char *args[])
{
	char b1[32], b2[32];

	if (args[0] && !strcmp(args[0], "off")) {
		configured = 0;
		return 0;
	}
	if (!args[1]) {
		pp_printf("use: daclog <ipaddr> <macaddr> (or just \"off\"\n");
		return -1;
	}
	decode_ip(args[0], (void *)&daclog_addr.daddr);
	decode_mac(args[1], daclog_mac);
	pp_printf("Dac logger parameters: %s, %s, port 1050\n",
		  format_ip(b1, (void *)&daclog_addr.daddr),
		  format_mac(b2, daclog_mac));
	configured = 1;
	ready[0] = ready[1] = 0; /* try to ensure data ordering */
	return 0;
}


DEFINE_WRC_COMMAND(daclog) = {
	.name = "daclog",
	.exec = cmd_daclog,
};
