// Network API for WR-PTPd

#ifndef __PTPD_NETIF_H
#define __PTPD_NETIF_H

#include <stdio.h>
//#include <inttypes.h>

#define PTPD_SOCK_RAW_ETHERNET 	1
#define PTPD_SOCK_UDP 		2

#define PTPD_FLAGS_MULTICAST		0x1

// error codes (to be extended)
#define PTPD_NETIF_READY		1

#define PTPD_NETIF_OK 			0
#define PTPD_NETIF_ERROR 		-1
#define PTPD_NETIF_NOT_READY 		-2
#define PTPD_NETIF_NOT_FOUND 		-3

// GCC-specific
#define PACKED __attribute__((packed))

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
// Network interface name (eth0, ...)
	char if_name[IFACE_NAME_LEN];
// Socket family (RAW ethernet/UDP)
	int family;
// MAC address
	mac_addr_t mac;
// Destination MASC address, filled by recvfrom() function on interfaces bound to multiple addresses
	mac_addr_t mac_dest;
// IP address
	ipv4_addr_t ip;
// UDP port
	uint16_t port;
// RAW ethertype
	uint16_t ethertype;
// physical port to bind socket to
	uint16_t physical_port;
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
int ptpd_netif_init();

// Creates UDP or Ethernet RAW socket (determined by sock_type) bound to bind_addr. If PTPD_FLAG_MULTICAST is set, the socket is
// automatically added to multicast group. User can specify physical_port field to bind the socket to specific switch port only.
wr_socket_t *ptpd_netif_create_socket(int sock_type, int flags,
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

int ptpd_netif_poll(wr_socket_t *);

int ptpd_netif_get_hw_addr(wr_socket_t * sock, mac_addr_t * mac);

/*
 * Function start HW locking of freq on WR Slave
 * return:
 * 	PTPD_NETIF_ERROR - locking not started
 * 	PTPD_NETIF_OK	 - locking started
 */
int ptpd_netif_locking_enable(int txrx, const char *ifaceName, int priority);

/*
 *
 * return:
 *
 * 	PTPD_NETIF_OK	 - locking started
 */

int ptpd_netif_locking_disable(int txrx, const char *ifaceName, int priority);

int ptpd_netif_locking_poll(int txrx, const char *ifaceName, int priority);

/*
 * Function turns on calibration (measurement of delay)
 * Tx or Rx depending on the txrx param
 * return:
 * 	PTPD_NETIF_NOT_READY	- if there is calibratin going on on another port
 * 	PTPD_NETIF_OK	 	- calibration started
 */
int ptpd_netif_calibrating_enable(int txrx, const char *ifaceName);

/*
 * Function turns off calibration (measurement of delay)
 * Tx or Rx depending on the txrx param
 * return:
 * 	PTPD_NETIF_ERROR	- if there is calibratin going on on another port
 * 	PTPD_NETIF_OK	 	- calibration started
 */
int ptpd_netif_calibrating_disable(int txrx, const char *ifaceName);

/*
 * Function checks if Rx/Tx (depending on the param) calibration is finished
 * if finished, returns measured delay in delta
 * return:
 *
 * 	PTPD_NETIF_OK	 - locking started
 */
int ptpd_netif_calibrating_poll(int txrx, const char *ifaceName,
				uint64_t * delta);

/*
 * Function turns on calibration pattern.
 * return:
 * 	PTPD_NETIF_NOT_READY	- if WRSW is busy with calibration on other switch or error occured
 * 	PTPD_NETIF_OK	 	- calibration started
 */
int ptpd_netif_calibration_pattern_enable(const char *ifaceName,
					  unsigned int calibrationPeriod,
					  unsigned int calibrationPattern,
					  unsigned int calibrationPatternLen);

/*
 * Function turns off calibration pattern
 * return:
 * 	PTPD_NETIF_ERROR	- turning off not successful
 * 	PTPD_NETIF_OK	 	- turning off  successful
 */
int ptpd_netif_calibration_pattern_disable(const char *ifaceName);

/*
 * Function reads calibration data if it's available, used at the beginning of PTPWRd to check if
 * HW knows already the interface's deltax, and therefore no need for calibration
 * return:
 *	PTPD_NETIF_NOT_FOUND 	- if deltas are not known
 *	PTPD_NETIF_OK		- if deltas are known, in such case, deltaTx and deltaRx have valid data
 */
int ptpd_netif_read_calibration_data(const char *ifaceName, uint64_t * deltaTx,
				     uint64_t * deltaRx, int32_t * fix_alpha,
				     int32_t * clock_period);

int ptpd_netif_select(wr_socket_t *);
int ptpd_netif_get_hw_addr(wr_socket_t * sock, mac_addr_t * mac);

/*
 * Function reads state of the given port (interface in our case), if the port is up, everything is OK, otherwise ERROR
 * return:
 *	PTPD_NETIF_ERROR 	- if the port is down
 *	PTPD_NETIF_OK		- if the port is up
 */
int ptpd_netif_get_port_state(const char *ifaceName);

/*
 * Function looks for a port (interface) for the port number 'number'
 * it will return in the argument ifname the port name
 * return:
 *	PTPD_NETIF_ERROR 	- port not found
 *	PTPD_NETIF_OK		- if the port found
 */
int ptpd_netif_get_ifName(char *ifname, int number);

/* Returns the millisecond "tics" counter value */
uint64_t ptpd_netif_get_msec_tics();

/*
 * Function detects external source lock,
 *
 * return:
 * HEXP_EXTSRC_STATUS_LOCKED 	0
 * HEXP_LOCK_STATUS_BUSY  	1
 * HEXP_EXTSRC_STATUS_NOSRC  	2
 */
int ptpd_netif_extsrc_detection();

/* Timebase adjustment functions - the servo should not call the HAL directly */
int ptpd_netif_adjust_counters(int64_t adjust_sec, int32_t adjust_nsec);
int ptpd_netif_adjust_phase(int32_t phase_ps);
int ptpd_netif_adjust_in_progress();
int ptpd_netif_get_dmtd_phase(wr_socket_t * sock, int32_t * phase);
void ptpd_netif_linearize_rx_timestamp(wr_timestamp_t * ts, int32_t dmtd_phase,
				       int cntr_ahead, int transition_point,
				       int clock_period);
int ptpd_netif_enable_timing_output(int enable);
int ptpd_netif_enable_phase_tracking(const char *if_name);

#endif
