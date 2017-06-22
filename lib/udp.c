/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2016 GSI (www.gsi.de)
 * Author: Alessandro Rubini <a.rubini@gsi.de>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

#include <string.h>
#include "ipv4.h"
#include "ptpd_netif.h"

void fill_udp(uint8_t * buf, int len, struct wr_udp_addr *uaddr)
{
	unsigned short sum;
	struct wr_udp_addr addr;
	uint8_t oddb;

	/* if there is no user-provided uaddr, we are replying */
	if (!uaddr) {
		memcpy(&addr.daddr, buf + IP_SOURCE, 4);
		memcpy(&addr.saddr, buf + IP_DEST, 4);
		memcpy(&addr.dport, buf + UDP_SPORT, 2);
		memcpy(&addr.sport, buf + UDP_DPORT, 2);
		uaddr = &addr;
	}

	// ------------ UDP -------------
	memcpy(buf + UDP_VIRT_SADDR, &uaddr->saddr, 4);
	memcpy(buf + UDP_VIRT_DADDR, &uaddr->daddr, 4);
	buf[UDP_VIRT_ZEROS] = 0;
	buf[UDP_VIRT_PROTO] = 0x11;	/* UDP */
	buf[UDP_VIRT_LENGTH] = (len - IP_END) >> 8;
	buf[UDP_VIRT_LENGTH + 1] = (len - IP_END) & 0xff;

	memcpy(buf + UDP_SPORT, &uaddr->sport, 2);
	memcpy(buf + UDP_DPORT, &uaddr->dport, 2);
	buf[UDP_LENGTH] = (len - IP_END) >> 8;
	buf[UDP_LENGTH + 1] = (len - IP_END) & 0xff;
	buf[UDP_CHECKSUM] = 0;
	buf[UDP_CHECKSUM + 1] = 0;

	/* pad, in case the payload is odd, but we can avoid the if */
	oddb = buf[len];
	buf[len] = '\0';
	sum = ipv4_checksum((unsigned short *)(buf + UDP_VIRT_SADDR),
			    (len + 1 - UDP_VIRT_SADDR) / 2);
	buf[len] = oddb;
	if (sum == 0)
		sum = 0xFFFF;

	buf[UDP_CHECKSUM + 0] = (sum >> 8);
	buf[UDP_CHECKSUM + 1] = sum & 0xff;

	// ------------ IP --------------
	buf[IP_VERSION] = 0x45;
	buf[IP_TOS] = 0;
	buf[IP_LEN + 0] = len >> 8;
	buf[IP_LEN + 1] = len & 0xff;
	buf[IP_ID + 0] = 0;
	buf[IP_ID + 1] = 0;
	buf[IP_FLAGS + 0] = 0;
	buf[IP_FLAGS + 1] = 0;
	buf[IP_TTL] = 63;
	buf[IP_PROTOCOL] = 17;	/* UDP */
	buf[IP_CHECKSUM + 0] = 0;
	buf[IP_CHECKSUM + 1] = 0;
	memcpy(buf + IP_SOURCE, &uaddr->saddr, 4);
	memcpy(buf + IP_DEST, &uaddr->daddr, 4);

	sum =
	    ipv4_checksum((unsigned short *)(buf + IP_VERSION),
			  (IP_END - IP_VERSION) / 2);
	buf[IP_CHECKSUM + 0] = sum >> 8;
	buf[IP_CHECKSUM + 1] = sum & 0xff;

	return;
}
