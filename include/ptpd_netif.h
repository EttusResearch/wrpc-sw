/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2010 - 2013 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 * Author: Grzegorz Daniluk <grzegorz.daniluk@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

#ifndef __PTPD_NETIF_H
#define __PTPD_NETIF_H

#include <stdio.h>
#include <board.h>
//#include <inttypes.h>

#define PTPD_SOCK_UDP		0 /* wrong name, it should be "WRPC" */
#define PTPD_SOCK_RAW_ETHERNET 	1 /* but used in ppsi, which I won't change */

extern int link_status;
#define LINK_DOWN 0
#define LINK_WENT_UP 1
#define LINK_WENT_DOWN 2
#define LINK_UP 3

// GCC-specific
#ifndef PACKED
#  define PACKED __attribute__((packed))
#endif

#define PHYS_PORT_ANY			(0xffff)

// Some system-independent definitions
typedef uint8_t mac_addr_t[6];

// Socket address for ptp_netif_ functions
struct wr_sockaddr {
	// MAC address
	mac_addr_t mac;
	// Destination MAC address, filled by recvfrom()
	mac_addr_t mac_dest;
	// RAW ethertype
	uint16_t ethertype;
	uint16_t udpport;
	uint16_t vlan;
};

struct sockq {
	uint16_t head, tail, avail, size;
	uint16_t n;
	uint8_t *buff;
};

struct wrpc_socket {
	struct wr_sockaddr bind_addr;
	mac_addr_t local_mac;
	uint16_t prio;

	uint32_t phase_transition;
	uint32_t dmtd_phase;
	struct sockq queue;
};

PACKED struct wr_timestamp {

	// Seconds
	int64_t sec;

	// Nanoseconds
	int32_t nsec;

	// Phase (in picoseconds), linearized for rx, zero for send timestamps
	int32_t phase;		// phase(picoseconds)

	/* Raw time (non-linearized) for debugging purposes */
	int32_t raw_phase;
	int32_t raw_nsec;
	int32_t raw_ahead;

	// when 0, tstamp MAY be incorrect (e.g. during timebase adjustment)
	int correct;
};

// Creates UDP or Ethernet RAW socket (determined by sock_type) bound
// to bind_addr.
struct wrpc_socket *ptpd_netif_create_socket(struct wrpc_socket *s,
					     struct wr_sockaddr * bind_addr,
					     int udp_or_raw, int udpport);

// Sends a UDP/RAW packet (data, data_length) to addr in wr_sockaddr.
// For raw frames, mac/ethertype needs to be provided, for UDP - ip/port.
// Every transmitted frame has assigned a tag value, stored at tag parameter.
// This value is later used for recovering the precise transmit timestamp.
// If user doesn't need it, tag parameter can be left NULL.
int ptpd_netif_sendto(struct wrpc_socket *sock, struct wr_sockaddr *to, void *data,
		      size_t data_length, struct wr_timestamp *tx_ts);

// Receives an UDP/RAW packet. Data is written to (data) and len is returned.
// Maximum buffer length can be specified by data_length parameter.
// Sender information is stored in structure specified in 'from'.
// All RXed packets are timestamped and the timestamp
// is stored in rx_timestamp (unless it's NULL).
int ptpd_netif_recvfrom(struct wrpc_socket *sock, struct wr_sockaddr *from, void *data,
			size_t data_length, struct wr_timestamp *rx_timestamp);

// Closes the socket.
int ptpd_netif_close_socket(struct wrpc_socket * sock);

int ptpd_netif_get_hw_addr(struct wrpc_socket * sock, mac_addr_t * mac);

void ptpd_netif_linearize_rx_timestamp(struct wr_timestamp *ts,
				       int32_t dmtd_phase,
				       int cntr_ahead, int transition_point,
				       int clock_period);
void ptpd_netif_set_phase_transition(uint32_t phase);

struct hal_port_state;
int wrpc_get_port_state(struct hal_port_state *port,
			const char *port_name /* unused */);

#endif /* __PTPD_NETIF_H */
