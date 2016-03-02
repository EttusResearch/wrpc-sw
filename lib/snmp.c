/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2016 GSI (www.gsi.de)
 * Author: Alessandro Rubini <a.rubini@gsi.de>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <string.h>
#include <wrpc.h>

#include "endpoint.h"
#include "ipv4.h"
#include "ptpd_netif.h"
#include "pps_gen.h"
#include "hw/memlayout.h"
#include "hw/etherbone-config.h"

#ifndef htons
#define htons(x) x
#endif

static uint8_t __snmp_queue[256];
static struct wrpc_socket __static_snmp_socket = {
	.queue.buff = __snmp_queue,
	.queue.size = sizeof(__snmp_queue),
};
static struct wrpc_socket *snmp_socket;

static void snmp_init(void)
{
	/* Use UDP engine activated by function arguments  */
	snmp_socket = ptpd_netif_create_socket(&__static_snmp_socket, NULL,
						PTPD_SOCK_UDP, 161 /* snmp */);
}

/* These fill the actual information, returning the lenght */
static int fill_name(uint8_t *buf)
{
	strcpy((void *)(buf + 2), "wrc");
	buf[1] = strlen((void *)(buf + 2));
	buf[0] = 0x04; /* string */
	return 2 + buf[1];
}
static int fill_tics(uint8_t *buf)
{
	uint32_t tics = htonl(timer_get_tics() / 10); /* hundredths... bah! */

	memcpy(buf + 2, &tics, sizeof(tics));
	buf[1] = sizeof(tics);
	buf[0] = 0x43; /* who knows... */
	return 2 + sizeof(tics);
}
static int fill_date(uint8_t *buf)
{
	uint64_t secs;
	char *datestr;

	shw_pps_gen_get_time(&secs, NULL);
	datestr = format_time(secs, TIME_FORMAT_SNMP);

	memcpy((void *)(buf + 2), datestr, 12);
	buf[1] = 8; /* size is hardwired. See mib document or prev. commit */
	buf[0] = 0x04; /* string! (one more f word entering the repo... */
	return 2 + buf[1];
}



static uint8_t oid_name[] = {0x2B,0x06,0x01,0x02,0x01,0x01,0x05,0x00};
static uint8_t oid_tics[] = {0x2B,0x06,0x01,0x02,0x01,0x19,0x01,0x01,0x00};
static uint8_t oid_date[] = {0x2B,0x06,0x01,0x02,0x01,0x19,0x01,0x02,0x00};

struct snmp_oid {
	uint8_t *oid_match;
	int oid_len;
	int (*fill)(uint8_t *buf);
};

static struct snmp_oid oid_array[] = {
	{ oid_name, sizeof(oid_name), fill_name},
	{ oid_tics, sizeof(oid_tics), fill_tics},
	{ oid_date, sizeof(oid_date), fill_date},
	{ 0, }
};

/*
 * Perverse...  snmpwalk does getnext anyways. Let's support snmpget only
 *
 * We support the following ones:
 *    RFC1213-MIB::sysName.0 (string)              .1.3.6.1.2.1.1.5.0
 *    HOST-RESOURCES-MIB::hrSystemUptime.0 (ticks) .1.3.6.1.2.1.25.1.1.0
 *    HOST-RESOURCES-MIB::hrSystemDate.0 (string)  .1.3.6.1.2.1.25.1.2.0
 *
 *   The request for sysname is like this (-c public):
 *   30 29                                   sequence of 0x29
 *     02 01 00                              int: snmp version 1 (0x00)
 *     04 06 70 75 62 6C 69 63               string of 6: "public"
 *     A0 1C                                 get request of 0x1c
 *       02 04 27 05 36 A9                   int of 4: request ID
 *       02 01 00                            int of 1: error
 *       02 01 00                            int of 1: error
 *       30 0E                               sequence of 0x0e
 *         30 0C                             sequence of 0x0c
 *           06 08  2B 06 01 02 01 01 05 00  oid of 8: .1.3.6.1.2.1.1.5.0
 *           05 00                           null
 *
 *
 *    Response for sysname:
 *    30 30
 *      02 01 00
 *      04 06 70 75 62 6C 69 63    "public"
 *      A2 23
 *        02 04 27 05 36 A9     seqID
 *        02 01 00
 *        02 01 00
 *        30 15
 *          30 13
 *            06 08  2B 06 01  02 01 01 05 00
 *            04 07  6C 61 70 74 6F 70 6F          "laptopo"
 *
 */

