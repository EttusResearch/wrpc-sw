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

//#define TRACE_WRAP mprintf
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

/*
 * The new, fully verified linearization algorithm.
 * Merges the phase, measured by the DDMTD with the number of clock ticks,
 * and makes sure there are no jumps resulting from different moments of transitions in the
 * coarse counter and the phase values.
 * As a result, we get the full, sub-ns RX timestamp.
 *
 * Have a look at the note at http://ohwr.org/documents/xxx for details.
 */
void ptpd_netif_linearize_rx_timestamp(wr_timestamp_t * ts, int32_t dmtd_phase,
				       int cntr_ahead, int transition_point,
				       int clock_period)
{
	int nsec_f, nsec_r;

	ts->raw_phase =  dmtd_phase;

/* The idea is simple: the asynchronous RX timestamp trigger is tagged by two counters:
   one counting at the rising clock edge, and the other on the falling. That means, the rising
   timestamp is 180 degree in advance wrs to the falling one. */

/* Calculate the nanoseconds value for both timestamps. The rising edge one
   is just the HW register */
  nsec_r = ts->nsec;
/* The falling edge TS is the rising - 1 thick if the "rising counter ahead" bit is set. */
 	nsec_f = cntr_ahead ? ts->nsec - (clock_period / 1000) : ts->nsec;
        

/* Adjust the rising edge timestamp phase so that it "jumps" roughly around the point
   where the counter value changes */
	int phase_r = ts->raw_phase - transition_point;
	if(phase_r < 0) /* unwrap negative value */
		phase_r += clock_period;

/* Do the same with the phase for the falling edge, but additionally shift it by extra 180 degrees
  (so that it matches the falling edge counter) */ 
	int phase_f = ts->raw_phase - transition_point + (clock_period / 2);
	if(phase_f < 0)
		phase_f += clock_period;
	if(phase_f >= clock_period)
		phase_f -= clock_period;

/* If we are within +- 25% from the transition in the rising edge counter, pick the falling one */	
  if( phase_r > 3 * clock_period / 4 || phase_r < clock_period / 4 )
  {
		ts->nsec = nsec_f;

		/* The falling edge timestamp is half a cycle later with respect to the rising one. Add
			 the extra delay, as rising edge is our reference */
		ts->phase = phase_f + clock_period / 2;
		if(ts->phase >= clock_period) /* Handle overflow */
		{
			ts->phase -= clock_period;
			ts->nsec += (clock_period / 1000);
		}
	} else { /* We are closer to the falling edge counter transition? Pick the opposite timestamp */
		ts->nsec = nsec_r;
		ts->phase = phase_r;
	}

	/* In an unlikely case, after all the calculations, the ns counter may be overflown. */
	if(ts->nsec >= 1000000000)
	{
		ts->nsec -= 1000000000;
		ts->sec++;
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
