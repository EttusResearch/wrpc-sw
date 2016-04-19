/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2016 GSI (www.gsi.de), CERN (www.cern.ch)
 * Author: Alessandro Rubini <a.rubini@gsi.de>
 * Author: Adam Wujek <adam.wujek@cern.ch>
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
#include "revision.h"

#ifndef htons
#define htons(x) x
#endif

#define ASN_APPLICATION	((u_char)0x40)
#define ASN_INTEGER	((u_char)0x02)
#define ASN_OCTET_STR	((u_char)0x04)
#define ASN_OBJECT_ID	((u_char)0x06)
#define ASN_IPADDRESS	(ASN_APPLICATION | 0)
#define ASN_COUNTER	(ASN_APPLICATION | 1)
#define ASN_GAUGE	(ASN_APPLICATION | 2)
#define ASN_UNSIGNED	(ASN_APPLICATION | 2)   /* RFC 1902 - same as GAUGE */
#define ASN_TIMETICKS	(ASN_APPLICATION | 3)
#define ASN_COUNTER64   (ASN_APPLICATION | 6)
/*
 * (error codes from net-snmp's snmp.h)
 * Error codes (the value of the field error-status in PDUs)
 */
#define SNMP_ERR_NOERROR		(0)
#define SNMP_ERR_TOOBIG			(1)
#define SNMP_ERR_NOSUCHNAME		(2)
#define SNMP_ERR_BADVALUE		(3)
#define SNMP_ERR_READONLY		(4)
#define SNMP_ERR_GENERR			(5)

#define SNMP_GET 0xA0
#define SNMP_GET_NEXT 0xA1
#define SNMP_GET_RESPONSE 0xA2
#define SNMP_SET 0xA3

#define SNMP_V1 0
#define SNMP_V2c 1
#define SNMP_V_MAX 1 /* Maximum supported version */

#ifdef CONFIG_SNMP_SET
#define SNMP_SET_ENABLED 1
#define SNMP_SET_FUNCTION(X) .set = (X)
#else
#define SNMP_SET_ENABLED 0
#define SNMP_SET_FUNCTION(X) .set = NULL
#endif

/* used to define write only OIDs */
#define NO_SET NULL

/* limit string len */
#define MAX_OCTET_STR_LEN 32

#define OID_FIELD_STRUCT(_oid, _getf, _setf, _asn, _type, _pointer, _field) { \
	.oid_match = _oid, \
	.oid_len = sizeof(_oid), \
	.get = _getf, \
	SNMP_SET_FUNCTION(_setf), \
	.asn = _asn, \
	.p = _pointer, \
	.offset = offsetof(_type, _field), \
	.data_size = sizeof(((_type *)0)->_field) \
}

#define OID_FIELD(_oid, _fname, _asn) { \
	.oid_match = _oid, \
	.oid_len = sizeof(_oid), \
	.get = _fname, \
	.asn = _asn, \
}

#define OID_FIELD_VAR(_oid, _getf, _setf, _asn, _pointer) { \
	.oid_match = _oid, \
	.oid_len = sizeof(_oid), \
	.get = _getf, \
	SNMP_SET_FUNCTION(_setf), \
	.asn = _asn, \
	.p = _pointer, \
	.offset = 0, \
	.data_size = sizeof(*_pointer) \
}

struct snmp_oid {
	uint8_t *oid_match;
	int (*get)(uint8_t *buf, struct snmp_oid *obj);
	/* *set is needed only when support for SNMP SET is enabled */
	int (*set)(uint8_t *buf, struct snmp_oid *obj);
	void *p;
	uint8_t oid_len;
	uint8_t asn;
	uint8_t offset; /* increase it if it is not enough */
	uint8_t data_size;
};

extern struct pp_instance ppi_static;
static struct wr_servo_state *wr_s_state;
#define SNMP_HW_TYPE_LEN 32
char snmp_hw_type[SNMP_HW_TYPE_LEN] = CONFIG_SNMP_HW_TYPE;
/* __DATE__ and __TIME__ is already stored in struct spll_stats stats, but
 * redefining it here makes code smaller than concatenate existing one */
