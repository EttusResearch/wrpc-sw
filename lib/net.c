#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "hal_exports.h"
#include "ptpd_netif.h"

#include "board.h"
#include "pps_gen.h"
#include "minic.h"
#include "endpoint.h"
#include "softpll_ng.h"

#define min(x,y) ((x) < (y) ? (x) : (y))

__attribute__ ((packed))
struct ethhdr {
	uint8_t dstmac[6];
	uint8_t srcmac[6];
	uint16_t ethtype;
};

struct timeout {
	uint64_t start_tics;
	uint64_t timeout;
};

struct sockq {
	uint8_t buf[NET_SKBUF_SIZE];
	uint16_t head, tail, avail;
	uint16_t n;
};

struct my_socket {
	int in_use;
	wr_sockaddr_t bind_addr;
	mac_addr_t local_mac;

	uint32_t phase_transition;
	uint32_t dmtd_phase;
	struct sockq queue;
};

static struct my_socket socks[NET_MAX_SOCKETS];

int ptpd_netif_init()
{
	memset(socks, 0, sizeof(socks));
	return PTPD_NETIF_OK;
}

int ptpd_netif_get_hw_addr(wr_socket_t * sock, mac_addr_t * mac)
{
	get_mac_addr((uint8_t *) mac);

	return 0;
}

void ptpd_netif_set_phase_transition(uint32_t phase)
{
	int i;

	for (i=0; i< NET_MAX_SOCKETS; ++i) {
		socks[i].phase_transition = phase;
	}
}

wr_socket_t *ptpd_netif_create_socket(int sock_type, int flags,
				      wr_sockaddr_t * bind_addr)
{
	int i;
	hexp_port_state_t pstate;
	struct my_socket *sock;

	/* Look for the first available socket. */
	for (sock = NULL, i = 0; i < NET_MAX_SOCKETS; i++)
		if (!socks[i].in_use) {
			sock = &socks[i];
			break;
		}

	if (!sock) {
		TRACE_WRAP("No sockets left.\n");
		return NULL;
	}

	if (sock_type != PTPD_SOCK_RAW_ETHERNET)
		return NULL;

	if (halexp_get_port_state(&pstate, bind_addr->if_name) < 0)
		return NULL;

	memcpy(&sock->bind_addr, bind_addr, sizeof(wr_sockaddr_t));

	/*get mac from endpoint */
	get_mac_addr(sock->local_mac);

	sock->phase_transition = pstate.t2_phase_transition;
	sock->dmtd_phase = pstate.phase_val;

	/*packet queue */
	sock->queue.head = sock->queue.tail = 0;
	sock->queue.avail = NET_SKBUF_SIZE;
	sock->queue.n = 0;
	sock->in_use = 1;

	return (wr_socket_t *) (sock);
}

int ptpd_netif_close_socket(wr_socket_t * sock)
{
	struct my_socket *s = (struct my_socket *)sock;

	if (s)
		s->in_use = 0;
	return 0;
}

static inline int inside_range(int min, int max, int x)
{
	if (min < max)
		return (x >= min && x <= max);
	else
		return (x <= max || x >= min);
}

