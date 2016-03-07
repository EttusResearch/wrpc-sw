/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2016 GSI (www.gsi.de)
 * Author: Alessandro Rubini <a.rubini@gsi.de>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <string.h>
#include <wrpc.h>
#include <minic.h>
#include <limits.h>

#include "endpoint.h"
#include "ipv4.h"
#include "ptpd_netif.h"
#include "pps_gen.h"
#include "hw/memlayout.h"
#include "hw/etherbone-config.h"

#ifndef htons
#define htons(x) x
#endif

#define ASN_APPLICATION	((u_char)0x40)
#define ASN_INTEGER	((u_char)0x02)
#define ASN_OCTET_STR	((u_char)0x04)
#define ASN_IPADDRESS	(ASN_APPLICATION | 0)
#define ASN_COUNTER	(ASN_APPLICATION | 1)
#define ASN_GAUGE	(ASN_APPLICATION | 2)
#define ASN_UNSIGNED	(ASN_APPLICATION | 2)   /* RFC 1902 - same as GAUGE */
#define ASN_TIMETICKS	(ASN_APPLICATION | 3)
#define ASN_COUNTER64   (ASN_APPLICATION | 6)

#define SNMP_GET 0xA0
#define SNMP_GET_NEXT 0xA1
#define SNMP_GET_RESPONSE 0xA2

struct snmp_oid {
	uint8_t *oid_match;
	int (*fill)(uint8_t *buf, struct snmp_oid *obj);
	void **p;
	uint8_t oid_len;
	uint8_t asn;
	uint8_t offset; /* increase it if it is not enough */
};

extern struct pp_instance ppi_static;
static struct wr_servo_state *wr_s_state;
struct wr_minic * minic_p = &minic;
/* temp memory used in case we have to "correct" values before we send them */
static void *tmp_p;
static int32_t tmp_int32;
struct snmp_oid tmp_obj;

static uint8_t __snmp_queue[256];
static struct wrpc_socket __static_snmp_socket = {
	.queue.buff = __snmp_queue,
	.queue.size = sizeof(__snmp_queue),
};
static struct wrpc_socket *snmp_socket;

static void snmp_init(void)
{
	/* Use UDP engine activated by function arguments  */
	snmp_socket = ptpd_netif_create_socket(&__static_snmp_socket, NULL,
						PTPD_SOCK_UDP, 161 /* snmp */);
	/* TODO: check if pointer(s) is initialized already */
	wr_s_state =
		&((struct wr_data *)ppi_static.ext_data)->servo_state;
}

/* These fill the actual information, returning the lenght */
static int fill_name(uint8_t *buf, struct snmp_oid *obj)
{
	strcpy((void *)(buf + 2), "wrc");
	buf[1] = strlen((void *)(buf + 2));
	buf[0] = ASN_OCTET_STR;
	return 2 + buf[1];
}
static int fill_tics(uint8_t *buf, struct snmp_oid *obj)
{
	uint32_t tics = htonl(timer_get_tics() / 10); /* hundredths... bah! */

	memcpy(buf + 2, &tics, sizeof(tics));
	buf[1] = sizeof(tics);
	buf[0] = ASN_TIMETICKS;
	return 2 + sizeof(tics);
}
static int fill_date(uint8_t *buf, struct snmp_oid *obj)
{
	uint64_t secs;
	char *datestr;

	shw_pps_gen_get_time(&secs, NULL);
	datestr = format_time(secs, TIME_FORMAT_SNMP);

	memcpy((void *)(buf + 2), datestr, 12);
	buf[1] = 8; /* size is hardwired. See mib document or prev. commit */
	buf[0] = ASN_OCTET_STR;
	return 2 + buf[1];
}

#define MAX_OCTET_STR_LEN 32
static int fill_struct_asn(uint8_t *buf, struct snmp_oid *obj)
{
	uint32_t tmp;
	int tmp_int;
	uint8_t *oid_data = buf + 2;
	void *src_data = (*(obj->p)) + obj->offset;
	uint8_t *len;
	buf[0] = obj->asn;
	len = &buf[1];
	switch(obj->asn){
	    case ASN_COUNTER:
	    case ASN_INTEGER:
		tmp = htonl(*(uint32_t*)src_data);
		memcpy(oid_data, &tmp, sizeof(tmp));
		*len = sizeof(tmp);
		snmp_verbose("fill_struct_asn 0x%x\n", *(uint32_t*)src_data);
		break;
	    case ASN_OCTET_STR:
		*len = strnlen((char *)src_data, MAX_OCTET_STR_LEN - 1);
		memcpy(oid_data, src_data, *len + 1);
		snmp_verbose("fill_struct_asn %s len %d\n",
			     (char*)src_data, *len);
		break;
	    default:
		break;
	}
	return 2 + buf[1];
}