static char *snmp_build_date = __DATE__ " " __TIME__;
/* store SNMP version, not fully used yet */
uint8_t snmp_version;


static uint8_t __snmp_queue[256];
static struct wrpc_socket __static_snmp_socket = {
	.queue.buff = __snmp_queue,
	.queue.size = sizeof(__snmp_queue),
};
static struct wrpc_socket *snmp_socket;

static int get_value(uint8_t *buf, uint8_t asn, void *p);
static int get_pp(uint8_t *buf, struct snmp_oid *obj);
static int get_p(uint8_t *buf, struct snmp_oid *obj);
static int get_i32sat(uint8_t *buf, uint8_t asn, void *p);
static int set_value(uint8_t *set_buff, struct snmp_oid *obj, void *p);
static int set_pp(uint8_t *buf, struct snmp_oid *obj);
static int set_p(uint8_t *buf, struct snmp_oid *obj);

static uint8_t oid_wrpcVersionHwType[] =         {0x2B,6,1,4,1,96,101,1,1,1,0};
static uint8_t oid_wrpcVersionSwVersion[] =      {0x2B,6,1,4,1,96,101,1,1,2,0};
static uint8_t oid_wrpcVersionSwBuildBy[] =      {0x2B,6,1,4,1,96,101,1,1,3,0};
static uint8_t oid_wrpcVersionSwBuildDate[] =    {0x2B,6,1,4,1,96,101,1,1,4,0};

/* NOTE: to have SNMP_GET_NEXT working properly this array has to be sorted by
	 OIDs */
static struct snmp_oid oid_array[] = {
	OID_FIELD_VAR(   oid_wrpcVersionHwType,      get_p,        NO_SET,   ASN_OCTET_STR, &snmp_hw_type),
	OID_FIELD_VAR(   oid_wrpcVersionSwVersion,   get_pp,       NO_SET,   ASN_OCTET_STR, &build_revision),
	OID_FIELD_VAR(   oid_wrpcVersionSwBuildBy,   get_pp,       NO_SET,   ASN_OCTET_STR, &build_by),
	OID_FIELD_VAR(   oid_wrpcVersionSwBuildDate, get_pp,       NO_SET,   ASN_OCTET_STR, &snmp_build_date),

	{ 0, }
};


static void snmp_init(void)
{
	/* Use UDP engine activated by function arguments  */
	snmp_socket = ptpd_netif_create_socket(&__static_snmp_socket, NULL,
						PTPD_SOCK_UDP, 161 /* snmp */);
	/* TODO: check if pointer(s) is initialized already */
	wr_s_state =
		&((struct wr_data *)ppi_static.ext_data)->servo_state;
}

/* These get the actual information, returning the length */
static int get_tics(uint8_t *buf, struct snmp_oid *obj)
{
	uint32_t tics = htonl(timer_get_tics() / 10); /* hundredths... bah! */

	memcpy(buf + 2, &tics, sizeof(tics));
	buf[1] = sizeof(tics);
	buf[0] = ASN_TIMETICKS;
	return 2 + sizeof(tics);
}
static int get_date(uint8_t *buf, struct snmp_oid *obj)
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

static int get_value(uint8_t *buf, uint8_t asn, void *p)
{
	uint32_t tmp;
	uint64_t tmp_uint64;
	uint8_t *oid_data = buf + 2;
	uint8_t *len;

	buf[0] = asn;
	len = &buf[1];
	switch (asn) {
	case ASN_COUNTER:
	case ASN_INTEGER:
	case ASN_TIMETICKS:
	    tmp = htonl(*(uint32_t *)p);
	    memcpy(oid_data, &tmp, sizeof(tmp));
	    *len = sizeof(tmp);
	    snmp_verbose("%s: 0x%x\n", __func__, *(uint32_t *)p);
	    break;
	case ASN_COUNTER64:
	    tmp_uint64 = *(uint64_t *)p;
	    memcpy(oid_data, &tmp_uint64, sizeof(tmp_uint64));
	    *len = sizeof(tmp_uint64);
	    /* Our printf has disabled printing of 64bit values */
	    snmp_verbose("%s: 64bit value 0x%08x|%08x\n", __func__,
		     (uint32_t)(tmp_uint64 >> 32), (uint32_t)tmp_uint64);
	    break;
	case ASN_OCTET_STR:
	    *len = strnlen((char *)p, MAX_OCTET_STR_LEN - 1);
	    memcpy(oid_data, p, *len + 1);
	    snmp_verbose("%s: %s len %d\n", __func__, (char *)p, *len);
	    break;
	default:
	    break;
	}
	return 2 + buf[1];
}