void ptpd_netif_linearize_rx_timestamp(wr_timestamp_t * ts, int32_t dmtd_phase,
				       int cntr_ahead, int transition_point,
				       int clock_period)
{
	int trip_lo, trip_hi;
	int phase;

	// "phase" transition: DMTD output value (in picoseconds)
	// at which the transition of rising edge
	// TS counter will appear
	ts->raw_phase = dmtd_phase;

	phase = dmtd_phase;

	// calculate the range within which falling edge timestamp is stable
	// (no possible transitions)
	trip_lo = transition_point - clock_period / 4;
	if (trip_lo < 0)
		trip_lo += clock_period;

	trip_hi = transition_point + clock_period / 4;
	if (trip_hi >= clock_period)
		trip_hi -= clock_period;

	// pp_printf("linearize: %d %d %d %d -- %d\n", trip_lo, transition_point, trip_hi, ep_get_bitslide(), phase);

	if (inside_range(trip_lo, trip_hi, phase)) {
		// We are within +- 25% range of transition area of
		// rising counter. Take the falling edge counter value as the
		// "reliable" one. cntr_ahead will be 1 when the rising edge
		//counter is 1 tick ahead of the falling edge counter

		ts->nsec -= cntr_ahead ? (clock_period / 1000) : 0;

		// check if the phase is before the counter transition value
		// and eventually increase the counter by 1 to simulate a
		// timestamp transition exactly at s->phase_transition
		//DMTD phase value
		if (inside_range(transition_point, trip_hi, phase))
			ts->nsec += clock_period / 1000;

	}

	ts->phase = phase - transition_point;

	/* normalize timestamp after subtraction */
	if(ts->phase < 0)
	{
		ts->phase += clock_period;
		ts->nsec -= clock_period / 1000;
	}
	
	if(ts->nsec < 0)
	{
		ts->nsec += 1000000000;
		ts->sec--;
	}
}

/* Slow, but we don't care much... */
static int wrap_copy_in(void *dst, struct sockq *q, size_t len)
{
	char *dptr = dst;
	int i = len;

	TRACE_WRAP("copy_in: tail %d avail %d len %d\n", q->tail, q->avail,
		   len);

	while (i--) {
		*dptr++ = q->buf[q->tail];
		q->tail++;
		if (q->tail == NET_SKBUF_SIZE)
			q->tail = 0;
	}
	return len;
}

static int wrap_copy_out(struct sockq *q, void *src, size_t len)
{
	char *sptr = src;
	int i = len;

	TRACE_WRAP("copy_out: head %d avail %d len %d\n", q->head, q->avail,
		   len);

	while (i--) {
		q->buf[q->head++] = *sptr++;
		if (q->head == NET_SKBUF_SIZE)
			q->head = 0;
	}
	return len;
}

int ptpd_netif_recvfrom(wr_socket_t * sock, wr_sockaddr_t * from, void *data,
			size_t data_length, wr_timestamp_t * rx_timestamp)
{
	struct my_socket *s = (struct my_socket *)sock;
	struct sockq *q = &s->queue;

	uint16_t size;
	struct ethhdr hdr;
	struct hw_timestamp hwts;

	/*check if there is something to fetch */
	if (!q->n)
		return 0;

	q->n--;

	q->avail += wrap_copy_in(&size, q, 2);
	q->avail += wrap_copy_in(&hdr, q, sizeof(struct ethhdr));
	q->avail += wrap_copy_in(&hwts, q, sizeof(struct hw_timestamp));
	q->avail += wrap_copy_in(data, q, min(size, data_length));

	from->ethertype = ntohs(hdr.ethtype);
	memcpy(from->mac, hdr.srcmac, 6);
	memcpy(from->mac_dest, hdr.dstmac, 6);

	if (rx_timestamp) {
		rx_timestamp->raw_nsec = hwts.nsec;
		rx_timestamp->raw_ahead = hwts.ahead;
		spll_read_ptracker(0, &rx_timestamp->raw_phase, NULL);

		rx_timestamp->sec = hwts.sec;
		rx_timestamp->nsec = hwts.nsec;
		rx_timestamp->phase = 0;
		rx_timestamp->correct = hwts.valid;

		ptpd_netif_linearize_rx_timestamp(rx_timestamp,
						  rx_timestamp->raw_phase,
						  hwts.ahead,
						  s->phase_transition,
						  REF_CLOCK_PERIOD_PS);
	}

	TRACE_WRAP("RX: Size %d tail %d Smac %x:%x:%x:%x:%x:%x\n", size,
		   q->tail, hdr.srcmac[0], hdr.srcmac[1], hdr.srcmac[2],
		   hdr.srcmac[3], hdr.srcmac[4], hdr.srcmac[5]);

/*  TRACE_WRAP("%s: received data from %02x:%02x:%02x:%02x:%02x:%02x to %02x:%02x:%02x:%02x:%02x:%02x\n", __FUNCTION__, from->mac[0],from->mac[1],from->mac[2],from->mac[3],
                                                                                                                   from->mac[4],from->mac[5],from->mac[6],from->mac[7],
                                                                                                                   from->mac_dest[0],from->mac_dest[1],from->mac_dest[2],from->mac_dest[3],
                                                                                                                   from->mac_dest[4],from->mac_dest[5],from->mac_dest[6],from->mac_dest[7]);*/
	return min(size - sizeof(struct ethhdr), data_length);
	return 0;
}