static int fill_int32_saturate(uint8_t *buf, struct snmp_oid *obj) {
	int64_t tmp_int64;
	/* if we want to modify an object we need to do that on a copy,
	 * otherwise pointers to the values will be overwritten */
	tmp_obj = *obj;
	tmp_int64 = *(int64_t *)(*(obj->p) + obj->offset);
	/* gcc doesn't support printing 64bit values */
	snmp_verbose("fill_int32_saturate 64bit value 0x%08x|%08x\n",
		     (int32_t)((tmp_int64) >> 32), (uint32_t)tmp_int64);
	/* saturate int32 */
	if (tmp_int64 >= INT_MAX)
		tmp_int64 = INT_MAX;
	else if (tmp_int64 <= INT_MIN)
		tmp_int64 = INT_MIN;
	/* fill_struct_asn will print saturated value anyway */
	tmp_int32 = (int32_t) tmp_int64;
	tmp_obj.offset = 0;
	tmp_p = &tmp_int32;
	tmp_obj.p = &tmp_p;
	return fill_struct_asn(buf, &tmp_obj);
}

static uint8_t oid_start[] = {0x2B}; /* magic entry for snmpwalk (.1.3), when
		    * real snmpget next is implemented it may be unnecessary */
static uint8_t oid_name[] = {0x2B,0x06,0x01,0x02,0x01,0x01,0x05,0x00};
static uint8_t oid_tics[] = {0x2B,0x06,0x01,0x02,0x01,0x19,0x01,0x01,0x00};
static uint8_t oid_date[] = {0x2B,0x06,0x01,0x02,0x01,0x19,0x01,0x02,0x00};
static uint8_t oid_wrsPtpServoState[] =   {0x2B,6,1,4,1,96,100,7,5,1, 6,1};
static uint8_t oid_wrsPtpServoStateN[] =  {0x2B,6,1,4,1,96,100,7,5,1, 7,1};
static uint8_t oid_wrsPtpClockOffsetPsHR[] = {0x2B,6,1,4,1,96,100,7,5,1,11,1};
static uint8_t oid_wrsPtpSkew[] =         {0x2B,6,1,4,1,96,100,7,5,1,12,1};
static uint8_t oid_wrsPtpServoUpdates[] = {0x2B,6,1,4,1,96,100,7,5,1,15,1};
static uint8_t oid_wrsPtpDeltaTxM[] =     {0x2B,6,1,4,1,96,100,7,5,1,16,1};
static uint8_t oid_wrsPtpDeltaRxM[] =     {0x2B,6,1,4,1,96,100,7,5,1,17,1};
static uint8_t oid_wrsPtpDeltaTxS[] =     {0x2B,6,1,4,1,96,100,7,5,1,18,1};
static uint8_t oid_wrsPtpDeltaRxS[] =     {0x2B,6,1,4,1,96,100,7,5,1,19,1};
static uint8_t oid_wrpcPtpRTTHR[]    =    {0x2B,6,1,4,1,96,101,1,1,0}; /* wrsPtpRTT is 64bit, use here saturated version */
static uint8_t oid_wrpcPtpDeltaMs[] =     {0x2B,6,1,4,1,96,101,1,2,0};
static uint8_t oid_wrpcPtpCurSetpoint[] = {0x2B,6,1,4,1,96,101,1,3,0};
static uint8_t oid_wrpcNicTX[] =          {0x2B,6,1,4,1,96,101,2,1,0};
static uint8_t oid_wrpcNicRX[] =          {0x2B,6,1,4,1,96,101,2,2,0};


#define OID_FIELD_STRUCT(_oid, _fname, _asn, _type, _pointer, _field) { \
	.oid_match = _oid, \
	.oid_len = sizeof(_oid), \
	.fill = _fname, \
	.asn = _asn, \
	.p = (void **)_pointer, \
	.offset = offsetof(_type, _field), \
}

#define OID_FIELD(_oid, _fname, _asn) { \
	.oid_match = _oid, \
	.oid_len = sizeof(_oid), \
	.fill = _fname, \
	.asn = _asn, \
}