static int get_pp(uint8_t *buf, struct snmp_oid *obj)
{
	/* calculate pointer, treat obj-> as void ** */
	return get_value(buf, obj->asn, *(void **)obj->p + obj->offset);
}

static int get_p(uint8_t *buf, struct snmp_oid *obj)
{
	/* calculate pointer, treat obj-> as void * */
	return get_value(buf, obj->asn, obj->p + obj->offset);
}

static int get_i32sat(uint8_t *buf, uint8_t asn, void *p)
{
	int64_t tmp_int64;
	int32_t tmp_int32;

	/* if we want to modify an object we need to do that on a copy,
	 * otherwise pointers to the values will be overwritten */
	tmp_int64 = *(int64_t *)p;
	/* Our printf has disabled printing of 64bit values */
	snmp_verbose("%s: 64bit value 0x%08x|%08x\n", __func__,
		     (int32_t)((tmp_int64) >> 32), (uint32_t)tmp_int64);
	/* saturate int32 */
	if (tmp_int64 >= INT_MAX)
		tmp_int64 = INT_MAX;
	else if (tmp_int64 <= INT_MIN)
		tmp_int64 = INT_MIN;
	/* get_value will print saturated value anyway */
	tmp_int32 = (int32_t) tmp_int64;
	return get_value(buf, asn, &tmp_int32);
}

static int get_i32sat_pp(uint8_t *buf, struct snmp_oid *obj)
{
	/* calculate pointer, treat obj-> as void ** */
	return get_i32sat(buf, obj->asn,
				   *(void **)obj->p + obj->offset);
}

static int set_value(uint8_t *set_buff, struct snmp_oid *obj, void *p)
{
	uint8_t asn_incoming = set_buff[0];
	uint8_t asn_expected = obj->asn;
	uint8_t len = set_buff[1];
	uint8_t *oid_data = set_buff + 2;
	uint32_t tmp_u32;

	if (asn_incoming != asn_expected) { /* wrong asn */
		snmp_verbose("%s: wrong asn 0x%02x, expected 0x%02x\n",
			     __func__, asn_incoming, asn_expected);
		return -1;
	}
	switch (asn_incoming) {
	case ASN_INTEGER:
	    if (len > sizeof(uint32_t))
		return -1;
	    tmp_u32 = 0;
	    memcpy(&tmp_u32, oid_data, len);
	    tmp_u32 = ntohl(tmp_u32);
	    /* move data when shorter than 4 bytes */
	    tmp_u32 = tmp_u32 >> ((4 - len) * 8);
	    *(uint32_t *)p = tmp_u32;
	    snmp_verbose("%s: 0x%08x 0x%08x len %d\n", __func__,
			 *(uint32_t *)p, tmp_u32, len);
	    break;
	case ASN_OCTET_STR:
	    /* Check the string size */
	    if (len > obj->data_size)
		return -1;
	    memcpy(p, oid_data, len);
	    *(char *)(p + len) = '\0';
	    snmp_verbose("%s: %s len %d\n", __func__, (char *)p, len);
	    break;
	default:
	    break;
	}
	return len + 2;
}

static int set_pp(uint8_t *buf, struct snmp_oid *obj)
{
	/* calculate pointer, treat obj-> as void ** */
	return set_value(buf, obj, *(void **)obj->p + obj->offset);
}

static int set_p(uint8_t *buf, struct snmp_oid *obj)
{
	/* calculate pointer, treat obj-> as void * */
	return set_value(buf, obj, obj->p + obj->offset);
}


