/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>
#include <wrc.h>

#define INET_ADDRSTRLEN 16

#include "syscon.h"
#include "uart.h"
#include "endpoint.h"
#include "hw/endpoint_mdio.h"
#include "minic.h"
#include "pps_gen.h"
#include "softpll_ng.h"

#undef PACKED
#include "ptpd_netif.h"

#ifdef CONFIG_PPSI
#include <ppsi/ppsi.h>
#else
#include "ptpd.h"
#endif

#if 0 /* not used, currently */
static int get_bitslide(int ep)
{
	return (pcs_read(16) >> 4) & 0x1f;
}
#endif

struct meas_entry {
	int delta_ns;
	int phase;
	int phase_sync;
	int ahead;
};


static void purge_socket(wr_socket_t *sock, char *buf)
{
	wr_sockaddr_t from;

	update_rx_queues();
	while (ptpd_netif_recvfrom(sock, &from, buf, 128, NULL) > 0)
		update_rx_queues();
}

#ifdef CONFIG_PPSI

extern struct pp_instance ppi_static;
static struct pp_instance *ppi = &ppi_static;

static int meas_phase_range(wr_socket_t * sock, int phase_min, int phase_max,
			    int phase_step, struct meas_entry *results)
{
	char buf[128];
	TimeInternal ts_rx, ts_sync = {0,};
	MsgHeader *mhdr;
	int setpoint = phase_min, i = 0, phase;

	mhdr = &ppi->msg_tmp_header;

	spll_set_phase_shift(SPLL_ALL_CHANNELS, phase_min);

	while (spll_shifter_busy(0)) ;

	purge_socket(sock, buf);

	i = 0;
	while (setpoint <= phase_max) {
		ptpd_netif_get_dmtd_phase(sock, &phase);

		update_rx_queues();
		int n = pp_recv_packet(ppi, buf, 128, &ts_rx);

		if (n > 0) {
			msg_unpack_header(ppi, buf);
			if (mhdr->messageType == 0)
				assign_TimeInternal(&ts_sync, &ts_rx);
			else if (mhdr->messageType == 8 && ts_sync.correct) {
				MsgFollowUp fup;
				msg_unpack_follow_up(buf, &fup);

				mprintf("Shift: %d/%dps [step %dps]        \r",
					setpoint, phase_max, phase_step);
				results[i].phase = phase;
				results[i].phase_sync = ts_sync.phase;
				results[i].ahead = ts_sync.raw_ahead;
				results[i].delta_ns =
				    fup.preciseOriginTimestamp.
				    nanosecondsField - ts_sync.nanoseconds;
				results[i].delta_ns +=
				    (fup.preciseOriginTimestamp.secondsField.
				     lsb - ts_sync.seconds) * 1000000000;

				setpoint += phase_step;
				spll_set_phase_shift(0, setpoint);
				while (spll_shifter_busy(0)) ;
				purge_socket(sock, buf);

				ts_sync.correct = 0;
				i++;
			}
		}
	}
	mprintf("\n");
	return i;
}

#else

static int meas_phase_range(wr_socket_t * sock, int phase_min, int phase_max,
			    int phase_step, struct meas_entry *results)
{
	char buf[128];
	wr_timestamp_t ts_rx, ts_sync = {0,};
	wr_sockaddr_t from;
	MsgHeader mhdr;
	int setpoint = phase_min, i = 0, phase;
	spll_set_phase_shift(SPLL_ALL_CHANNELS, phase_min);

	while (spll_shifter_busy(0)) ;

	purge_socket(sock, buf);

	i = 0;
	while (setpoint <= phase_max) {
		ptpd_netif_get_dmtd_phase(sock, &phase);

		update_rx_queues();
		int n = ptpd_netif_recvfrom(sock, &from, buf, 128, &ts_rx);

		if (n > 0) {
			msgUnpackHeader(buf, &mhdr);
			if (mhdr.messageType == 0)
				ts_sync = ts_rx;
			else if (mhdr.messageType == 8 && ts_sync.correct) {
				MsgFollowUp fup;
				msgUnpackFollowUp(buf, &fup);

				mprintf("Shift: %d/%dps [step %dps]        \r",
					setpoint, phase_max, phase_step);
				results[i].phase = phase;
				results[i].phase_sync = ts_sync.phase;
				results[i].ahead = ts_sync.raw_ahead;
				results[i].delta_ns =
				    fup.preciseOriginTimestamp.
				    nanosecondsField - ts_sync.nsec;
				results[i].delta_ns +=
				    (fup.preciseOriginTimestamp.secondsField.
				     lsb - ts_sync.sec) * 1000000000;

				setpoint += phase_step;
				spll_set_phase_shift(0, setpoint);
				while (spll_shifter_busy(0)) ;
				purge_socket(sock, buf);

				ts_sync.correct = 0;
				i++;
			}
		}
	}
	mprintf("\n");
	return i;
}

