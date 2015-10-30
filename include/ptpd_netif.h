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

#define PTPD_SOCK_RAW_ETHERNET 	1 /* used in ppsi to no aim: remove this */

#define PTPD_FLAGS_MULTICAST		0x1

// error codes (to be extended)
#define PTPD_NETIF_READY		1

#define PTPD_NETIF_OK 			0
#define PTPD_NETIF_ERROR 		-1
#define PTPD_NETIF_NOT_READY 		-2
#define PTPD_NETIF_NOT_FOUND 		-3

// GCC-specific
#ifndef PACKED
#  define PACKED __attribute__((packed))
#endif

#define PHYS_PORT_ANY			(0xffff)

#define PTPD_NETIF_TX  1
#define PTPD_NETIF_RX  2

#define IFACE_NAME_LEN 16

#define SLAVE_PRIORITY_0 0
#define SLAVE_PRIORITY_1 1
#define SLAVE_PRIORITY_2 2
#define SLAVE_PRIORITY_3 3
#define SLAVE_PRIORITY_4 4

// Some system-independent definitions
typedef uint8_t mac_addr_t[6];
typedef uint32_t ipv4_addr_t;

// WhiteRabbit socket - it's void pointer as the real socket structure is private and probably platform-specific.
typedef void *wr_socket_t;

// Socket address for ptp_netif_ functions
typedef struct {
// MAC address
	mac_addr_t mac;
// Destination MASC address, filled by recvfrom() function on interfaces bound to multiple addresses
	mac_addr_t mac_dest;
// RAW ethertype
	uint16_t ethertype;
} wr_sockaddr_t;

PACKED struct _wr_timestamp {

	// Seconds
	int64_t sec;

	// Nanoseconds
	int32_t nsec;

	// Phase (in picoseconds), linearized for receive timestamps, zero for send timestamps
	int32_t phase;		// phase(picoseconds)

	/* Raw time (non-linearized) for debugging purposes */
	int32_t raw_phase;
	int32_t raw_nsec;
	int32_t raw_ahead;

	// correctness flag: when 0, the timestamp MAY be incorrect (e.g. generated during timebase adjustment)
	int correct;
	//int cntr_ahead;
};

typedef struct _wr_timestamp wr_timestamp_t;

/* OK. These functions we'll develop along with network card driver. You can write your own UDP-based stubs for testing purposes. */

// Initialization of network interface:
// - opens devices
// - does necessary ioctls()
// - initializes connection with the mighty HAL daemon
int ptpd_netif_init(void);

// Creates UDP or Ethernet RAW socket (determined by sock_type) bound to bind_addr. If PTPD_FLAG_MULTICAST is set, the socket is
// automatically added to multicast group. User can specify physical_port field to bind the socket to specific switch port only.
wr_socket_t *ptpd_netif_create_socket(int unused, int unused2,
				      wr_sockaddr_t * bind_addr);

// Sends a UDP/RAW packet (data, data_length) to address provided in wr_sockaddr_t.
// For raw frames, mac/ethertype needs to be provided, for UDP - ip/port.
// Every transmitted frame has assigned a tag value, stored at tag parameter. This value is later used
// for recovering the precise transmit timestamp. If user doesn't need it, tag parameter can be left NULL.

int ptpd_netif_sendto(wr_socket_t * sock, wr_sockaddr_t * to, void *data,
		      size_t data_length, wr_timestamp_t * tx_ts);

// Receives an UDP/RAW packet. Data is written to (data) and length is returned. Maximum buffer length can be specified
// by data_length parameter. Sender information is stored in structure specified in 'from'. All RXed packets are timestamped and the timestamp
// is stored in rx_timestamp (unless it's NULL).
int ptpd_netif_recvfrom(wr_socket_t * sock, wr_sockaddr_t * from, void *data,
			size_t data_length, wr_timestamp_t * rx_timestamp);

// Closes the socket.
int ptpd_netif_close_socket(wr_socket_t * sock);

int ptpd_netif_get_hw_addr(wr_socket_t * sock, mac_addr_t * mac);


int ptpd_netif_get_hw_addr(wr_socket_t * sock, mac_addr_t * mac);

void ptpd_netif_linearize_rx_timestamp(wr_timestamp_t * ts, int32_t dmtd_phase,
				       int cntr_ahead, int transition_point,
				       int clock_period);
void ptpd_netif_set_phase_transition(uint32_t phase);

struct hal_port_state;
int wrpc_get_port_state(struct hal_port_state *port, const char *port_name /* unused */);

#endif /* __PTPD_NETIF_H */
