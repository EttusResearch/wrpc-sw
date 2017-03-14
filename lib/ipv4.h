/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#ifndef IPV4_H
#define IPV4_H

#include <inttypes.h>
#include "ptpd_netif.h" /* for sockaddr in prototype */

#undef IP_TOS /* These two are defined by arpa/inet.h, and will conflict */
#undef IP_TTL /* when we build for the host */

#define IP_VERSION	0
#define IP_TOS		(IP_VERSION+1)
#define IP_LEN		(IP_TOS+1)
#define IP_ID		(IP_LEN+2)
#define IP_FLAGS	(IP_ID+2)
#define IP_TTL		(IP_FLAGS+2)
#define IP_PROTOCOL	(IP_TTL+1)
#define IP_CHECKSUM	(IP_PROTOCOL+1)
#define IP_SOURCE	(IP_CHECKSUM+2)
#define IP_DEST		(IP_SOURCE+4)
#define IP_END		(IP_DEST+4)

#define UDP_VIRT_SADDR	(IP_END-12)
#define UDP_VIRT_DADDR	(UDP_VIRT_SADDR+4)
#define UDP_VIRT_ZEROS	(UDP_VIRT_DADDR+4)
#define UDP_VIRT_PROTO	(UDP_VIRT_ZEROS+1)
#define UDP_VIRT_LENGTH	(UDP_VIRT_PROTO+1)

#define UDP_SPORT	(IP_END)
#define UDP_DPORT	(UDP_SPORT+2)
#define UDP_LENGTH	(UDP_DPORT+2)
#define UDP_CHECKSUM	(UDP_LENGTH+2)
#define UDP_END		(UDP_CHECKSUM+2)

/* Internal to IP stack: */
unsigned int ipv4_checksum(unsigned short *buf, int shorts);

enum ip_status {
	IP_TRAINING,
	IP_OK_BOOTP,
	IP_OK_STATIC,
};
extern enum ip_status ip_status;
void setIP(unsigned char *IP);
void getIP(unsigned char *IP);

int process_icmp(uint8_t * buf, int len);
int process_bootp(uint8_t * buf, int len);	/* non-zero if IP was set */
int prepare_bootp(struct wr_sockaddr *addr, uint8_t * buf, int retry);

/* The UDP helper needs some information, if not replying to a frame */
struct wr_udp_addr {
	uint32_t saddr; /* all fields in network order, for memcpy */
	uint32_t daddr;
	uint16_t sport;
	uint16_t dport;
};

void fill_udp(uint8_t * buf, int len, struct wr_udp_addr *uaddr);
int check_dest_ip(unsigned char *buf);

void syslog_init(void);
int syslog_poll(void);
void syslog_report(const char *buf);

#endif
