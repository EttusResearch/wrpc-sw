/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2010 - 2015 GSI (www.gsi.de), CERN (www.cern.ch)
 * Author: Wesley W. Terpstra <w.terpstra@gsi.de>
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 * Author: Adam Wujek <adam.wujek@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>

#include "hal_exports.h"
#include <wrpc.h>
#include "ptpd_netif.h"

#include "board.h"
#include "pps_gen.h"
#include "minic.h"
#include "endpoint.h"
#include "softpll_ng.h"
#include "ipv4.h"

static struct wrpc_socket *socks[NET_MAX_SOCKETS];

//#define net_verbose pp_printf
int ptpd_netif_get_hw_addr(struct wrpc_socket *sock, mac_addr_t *mac)
{
	get_mac_addr((uint8_t *) mac);

	return 0;
}

void ptpd_netif_set_phase_transition(uint32_t phase)
{
	int i;

	for (i=0; i< ARRAY_SIZE(socks); ++i) {
		socks[i]->phase_transition = phase;
	}
}


struct wrpc_socket *ptpd_netif_create_socket(struct wrpc_socket *sock,
					     struct wr_sockaddr * bind_addr,
					     int udp_or_raw, int udpport)
{
	int i;
	struct hal_port_state pstate;

	/* Look for the first available socket. */
	for (i = 0; i < ARRAY_SIZE(socks); i++)
		if (!socks[i]) {
			socks[i] = sock;
			break;
		}
	if (i == ARRAY_SIZE(socks)) {
		pp_printf("%s: no socket slots left\n", __func__);
		return NULL;
	}
	net_verbose("%s: socket %p for %04x:%04x, slot %i\n", __func__,
		    sock, ntohs(bind_addr->ethertype),
		    udpport, i);

	if (wrpc_get_port_state(&pstate, "wr0" /* unused */) < 0)
		return NULL;

	/* copy and complete the bind information. If MAC is 0 use unicast */
	memset(&sock->bind_addr, 0, sizeof(struct wr_sockaddr));
	if (bind_addr)
		memcpy(&sock->bind_addr, bind_addr, sizeof(struct wr_sockaddr));
	sock->bind_addr.udpport = 0;
	if (udp_or_raw == PTPD_SOCK_UDP) {
		sock->bind_addr.ethertype = htons(0x0800); /* IPv4 */
		sock->bind_addr.udpport = udpport;
	}

	/*get mac from endpoint */
	get_mac_addr(sock->local_mac);

	sock->phase_transition = pstate.t2_phase_transition;
	sock->dmtd_phase = pstate.phase_val;

	/*packet queue */
	sock->queue.head = sock->queue.tail = 0;
	sock->queue.avail = sock->queue.size;
	sock->queue.n = 0;

	return sock;
}

int ptpd_netif_close_socket(struct wrpc_socket *s)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(socks); i++)
		if (socks[i] == s)
			socks[i] = NULL;
	return 0;
}

/*
 * The new, fully verified linearization algorithm.
 * Merges the phase, measured by the DDMTD with the number of clock
 * ticks, and makes sure there are no jumps resulting from different
 * moments of transitions in the coarse counter and the phase values.
 * As a result, we get the full, sub-ns RX timestamp.
 *
 * Have a look at the note at http://ohwr.org/documents/xxx for details.
 */
void ptpd_netif_linearize_rx_timestamp(struct wr_timestamp *ts,
				       int32_t dmtd_phase,
				       int cntr_ahead, int transition_point,
				       int clock_period)
{
	int nsec_f, nsec_r;

	ts->raw_phase =  dmtd_phase;

/* The idea is simple: the asynchronous RX timestamp trigger is tagged
 * by two counters: one counting at the rising clock edge, and the
 * other on the falling. That means, the rising timestamp is 180
 * degree in advance wrs to the falling one.
 */

/* Calculate the nanoseconds value for both timestamps. The rising edge one
   is just the HW register */
	nsec_r = ts->nsec;
/* The falling edge TS is the rising - 1 thick
    if the "rising counter ahead" bit is set. */
	nsec_f = cntr_ahead ? ts->nsec - (clock_period / 1000) : ts->nsec;

/* Adjust the rising edge timestamp phase so that it "jumps" roughly
   around the point where the counter value changes */
	int phase_r = ts->raw_phase - transition_point;
	if(phase_r < 0) /* unwrap negative value */
		phase_r += clock_period;

/* Do the same with the phase for the falling edge, but additionally shift
   it by extra 180 degrees (so that it matches the falling edge counter) */
	int phase_f = ts->raw_phase - transition_point + (clock_period / 2);
	if(phase_f < 0)
		phase_f += clock_period;
	if(phase_f >= clock_period)
		phase_f -= clock_period;

/* If we are within +- 25% from the transition in the rising edge counter,
   pick the falling one */
	if( phase_r > 3 * clock_period / 4 || phase_r < clock_period / 4 ) {
		ts->nsec = nsec_f;

		/* The falling edge timestamp is half a cycle later
		   with respect to the rising one. Add
		   the extra delay, as rising edge is our reference */
		ts->phase = phase_f + clock_period / 2;
		if(ts->phase >= clock_period) /* Handle overflow */
		{
			ts->phase -= clock_period;
			ts->nsec += (clock_period / 1000);
		}
	} else { /* We are closer to the falling edge counter transition?
		    Pick the opposite timestamp */
		ts->nsec = nsec_r;
		ts->phase = phase_r;
	}

	/* In an unlikely case, after all the calculations,
	   the ns counter may be overflown. */
	if(ts->nsec >= 1000000000)
	{
		ts->nsec -= 1000000000;
		ts->sec++;
	}

}