static struct snmp_oid oid_array[] = {
	OID_FIELD(oid_start, fill_name, 0), /* return whatever, used only for walks */
	OID_FIELD(oid_name, fill_name, 0),
	OID_FIELD(oid_tics, fill_tics, 0),
	OID_FIELD(oid_date, fill_date, 0),
	OID_FIELD_STRUCT(oid_wrsPtpServoState,   fill_struct_asn, ASN_OCTET_STR, struct wr_servo_state, &wr_s_state, servo_state_name),
	OID_FIELD_STRUCT(oid_wrsPtpServoStateN,  fill_struct_asn, ASN_INTEGER,   struct wr_servo_state, &wr_s_state, state),
	OID_FIELD_STRUCT(oid_wrsPtpClockOffsetPsHR,fill_int32_saturate, ASN_INTEGER, struct wr_servo_state, &wr_s_state, offset), /* saturated */
	OID_FIELD_STRUCT(oid_wrsPtpSkew,         fill_int32_saturate, ASN_INTEGER, struct wr_servo_state, &wr_s_state, skew), /* saturated */
	OID_FIELD_STRUCT(oid_wrsPtpServoUpdates, fill_struct_asn, ASN_COUNTER,   struct wr_servo_state, &wr_s_state, update_count),
	OID_FIELD_STRUCT(oid_wrsPtpDeltaTxM,     fill_struct_asn, ASN_INTEGER,   struct wr_servo_state, &wr_s_state, delta_tx_m),
	OID_FIELD_STRUCT(oid_wrsPtpDeltaRxM,     fill_struct_asn, ASN_INTEGER,   struct wr_servo_state, &wr_s_state, delta_rx_m),
	OID_FIELD_STRUCT(oid_wrsPtpDeltaTxS,     fill_struct_asn, ASN_INTEGER,   struct wr_servo_state, &wr_s_state, delta_tx_s),
	OID_FIELD_STRUCT(oid_wrsPtpDeltaRxS,     fill_struct_asn, ASN_INTEGER,   struct wr_servo_state, &wr_s_state, delta_rx_s),
	OID_FIELD_STRUCT(oid_wrpcPtpRTTHR,       fill_int32_saturate, ASN_INTEGER, struct wr_servo_state, &wr_s_state, picos_mu), /* saturated */
	OID_FIELD_STRUCT(oid_wrpcPtpDeltaMs,     fill_int32_saturate, ASN_INTEGER, struct wr_servo_state, &wr_s_state, delta_ms), /* raw value used to calculate wrsPtpLinkLength, original calculations uses float */
	OID_FIELD_STRUCT(oid_wrpcPtpCurSetpoint, fill_struct_asn, ASN_INTEGER,  struct wr_servo_state, &wr_s_state, cur_setpoint),
	OID_FIELD_STRUCT(oid_wrpcNicTX,          fill_struct_asn, ASN_COUNTER,  struct wr_minic, &minic_p, tx_count),
	OID_FIELD_STRUCT(oid_wrpcNicRX,          fill_struct_asn, ASN_COUNTER,  struct wr_minic, &minic_p, rx_count),
	
	{ 0, }
};

/*
 * Perverse...  snmpwalk does getnext anyways. Let's support snmpget only
 *
 * We support the following ones:
 *    RFC1213-MIB::sysName.0 (string)              .1.3.6.1.2.1.1.5.0
 *    HOST-RESOURCES-MIB::hrSystemUptime.0 (ticks) .1.3.6.1.2.1.25.1.1.0
 *    HOST-RESOURCES-MIB::hrSystemDate.0 (string)  .1.3.6.1.2.1.25.1.2.0
 *
 *   The request for sysname is like this (-c public):
 *   30 29                                   sequence of 0x29
 *     02 01 00                              int: snmp version 1 (0x00)
 *     04 06 70 75 62 6C 69 63               string of 6: "public"
 *     A0 1C                                 get request of 0x1c
 *       02 04 27 05 36 A9                   int of 4: request ID
 *       02 01 00                            int of 1: error
 *       02 01 00                            int of 1: error
 *       30 0E                               sequence of 0x0e
 *         30 0C                             sequence of 0x0c
 *           06 08  2B 06 01 02 01 01 05 00  oid of 8: .1.3.6.1.2.1.1.5.0
 *           05 00                           null
 *
 *
 *    Response for sysname:
 *    30 30
 *      02 01 00
 *      04 06 70 75 62 6C 69 63    "public"
 *      A2 23
 *        02 04 27 05 36 A9     seqID
 *        02 01 00
 *        02 01 00
 *        30 15
 *          30 13
 *            06 08  2B 06 01  02 01 01 05 00
 *            04 07  6C 61 70 74 6F 70 6F          "laptopo"
 *
 */

