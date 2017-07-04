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

#define LLDP_MCAST_MAC 	"\x01\x80\xC2\x00\x00\x0E"
#define LLDP_ETH_TYP 	0x88CC

#define LLDP_PKT_LEN	0x9E /* 158 bytes */
#define TLV_MAX		0xA
#define LLDP_HEADER 	0x2
#define LLDP_SUBTYPE	0x1
#define IF_SUBTYPE	0x6
#define IF_NUM		0x10

#define LLDP_ID_SUBTYPE_MAC	3

#define LLDP_TX_FQ	1000

enum TLV_TYPE { END_LLDP = 0, 	/* mandatory TLVs */
		CHASSIS_ID,
		PORT_ID,
		TTL,
		PORT,      	/* optional TLVs */
		SYS_NAME,
		SYS_DESCR,
		SYS_CAPLTY,
		MNG_ADD,
		USER_DEF
		};

uint16_t tlv_type_len[TLV_MAX] = {	0x0,	/* LEN_LLDP_END */
					0x7,	/* LEN_CHASSIS_ID */
					0x7,	/* LEN_PORT_ID */
					0x2,	/* LEN_TTL */
					0x14,	/* LEN_PORT */
					0x14,	/* LEN_SYS_NAME */
					0x14,	/* LEN_SYS_DESCR */
					0x4,	/* LEN_SYS_CAPLTY */
					0xC	/* LEN_MNG_ADD */
					};

#endif /* __LLDP_H */