/*
 * Perverse...  snmpwalk does getnext anyways.
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
 *       02 01 00                            int of 1: error index
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
#define BYTE_ERROR 0xfc
#define BYTE_ERROR_INDEX 0xfb
#define BYTE_VERSION 0xfa
/* indexes of bytes in snmp packet */
#define BYTE_PACKET_SIZE_i 1
#define BYTE_COMMUNITY_LEN_i 6
/* indexes below are without the length of the community string */
#define BYTE_PDU_i 7
#define BYTE_ERROR_i 17
#define BYTE_ERROR_INDEX_i 20

static uint8_t match_array[] = {
	0x30, BYTE_SIZE,
	0x02, 0x01, BYTE_VERSION, /* ASN_INTEGER, size 1 byte, version */
	0x04, 0x06, /* ASN_OCTET_STR, strlen("public") */
		0x70, 0x75, 0x62, 0x6C, 0x69, 0x63, /* "public" */
	BYTE_PDU, BYTE_SIZE, /* SNMP packet type, size of following data */
	0x02, 0x04, BYTE_IGNORE, BYTE_IGNORE, BYTE_IGNORE, BYTE_IGNORE, /*
	 * ASN_INTEGER, size 4 bytes, Request/Message ID */
	0x02, 0x01, BYTE_ERROR, /* ASN_INTEGER, size 1 byte, error type */
	0x02, 0x01, BYTE_ERROR_INDEX, /* ASN_INTEGER, size 1 byte,
				       * error index */
	0x30, BYTE_SIZE,
	0x30, BYTE_SIZE,
	0x06, BYTE_IGNORE, /* ASN_OBJECT_ID, OID length */
	/* oid follows */
};


static void print_oid_verbose(uint8_t *oid, int len)
{
	/*uint8_t * oid_end = oid + len;*/
	int i;

	snmp_verbose(".1.3");
	for (i = 1; i < len; i++)
		snmp_verbose(".%d", *(oid + i));
}

/* Limit community length. Standard says nothing about maximum length, but we
 * want to limit it to save memory */
#define MAX_COMMUNITY_LEN 32

/* prepare packet to return error, leave the rest of packet as it was */
static uint8_t snmp_prepare_error(uint8_t *buf, uint8_t error)
{
	uint8_t ret_size;
	uint8_t community_len;

	community_len = buf[BYTE_COMMUNITY_LEN_i];
	/* If community_len is too long, it will return malformed
	  * packet, but at least something useful for debugging */
	community_len = min(community_len, MAX_COMMUNITY_LEN);
	buf[BYTE_PDU_i + community_len] = SNMP_GET_RESPONSE;
	buf[BYTE_ERROR_i + community_len] = error;
	buf[BYTE_ERROR_INDEX_i + community_len] = 1;
	ret_size = buf[BYTE_PACKET_SIZE_i] + 2;
		snmp_verbose("%s: error returning %i bytes\n", __func__,
			     ret_size);
	return ret_size;
}


