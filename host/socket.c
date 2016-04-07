#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <net/if_arp.h>
#include "include/types.h" /* with "types.h" I might get the standard one... */
#include "endpoint.h"
#include "ptpd_netif.h"
#include "minic.h"
#include "hw/pps_gen_regs.h"

#include "host.h"

static int dumpstruct(FILE *dest, char *name, void *ptr, int size)
{
	int ret, i;
	unsigned char *p = ptr;

	if (!NET_IS_VERBOSE)
		return 0;

	ret = fprintf(dest, "dump %s at %p (size 0x%x)\n", name, ptr, size);
	for (i = 0; i < size; ) {
		ret += fprintf(dest, "%02x", p[i]);
		i++;
		ret += fprintf(dest, i & 3 ? " " : i & 0xf ? "  " : "\n");
	}
	if (i & 0xf)
		ret += fprintf(dest, "\n");
	return ret;
}



static char ifname[16];
static char ethaddr[ETH_ALEN];
static int ethaddr_ok;

int sock;

void minic_init(void)
{
	char *name = getenv("WRPC_MINIC");
	struct ifreq ifr;
	struct packet_mreq req;
	struct sockaddr_ll addr;
	int ethindex;

	if (!name)
		name = "eth0";
	strcpy(ifname, name);
	printf("%s: using %s as interface\n", __func__, ifname);

	sock = socket(PF_PACKET, SOCK_RAW | SOCK_NONBLOCK, ETH_P_1588);
	if (sock < 0) {
		printf("%s: can`t open raw socket: %s\n",
		       __func__, strerror(errno));
	}
	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, ifname);
	if (ioctl(sock, SIOCGIFHWADDR, &ifr) < 0) {
		printf("%s: can`t get hw address of \"%s\": %s\n",
		       __func__, ifname, strerror(errno));
	}
	memcpy(ethaddr, &ifr.ifr_ifru.ifru_hwaddr.sa_data, ETH_ALEN);
	ethaddr_ok = 1;
	strcpy(ifr.ifr_name, ifname);
	if (ioctl(sock, SIOCGIFINDEX, &ifr) < 0) {
		printf("%s: can`t get index of \"%s\": %s\n",
		       __func__, ifname, strerror(errno));
	}
	ethindex = ifr.ifr_ifindex;

	memset(&req, 0, sizeof(req));
	req.mr_ifindex = ethindex;
	req.mr_type = PACKET_MR_PROMISC;

	if (setsockopt(sock, SOL_PACKET, PACKET_ADD_MEMBERSHIP,
		       &req, sizeof(req)) < 0) {
		printf("%s: can`t set \"%s\" to promiscuous mode: %s\n",
		       __func__, ifname, strerror(errno));
	}

	addr.sll_family = AF_PACKET;
	addr.sll_protocol = htons(ETH_P_ALL);
	addr.sll_ifindex = ifr.ifr_ifindex;
	if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		fprintf(stderr, "%s: can't bind to \"%s\": %s\n",
		       __func__, ifname, strerror(errno));
	}

	/* Finally, put the current date into the SEC counter of ppsg */
	{
		uint64_t utc = time(NULL);
		uint32_t *ptr;

		ptr = (void *)BASE_PPS_GEN
			+ offsetof(struct PPSG_WB, CNTR_UTCLO);
		*ptr = utc;
		ptr = (void *)BASE_PPS_GEN
			+ offsetof(struct PPSG_WB, CNTR_UTCHI);
		*ptr = utc >> 32;
	}
}

/* We have a problem: this is called before minic_init(), so we dup code */
void get_mac_addr(uint8_t dev_addr[])
{
	char *name = getenv("WRPC_MINIC");
	struct ifreq ifr;
	int sock;

	if (ethaddr_ok) {
		memcpy(dev_addr, ethaddr, ETH_ALEN);
		return;
	}

	if (!name)
		name = "eth0";
	sock = socket(PF_PACKET, SOCK_RAW | SOCK_NONBLOCK, ETH_P_1588);
	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, name);
	ioctl(sock, SIOCGIFHWADDR, &ifr);
	memcpy(dev_addr, &ifr.ifr_ifru.ifru_hwaddr.sa_data, ETH_ALEN);
	close(sock);
}

void set_mac_addr(uint8_t dev_addr[])
{
	printf("%s: no implemented yet\n", __func__);
}

int minic_rx_frame(struct wr_ethhdr *hdr, uint8_t * payload, uint32_t buf_size,
                   struct hw_timestamp *hwts)
{
	unsigned char frame[1500];
	struct timespec ts;
	int ret;

	ret = recv(sock, frame, sizeof(frame), MSG_DONTWAIT);
	clock_gettime(CLOCK_REALTIME, &ts);

	if (ret < 0 && errno == EAGAIN)
		return 0;
	if (ret < 0) {
		printf("recv(): %s\n", strerror(errno));
		uart_exit(1);
	}
	memcpy(hdr, frame, 14);
	dumpstruct(stdout, "rx header", hdr, 14);
	ret -= 14;
	dumpstruct(stdout, "rx payload", frame + 14, ret);
	if (ret > buf_size) {
		printf("warning: truncating frame to %i\n", buf_size);
		ret = buf_size;
	}
	memcpy(payload, frame + 14, ret);
	hwts->sec = ts.tv_sec;
	hwts->nsec = ts.tv_nsec;
	hwts->phase = 0;
	net_verbose("%s: %9li.%09i.%03i\n", __func__, (long)hwts->sec,
		    hwts->nsec, hwts->phase);
	return ret;
}

int minic_tx_frame(struct wr_ethhdr_vlan *hdr, uint8_t * payload, uint32_t size,
                   struct hw_timestamp *hwts)
{
	unsigned char frame[1500];
	struct timespec ts;
	int hsize, len;

	if (hdr->ethtype == htons(0x8100))
		hsize = sizeof(struct wr_ethhdr_vlan);
	else
		hsize = sizeof(struct wr_ethhdr);

	dumpstruct(stdout, "tx header", hdr, hsize);
	dumpstruct(stdout, "tx payload", payload, size);

	memcpy(frame, hdr, hsize);
	memcpy(frame + hsize, payload, size);
	clock_gettime(CLOCK_REALTIME, &ts);
	len = send(sock, frame, size + hsize, 0);
	hwts->sec = ts.tv_sec;
	hwts->nsec = ts.tv_nsec;
	hwts->phase = 0;
	net_verbose("%s: %9li.%09i.%03i\n", __func__, (long)hwts->sec,
		    hwts->nsec, hwts->phase);
	return len;
}


int wrpc_get_port_state(struct hal_port_state *port, const char *port_name)
{
	return 0;
}

int ep_link_up(uint16_t * lpa)
{
	struct ifreq ifr;
	strcpy(ifr.ifr_name, ifname);
	if ( ioctl(sock, SIOCGIFFLAGS, &ifr) < 0) {
		ifr.ifr_flags = ~0; /* assume up, don't be messy */
	}
	if (ifr.ifr_flags & IFF_UP
	    && ifr.ifr_flags & IFF_RUNNING)
		return 1;
	return 0;
}


