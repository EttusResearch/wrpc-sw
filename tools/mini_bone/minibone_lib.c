/* MiniBone library. BUGGY CRAP CODE INTENDED FOR TESTING ONLY! */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/socket.h>
#include <sys/time.h>

#include "ptpd_netif.h"

#define F_SEL(x) (x & 0xf)
#define F_ERROR (1<<1)
#define F_READBACK (1<<0)
#define F_WRITE (1<<4)

#define RX_TIMEOUT 10

#define MBN_ETHERTYPE 0xa0a0

struct mbn_packet {
	uint16_t flags ;
	uint32_t a_d;
	uint32_t d;
} __attribute__((packed));


struct mb_device {
  mac_addr_t dest;
  uint16_t ethertype;
  wr_socket_t *sock;
  int tx_packets, rx_packets, tx_retries, rx_retries;
};

typedef struct
{
	uint64_t start_tics;
	uint64_t timeout;
} timeout_t ;

static uint64_t get_tics()
{
	struct timezone tz = {0, 0};
	struct timeval tv;
	gettimeofday(&tv, &tz);

	return (uint64_t) tv.tv_sec * 1000000ULL + (uint64_t) tv.tv_usec;
}

static inline int tmo_init(timeout_t *tmo, uint32_t milliseconds)
{
	tmo->start_tics = get_tics();
	tmo->timeout = (uint64_t) milliseconds * 1000ULL;
	return 0;
}

static inline int tmo_restart(timeout_t *tmo)
{
	tmo->start_tics = get_tics();
	return 0;
}

static inline int tmo_expired(timeout_t *tmo)
{
	return (get_tics() - tmo->start_tics > tmo->timeout);
}



void *mbn_open(const char *if_name, mac_addr_t target)
{
	struct mb_device *dev = malloc(sizeof(struct mb_device));
	wr_sockaddr_t saddr;

	if(!dev)
		return NULL;
	
	memset(dev, 0, sizeof(struct mb_device));
	
	memcpy(dev->dest, target, 6);
	strcpy(saddr.if_name, if_name);
	memcpy(saddr.mac, target, 6);
	
	saddr.ethertype = htons(MBN_ETHERTYPE);
	saddr.family = PTPD_SOCK_RAW_ETHERNET;
		
	dev->sock = ptpd_netif_create_socket(PTPD_SOCK_RAW_ETHERNET, 0, &saddr);

	if(!dev->sock)
	{
		free(dev);
	 	return NULL;
	}
	return (void *)dev;
}


static int mbn_send(void *priv, uint8_t *data, int size)
{
	struct mb_device *dev = (struct mb_device *)priv;
	wr_sockaddr_t to;
	
	memcpy(to.mac, dev->dest, 6);
	to.ethertype = MBN_ETHERTYPE;

	return ptpd_netif_sendto(dev->sock, &to, (void*)data, size, NULL);
}

static int mbn_recv(void *handle, uint8_t *data, int size, int timeout)
{
	struct mb_device *dev = (struct mb_device *)handle;
	wr_sockaddr_t from;
    timeout_t rx_tmo;
	tmo_init(&rx_tmo, timeout);

	do {	
 	int n = ptpd_netif_recvfrom(dev->sock, &from, (void*)data, size, NULL);
 	
 		if(n > 0 && from.ethertype == MBN_ETHERTYPE && !memcmp(from.mac, dev->dest, 6))
	 	{
	 		dev->rx_packets++;
 			return n;
	 	}
//	 	dev->rx_retries++;
 	} while(!tmo_expired(&rx_tmo));
    return 0;
}

void mbn_writel(void *handle, uint32_t d, uint32_t a)
{
	struct mb_device *dev = (struct mb_device *)handle;
	int n_retries = 3;
	struct mbn_packet pkt;

	while(n_retries--)
	{
		pkt.flags = htons(F_SEL(0xf) | F_WRITE);
		pkt.a_d= htonl(a);
		pkt.d=htonl(d);

		mbn_send(handle, (uint8_t *)&pkt, sizeof(pkt));
		int n = mbn_recv(handle, (uint8_t *)&pkt, sizeof(pkt), RX_TIMEOUT);


		pkt.flags = ntohs(pkt.flags);
		if(n == sizeof(pkt) && ! (!(pkt.flags && F_READBACK) && !(pkt.flags & F_ERROR)))
		{
			int i;
			fprintf(stderr,"\nBadPacket:  ");
			for(i=0;i<n; i++) fprintf(stderr,"%02x ", *(uint8_t*) (&pkt + i));
			fprintf(stderr,"\n");
			
		} if(n == sizeof(pkt) && !(pkt.flags && F_READBACK) && !(pkt.flags & F_ERROR))
		{
			int i;
//			fprintf(stderr,"GoodFlags: %x\n", pkt.flags);
  			/*fprintf(stderr,"\nGoodPacket: ");
			for(i=0;i<n; i++) fprintf(stderr,"%02x ", *(uint8_t*) (&pkt + i));
			fprintf(stderr,"\n");*/

			dev->tx_packets++;
			return ;
		}
		dev->tx_retries++;
	}

	fprintf(stderr, "No ack.\n");
}

uint32_t mbn_readl(void *handle, uint32_t a)
{
	int n_retries = 3;
	struct mb_device *dev = (struct mb_device *)handle;
	struct mbn_packet pkt;
	pkt.flags = htons(F_SEL(0xf));
	pkt.a_d= htonl(a);

	while(n_retries--)
	{
		mbn_send(handle, (uint8_t *)&pkt, sizeof(pkt));
		int n = mbn_recv(handle, (uint8_t *)&pkt, sizeof(pkt), RX_TIMEOUT);
		pkt.flags = ntohs(pkt.flags);
		if(n == sizeof(pkt) && (pkt.flags & F_READBACK) && !(pkt.flags & F_ERROR))
		{
			return ntohl(pkt.a_d);
		}
		dev->tx_retries++;
	}

	fprintf(stderr, "No ack.\n");
}

void mbn_stats(void *handle)
{
	struct mb_device *dev = (struct mb_device *)handle;
	
	fprintf(stderr,"Sent: %d [retries: %d], rcvd: %d [retries: %d]\n", dev->tx_packets, dev->tx_retries, dev->rx_packets, dev->rx_retries);

}

void mbn_close(void *handle)
{
	struct mb_device *dev = (struct mb_device *)handle;
	
	ptpd_netif_close_socket(dev->sock);
}