/* And, now, work out your generic frame responder... */
static int snmp_respond(uint8_t *buf)
{
	struct snmp_oid *oid = NULL;
	uint8_t *newbuf = NULL;
	uint8_t *new_oid;
	int i;
	int8_t len;
	uint8_t snmp_mode_get = 0;
	uint8_t snmp_mode_get_next = 0;
	uint8_t snmp_mode_set = 0;
	uint8_t incoming_oid_len;
	int8_t cmp_result;

	/* Hack to avoid compiler warnings "function defined but not used" for
	 * set_p and set_pp when SNMP compiled without SET support.
	 * These functions will never be called here. */
	if (0) {
		set_p(newbuf, oid);
		set_pp(newbuf, oid);
	}

	for (i = 0; i < sizeof(match_array); i++) {
		switch (match_array[i]) {
		case BYTE_SIZE:
		case BYTE_IGNORE:
		case BYTE_ERROR:
		case BYTE_ERROR_INDEX:
			continue;
		case BYTE_VERSION:
			snmp_version = buf[i];
			if (snmp_version > SNMP_V_MAX)
				return snmp_prepare_error(buf,
							SNMP_ERR_GENERR);
			continue;
		case BYTE_PDU:
			snmp_mode_get = (SNMP_GET == buf[i]);
			snmp_mode_get_next = (SNMP_GET_NEXT == buf[i]);
			snmp_mode_set = SNMP_SET_ENABLED &&
						(SNMP_SET == buf[i]);
			if (!(snmp_mode_get
			      || snmp_mode_get_next
			      || snmp_mode_set))
				return snmp_prepare_error(buf,
							SNMP_ERR_GENERR);
			break;
		default:
			if (buf[i] != match_array[i])
				return snmp_prepare_error(buf,
							SNMP_ERR_GENERR);
		}
	}
	snmp_verbose("%s: header match ok\n", __func__);

	newbuf = buf + i;
	new_oid = buf + i;
	/* save size of incoming oid */
	incoming_oid_len = buf[i - 1];
	for (oid = oid_array; oid->oid_len; oid++) {
		if (snmp_mode_get &&
		    oid->oid_len != incoming_oid_len) {
			/* for SNMP_GET, we need equal lengths */
			continue;
		}

		cmp_result = memcmp(oid->oid_match, new_oid,
				    min(oid->oid_len, incoming_oid_len));
		if (snmp_mode_get || snmp_mode_set) {
			/* we need exact match for SNMP_GET */
			if (cmp_result == 0) {
				snmp_verbose("%s: exact match for %s\n",
					     __func__,
					     snmp_mode_set ? "SET" : "GET");
				break;
			}
		} else if (snmp_mode_get_next) {
			if (cmp_result > 0) { /* current OID is after the one
					       * that is requested, so use it
					       */
				snmp_verbose("%s: take next match for GET_NEXT"
					     "\n", __func__);
				break;
			}
			if (cmp_result == 0) { /* exact match */
				if (oid->oid_len <= incoming_oid_len) {
					/* when incoming OID is equal or longer
					 * than OID beeing compared, go to next
					 * OID and break */
					oid++;
				}
				snmp_verbose("%s: exact match for GET_NEXT\n",
					     __func__);
				break;
			}
		}
	}
	if (!oid->oid_len) {
		snmp_verbose("%s: OID not found! ", __func__);
		print_oid_verbose(new_oid, incoming_oid_len);
		snmp_verbose("\n");
		/* also for last GET_NEXT element */
		return snmp_prepare_error(buf, SNMP_ERR_NOSUCHNAME);
	}
	if (snmp_mode_get_next) {
		/* copy new OID */
		memcpy(new_oid, oid->oid_match, oid->oid_len);
		/* update OID len, since it might be different */
		*(new_oid - 1) = oid->oid_len;
	}
	snmp_verbose("%s: oid found: ", __func__);
	print_oid_verbose(oid->oid_match, oid->oid_len);
	snmp_verbose(" calling %p\n", oid->get);
	/* Phew.... we matched the OID, so let's call the filler  */
	newbuf += oid->oid_len;
	if (SNMP_SET_ENABLED && snmp_mode_set) {
		uint8_t community_len;
		if (!oid->set) {
			/* Oid found but there is no set function.
			 * It is read-only */
			return snmp_prepare_error(buf,
						  SNMP_ERR_READONLY);
		}
		len = oid->set(newbuf, oid);
		if (len < 0)
			return snmp_prepare_error(buf,
						  SNMP_ERR_BADVALUE);
		/* After set, perform get */
		community_len = buf[BYTE_COMMUNITY_LEN_i];
		/* If community_len is too long, it will return malformed
		 * packet, but at least something useful for debugging */
		community_len = min(community_len, MAX_COMMUNITY_LEN);
		buf[BYTE_PDU_i + community_len] = SNMP_GET;
		/* recursive call of snmp_respond */
		return snmp_respond(buf);
	} else {
		len = oid->get(newbuf, oid);
		newbuf += len;
	}
	/* now fix all size fields and change PDU */
	for (i = 0; i < sizeof(match_array); i++) {
		int remain = newbuf - buf - i - 1;

		if (match_array[i] == BYTE_PDU)
			buf[i] = SNMP_GET_RESPONSE;
		if (match_array[i] != BYTE_SIZE)
			continue;
		snmp_verbose("%s: offset %i 0x%02x is len %i\n", i, i,
			     __func__, remain);
		buf[i] = remain;
	}
	snmp_verbose("%s: returning %i bytes\n", __func__, newbuf - buf);
	return newbuf - buf;
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
