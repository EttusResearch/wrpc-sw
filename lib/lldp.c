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

	lldpdu[lldpdu_len] = tlv_type * 2;
	lldpdu[lldpdu_len + 1] = tlv_type_len[tlv_type];
	lldpdu_len += LLDP_HEADER;

}

#ifndef htons
#define htons(x) x
#endif

static void lldp_add_tlv(int tlv_type) {

	unsigned char mac[6];
	uint8_t myIP[4];

	switch(tlv_type) {
		case END_LLDP:
			/* header */
			lldp_header_tlv(tlv_type);

			/* End TLV */
			memcpy(lldpdu+(lldpdu_len), 0x0, tlv_type_len[tlv_type]);
			lldpdu_len += tlv_type_len[tlv_type];

			break;
		case CHASSIS_ID:
			/* header */
			lldp_header_tlv(tlv_type);

			/* TLV Chassis Component */
			lldpdu[lldpdu_len] = 4;
			get_mac_addr(mac);
			memcpy(lldpdu+(lldpdu_len + LLDP_SUBTYPE), mac, 6);
			lldpdu_len += tlv_type_len[tlv_type];

			break;
		case PORT_ID:
			/* header */
			lldp_header_tlv(tlv_type);

			/* TLV Interce Alias */
			lldpdu[lldpdu_len] = 7;
			strcpy(lldpdu+(lldpdu_len + LLDP_SUBTYPE), "WR Timing Receiver");
			lldpdu_len += tlv_type_len[tlv_type];
			break;
		case TTL:
			/* header */
			lldp_header_tlv(tlv_type);

			/* TLV Time to Live */
			lldpdu[lldpdu_len + 1] = 0xFF; /* sec */
			lldpdu_len += tlv_type_len[tlv_type];
			break;
		case PORT:
			/* header */
			lldp_header_tlv(tlv_type);

			/* TLV Info srting */
			getIP(myIP);
			memcpy(lldpdu + lldpdu_len, myIP, 4);
			lldpdu_len += tlv_type_len[tlv_type];
			break;
		case SYS_NAME:
			/* header */
			lldp_header_tlv(tlv_type);

			/* TLV Info srting */
			/* fixme, define how to get formfactor name */
			//strcpy(lldpdu+lldpdu_len, form_factor);
			lldpdu_len += tlv_type_len[tlv_type];
			break;
		case SYS_DESCR:
			/* header */
			lldp_header_tlv(tlv_type);

			/* TLV Info srting */
			strcpy(lldpdu+lldpdu_len, build_revision);
			lldpdu_len += tlv_type_len[tlv_type];
			break;
		case SYS_CAPLTY:
			/* header */
			lldp_header_tlv(tlv_type);

			/* TLV Info string */
			memset(lldpdu + lldpdu_len, 0x0, 4);
			lldpdu_len += tlv_type_len[tlv_type];
			break;
		case MNG_ADD:
			/* header */
			lldp_header_tlv(tlv_type);

			/* TLV Info string */
			lldpdu[lldpdu_len] = 0x5; /* len */
			lldpdu[lldpdu_len + LLDP_SUBTYPE] = 0x1; /* mngt add subtype */
			/* fixme, defien how to get the mngt IP from the host */
			//memcpy(lldpdu + (lldpdu_len + 2), MNG_IP , 4); /* mngt IP */
			lldpdu[lldpdu_len + IF_SUBTYPE] = 0x1; /* if subtype */
			lldpdu[lldpdu_len + IF_NUM] = 0x1; /* if number */
			lldpdu_len += tlv_type_len[tlv_type];
			break;
		case USER_DEF:
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

	/* add optional and end TLVs */
	lldp_add_tlv(MNG_ADD);
	lldp_add_tlv(END_LLDP);

}

static void lldp_poll(void)
{
        static int ticks;

	/* fix me, waiting for periodic tasks */
        if (ticks > LLDP_TX_FQ) {
		ptpd_netif_sendto(lldp_socket, &addr, lldpdu, LLDP_PKT_LEN, 0);
                ticks = 0;
        }
        else
                ticks += 1;
}

DEFINE_WRC_TASK(lldp) = {
	.name = "lldp",
	.init = lldp_init,
	.job = lldp_poll,
};
