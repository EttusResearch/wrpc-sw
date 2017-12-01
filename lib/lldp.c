/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2011 GSI (www.gsi.de)
 * Copyright (C) 2017 CERN
 *
 * Author: Cesar Prados <c.prados@gsi.de>
 * Author: Adam Wujek <adam.wujek@cern.ch>
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
#include "syscon.h"
#include <wrpc.h> /*needed for htons()*/

static uint8_t lldpdu[LLDP_MAX_PKT_LEN];
static uint16_t lldpdu_len;

/* tx-only socket */
static struct wrpc_socket __static_lldp_socket = {
	.queue.buff = NULL,
	.queue.size = 0,
};

static struct wrpc_socket *lldp_socket;
static struct wr_sockaddr addr;

extern char wrc_hw_name[HW_NAME_LENGTH];

static void lldp_header_tlv(uint8_t tlv_type, uint8_t tlv_len)
{
	lldpdu[lldpdu_len] = tlv_type << 1;
	lldpdu[lldpdu_len + LLDP_SUBTYPE] = tlv_len;
	lldpdu_len += LLDP_HEADER;
}

static void fill_mac(uint8_t *tlv, uint8_t type)
{
	*tlv = type;
	/* write MAC after subtype byte */
	get_mac_addr(tlv + LLDP_SUBTYPE);
}

static void lldp_add_tlv(int tlv_type) {

	uint8_t mac[6];
	unsigned char ipWR[4];
	int tlv_len = 0;

	switch (tlv_type) {
	case END_LLDP:
		/* End TLV */
		/* header */
		lldp_header_tlv(tlv_type, tlv_len);

		break;
	case CHASSIS_ID:
		tlv_len = CHASSIS_ID_TLV_LEN;
		/* header */
		lldp_header_tlv(tlv_type, tlv_len);

		/* TLV Chassis Component */
		fill_mac(&lldpdu[lldpdu_len], CHASSIS_ID_TYPE_MAC);
		break;
	case PORT_ID:
		tlv_len = PORT_ID_TLV_LEN;
		/* header */
		lldp_header_tlv(tlv_type, tlv_len);

		/* TLV portID */
		fill_mac(&lldpdu[lldpdu_len], PORT_ID_SUBTYPE_MAC);
		break;
	case TTL:
		tlv_len = TTL_ID_TLV_LEN;
		/* header */
		lldp_header_tlv(tlv_type, tlv_len);

		/* TLV Time to Live */
		/* use LLDP_TX_TICK_INTERVAL in seconds times 4 */
		lldpdu[lldpdu_len + TTL_BYTE_MSB] =
			(((LLDP_TX_TICK_INTERVAL / 1000) * 4) >> 8) & 0xff;
		lldpdu[lldpdu_len + TTL_BYTE_LSB] =
			((LLDP_TX_TICK_INTERVAL / 1000) * 4) & 0xff;
		break;
	case PORT:
		tlv_len = strlen(PORT_NAME) + 1;
		/* header */
		lldp_header_tlv(tlv_type, tlv_len);

		strcpy((char *)lldpdu + lldpdu_len, PORT_NAME);
		break;
	case SYS_NAME:
		{
		/* TODO get host system name from wr-core outer world */
		/* NOTE: according to 802.1AB-2005 9.5.6.2 and then
		 * IETF RFC 3418:
		 * "If the name is unknown, the value is the zero-length
		 * string."
		 * However, we put the IP, if not set MAC to be able to
		 * identify a system */
		char buf[32];
		getIP(ipWR);

		if (HAS_IP && memcmp(ipWR, "\0\0\0\0", 4)) {
			/* NOTE: no subtype */
			format_ip(buf, ipWR);
			tlv_len = strlen((char *)buf);
			strcpy((char *)(lldpdu + lldpdu_len + LLDP_HEADER),
			       (char *)buf);
		} else {
			/* NOTE: no subtype */
			get_mac_addr(mac);
			pp_sprintf(buf,
				   "%02x:%02x:%02x:%02x:%02x:%02x",
				   mac[0], mac[1], mac[2], mac[3],
				   mac[4], mac[5]);
			tlv_len = 17;
			strncpy((char *)(lldpdu + lldpdu_len + LLDP_HEADER),
			       (char *)buf, tlv_len);
		}
		/* header */
		lldp_header_tlv(tlv_type, tlv_len);

		break;
		}
	case SYS_DESCR:
		tlv_len = 0; /* will be calculated later */
		uint8_t *pdu_p;

		/* set the pointer after TLV's header */
		pdu_p = &lldpdu[lldpdu_len + LLDP_HEADER];

		/* TLV Info srting */
		strncpy((char *)(pdu_p), wrc_hw_name, HW_NAME_LENGTH - 1);
		pdu_p += strnlen(wrc_hw_name, HW_NAME_LENGTH - 1);
		strcpy((char *)(pdu_p), ": ");
		pdu_p += 2; /* length of ": " */
		strncpy((char *)(pdu_p), build_revision, 32);
		pdu_p += strnlen(build_revision, 32);
		tlv_len = (uint8_t)(pdu_p - &lldpdu[lldpdu_len + LLDP_HEADER]);
		lldp_header_tlv(tlv_type, tlv_len);

		break;
	case SYS_CAPLTY:
		/* don't implement this, this TLV is optional */
		break;
		tlv_len = 4;
		/* header */
		lldp_header_tlv(tlv_type, tlv_len);

		/* TLV Info string */
		memset(lldpdu + lldpdu_len, 0x0, tlv_len);
		break;
	case MNG_ADD:
		/* TODO: fill with MAC if no IP present */
		if (!HAS_IP || !memcmp(ipWR, "\0\0\0\0", 4)) {
			/* if no IP present skip this field */
			break;
		}
		/* TODO: dynamic len */
		tlv_len = 0xc;
		/* header */
		lldp_header_tlv(tlv_type, tlv_len);

		/* TLV Info string */
		lldpdu[lldpdu_len] = MNG_ADDR_LEN; /* len */
		/* mngt add subtype */
		lldpdu[lldpdu_len + LLDP_SUBTYPE] = MNG_ADDR_SUBTYPE_IPv4;
		/* if subtype (ifIndex)*/
		lldpdu[lldpdu_len + MNT_IF_SUBTYPE] =
						MNG_IF_NUM_SUBTYPE_IFINDEX;
		/* if number */
		lldpdu[lldpdu_len + MNT_IF_NUM] = 0x1;

		/* TLV Info srting */
		if (HAS_IP) {
			getIP(ipWR);
			char buf[32];

			format_ip(buf, ipWR);
			memcpy(&lldpdu[lldpdu_len + LLDP_SUBTYPE + 1],
			       ipWR, 4);
		}
		/* TODO: add info about VLAN 9.5.9.9g */
		break;
	case USER_DEF:
		/* TODO define WR TLV */
		return;
		break;
	default:
		return;
		break;
	}
	lldpdu_len += tlv_len;
}

