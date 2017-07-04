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

#include "revision.h"
#include "ptpd_netif.h"
#include "lldp.h"
#include "endpoint.h"
#include "ipv4.h"
#include "shell.h"
#include <wrpc.h> /*needed for htons()*/

static char lldpdu[LLDP_PKT_LEN];
static uint16_t lldpdu_len;

/* tx-only socket */
static struct wrpc_socket __static_lldp_socket = {
	.queue.buff = NULL,
	.queue.size = 0,
};

static struct wrpc_socket *lldp_socket;
static struct wr_sockaddr addr;

static void lldp_header_tlv(int tlv_type) {

	lldpdu_len = tlv_offset[tlv_type];
	lldpdu[lldpdu_len] = tlv_type * 2;
	lldpdu[lldpdu_len + LLDP_SUBTYPE] = tlv_type_len[tlv_type];
	lldpdu_len += LLDP_HEADER;
}

static void lldp_add_tlv(int tlv_type) {

	unsigned char mac[6];
	unsigned char ipWR[4];

	switch(tlv_type) {
		case END_LLDP:
			/* header */
			lldp_header_tlv(tlv_type);

			/* End TLV */
			memcpy(lldpdu + lldpdu_len, 0x0, tlv_type_len[tlv_type]);
			break;
		case CHASSIS_ID:
			/* header */
			lldp_header_tlv(tlv_type);

			/* TLV Chassis Component */
			lldpdu[lldpdu_len] = 4;
			get_mac_addr(mac);
			memcpy(lldpdu + (lldpdu_len + LLDP_SUBTYPE), mac, 6);
			break;
		case PORT_ID:
			/* header */
			lldp_header_tlv(tlv_type);

			/* TLV Interce Alias */
			lldpdu[lldpdu_len] = 7;
			strcpy(lldpdu + (lldpdu_len + LLDP_SUBTYPE), "WR Port");
			break;
		case TTL:
			/* header */
			lldp_header_tlv(tlv_type);

			/* TLV Time to Live */
			lldpdu[lldpdu_len + LLDP_SUBTYPE] = 0x20; /* sec */
			break;
		case PORT:
			/* header */
			lldp_header_tlv(tlv_type);

			/* TLV Info srting */
                        if (HAS_IP) {
                                getIP(ipWR);
                                char buf[32];
                                format_ip(buf, ipWR);
			        strcpy(lldpdu + lldpdu_len, buf);
                        }
			break;
		case SYS_NAME:
			/* header */
			lldp_header_tlv(tlv_type);

			/* TLV Info srting */
			/* TODO get host system name from wr-core outer world  */
			strcpy(lldpdu + lldpdu_len, "WR Timing Receiver");

			break;
		case SYS_DESCR:
			/* header */
			lldp_header_tlv(tlv_type);

			/* TLV Info srting */
			strcpy(lldpdu + lldpdu_len, build_revision);
			break;
		case SYS_CAPLTY:
			/* header */
			lldp_header_tlv(tlv_type);

			/* TLV Info string */
			memset(lldpdu + lldpdu_len, 0x0, 4);
			break;
		case MNG_ADD:
			/* header */
			lldp_header_tlv(tlv_type);

			/* TLV Info string */
                        /* TODO get host system name from wr-core outer world  */
			lldpdu[lldpdu_len] = 0x4; /* len */
			lldpdu[lldpdu_len + LLDP_SUBTYPE] = 0x1; /* mngt add subtype */
			lldpdu[lldpdu_len + IF_SUBTYPE] = 0x1; /* if subtype */
			lldpdu[lldpdu_len + IF_NUM] = 0x1; /* if number */
			break;
		case USER_DEF:
			/* TODO define WR TLV */
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
	memset(&saddr, 0x0, sizeof(saddr));
	saddr.ethertype = htons(LLDP_ETH_TYP);

	lldp_socket = ptpd_netif_create_socket(&__static_lldp_socket, &saddr,
					       PTPD_SOCK_RAW_ETHERNET, 0);

	memset(&addr, 0x0, sizeof(struct wr_sockaddr));
	memcpy(addr.mac, LLDP_MCAST_MAC, 6);

	/* add mandatory LLDP TLVs */
	memset(lldpdu, 0x0, LLDP_PKT_LEN);
	lldpdu_len = 0;

	for (i=CHASSIS_ID; i <= SYS_CAPLTY; i++)
		lldp_add_tlv(i);

	/* add optional TLVs */
	lldp_add_tlv(MNG_ADD);

	/* end TLVs */
	lldp_add_tlv(END_LLDP);
}

static void lldp_poll(void)
{
        static int ticks;

	/* periodic tasks */
	if (ticks > LLDP_TX_FQ) {

		if (HAS_IP && (ip_status != IP_TRAINING)) {
			lldp_add_tlv(PORT);
			/* update other dynamic TLVs */
		}

		ptpd_netif_sendto(lldp_socket, &addr, lldpdu, LLDP_PKT_LEN, 0);

		ticks = 0;
	} else {
		ticks += 1;
	}
}

DEFINE_WRC_TASK(lldp) = {
	.name = "lldp",
	.init = lldp_init,
	.job = lldp_poll,
};