/* Slow, but we don't care much... */
static int wrap_copy_in(void *dst, struct sockq *q, size_t len, size_t buflen)
{
	char *dptr = dst;
	int i;

	if (!buflen)
		buflen = len;
	net_verbose("copy_in: tail %d avail %d len %d (buf %d)\n",
		    q->tail, q->avail, len, buflen);
	i = min(len, buflen);
	while (i--) {
		*dptr++ = q->buff[q->tail];
		q->tail++;
		if (q->tail == q->size)
			q->tail = 0;
	}
	if (len > buflen) {
		q->tail += len - buflen;
		while (q->tail > q->size)
			q->tail -= q->size;
	}
	return len;
}

static int wrap_copy_out(struct sockq *q, void *src, size_t len)
{
	char *sptr = src;
	int i = len;

	net_verbose("copy_out: head %d avail %d len %d\n", q->head, q->avail,
		   len);

	while (i--) {
		q->buff[q->head++] = *sptr++;
		if (q->head == q->size)
			q->head = 0;
	}
	return len;
}

int ptpd_netif_recvfrom(struct wrpc_socket *s, struct wr_sockaddr *from, void *data,
			size_t data_length, struct wr_timestamp *rx_timestamp)
{
	struct sockq *q = &s->queue;

	uint16_t size;
	struct wr_ethhdr hdr;
	struct hw_timestamp hwts;
	uint8_t spll_busy;

	/*check if there is something to fetch */
	if (!q->n)
		return 0;

	q->n--;

	q->avail += wrap_copy_in(&size, q, 2, 0);
	q->avail += wrap_copy_in(&hwts, q, sizeof(struct hw_timestamp), 0);
	q->avail += wrap_copy_in(&hdr, q, sizeof(struct wr_ethhdr), 0);
	q->avail += wrap_copy_in(data, q, size, data_length);

	from->ethertype = ntohs(hdr.ethtype);
	from->vlan = wrc_vlan_number; /* has been checked in rcvd frame */
	memcpy(from->mac, hdr.srcmac, 6);
	memcpy(from->mac_dest, hdr.dstmac, 6);

	if (rx_timestamp) {
		rx_timestamp->raw_nsec = hwts.nsec;
		rx_timestamp->raw_ahead = hwts.ahead;
		spll_busy = (uint8_t) spll_shifter_busy(0);
		spll_read_ptracker(0, &rx_timestamp->raw_phase, NULL);

		rx_timestamp->sec = hwts.sec;
		rx_timestamp->nsec = hwts.nsec;
		rx_timestamp->phase = 0;
		rx_timestamp->correct = hwts.valid && (!spll_busy);

		ptpd_netif_linearize_rx_timestamp(rx_timestamp,
						  rx_timestamp->raw_phase,
						  hwts.ahead,
						  s->phase_transition,
						  REF_CLOCK_PERIOD_PS);
	}

	net_verbose("%s: called from %p\n",
		    __func__, __builtin_return_address(0));
	net_verbose("RX: Size %d tail %d Smac %x:%x:%x:%x:%x:%x\n", size,
		   q->tail, hdr.srcmac[0], hdr.srcmac[1], hdr.srcmac[2],
		   hdr.srcmac[3], hdr.srcmac[4], hdr.srcmac[5]);

	return min(size, data_length);
}

