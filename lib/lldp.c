/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2011 GSI (www.gsi.de)
 * Author: Cesar Prados <c.prados@gsi.de>
 *
 * LLDP transmit-only station
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

#include <string.h>

#include "endpoint.h"
#include "ipv4.h"
#include "ptpd_netif.h"
#include "lldp.h"

#ifndef htons
#define htons(x) x
#endif

static char lldpdu[LLDP_PKT_LEN];
static uint16_t lldpdu_len;

/* LLDPC: a tx-only socket */
static struct wrpc_socket __static_lldp_socket = {
	.queue.buff = NULL,
	.queue.size = 0,
};

static struct wrpc_socket *lldp_socket;

static void lldp_header_tlv(int tlv_type) {

	lldpdu[lldpdu_len] = tlv_type * 2;
	lldpdu[lldpdu_len + 1] = tlv_type_len[tlv_type];
	lldpdu_len += LLDP_HEADER;

}

static void lldp_add_tlv(int tlv_type) {

	switch(tlv_type) {
		case END_LLDP:
			break;
		case CHASSIS_ID:
			/* header */
			lldp_header_tlv(tlv_type);

			/* TLV Info srting */
			lldpdu[lldpdu_len] = 1; /* Chassis Component */
			strcpy(lldpdu+(lldpdu_len + LLDP_SUBTYPE), "WR Timing Receiver");
			lldpdu_len += tlv_type_len[tlv_type];

			break;
		case PORT_ID:
			/* header */
			lldp_header_tlv(tlv_type);

			/* TLV Info string */
			lldpdu[lldpdu_len] = 3; /* Interface Alias */
			memcpy(lldpdu+(lldpdu_len + LLDP_SUBTYPE), LLDP_MCAST_MAC, 6);
			lldpdu_len += tlv_type_len[tlv_type];
			break;
		case TTL:
			/* header */
			lldp_header_tlv(tlv_type);

			/* TLV Info srting */
			lldpdu[lldpdu_len] = 0x30; /* Interface Alias */
			lldpdu_len += tlv_type_len[tlv_type];
			break;
		case PORT:
			/* header */
			lldp_header_tlv(tlv_type);

			/* TLV Info srting */
			strcpy(lldpdu+lldpdu_len, "wr0");
			lldpdu_len += tlv_type_len[tlv_type];
			break;
		case SYS_NAME:
			/* header */
			lldp_header_tlv(tlv_type);

			/* TLV Info srting */
			strcpy(lldpdu+lldpdu_len, "WR Timing Receiver");
			lldpdu_len += tlv_type_len[tlv_type];
			break;
		case SYS_DESCR:
			/* header */
			lldp_header_tlv(tlv_type);

			/* TLV Info srting */
			strcpy(lldpdu+lldpdu_len, "Firmware Commit/Version");
			lldpdu_len += tlv_type_len[tlv_type];
			break;
		case SYS_CAPLTY:
			/* header */
			lldp_header_tlv(tlv_type);

			/* TLV Info string */
			lldpdu[lldpdu_len] = 7; /* Station Only */
			lldpdu_len += tlv_type_len[tlv_type];

			break;
		case MNG_ADD:
			/* to be added */
			break;
		default:
			break;
	}
}

static void lldp_init(void)
{
	struct wr_sockaddr saddr;
	int i;

        /* LLDP: raw ethernet*/
	memset(&saddr, 0, sizeof(saddr));
	saddr.ethertype = htons(0x88CC);

	lldp_socket = ptpd_netif_create_socket(&__static_lldp_socket, &saddr,
					       PTPD_SOCK_RAW_ETHERNET, 0);
	memset(lldpdu,0x0,500);
	lldpdu_len = 0;

	/* Create LLDP TLVs */
	for (i=CHASSIS_ID; i <= SYS_CAPLTY; i++)
		lldp_add_tlv(i);
}

static void lldp_poll(void)
{
	uint8_t buf[LLDP_PKT_LEN];
	struct wr_sockaddr addr;
        static int tick;

        memset(&addr, 0x0, sizeof(struct wr_sockaddr));
	memcpy(addr.mac, LLDP_MCAST_MAC, 6);

        memset(buf, 0x0, LLDP_PKT_LEN);
	memcpy(buf, lldpdu, LLDP_PKT_LEN);

        if (tick > 10000) { /* to be fixed, waiting for periodic tasks */
                ptpd_netif_sendto(lldp_socket, &addr, buf, LLDP_PKT_LEN, 0);
                tick = 0;
        }
        else
                tick += 1;
}

DEFINE_WRC_TASK(lldp) = {
	.name = "lldp",
	.init = lldp_init,
	.job = lldp_poll,
};
