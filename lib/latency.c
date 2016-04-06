/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2016 GSI (www.gsi.de)
 * Author: Alessandro Rubini <a.rubini@gsi.de>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <wrc.h>
#include <wrpc.h>
#include <ptpd_netif.h>
#include <shell.h>
#include "ipv4.h"

/* latency probe: we need to enqueue 3 short frames: 64*3+overhead = 256 */
static uint8_t __latency_queue[256];
static struct wrpc_socket *latency_socket, __static_latency_socket = {
	.queue.buff = __latency_queue,
	.queue.size = sizeof(__latency_queue),
};

static struct wr_sockaddr latency_addr = {
	.mac = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, /* for binding */
	.mac_dest = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, /* for sending */
	.ethertype = 0, /* htons(CONFIG_LATENCY_ETHTYPE) -- not constant! */
};

static void latency_init(void)
{
	latency_addr.ethertype = htons(CONFIG_LATENCY_ETHTYPE);
	latency_socket = ptpd_netif_create_socket(&__static_latency_socket,
						  &latency_addr,
						  PTPD_SOCK_RAW_ETHERNET, 0);
}

static struct latency_frame {
	uint16_t type;   /* 0, 1, 2 */
	uint16_t sequence;
	struct wr_timestamp ts1, ts2;
} frame;

static uint16_t prev_sequence, prev_type;

static void ts_sub(struct wr_timestamp *t2, struct wr_timestamp *t1,
		      struct wr_timestamp *res)
{
	int64_t resll; /* every field is signed: good */

	resll = t2->sec - t1->sec;
	resll *= 1000 * 1000 * 1000;
	resll += t2->nsec - t1->nsec;
	resll *= 1000;
	resll += t2->phase - t1->phase;
	/* seconds must be 0 and result must be positive */
	res->sec = 0;
	res->phase = __div64_32((uint64_t *)&resll, 1000);
	res->nsec = resll;
}

static void latency_warning(void) {
	pp_printf("%s: unexpected %i.%i after %i.%i\n", __func__,
		  frame.sequence, frame.type, prev_sequence, prev_type);
}

static int latency_poll_rx(void)
{
	static struct wr_timestamp ts[2];
	struct wr_timestamp ts_tmp;
	struct wr_sockaddr addr;
	int len;

	len = ptpd_netif_recvfrom(latency_socket, &addr,
				  &frame, sizeof(frame), &ts_tmp);
	if (len < sizeof(frame))
		return 0;

	/* check sequence and type is ok */
	switch(frame.type) {
	case 0:
		if (frame.sequence != prev_sequence + 1 || prev_type != 2)
			latency_warning();
		ts[0] = ts_tmp; /* 0 is always valid, it's a new set */
		ts[1].sec = 0; /* but still incomplete info */
		break;

	case 1:
		if (frame.sequence != prev_sequence || prev_type != 0)
			latency_warning();
		else
			ts[1] = ts_tmp;
		break;

	case 2:
		if (frame.sequence != prev_sequence || prev_type != 1) {
			latency_warning();
			break;
		}
		if (!ts[1].sec)
			break;
	}
	prev_sequence = frame.sequence;
	prev_type = frame.type;
	if (frame.type != 2)
		return 1;


	net_verbose("ts_rx 1: %9li.%09i.%03i\n", (long)ts[0].sec,
		    ts[0].nsec, ts[0].phase);
	net_verbose("ts_tx 1: %9li.%09i.%03i\n", (long)frame.ts1.sec,
		    frame.ts1.nsec, frame.ts1.phase);

	net_verbose("ts_rx 2: %9li.%09i.%03i\n", (long)ts[1].sec,
		    ts[1].nsec, ts[1].phase);
	net_verbose("ts_tx 2: %9li.%09i.%03i\n", (long)frame.ts2.sec,
		    frame.ts2.nsec, frame.ts2.phase);

	ts_sub(ts + 0, &frame.ts1, &ts_tmp);
	pp_printf("latency prio7: %i.%03i\n", ts_tmp.nsec, ts_tmp.phase);

	ts_sub(ts + 1, &frame.ts2, &ts_tmp);
	pp_printf("latency prio6: %i.%03i\n", ts_tmp.nsec, ts_tmp.phase);
	return 1;
}

static int latency_poll_tx(void)
{
	static uint16_t sequence;

	/* Send three frames -- lazily in native byte order */
	memset(&frame, 0, sizeof(frame));
	frame.sequence = sequence++;

	latency_socket->prio = 7;
	ptpd_netif_sendto(latency_socket, &latency_addr, &frame, sizeof(frame),
			  &frame.ts1);

	frame.type = 1;
	latency_socket->prio = 6;
	ptpd_netif_sendto(latency_socket, &latency_addr, &frame, sizeof(frame),
			  &frame.ts2);

	frame.type = 2;
	latency_socket->prio = 0;
	ptpd_netif_sendto(latency_socket, &latency_addr, &frame, sizeof(frame),
			  NULL);
	return 1;

}

static uint32_t lastt;
static uint32_t latency_period_ms;

static int latency_poll(void)
{
	if (!latency_period_ms)
		return latency_poll_rx();

	/* Periodically send the frames */
	if (task_not_yet(&lastt, latency_period_ms))
		return 0;
	return latency_poll_tx();
}

DEFINE_WRC_TASK(uptime) = {
	.name = "latency-probe",
	.init = latency_init,
	.job = latency_poll,
};


static int cmd_ltest(const char *args[])
{
	int v = 0, v1 = 0;

	if (args[1]) {
		fromdec(args[1], &v1); /* ms */
	}
	if (args[0]) {
		fromdec(args[0], &v);
		latency_period_ms = v * 1000 + v1;
		lastt = 0; /* reset, so it fires immediately */
	}
	pp_printf("%i.%03i\n", latency_period_ms / 1000,
		  latency_period_ms % 1000);
	return 0;
}

DEFINE_WRC_COMMAND(ltest) = {
	.name = "ltest",
	.exec = cmd_ltest,
};