static void lldp_update(void)
{
	int i;
	/* add mandatory LLDP TLVs */
	memset(lldpdu, 0x0, LLDP_MAX_PKT_LEN);
	lldpdu_len = 0;

	pp_printf("lldp update\n");
	/* add all TLV's */
	for (i = CHASSIS_ID; i <= MNG_ADD; i++)
		lldp_add_tlv(i);

	/* end TLVs */
	lldp_add_tlv(END_LLDP);
}

static void lldp_init(void)
{
	struct wr_sockaddr saddr;

	/* LLDP: raw ethernet*/
	memset(&saddr, 0x0, sizeof(saddr));
	saddr.ethertype = htons(LLDP_ETH_TYP);

	lldp_socket = ptpd_netif_create_socket(&__static_lldp_socket, &saddr,
					       PTPD_SOCK_RAW_ETHERNET, 0);

	memset(&addr, 0x0, sizeof(struct wr_sockaddr));
	memcpy(addr.mac, LLDP_MCAST_MAC, 6);

	lldp_update();
}

static int lldp_poll(void)
{
	static int ticks;
	unsigned char new_ipWR;
	static unsigned char old_ipWR;
	uint8_t new_mac[ETH_ALEN];
	static uint8_t old_mac[ETH_ALEN];

	/* periodic tasks */
	if (ticks > LLDP_TX_TICK_INTERVAL) {
		get_mac_addr(new_mac);
		if (HAS_IP) {
			getIP(&new_ipWR);
		}

		/* Update only when IP or MAC changed */
		/* TODO: or VLAN changed */
		if (memcmp(&new_mac, &old_mac, ETH_ALEN)
		    || (HAS_IP && (ip_status != IP_TRAINING)
			&& memcmp(&new_ipWR, &old_ipWR, IPLEN))
		   ) {
			/* update LLDP info */
			lldp_update();
			/* copy new MAC nad IP */
			memcpy(&old_mac, &new_mac, ETH_ALEN);
			memcpy(&old_ipWR, &new_ipWR, IPLEN);
		}

		ptpd_netif_sendto(lldp_socket, &addr, lldpdu, lldpdu_len, 0);

		ticks = 0;
		return 1;
	} else {
		ticks += 1;
		return 0;
	}
}

DEFINE_WRC_TASK(lldp) = {
	.name = "lldp",
	.init = lldp_init,
	.job = lldp_poll,
};