int ptpd_netif_sendto(struct wrpc_socket * sock, struct wr_sockaddr *to, void *data,
		      size_t data_length, struct wr_timestamp *tx_timestamp)
{
	struct wrpc_socket *s = (struct wrpc_socket *)sock;
	struct hw_timestamp hwts;
	struct wr_ethhdr_vlan hdr;
	int rval;

	memcpy(hdr.dstmac, to->mac, 6);
	memcpy(hdr.srcmac, s->local_mac, 6);
	if (wrc_vlan_number) {
		hdr.ethtype = htons(0x8100);
		hdr.tag = htons(wrc_vlan_number | (sock->prio << 13));
		hdr.ethtype_2 = sock->bind_addr.ethertype; /* net order */
	} else {
		hdr.ethtype = sock->bind_addr.ethertype;
	}
	net_verbose("TX: socket %04x:%04x, len %i\n",
		    ntohs(s->bind_addr.ethertype),
		    s->bind_addr.udpport,
		    data_length);

	rval =
	    minic_tx_frame(&hdr, (uint8_t *) data,
			   data_length, &hwts);


	if (tx_timestamp) {
		tx_timestamp->sec = hwts.sec;
		tx_timestamp->nsec = hwts.nsec;
		tx_timestamp->phase = 0;
		tx_timestamp->correct = hwts.valid;
	}
	return rval;
}

static int update_rx_queues(void)
{
	struct wrpc_socket *s = NULL, *raws = NULL, *udps = NULL;
	struct sockq *q;
	struct hw_timestamp hwts;
	static struct wr_ethhdr hdr;
	int recvd, i, q_required;
	static uint8_t buffer[NET_MAX_SKBUF_SIZE - 32];
	uint8_t *payload = buffer;
	uint16_t size, port;
	uint16_t ethtype, tag;

	recvd =
	    minic_rx_frame(&hdr, buffer, sizeof(buffer),
			   &hwts);

	if (recvd <= 0)		/* No data received? */
		return 0;

	/* Remove the vlan tag, but  make sure it's the right one */
	ethtype = hdr.ethtype;
	tag = 0;
	if (ntohs(ethtype) == 0x8100) {
		memcpy(&tag, buffer, 2);
		memcpy(&hdr.ethtype, buffer + 2, 2);
		payload += 4;
		recvd -= 4;
	}
	if ((ntohs(tag) & 0xfff) != wrc_vlan_number) {
		net_verbose("%s: want vlan %i, got %i: discard\n",
				    __func__, wrc_vlan_number,
				    ntohs(tag) & 0xfff);
			return 0;
	}

	/* Prepare for IP/UDP checks */
	if (payload[IP_VERSION] == 0x45 && payload[IP_PROTOCOL] == 17)
		port = payload[UDP_DPORT] << 8 | payload[UDP_DPORT + 1];
	else
		port = 0;

	for (i = 0; i < ARRAY_SIZE(socks); i++) {
		s = socks[i];
		if (!s)
			continue;
		if (hdr.ethtype != s->bind_addr.ethertype)
			continue;
		if (!port && !s->bind_addr.udpport)
			raws = s; /* match with raw socket */
		if (port && s->bind_addr.udpport == port)
			udps = s; /*  match with udp socket */
	}
	s = udps;
	if (!s)
		s = raws;
	if (!s) {
		net_verbose("%s: could not find socket for packet\n",
			   __FUNCTION__);
		return 1;
	}

	q = &s->queue;
	q_required =
	    sizeof(struct wr_ethhdr) + recvd + sizeof(struct hw_timestamp) + 2;

	if (q->avail < q_required) {
		net_verbose
		    ("%s: queue for socket full; [avail %d required %d]\n",
		     __FUNCTION__, q->avail, q_required);
		return 1;
	}

	size = recvd;

	q->avail -= wrap_copy_out(q, &size, 2);
	q->avail -= wrap_copy_out(q, &hwts, sizeof(struct hw_timestamp));
	q->avail -= wrap_copy_out(q, &hdr, sizeof(struct wr_ethhdr));
	q->avail -= wrap_copy_out(q, payload, size);
	q->n++;

	net_verbose("Q: Size %d head %d Smac %x:%x:%x:%x:%x:%x\n", recvd,
		   q->head, hdr.srcmac[0], hdr.srcmac[1], hdr.srcmac[2],
		   hdr.srcmac[3], hdr.srcmac[4], hdr.srcmac[5]);

	net_verbose("%s: saved packet to socket %04x:%04x "
		    "[avail %d n %d size %d]\n", __FUNCTION__,
		    ntohs(s->bind_addr.ethertype),
		    s->bind_addr.udpport,
		    q->avail, q->n, q_required);
	return 1;
}
DEFINE_WRC_TASK(net_bh) = {
	.name = "net-bh",
	.enable = &link_status,
	.job = update_rx_queues,
};
