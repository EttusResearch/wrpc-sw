/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012 GSI (www.gsi.de)
 * Author: Wesley W. Terpstra <w.terpstra@gsi.de>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <string.h>

#include "endpoint.h"
#include "ipv4.h"
#include "ptpd_netif.h"
#include "hw/memlayout.h"
#include "hw/etherbone-config.h"

#ifndef htons
#define htons(x) x
#endif

int needIP;
static uint8_t myIP[4];

/* bootp: bigger buffer, UDP based */
static uint8_t __bootp_queue[512];
static struct wrpc_socket __static_bootp_socket = {
	.queue.buff = __bootp_queue,
	.queue.size = sizeof(__bootp_queue),
};
static struct wrpc_socket *bootp_socket;

/* ICMP: smaller buffer */
static uint8_t __icmp_queue[128];
static struct wrpc_socket __static_icmp_socket = {
	.queue.buff = __icmp_queue,
	.queue.size = sizeof(__icmp_queue),
};
static struct wrpc_socket *icmp_socket;


unsigned int ipv4_checksum(unsigned short *buf, int shorts)
{
	int i;
	unsigned int sum;

	sum = 0;
	for (i = 0; i < shorts; ++i)
		sum += ntohs(buf[i]);

	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);

	return (~sum & 0xffff);
}

void ipv4_init(void)
{
	struct wr_sockaddr saddr;

	/* Reset => need a fresh IP */
	needIP = 1;

	/* Bootp: use UDP engine activated but function arguments  */
	memset(&saddr, 0, sizeof(saddr));

	bootp_socket = ptpd_netif_create_socket(&__static_bootp_socket, &saddr,
						PTPD_SOCK_UDP, 68 /* bootpc */);

	/* ICMP: specify raw (not UDP), with IPV4 ethtype */
	saddr.ethertype = htons(0x0800);
	icmp_socket = ptpd_netif_create_socket(&__static_icmp_socket, &saddr,
					       PTPD_SOCK_RAW_ETHERNET, 0);
}

static int bootp_retry = 0;
static uint32_t bootp_tics;

/* receive bootp through the UDP mechanism */
static void bootp_poll(void)
{
	struct wr_sockaddr addr;
	uint8_t buf[400];
	int len;

	if (!bootp_tics) /* first time ever */
		bootp_tics = timer_get_tics() - 1;

	len = ptpd_netif_recvfrom(bootp_socket, &addr,
				  buf, sizeof(buf), NULL);
	if (len > 0 && needIP)
		process_bootp(buf, len);

	if (!needIP)
		return;
	if (time_before(timer_get_tics(), bootp_tics))
		return;

	len = prepare_bootp(&addr, buf, ++bootp_retry);
	ptpd_netif_sendto(bootp_socket, &addr, buf, len, 0);
	bootp_tics = timer_get_tics() + TICS_PER_SECOND;
}

static void icmp_poll(void)
{
	struct wr_sockaddr addr;
	uint8_t buf[128];
	int len;

	len = ptpd_netif_recvfrom(icmp_socket, &addr,
				  buf, sizeof(buf), NULL);
	if (len <= 0)
		return;
	if (needIP)
		return;

	if ((len = process_icmp(buf, len)) > 0)
		ptpd_netif_sendto(icmp_socket, &addr, buf, len, 0);
}

void ipv4_poll(void)
{
	bootp_poll();

	icmp_poll();
}

void getIP(unsigned char *IP)
{
	memcpy(IP, myIP, 4);
}

void setIP(unsigned char *IP)
{
	volatile unsigned int *eb_ip =
	    (unsigned int *)(BASE_ETHERBONE_CFG + EB_IPV4);
	unsigned int ip;

	memcpy(myIP, IP, 4);

	ip = (myIP[0] << 24) | (myIP[1] << 16) | (myIP[2] << 8) | (myIP[3]);
	while (*eb_ip != ip)
		*eb_ip = ip;

	needIP = (ip == 0);
	if (!needIP)
		bootp_retry = 0;
}