/*
 * The following array defines the frame format, sizes are ignored on read
 * but must be filled on write, so mark them specially
 */
#define BYTE_SIZE 0xff
#define BYTE_IGNORE 0xfe
#define BYTE_PDU 0xfd
static uint8_t match_array[] = {
	0x30, BYTE_SIZE,
	0x02, 0x01, 0x00,
	0x04, 0x06, 0x70, 0x75, 0x62, 0x6C, 0x69, 0x63,
	BYTE_PDU, BYTE_SIZE,
	0x02, 0x04, BYTE_IGNORE, BYTE_IGNORE, BYTE_IGNORE, BYTE_IGNORE,
	0x02, 0x01, 0x00,
	0x02, 0x01, 0x00,
	0x30, BYTE_SIZE,
	0x30, BYTE_SIZE,
	0x06, BYTE_IGNORE,  /* oid follows */
};


static void print_oid_verbose(uint8_t *oid, int len) {
	/*uint8_t * oid_end = oid + len;*/
	int i;
	snmp_verbose(".1.3");
	for (i = 1; i < len; i++)
		snmp_verbose(".%d", *(oid + i));
}

/* And, now, work out your generic frame responder... */
static int snmp_respond(uint8_t *buf)
{
	struct snmp_oid *oid;
	uint8_t *newbuf;
	int i;
	uint8_t len;
	uint8_t snmp_mode;
	uint8_t incoming_oid_len;
	for (i = 0; i < sizeof(match_array); i++) {
		switch (match_array[i]) {
		case BYTE_SIZE:
		case BYTE_IGNORE:
			continue;
		case BYTE_PDU:
			if (buf[i] != SNMP_GET && buf[i] != SNMP_GET_NEXT)
				return -1;
			snmp_mode = buf[i];
			break;
		default:
			if (buf[i] != match_array[i])
				return -1;
		}
	}
	snmp_verbose("%s: header match ok\n", __func__);

	newbuf = buf + i;
	/* save size of incoming oid */
	incoming_oid_len = buf[i - 1];
	for (oid = oid_array; oid->oid_len; oid++)
		if (oid->oid_len == incoming_oid_len &&
		    !memcmp(oid->oid_match, newbuf, oid->oid_len))
			break;
	if (!oid->oid_len)
		return -1;
	if (snmp_mode == SNMP_GET_NEXT) {
		/* for SNMP_GET_NEXT go to next OID */
		oid++;
		if (!oid->oid_len) {
			/* we got request with last available OID */
			return -1;
		}
		memcpy(newbuf, oid->oid_match, oid->oid_len);
		/* update OID len, since it might be different */
		*(newbuf - 1) = oid->oid_len;
	}
	snmp_verbose("%s: oid found: ", __func__);
	print_oid_verbose(oid->oid_match, oid->oid_len);
	snmp_verbose(" calling %p\n", oid->fill);
	/* Phew.... we matched the OID, so let's call the filler  */
	newbuf += oid->oid_len;
	len = oid->fill(newbuf, oid);
	/* now fix all size fields and change PDU */
	for (i = 0; i < sizeof(match_array); i++) {
		int remain = (sizeof(match_array) - 1) - i;

		if (match_array[i] == BYTE_PDU)
			buf[i] = SNMP_GET_RESPONSE;
		if (match_array[i] != BYTE_SIZE)
			continue;
		snmp_verbose("offset %i 0x%02x is len %i\n", i, i,
			    remain + oid->oid_len + len);
		buf[i] = remain + oid->oid_len + len;
	}
	snmp_verbose("%s: returning %i bytes\n", __func__, len + (newbuf - buf));
	return len + (newbuf - buf);
}


/* receive snmp through the UDP mechanism */
static int snmp_poll(void)
{
	struct wr_sockaddr addr;
	uint8_t buf[200];
	int len;

	/* no need to wait for IP address: we won't get queries */
	len = ptpd_netif_recvfrom(snmp_socket, &addr,
				  buf, sizeof(buf), NULL);
	if (len <= UDP_END + sizeof(match_array))
		return 0;

	len = snmp_respond(buf + UDP_END);
	if (len < 0)
		return 0;
	len += UDP_END;

	fill_udp(buf, len, NULL);
	ptpd_netif_sendto(snmp_socket, &addr, buf, len, 0);
	return 1;
}

DEFINE_WRC_TASK(snmp) = {
	.name = "snmp",
	.enable = &link_status,
	.init = snmp_init,
	.job = snmp_poll,
};