int ptpd_netif_select(wr_socket_t * wrSock)
{
	return 0;
}

int ptpd_netif_sendto(wr_socket_t * sock, wr_sockaddr_t * to, void *data,
		      size_t data_length, wr_timestamp_t * tx_timestamp)
{
	struct my_socket *s = (struct my_socket *)sock;
	struct hw_timestamp hwts;
	struct ethhdr hdr;
	int rval;

	memcpy(hdr.dstmac, to->mac, 6);
	memcpy(hdr.srcmac, s->local_mac, 6);
	hdr.ethtype = to->ethertype;

	rval =
	    minic_tx_frame((uint8_t *) & hdr, (uint8_t *) data,
			   data_length + ETH_HEADER_SIZE, &hwts);

	if (tx_timestamp) {
		tx_timestamp->sec = hwts.sec;
		tx_timestamp->nsec = hwts.nsec;
		tx_timestamp->phase = 0;
		tx_timestamp->correct = hwts.valid;
	}
	return rval;
}

void update_rx_queues()
{
	struct my_socket *s = NULL;
	struct sockq *q;
	struct hw_timestamp hwts;
	static struct ethhdr hdr;
	int recvd, i, q_required;
	static uint8_t payload[NET_SKBUF_SIZE - 32];
	uint16_t size;

	recvd =
	    minic_rx_frame((uint8_t *) & hdr, payload, NET_SKBUF_SIZE - 32,
			   &hwts);

	if (recvd <= 0)		/* No data received? */
		return;

	for (i = 0; i < NET_MAX_SOCKETS; i++) {
		s = &socks[i];
		if (s->in_use && !memcmp(hdr.dstmac, s->bind_addr.mac, 6)
		    && hdr.ethtype == s->bind_addr.ethertype)
			break;	/*they match */
		s = NULL;
	}

	if (!s) {
		TRACE_WRAP("%s: could not find socket for packet\n",
			   __FUNCTION__);
		return;
	}

	q = &s->queue;
	q_required =
	    sizeof(struct ethhdr) + recvd + sizeof(struct hw_timestamp) + 2;

	if (q->avail < q_required) {
		TRACE_WRAP
		    ("%s: queue for socket full; [avail %d required %d]\n",
		     __FUNCTION__, q->avail, q_required);
		return;
	}

	size = recvd;

	q->avail -= wrap_copy_out(q, &size, 2);
	q->avail -= wrap_copy_out(q, &hdr, sizeof(struct ethhdr));
	q->avail -= wrap_copy_out(q, &hwts, sizeof(struct hw_timestamp));
	q->avail -= wrap_copy_out(q, payload, size);
	q->n++;

	TRACE_WRAP("Q: Size %d head %d Smac %x:%x:%x:%x:%x:%x\n", recvd,
		   q->head, hdr.srcmac[0], hdr.srcmac[1], hdr.srcmac[2],
		   hdr.srcmac[3], hdr.srcmac[4], hdr.srcmac[5]);

	TRACE_WRAP("%s: saved packet to queue [avail %d n %d size %d]\n",
		   __FUNCTION__, q->avail, q->n, q_required);
}
