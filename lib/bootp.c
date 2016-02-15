/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012 GSI (www.gsi.de)
 * Author: Wesley W. Terpstra <w.terpstra@gsi.de>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <string.h>
#include <wrc.h>
#include "endpoint.h"

#include "ipv4.h"

#define BOOTP_OP	(UDP_END)
#define BOOTP_HTYPE	(BOOTP_OP+1)
#define BOOTP_HLEN	(BOOTP_HTYPE+1)
#define BOOTP_HOPS	(BOOTP_HLEN+1)
#define BOOTP_XID	(BOOTP_HOPS+1)
#define BOOTP_SECS	(BOOTP_XID+4)
#define BOOTP_UNUSED	(BOOTP_SECS+2)
#define BOOTP_CIADDR	(BOOTP_UNUSED+2)
#define BOOTP_YIADDR	(BOOTP_CIADDR+4)
#define BOOTP_SIADDR	(BOOTP_YIADDR+4)
#define BOOTP_GIADDR	(BOOTP_SIADDR+4)
#define BOOTP_CHADDR	(BOOTP_GIADDR+4)
#define BOOTP_SNAME	(BOOTP_CHADDR+16)
#define BOOTP_FILE	(BOOTP_SNAME+64)
#define BOOTP_VEND	(BOOTP_FILE+128)
#define BOOTP_END	(BOOTP_VEND+64)

int prepare_bootp(struct wr_sockaddr *addr, uint8_t * buf, int retry)
{
	struct wr_udp_addr uaddr;

	// ----------- BOOTP ------------
	buf[BOOTP_OP] = 1;	/* bootrequest */
	buf[BOOTP_HTYPE] = 1;	/* ethernet */
	buf[BOOTP_HLEN] = 6;	/* MAC length */
	buf[BOOTP_HOPS] = 0;

	/* A unique identifier for the request !!! FIXME */
	get_mac_addr(buf + BOOTP_XID);
	buf[BOOTP_XID + 0] ^= buf[BOOTP_XID + 4];
	buf[BOOTP_XID + 1] ^= buf[BOOTP_XID + 5];
	buf[BOOTP_XID + 2] ^= (retry >> 8) & 0xFF;
	buf[BOOTP_XID + 3] ^= retry & 0xFF;

	buf[BOOTP_SECS] = (retry >> 8) & 0xFF;
	buf[BOOTP_SECS + 1] = retry & 0xFF;
	memset(buf + BOOTP_UNUSED, 0, 2);

	memset(buf + BOOTP_CIADDR, 0, 4);	/* own IP if known */
	memset(buf + BOOTP_YIADDR, 0, 4);
	memset(buf + BOOTP_SIADDR, 0, 4);
	memset(buf + BOOTP_GIADDR, 0, 4);

	memset(buf + BOOTP_CHADDR, 0, 16);
	get_mac_addr(buf + BOOTP_CHADDR);	/* own MAC address */

	memset(buf + BOOTP_SNAME, 0, 64);	/* desired BOOTP server */
	memset(buf + BOOTP_FILE, 0, 128);	/* desired BOOTP file */
	memset(buf + BOOTP_VEND, 0, 64);	/* vendor extensions */

	/* complete with udp helper */
	memset(&uaddr.saddr, 0, 4);
	memset(&uaddr.daddr, 0xff, 4);
	uaddr.sport = ntohs(68);
	uaddr.dport = ntohs(67);

	fill_udp(buf, BOOTP_END, &uaddr);

	/* and fix destination before sending it */
	memset(addr->mac, 0xFF, 6);
	// pp_printf("Sending BOOTP request...\n");
	return BOOTP_END;
}

int process_bootp(uint8_t * buf, int len)
{
	uint8_t mac[6];
	uint8_t ip[4];

	get_mac_addr(mac);

	if (len != BOOTP_END)
		return 0;

	if (buf[UDP_SPORT] != 0 || buf[UDP_SPORT + 1] != 67)
		return 0;

	if (memcmp(buf + BOOTP_CHADDR, mac, 6))
		return 0;

	ip_status = IP_OK_BOOTP;
	setIP(buf + BOOTP_YIADDR);

	getIP(ip);
	pp_printf("Discovered IP address (%d.%d.%d.%d)!\n",
	        ip[0], ip[1], ip[2], ip[3]);

	return 1;
}