#endif /* CONFIG_PPSI else CONFIG_PTPNOPOSIX*/

static int find_transition(struct meas_entry *results, int n, int positive)
{
	int i;
	for (i = 0; i < n; i++) {
		if ((results[i].ahead ^ results[(i + 1) % n].ahead)
		    && (results[(i + 1) % n].ahead == positive))
			return i;
	}
	return -1;
}

extern void ptpd_netif_set_phase_transition(wr_socket_t * sock, int phase);

int measure_t24p(int *value)
{
	wr_socket_t *sock;
	wr_sockaddr_t sock_addr;
	int i, nr;
	struct meas_entry results[128];

#ifdef CONFIG_PPSI
	if (!NP(ppi)->inited) {
		if (pp_net_init(ppi) < 0)
			return -1;
		NP(ppi)->inited = 1;
	}
#endif
	spll_enable_ptracker(0, 1);

	sock_addr.family = PTPD_SOCK_RAW_ETHERNET;	// socket type
	sock_addr.ethertype = 0x88f7;
	sock_addr.mac[0] = 0x1;
	sock_addr.mac[1] = 0x1b;
	sock_addr.mac[2] = 0x19;
	sock_addr.mac[3] = 0;
	sock_addr.mac[4] = 0;
	sock_addr.mac[5] = 0;

	mprintf("LNK: ");
	while (!ep_link_up(NULL)) {
		mprintf(".");
		timer_delay(1000);
	}
	mprintf("\nPLL: ");

	spll_init(SPLL_MODE_SLAVE, 0, 1);

	while (!spll_check_lock(0)) ;
	mprintf("\n");

	if (ptpd_netif_init() != 0)
		return -1;

	sock = ptpd_netif_create_socket(PTPD_SOCK_RAW_ETHERNET, 0, &sock_addr);
	nr = meas_phase_range(sock, 0, 8000, 1000, results);
	int tplus = find_transition(results, nr, 1);
	int tminus = find_transition(results, nr, 0);

	int approx_plus = results[tplus].phase;
	int approx_minus = results[tminus].phase;

	nr = meas_phase_range(sock, approx_plus - 1000, approx_plus + 1000, 100,
			      results);
	tplus = find_transition(results, nr, 1);
	int phase_plus = results[tplus].phase;

	nr = meas_phase_range(sock, approx_minus - 1000, approx_minus + 1000,
			      100, results);
	tminus = find_transition(results, nr, 0);
	int phase_minus = results[tminus].phase;

	mprintf("Transitions found: positive @ %d ps, negative @ %d ps.\n",
		phase_plus, phase_minus);
	int ttrans = phase_minus - 1000;
	if (ttrans < 0)
		ttrans += 8000;

	mprintf("(t2/t4)_phase_transition = %dps\n", ttrans);
	ptpd_netif_set_phase_transition(sock, ttrans);

	mprintf("Verification... \n");
	nr = meas_phase_range(sock, 0, 16000, 500, results);

#ifdef CONFIG_PPSI
	pp_net_shutdown(ppi);
	NP(ppi)->inited = 0;
#endif

	for (i = 0; i < nr; i++)
		mprintf("phase_dmtd: %d delta_ns: %d, phase_sync: %d\n",
			results[i].phase, results[i].delta_ns,
			results[i].phase_sync);

	if (value)
		*value = ttrans;
	ptpd_netif_close_socket(sock);
	return 0;
}
