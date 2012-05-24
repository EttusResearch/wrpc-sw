// Supports only raw ethernet now.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>
#include <linux/errqueue.h>
#include <linux/sockios.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <asm/types.h>
#include <fcntl.h>
#include <errno.h>


#include <asm/socket.h>

#include "ptpd_netif.h"

#ifdef NETIF_VERBOSE
#define netif_dbg(...) printf(__VA_ARGS__)
#else
#define netif_dbg(...)
#endif

#define ETHER_MTU 1518
#define DMTD_UPDATE_INTERVAL 100

struct scm_timestamping {
	struct timespec systime;
	struct timespec hwtimetrans;
	struct timespec hwtimeraw;
};

PACKED struct etherpacket {
	struct ethhdr ether;
	char data[ETHER_MTU];
};

struct tx_timestamp {
	int valid;
	wr_timestamp_t ts;
	uint32_t tag;
	uint64_t t_acq;
};

struct my_socket {
	int fd;
	wr_sockaddr_t bind_addr;
	mac_addr_t local_mac;
	int if_index;

	// parameters for linearization of RX timestamps
	uint32_t clock_period;
	uint32_t phase_transition;
	uint32_t dmtd_phase;


};

struct nasty_hack{
	char if_name[20];
	int clockedAsPrimary;
};

#ifdef MACIEK_HACKs
struct nasty_hack locking_hack;
#endif

wr_socket_t *ptpd_netif_create_socket(int sock_type, int flags,
				      wr_sockaddr_t *bind_addr)
{
	struct my_socket *s;
	struct sockaddr_ll sll;
	struct ifreq f;

	int fd;

	//    fprintf(stderr,"CreateSocket!\n");

	if(sock_type != PTPD_SOCK_RAW_ETHERNET)
		return NULL;

	fd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));

	if(fd < 0)
	{
		perror("socket()");
		return NULL;
	}

	fcntl(fd, F_SETFL, O_NONBLOCK);

	// Put the controller in promiscious mode, so it receives everything
	strcpy(f.ifr_name, bind_addr->if_name);
	if(ioctl(fd, SIOCGIFFLAGS,&f) < 0) { perror("ioctl()"); return NULL; }
	f.ifr_flags |= IFF_PROMISC;
	if(ioctl(fd, SIOCSIFFLAGS,&f) < 0) { perror("ioctl()"); return NULL; }

	// Find the inteface index
	strcpy(f.ifr_name, bind_addr->if_name);
	ioctl(fd, SIOCGIFINDEX, &f);


	sll.sll_ifindex = f.ifr_ifindex;
	sll.sll_family   = AF_PACKET;
	sll.sll_protocol = htons(bind_addr->ethertype);
	sll.sll_halen = 6;

	memcpy(sll.sll_addr, bind_addr->mac, 6);

	if(bind(fd, (struct sockaddr *)&sll, sizeof(struct sockaddr_ll)) < 0)
	{
		close(fd);
		perror("bind()");
		return NULL;
	}

	s=calloc(sizeof(struct my_socket), 1);

	s->if_index = f.ifr_ifindex;

	// get interface MAC address
	if (ioctl(fd, SIOCGIFHWADDR, &f) < 0) {
		perror("ioctl()"); return NULL;
	}

	memcpy(s->local_mac, f.ifr_hwaddr.sa_data, 6);
	memcpy(&s->bind_addr, bind_addr, sizeof(wr_sockaddr_t));

	s->fd = fd;

	return (wr_socket_t*)s;
}

int ptpd_netif_close_socket(wr_socket_t *sock)
{
	struct my_socket *s = (struct my_socket *) sock;

	if(!s)
		return 0;
		
	close(s->fd);
	return 0;
}


int ptpd_netif_sendto(wr_socket_t *sock, wr_sockaddr_t *to, void *data,
		      size_t data_length, wr_timestamp_t *tx_ts)
{
	struct etherpacket pkt;
	struct my_socket *s = (struct my_socket *)sock;
	struct sockaddr_ll sll;
	int rval;
	wr_timestamp_t ts;

	if(s->bind_addr.family != PTPD_SOCK_RAW_ETHERNET)
		return -ENOTSUP;

	if(data_length > ETHER_MTU-8) return -EINVAL;

	memset(&pkt, 0, sizeof(struct etherpacket));

	memcpy(pkt.ether.h_dest, to->mac, 6);
	memcpy(pkt.ether.h_source, s->local_mac, 6);
	pkt.ether.h_proto =htons(to->ethertype);

	memcpy(pkt.data, data, data_length);

	size_t len = data_length + sizeof(struct ethhdr);

	if(len < 72)
		len = 72;

	memset(&sll, 0, sizeof(struct sockaddr_ll));

	sll.sll_ifindex = s->if_index;
	sll.sll_family = AF_PACKET;
	sll.sll_protocol = htons(to->ethertype);
	sll.sll_halen = 6;

	//    fprintf(stderr,"fd %d ifi %d ethertype %d\n", s->fd,
	// s->if_index, to->ethertype);

	rval =  sendto(s->fd, &pkt, len, 0, (struct sockaddr *)&sll,
		       sizeof(struct sockaddr_ll));

	return rval;
}


int ptpd_netif_recvfrom(wr_socket_t *sock, wr_sockaddr_t *from, void *data,
			size_t data_length, wr_timestamp_t *rx_timestamp)
{
	struct my_socket *s = (struct my_socket *)sock;
	struct etherpacket pkt;
	struct msghdr msg;
	struct iovec entry;
	struct sockaddr_ll from_addr;
	struct {
		struct cmsghdr cm;
		char control[1024];
	} control;
	struct cmsghdr *cmsg;
	struct scm_timestamping *sts = NULL;

	size_t len = data_length + sizeof(struct ethhdr);

	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = &entry;
	msg.msg_iovlen = 1;
	entry.iov_base = &pkt;
	entry.iov_len = len;
	msg.msg_name = (caddr_t)&from_addr;
	msg.msg_namelen = sizeof(from_addr);
	msg.msg_control = &control;
	msg.msg_controllen = sizeof(control);

	int ret = recvmsg(s->fd, &msg, MSG_DONTWAIT);

	if(ret < 0 && errno==EAGAIN) return 0; // would be blocking
	if(ret == -EAGAIN) return 0;

	if(ret <= 0) return ret;

	memcpy(data, pkt.data, ret - sizeof(struct ethhdr));

	from->ethertype = ntohs(pkt.ether.h_proto);
	memcpy(from->mac, pkt.ether.h_source, 6);
	memcpy(from->mac_dest, pkt.ether.h_dest, 6);

	return ret - sizeof(struct ethhdr);
}

