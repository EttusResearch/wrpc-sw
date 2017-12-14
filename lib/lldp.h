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

#ifndef __LLDP_H
#define __LLDP_H
#include "minic.h"
#define LLDP_MCAST_MAC	"\x01\x80\xC2\x00\x00\x0E" /* 802.1AB-2005,
						      Table 8-1 */
#define LLDP_ETH_TYP	0x88CC	/* 802.1AB-2005, Table 8-2 */

#define LLDP_MAX_PKT_LEN	0x9E /* 158 bytes */
#define TLV_MAX			0xA
#define LLDP_HEADER		0x2
#define LLDP_SUBTYPE		0x1
#define MNT_IF_SUBTYPE		0x6
#define MNT_IF_NUM		10

#define LLDP_TX_TICK_INTERVAL	10000

#define CHASSIS_ID_TLV_LEN	(1 + ETH_ALEN) /* chassis ID subtype byte
						* + MAC Len */
#define CHASSIS_ID_TYPE_MAC	4	/* 802.1AB-2005, table 9-2 */
#define PORT_ID_TLV_LEN		(1 + ETH_ALEN) /* port ID subtype byte
						* + MAC Len */
#define PORT_ID_SUBTYPE_MAC	3	/* 802.1AB-2005, table 9-3 */
#define TTL_ID_TLV_LEN		2	/* 802.1AB-2005, Figure 9-6 */
#define TTL_BYTE_MSB		0
#define TTL_BYTE_LSB		1
#define PORT_NAME		"wr0"
#define IPLEN			4	/* len of IP address in bytes */
#define MNG_ADDR_LEN		(1 + IPLEN) /* MNT addr subtype + IPLEN */
#define MNG_ADDR_SUBTYPE_IPv4	1	/* ianaAddressFamilyNumbers MIB */
#define MNG_ADDR_SUBTYPE_MAC	6	/* ianaAddressFamilyNumbers MIB */
#define MNG_IF_NUM_SUBTYPE_IFINDEX	2 /* 802.1AB-2005, 9.5.9.5 */
enum TLV_TYPE {
		END_LLDP = 0,	/* mandatory TLVs */
		CHASSIS_ID,
		PORT_ID,
		TTL,
		PORT,		/* optional TLVs */
		SYS_NAME,
		SYS_DESCR,
		SYS_CAPLTY,
		MNG_ADD,
		USER_DEF
		};

#endif /* __LLDP_H */