/*
 * The following array defines the frame format, sizes are ignored on read
 * but must be filled on write, so mark them specially
 */
#define BYTE_SIZE 0xff
#define BYTE_IGNORE 0xfe
#define BYTE_PDU 0xfd
static uint8_t match_array[] = {
	0x30, BYTE_SIZE,
	0x02, 0x01, 0x00,
	0x04, 0x06, 0x70, 0x75, 0x62, 0x6C, 0x69, 0x63,
	BYTE_PDU, BYTE_SIZE,
	0x02, 0x04, BYTE_IGNORE, BYTE_IGNORE, BYTE_IGNORE, BYTE_IGNORE,
	0x02, 0x01, 0x00,
	0x02, 0x01, 0x00,
	0x30, BYTE_SIZE,
	0x30, BYTE_SIZE,
	0x06, BYTE_IGNORE,  /* oid follows */
};

/* And, now, work out your generic frame responder... */
static int snmp_respond(uint8_t *buf)
{
	struct snmp_oid *oid;
	uint8_t *newbuf;
	int i, len;

	for (i = 0; i < sizeof(match_array); i++) {
		switch (match_array[i]) {
		case BYTE_SIZE:
		case BYTE_IGNORE:
			continue;
		case BYTE_PDU:
			if (buf[i] != 0xA0)
				return -1;
			break;
		default:
			if (buf[i] != match_array[i])
				return -1;
		}
	}
	net_verbose("%s: match ok\n", __func__);

	newbuf = buf + i;
	for (oid = oid_array; oid->oid_len; oid++)
		if (!memcmp(oid->oid_match, newbuf, oid->oid_len))
			break;
	if (!oid->oid_len)
		return -1;
	net_verbose("%s: oid found: calling %p\n", __func__, oid->fill);

	/* Phew.... we matched the OID, so let's call the filler  */
	newbuf += oid->oid_len;
	len = oid->fill(newbuf);
	/* now fix all size fields and change PDU */
	for (i = 0; i < sizeof(match_array); i++) {
		int remain = (sizeof(match_array) - 1) - i;

		if (match_array[i] == BYTE_PDU)
			buf[i] = 0xA2; /* get response */
		if (match_array[i] != BYTE_SIZE)
			continue;
		net_verbose("offset %i 0x%02x is len %i\n", i, i,
			    remain + oid->oid_len + len);
		buf[i] = remain + oid->oid_len + len;
	}
	net_verbose("%s: returning %i bytes\n", __func__, len + (newbuf - buf));
	return len + (newbuf - buf);
}


/* receive snmp through the UDP mechanism */
static int snmp_poll(void)
{
	struct wr_sockaddr addr;
	uint8_t buf[200];
	int len;

	/* no need to wait for IP address: we won't get queries */
	len = ptpd_netif_recvfrom(snmp_socket, &addr,
				  buf, sizeof(buf), NULL);
	if (len <= UDP_END + sizeof(match_array))
		return 0;

	len = snmp_respond(buf + UDP_END);
	if (len < 0)
		return 0;
	len += UDP_END;

	fill_udp(buf, len, NULL);
	ptpd_netif_sendto(snmp_socket, &addr, buf, len, 0);
	return 1;
}

DEFINE_WRC_TASK(snmp) = {
	.name = "snmp",
	.enable = &link_status,
	.init = snmp_init,
	.job = snmp_poll,
};
