/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2016 GSI (www.gsi.de), CERN (www.cern.ch)
 * Author: Alessandro Rubini <a.rubini@gsi.de>
 * Author: Adam Wujek <adam.wujek@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
/*
 * Naming:
 * Each OID is divided into the limb and twig part.
 * The twig part can be handled as a group or a table
 */
#include <wrc.h>
#include <wrpc.h>
#include <string.h>
#include <minic.h>
#include <limits.h>

#include "endpoint.h"
#include "ipv4.h"
#include "ptpd_netif.h"
#include "pps_gen.h"
#include "hw/memlayout.h"
#include "hw/etherbone-config.h"
#include "revision.h"
#include "softpll_ng.h"
#include "temperature.h"
#include "sfp.h"
#include "syscon.h"

#include "storage.h"

#define ASN_BOOLEAN	((u_char)0x01)
#define ASN_INTEGER	((u_char)0x02)
#define ASN_OCTET_STR	((u_char)0x04)
#define ASN_NULL	((u_char)0x05)
#define ASN_OBJECT_ID	((u_char)0x06)
#define ASN_APPLICATION	((u_char)0x40)
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

#ifdef CONFIG_SNMP_AUX_DIAG
#define SNMP_AUX_DIAG_ENABLED 1
#else
#define SNMP_AUX_DIAG_ENABLED 0
#endif

#define MASK_SET 0x1
#define MASK_GET 0x2
#define MASK_GET_NEXT 0x4
#define RETURN_FIRST 0x8
/* used to define write only OIDs */
#define NO_SET NULL

/* limit string len */
#define MAX_OCTET_STR_LEN 32

/* defines used by get_time function */
#define TIME_STRING 1
#define TIME_NUM 0
#define TYPE_MASK 1
#define UPTIME_MASK 2
#define TAI_MASK 4
#define TAI_STRING (void *) (TAI_MASK | TIME_STRING)
#define TAI_NUM (void *) (TAI_MASK | TIME_NUM)
#define UPTIME_NUM (void *) (UPTIME_MASK | TIME_NUM)

/* defines used by get_servo function */
#define SERVO_UPDATE_TIME (void *) 1
#define SERVO_ASYMMETRY (void *) 2

/* defines used by get_port function */
#define PORT_LINK_STATUS (void *) 1

/* defines for wrpcPtpConfigRestart */
#define restartPtp 1
#define restartPtpSuccessful 100
#define restartPtpFailed 200

/* defines for wrpcPtpConfigApply */
#define writeToFlashGivenSfp 1
#define writeToFlashCurrentSfp 2
#define writeToMemoryCurrentSfp 3
#define eraseFlash 50
#define applySuccessful 100
#define applySuccessfulMatchFailed 101
#define applyFailed 200
#define applyFailedI2CError 201
#define applyFailedDBFull 202
#define applyFailedInvalidPN 203

/* defines for wrpcTemperatureTable */
#define TABLE_ROW 1
#define TABLE_COL 0
#define TABLE_ENTRY 0
#define TABLE_FIRST_ROW 1
#define TABLE_FIRST_COL 2

/* Limit community length. Standard says nothing about maximum length, but we
 * want to limit it to save memory */
#define MAX_COMMUNITY_LEN 32
/* Limit the request-id. Standard says max is 4 */
#define MAX_REQID_LEN 4
/* Limit the OID length */
#define MAX_OID_LEN 40

/* Index of a byte in the OID corresponding to the ID of the aux diag
 * registers */
#define AUX_DIAG_ID_INDEX 8
/* Index of a byte in the OID corresponding to the version of the aux diag
 * registers */
#define AUX_DIAG_VER_INDEX 9

#define AUX_DIAG_RO (void *) DIAG_RO_BANK
#define AUX_DIAG_RW (void *) DIAG_RW_BANK
/* add the AUX_DIAG_DATA_COL_SHIFT to the aux diag register's index to get
 * column in the table. Columns are following:
 * col 1 -- index
 * col 2 -- number of registers
 * col 3 -- first aux diag register
 */
#define AUX_DIAG_DATA_COL_SHIFT 3

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

#define OID_LIMB_FIELD(_oid, _func, _obj_array) { \
	.oid_match = _oid, \
	.oid_len = sizeof(_oid), \
	.twig_func = _func, \
	.obj_array = _obj_array, \
}

struct snmp_oid_limb {
	uint8_t *oid_match;
	int (*twig_func)(uint8_t *buf, uint8_t in_oid_limb_matched_len,
			  struct snmp_oid *obj, uint8_t flags);
	struct snmp_oid *obj_array;
	uint8_t oid_len;
};

static struct s_sfpinfo snmp_ptp_config;
static int ptp_config_apply_status;
static int ptp_restart_status;
/* Keep the number of aux diag registers available in the FPGA bitstream */
static uint32_t aux_diag_reg_ro_num;
static uint32_t aux_diag_reg_rw_num;


extern struct pp_instance ppi_static;
static struct wr_servo_state *wr_s_state;

extern char wrc_hw_name[HW_NAME_LENGTH];
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


static int func_group(uint8_t *buf, uint8_t in_oid_limb_matched_len,
		      struct snmp_oid *obj, uint8_t flags);
static int func_table(uint8_t *buf, uint8_t in_oid_limb_matched_len,
		      struct snmp_oid *obj, uint8_t flags);
static int func_aux_diag(uint8_t *buf, uint8_t in_oid_limb_matched_len,
			 struct snmp_oid *obj, uint8_t flags);

static int get_value(uint8_t *buf, uint8_t asn, void *p);
static int get_pp(uint8_t *buf, struct snmp_oid *obj);
static int get_p(uint8_t *buf, struct snmp_oid *obj);
static int get_i32sat(uint8_t *buf, uint8_t asn, void *p);
static int get_i32sat_pp(uint8_t *buf, struct snmp_oid *obj);
static int get_time(uint8_t *buf, struct snmp_oid *obj);
static int get_servo(uint8_t *buf, struct snmp_oid *obj);
static int get_port(uint8_t *buf, struct snmp_oid *obj);
static int get_temp(uint8_t *buf, struct snmp_oid *obj);
static int get_sfp(uint8_t *buf, struct snmp_oid *obj);
static int get_aux_diag(uint8_t *buf, struct snmp_oid *obj);
static int set_value(uint8_t *set_buff, struct snmp_oid *obj, void *p);
static int set_pp(uint8_t *buf, struct snmp_oid *obj);
static int set_p(uint8_t *buf, struct snmp_oid *obj);
static int set_ptp_restart(uint8_t *buf, struct snmp_oid *obj);
static int set_ptp_config(uint8_t *buf, struct snmp_oid *obj);
static int set_aux_diag(uint8_t *buf, struct snmp_oid *obj);
static int data_aux_diag(uint8_t *buf, struct snmp_oid *obj, int mode);

static void print_oid_verbose(uint8_t *oid, int len);
static void snmp_fix_size(uint8_t *buf, int size);

static uint8_t oid_wrpcVersionGroup[] =     {0x2B,6,1,4,1,96,101,1,1};
static uint8_t oid_wrpcTimeGroup[] =        {0x2B,6,1,4,1,96,101,1,2};
/* Include wrpcTemperatureEntry into OID */
static uint8_t oid_wrpcTemperatureTable[] = {0x2B,6,1,4,1,96,101,1,3,1};
static uint8_t oid_wrpcSpllStatusGroup[] =  {0x2B,6,1,4,1,96,101,1,4};
static uint8_t oid_wrpcPtpGroup[] =         {0x2B,6,1,4,1,96,101,1,5};
static uint8_t oid_wrpcPtpConfigGroup[] =   {0x2B,6,1,4,1,96,101,1,6};
static uint8_t oid_wrpcPortGroup[] =        {0x2B,6,1,4,1,96,101,1,7};
/* Include wrpcSfpEntry into OID */
static uint8_t oid_wrpcSfpTable[] =         {0x2B,6,1,4,1,96,101,1,8,1};
/* In below OIDs zeros will be replaced in the snmp_init function by values
 * read from FPA */
static uint8_t oid_wrpcAuxRoTable[] =       {0x2B,6,1,4,1,96,101,2,0,0,1,1};
static uint8_t oid_wrpcAuxRwTable[] =       {0x2B,6,1,4,1,96,101,2,0,0,2,1};

/* wrpcVersionGroup */
static uint8_t oid_wrpcVersionHwType[] =         {1,0};
static uint8_t oid_wrpcVersionSwVersion[] =      {2,0};
static uint8_t oid_wrpcVersionSwBuildBy[] =      {3,0};
static uint8_t oid_wrpcVersionSwBuildDate[] =    {4,0};

/* wrpcTimeGroup */
static uint8_t oid_wrpcTimeTAI[] =               {1,0};
static uint8_t oid_wrpcTimeTAIString[] =         {2,0};
static uint8_t oid_wrpcTimeSystemUptime[] =      {3,0};

/* wrpcTemperatureTable */
static uint8_t oid_wrpcTemperatureName[] =       {2};
static uint8_t oid_wrpcTemperatureValue[] =      {3};

/* wrpcSpllStatusGroup */
static uint8_t oid_wrpcSpllMode[] =              {1,0};
static uint8_t oid_wrpcSpllIrqCnt[] =            {2,0};
static uint8_t oid_wrpcSpllSeqState[] =          {3,0};
static uint8_t oid_wrpcSpllAlignState[] =        {4,0};
static uint8_t oid_wrpcSpllHlock[] =             {5,0};
static uint8_t oid_wrpcSpllMlock[] =             {6,0};
static uint8_t oid_wrpcSpllHY[] =                {7,0};
static uint8_t oid_wrpcSpllMY[] =                {8,0};
static uint8_t oid_wrpcSpllDelCnt[] =            {9,0};

/* wrpcPtpGroup */
static uint8_t oid_wrpcPtpServoStateN[] =        { 5,0};
static uint8_t oid_wrpcPtpClockOffsetPsHR[] =    { 8,0};
static uint8_t oid_wrpcPtpSkew[] =               { 9,0};
static uint8_t oid_wrpcPtpRTT[] =                {10,0};
static uint8_t oid_wrpcPtpServoUpdates[] =       {12,0};
static uint8_t oid_wrpcPtpServoUpdateTime[] =    {13,0};
static uint8_t oid_wrpcPtpDeltaTxM[] =           {14,0};
static uint8_t oid_wrpcPtpDeltaRxM[] =           {15,0};
static uint8_t oid_wrpcPtpDeltaTxS[] =           {16,0};
static uint8_t oid_wrpcPtpDeltaRxS[] =           {17,0};
static uint8_t oid_wrpcPtpServoStateErrCnt[] =   {18,0};
static uint8_t oid_wrpcPtpClockOffsetErrCnt[] =  {19,0};
static uint8_t oid_wrpcPtpRTTErrCnt[] =          {20,0};
static uint8_t oid_wrpcPtpAsymmetry[] =          {22,0};
static uint8_t oid_wrpcPtpTX[] =                 {23,0};
static uint8_t oid_wrpcPtpRX[] =                 {24,0};
static uint8_t oid_wrpcPtpAlpha[] =              {26,0};

/* wrpcPtpConfigGroup */
static uint8_t oid_wrpcPtpConfigRestart[] =      {1,0};
static uint8_t oid_wrpcPtpConfigApply[] =        {2,0};
static uint8_t oid_wrpcPtpConfigSfpPn[] =        {3,0};
static uint8_t oid_wrpcPtpConfigDeltaTx[] =      {4,0};
static uint8_t oid_wrpcPtpConfigDeltaRx[] =      {5,0};
static uint8_t oid_wrpcPtpConfigAlpha[] =        {6,0};

/* wrpcPortGroup */
static uint8_t oid_wrpcPortLinkStatus[] =        {1,0};
static uint8_t oid_wrpcPortSfpPn[] =             {2,0};
static uint8_t oid_wrpcPortSfpInDB[] =           {3,0};
static uint8_t oid_wrpcPortInternalTX[] =        {4,0};
static uint8_t oid_wrpcPortInternalRX[] =        {5,0};

/* oid_wrpcSfpTable */
static uint8_t oid_wrpcSfpPn[] =                 {2};
static uint8_t oid_wrpcSfpDeltaTx[] =            {3};
static uint8_t oid_wrpcSfpDeltaRx[] =            {4};
static uint8_t oid_wrpcSfpAlpha[] =              {5};

/* NOTE: to have SNMP_GET_NEXT working properly this array has to be sorted by
	 OIDs */
/* wrpcVersionGroup */
static struct snmp_oid oid_array_wrpcVersionGroup[] = {
	OID_FIELD_VAR(   oid_wrpcVersionHwType,      get_p,        NO_SET,   ASN_OCTET_STR, &wrc_hw_name),
	OID_FIELD_VAR(   oid_wrpcVersionSwVersion,   get_pp,       NO_SET,   ASN_OCTET_STR, &build_revision),
	OID_FIELD_VAR(   oid_wrpcVersionSwBuildBy,   get_pp,       NO_SET,   ASN_OCTET_STR, &build_by),
	OID_FIELD_VAR(   oid_wrpcVersionSwBuildDate, get_pp,       NO_SET,   ASN_OCTET_STR, &snmp_build_date),
	{ 0, }
};

/* wrpcTimeGroup */
static struct snmp_oid oid_array_wrpcTimeGroup[] = {
	OID_FIELD_VAR(   oid_wrpcTimeTAI,            get_time,     NO_SET,   ASN_COUNTER64, TAI_NUM),
	OID_FIELD_VAR(   oid_wrpcTimeTAIString,      get_time,     NO_SET,   ASN_OCTET_STR, TAI_STRING),
	OID_FIELD_VAR(   oid_wrpcTimeSystemUptime,   get_time,     NO_SET,   ASN_TIMETICKS, UPTIME_NUM),
	{ 0, }
};

/* wrpcTemperatureTable */
static struct snmp_oid oid_array_wrpcTemperatureTable[] = {
	OID_FIELD_VAR(   oid_wrpcTemperatureName,    get_temp,     NO_SET,   ASN_OCTET_STR, NULL),
	OID_FIELD_VAR(   oid_wrpcTemperatureValue,   get_temp,     NO_SET,   ASN_OCTET_STR, NULL),
	{ 0, }
};

/* wrpcSpllStatusGroup */
static struct snmp_oid oid_array_wrpcSpllStatusGroup[] = {
	OID_FIELD_VAR(   oid_wrpcSpllMode,           get_p,        NO_SET,   ASN_INTEGER,   &stats.mode),
	OID_FIELD_VAR(   oid_wrpcSpllIrqCnt,         get_p,        NO_SET,   ASN_COUNTER,   &stats.irq_cnt),
	OID_FIELD_VAR(   oid_wrpcSpllSeqState,       get_p,        NO_SET,   ASN_INTEGER,   &stats.seq_state),
	OID_FIELD_VAR(   oid_wrpcSpllAlignState,     get_p,        NO_SET,   ASN_INTEGER,   &stats.align_state),
	OID_FIELD_VAR(   oid_wrpcSpllHlock,          get_p,        NO_SET,   ASN_COUNTER,   &stats.H_lock),
	OID_FIELD_VAR(   oid_wrpcSpllMlock,          get_p,        NO_SET,   ASN_COUNTER,   &stats.M_lock),
	OID_FIELD_VAR(   oid_wrpcSpllHY,             get_p,        NO_SET,   ASN_INTEGER,   &stats.H_y),
	OID_FIELD_VAR(   oid_wrpcSpllMY,             get_p,        NO_SET,   ASN_INTEGER,   &stats.M_y),
	OID_FIELD_VAR(   oid_wrpcSpllDelCnt,         get_p,        NO_SET,   ASN_COUNTER,   &stats.del_cnt),
	{ 0, }
};

/* wrpcPtpGroup */
static struct snmp_oid oid_array_wrpcPtpGroup[] = {
	OID_FIELD_STRUCT(oid_wrpcPtpServoStateN,     get_pp,       NO_SET,   ASN_INTEGER,   struct wr_servo_state, &wr_s_state, state),
	OID_FIELD_STRUCT(oid_wrpcPtpClockOffsetPsHR, get_i32sat_pp,NO_SET,   ASN_INTEGER,   struct wr_servo_state, &wr_s_state, offset),
	OID_FIELD_STRUCT(oid_wrpcPtpSkew,            get_i32sat_pp,NO_SET,   ASN_INTEGER,   struct wr_servo_state, &wr_s_state, skew),
	OID_FIELD_STRUCT(oid_wrpcPtpRTT,             get_pp,       NO_SET,   ASN_COUNTER64, struct wr_servo_state, &wr_s_state, picos_mu),
	OID_FIELD_STRUCT(oid_wrpcPtpServoUpdates,    get_pp,       NO_SET,   ASN_COUNTER,   struct wr_servo_state, &wr_s_state, update_count),
	OID_FIELD_VAR(   oid_wrpcPtpServoUpdateTime, get_servo,    NO_SET,   ASN_COUNTER64, SERVO_UPDATE_TIME),
	OID_FIELD_STRUCT(oid_wrpcPtpDeltaTxM,        get_pp,       NO_SET,   ASN_INTEGER,   struct wr_servo_state, &wr_s_state, delta_tx_m),
	OID_FIELD_STRUCT(oid_wrpcPtpDeltaRxM,        get_pp,       NO_SET,   ASN_INTEGER,   struct wr_servo_state, &wr_s_state, delta_rx_m),
	OID_FIELD_STRUCT(oid_wrpcPtpDeltaTxS,        get_pp,       NO_SET,   ASN_INTEGER,   struct wr_servo_state, &wr_s_state, delta_tx_s),
	OID_FIELD_STRUCT(oid_wrpcPtpDeltaRxS,        get_pp,       NO_SET,   ASN_INTEGER,   struct wr_servo_state, &wr_s_state, delta_rx_s),
	OID_FIELD_STRUCT(oid_wrpcPtpServoStateErrCnt,get_pp,       NO_SET,   ASN_COUNTER,   struct wr_servo_state, &wr_s_state, n_err_state),
	OID_FIELD_STRUCT(oid_wrpcPtpClockOffsetErrCnt,get_pp,      NO_SET,   ASN_COUNTER,   struct wr_servo_state, &wr_s_state, n_err_offset),
	OID_FIELD_STRUCT(oid_wrpcPtpRTTErrCnt,       get_pp,       NO_SET,   ASN_COUNTER,   struct wr_servo_state, &wr_s_state, n_err_delta_rtt),
	OID_FIELD_VAR(   oid_wrpcPtpAsymmetry,       get_servo,    NO_SET,   ASN_COUNTER64, SERVO_ASYMMETRY),
	OID_FIELD_VAR(   oid_wrpcPtpTX,              get_p,        NO_SET,   ASN_COUNTER,   &ppi_static.ptp_tx_count),
	OID_FIELD_VAR(   oid_wrpcPtpRX,              get_p,        NO_SET,   ASN_COUNTER,   &ppi_static.ptp_rx_count),
	OID_FIELD_STRUCT(oid_wrpcPtpAlpha,           get_pp,       NO_SET,   ASN_INTEGER,   struct wr_servo_state, &wr_s_state, fiber_fix_alpha),
	{ 0, }
};

/* wrpcPtpConfigGroup */
static struct snmp_oid oid_array_wrpcPtpConfigGroup[] = {
	OID_FIELD_VAR(   oid_wrpcPtpConfigRestart,   get_p,        set_ptp_restart,ASN_INTEGER, &ptp_restart_status),
	OID_FIELD_VAR(   oid_wrpcPtpConfigApply,     get_p,        set_ptp_config, ASN_INTEGER, &ptp_config_apply_status),
	OID_FIELD_VAR(   oid_wrpcPtpConfigSfpPn,     get_p,        set_p,    ASN_OCTET_STR, &snmp_ptp_config.pn),
	OID_FIELD_VAR(   oid_wrpcPtpConfigDeltaTx,   get_p,        set_p,    ASN_INTEGER,   &snmp_ptp_config.dTx),
	OID_FIELD_VAR(   oid_wrpcPtpConfigDeltaRx,   get_p,        set_p,    ASN_INTEGER,   &snmp_ptp_config.dRx),
	OID_FIELD_VAR(   oid_wrpcPtpConfigAlpha,     get_p,        set_p,    ASN_INTEGER,   &snmp_ptp_config.alpha),
	{ 0, }
};

/* wrpcPortGroup */
static struct snmp_oid oid_array_wrpcPortGroup[] = {
	OID_FIELD_VAR(   oid_wrpcPortLinkStatus,     get_port,     NO_SET,   ASN_INTEGER,   PORT_LINK_STATUS),
	OID_FIELD_VAR(   oid_wrpcPortSfpPn,          get_p,        NO_SET,   ASN_OCTET_STR, &sfp_pn),
	OID_FIELD_VAR(   oid_wrpcPortSfpInDB,        get_p,        NO_SET,   ASN_INTEGER,   &sfp_in_db),
	OID_FIELD_VAR(   oid_wrpcPortInternalTX,     get_p,        NO_SET,   ASN_COUNTER,   &minic.tx_count),
	OID_FIELD_VAR(   oid_wrpcPortInternalRX,     get_p,        NO_SET,   ASN_COUNTER,   &minic.rx_count),

	{ 0, }
};

/* wrpcSfpTable */
static struct snmp_oid oid_array_wrpcSfpTable[] = {
	OID_FIELD_VAR(   oid_wrpcSfpPn,        get_sfp,        NULL,    ASN_OCTET_STR, NULL),
	OID_FIELD_VAR(   oid_wrpcSfpDeltaTx,   get_sfp,        NULL,    ASN_INTEGER,   NULL),
	OID_FIELD_VAR(   oid_wrpcSfpDeltaRx,   get_sfp,        NULL,    ASN_INTEGER,   NULL),
	OID_FIELD_VAR(   oid_wrpcSfpAlpha,     get_sfp,        NULL,    ASN_INTEGER,   NULL),
	{ 0, }
};

static struct snmp_oid oid_array_wrpcAuxRoTable[] = {
	OID_FIELD_VAR(NULL, get_aux_diag, NO_SET, ASN_UNSIGNED, AUX_DIAG_RO),
	{ 0, }
};

static struct snmp_oid oid_array_wrpcAuxRwTable[] = {
	OID_FIELD_VAR(NULL, get_aux_diag, set_aux_diag, ASN_UNSIGNED, AUX_DIAG_RW),
	{ 0, }
};


/* Array of groups and tables */
static struct snmp_oid_limb oid_limb_array[] = {
	OID_LIMB_FIELD(oid_wrpcVersionGroup,     func_group, oid_array_wrpcVersionGroup),
	OID_LIMB_FIELD(oid_wrpcTimeGroup,        func_group, oid_array_wrpcTimeGroup),
	OID_LIMB_FIELD(oid_wrpcTemperatureTable, func_table, oid_array_wrpcTemperatureTable),
	OID_LIMB_FIELD(oid_wrpcSpllStatusGroup,  func_group, oid_array_wrpcSpllStatusGroup),
	OID_LIMB_FIELD(oid_wrpcPtpGroup,         func_group, oid_array_wrpcPtpGroup),
	OID_LIMB_FIELD(oid_wrpcPtpConfigGroup,   func_group, oid_array_wrpcPtpConfigGroup),
	OID_LIMB_FIELD(oid_wrpcPortGroup,        func_group, oid_array_wrpcPortGroup),
	OID_LIMB_FIELD(oid_wrpcSfpTable,         func_table, oid_array_wrpcSfpTable),
#ifdef CONFIG_SNMP_AUX_DIAG
	OID_LIMB_FIELD(oid_wrpcAuxRoTable,       func_aux_diag, oid_array_wrpcAuxRoTable),
	OID_LIMB_FIELD(oid_wrpcAuxRwTable,       func_aux_diag, oid_array_wrpcAuxRwTable),
#endif
	{ 0, }
};

static void snmp_init(void)
{
	uint32_t aux_diag_id;
	uint32_t aux_diag_ver;
	/* Use UDP engine activated by function arguments  */
	snmp_socket = ptpd_netif_create_socket(&__static_snmp_socket, NULL,
						PTPD_SOCK_UDP, 161 /* snmp */);
	/* TODO: check if pointer(s) is initialized already */
	wr_s_state =
		&((struct wr_data *)ppi_static.ext_data)->servo_state;
	if (SNMP_AUX_DIAG_ENABLED) {
		/* Fix ID and version of aux diag registers by values read from FPGA */
		diag_read_info(&aux_diag_id, &aux_diag_ver, &aux_diag_reg_rw_num,
			      &aux_diag_reg_ro_num);
		snmp_verbose("aux_diag_id %d, aux_diag_ver %d\n",
			    aux_diag_id, aux_diag_ver);
		oid_wrpcAuxRoTable[AUX_DIAG_ID_INDEX] = aux_diag_id;
		oid_wrpcAuxRoTable[AUX_DIAG_VER_INDEX] = aux_diag_ver;
		oid_wrpcAuxRwTable[AUX_DIAG_ID_INDEX] = aux_diag_id;
		oid_wrpcAuxRwTable[AUX_DIAG_VER_INDEX] = aux_diag_ver;
	}
}

static int func_group(uint8_t *buf, uint8_t in_oid_limb_matched_len,
		      struct snmp_oid *twigs_array, uint8_t flags)
{
	int oid_twig_len = buf[0] - in_oid_limb_matched_len;
	uint8_t *in_oid = &buf[1];
	uint8_t *in_oid_limb_end = &buf[1 + in_oid_limb_matched_len];
	int8_t cmp_result = 0;
	uint8_t oid_twig_matching_len;
	int return_len;
	struct snmp_oid *oid;
	uint8_t return_first = 0;

	if (flags & RETURN_FIRST)
		return_first = 1;
	for (oid = twigs_array; oid->oid_len; oid++) {
		snmp_verbose("%s: checking twig: ", __func__);
		print_oid_verbose(oid->oid_match, oid->oid_len);
		snmp_verbose("\n");

		if (return_first) {
			/* For the "return first" case, usually the length of
			 * the requested oid is smaller than
			 * in_oid_limb_matched_len, so the special case is
			 * needed */
			oid_twig_matching_len = oid->oid_len;
		} else {
			/* Decide what is shorter the twig part of the OID, or
			 * the matching part */
			oid_twig_matching_len = min(oid_twig_len,
						    oid->oid_len);
		}

		if ((flags & MASK_GET || flags & MASK_SET)
		    && (oid_twig_len != oid->oid_len)) {
			/* For GET and SET lengths have to be equal, we can
			 * skip to the next OID */
			continue;
		}

		if (return_first == 0) {
			/* Compare the OID's twig part. For the first match we
			 * want to use this OID anyway */
			cmp_result = memcmp(oid->oid_match, in_oid_limb_end,
					    oid_twig_matching_len);
			if (cmp_result == 0
			    && (flags & MASK_GET_NEXT)
			    && oid_twig_matching_len == oid->oid_len) {
				/* Exact match for the GET_NEXT, go to the
				 * next OID */
				continue;
			}
		}

		if ((cmp_result == 0) /* exact match */
		    || ((flags & MASK_GET_NEXT) && cmp_result > 0) /* Found next */
		    || (return_first)) {
			if (flags & MASK_GET_NEXT) {
				snmp_verbose("%s: OID match for GET_NEXT\n",
					     __func__);
				/* Copy new OID */
				snmp_verbose("%s: return OID ", __func__);
				print_oid_verbose(oid->oid_match, oid->oid_len);
				snmp_verbose("\n");
				memcpy(in_oid_limb_end, oid->oid_match,
				       oid->oid_len);
				/* Update OID len, since it might be different,
				 * this is only the length of the twig part
				 * add the rest after return */
				buf[0] = oid->oid_len;
			}
			snmp_verbose("%s: using OID: ", __func__);
			print_oid_verbose(in_oid, in_oid_limb_matched_len);
			snmp_verbose(" ");
			print_oid_verbose(in_oid_limb_end, oid->oid_len);
			snmp_verbose(" calling %p\n", oid->get);
			/* Handle the SET request */
			if (flags & MASK_SET) {
				snmp_verbose("%s SET\n", __func__);
				if (!oid->set) {
					snmp_verbose("%s: no SET function!\n",
						     __func__);
					return -SNMP_ERR_READONLY;
				}
				/* Finally call the SET function */
				return_len = oid->set(in_oid_limb_end
						      + oid->oid_len, oid);
				if (return_len < 0) /* if error */ {
					snmp_verbose("%s: SET error %d\n",
						     __func__, return_len);
					return return_len;
				}
			}
			/* Finally call the GET function */
			snmp_verbose("%s GET\n", __func__);
			return_len = oid->get(in_oid_limb_end + oid->oid_len,
					      oid);
			/* add the length of twig part of the OID */
			if (!return_len)
				continue;
			return_len += oid->oid_len;
			return return_len;
		}
		if (cmp_result > 0) {
			/* we went too far, no hope to find anything more */
			return 0;
		}

	}
	return 0;
}

static int func_table(uint8_t *buf, uint8_t in_oid_limb_matched_len,
		      struct snmp_oid *twigs_array, uint8_t flags)
{
	int oid_twig_len = buf[0] - in_oid_limb_matched_len;
	uint8_t *in_oid_limb_end = &buf[1 + in_oid_limb_matched_len];
	int8_t cmp_result = 0;
	uint8_t oid_twig_matching_len;
	struct snmp_oid *oid;
	struct snmp_oid leaf_obj;
	int return_first = 0;
	int i;
	int get_next_increase_oid = 0;
	int snmp_get_next = 0;
	int return_len;


	if (flags & MASK_SET) {
		/* We don't support SETs yet */
		return -SNMP_ERR_READONLY;
	}

	if (flags & RETURN_FIRST)
		return_first = 1;

	snmp_verbose("%s: table wants coordinates:", __func__);
	for (i = 0; i < oid_twig_len; i++)
		snmp_verbose(" %d", in_oid_limb_end[i]);
	snmp_verbose("\n");

	if (flags & MASK_GET_NEXT) {
		/* By default increase OID for SNMP_GET_NEXT */
		get_next_increase_oid = 1;
		snmp_get_next = 1;
	}

	for (oid = twigs_array; oid->oid_len; oid++) {
		snmp_verbose("%s: checking twig: ", __func__);
		print_oid_verbose(oid->oid_match, oid->oid_len);
		snmp_verbose("\n");

		/* Decide what is shorter the rest of the oid, or the
		 * matching part */
		oid_twig_matching_len = min(oid_twig_len, oid->oid_len);

		cmp_result = memcmp(oid->oid_match, in_oid_limb_end,
					    oid_twig_matching_len);
		if ((return_first)
		    || (snmp_get_next && cmp_result == 0 && (oid_twig_len < oid->oid_len + 1))
		    || (snmp_get_next && cmp_result > 0)) {
			/* For the "return first" case, usually the length of
			 * the requested oid is smaller than
			 * in_oid_limb_matched_len, so the special case is
			 * needed */

			/* Use the matched twig part of the OID */
			memcpy(in_oid_limb_end, oid->oid_match,
				       oid->oid_len);
			/* use the first row */
			*(in_oid_limb_end + oid->oid_len) = TABLE_FIRST_ROW;
			/* Don't increase OID for SNMP_GET_NEXT */
			get_next_increase_oid = 0;
			break;
		}

		if ((cmp_result == 0) /* We want a match */
		    && (
			/* And the exact OID len */
			(oid_twig_len == oid->oid_len + 1)
			   /* oid_twig_len can be longer for snmp_get_next */
			|| (snmp_get_next && (oid_twig_len > oid->oid_len + 1))
			)
		   ) {
			break;
		}
	}
	if (!oid->oid_len) {
		/* Return 0 if the end of twigs_array was reached */
		return 0;
	}

	snmp_verbose("%s: OID matched for table: ", __func__);
	print_oid_verbose(oid->oid_match, oid->oid_len + 1);
	snmp_verbose("%s\n", snmp_get_next ? "GET_NEXT" : "");

	/* Copy OID information to the leaf_obj */
	leaf_obj.oid_len = oid->oid_len;
	leaf_obj.oid_match = in_oid_limb_end;
	leaf_obj.asn = oid->asn;
	leaf_obj.p = oid->p;

	if (get_next_increase_oid)
		in_oid_limb_end[TABLE_ROW]++;

	/* Get the value for the leaf */
	return_len = oid->get(&in_oid_limb_end[oid->oid_len + 1], &leaf_obj);

	if (return_len == 0 && get_next_increase_oid) {
		oid++;
		/* Check if OID is valid */
		if (!oid->oid_len)
			return 0;

		/* Copy the new OID */
		memcpy(in_oid_limb_end, oid->oid_match,
				       oid->oid_len);
		/* Copy the new row number */
		*(in_oid_limb_end + oid->oid_len) = TABLE_FIRST_ROW;

		/* Update leaf_obj with changed values */
		leaf_obj.oid_len = oid->oid_len;
		leaf_obj.asn = oid->asn;
		leaf_obj.p = oid->p;

		/* Get the value for the leaf in the next column, first row */
		return_len = oid->get(&in_oid_limb_end[oid->oid_len + 1],
				      &leaf_obj);

		/* If no value is available for first row in next column,
		 * it means previous column was the last one */
		if (return_len == 0)
			return 0;
	}
	/* Leaf not found */
	if (return_len == 0)
		return 0;

	if (return_len < 0) /* If error */ {
		snmp_verbose("%s: GET error %d\n",
			      __func__, return_len);
		return return_len;
	}

	return_len += oid->oid_len + 1;
	if (snmp_get_next || return_first) {
		buf[0] = oid->oid_len + 1;
	}

	return return_len;
}

static int func_aux_diag(uint8_t *buf, uint8_t in_oid_limb_matched_len,
			 struct snmp_oid *twigs_array, uint8_t flags)
{
	int oid_twig_len = buf[0] - in_oid_limb_matched_len;
	uint8_t *in_oid_limb_end = &buf[1 + in_oid_limb_matched_len];
	uint8_t oid_twig_matching_len;
	struct snmp_oid *oid;
	struct snmp_oid leaf_obj;
	int return_first = 0;
	int i;
	int get_next_increase_oid = 0;
	int snmp_get_next = 0;
	int return_len;
	int table_size = 2; /* two */

	oid = twigs_array;
	if (flags & RETURN_FIRST)
		return_first = 1;

	snmp_verbose("%s: table wants coordinates:", __func__);
	for (i = 0; i < oid_twig_len; i++)
		snmp_verbose(" %d", in_oid_limb_end[i]);
	snmp_verbose("\n");

	if (flags & MASK_GET_NEXT) {
		/* By default increase OID for SNMP_GET_NEXT */
		get_next_increase_oid = 1;
		snmp_get_next = 1;
	}

	/* Fill packet with valid OID, when OID is shorter than it
	 * should be */
	if (return_first || (snmp_get_next && oid_twig_len < table_size)) {
		get_next_increase_oid = 0;
		in_oid_limb_end[TABLE_COL] = TABLE_FIRST_COL;
		in_oid_limb_end[TABLE_ROW] = TABLE_FIRST_ROW;
		oid_twig_len = table_size;
	}
	/* Decide what is shorter the rest of the OID, or the
	 * matching part */
	oid_twig_matching_len = min(oid_twig_len, table_size);

	/* For get and set twig size has to be exact */
	if (!snmp_get_next && (oid_twig_len != table_size)) {
		snmp_verbose("%s: wrong twig len %d (expected %d)",
			     __func__, oid_twig_len, table_size);
		return 0;
	}

	/* For get next requested twig part of oid is longer than table size */
	if (snmp_get_next && (oid_twig_len > table_size)) {
		get_next_increase_oid = 1;
	}

	/* Copy OID information to the leaf_obj */
	leaf_obj.oid_len = table_size;
	leaf_obj.oid_match = in_oid_limb_end;
	leaf_obj.asn = oid->asn;
	leaf_obj.p = oid->p;

	if (get_next_increase_oid)
		in_oid_limb_end[TABLE_ROW]++;

	if (flags & MASK_SET) {
		if (!oid->set) {
			/* No set function */
			snmp_verbose("%s: no set function!", __func__);
			return -SNMP_ERR_READONLY;
		}
		/* Set the value for the leaf */
		return_len = oid->set(&in_oid_limb_end[table_size], &leaf_obj);
		if (return_len < 0) {
			snmp_verbose("%s: SET error %d\n", __func__,
				     return_len);
			return return_len;
			}
	} else {
		/* Get the value for the leaf */
		return_len = oid->get(&in_oid_limb_end[table_size], &leaf_obj);
	}

	if (return_len == 0 && get_next_increase_oid) {
		in_oid_limb_end[TABLE_COL]++;
		in_oid_limb_end[TABLE_ROW] = TABLE_FIRST_ROW;

		/* Get the value for the leaf in the next column, first row */
		return_len = oid->get(&in_oid_limb_end[table_size],
				      &leaf_obj);

		/* If no value is available for first row in next column,
		 * it means previous column was the last one */
		if (return_len == 0)
			return 0;
	}
	/* Leaf not found */
	if (return_len == 0)
		return 0;

	if (return_len < 0) /* If error */ {
		snmp_verbose("%s: GET error %d\n",
			      __func__, return_len);
		return return_len;
	}

	return_len += table_size;
	if (snmp_get_next || return_first) {
		buf[0] = table_size;
	}

	return return_len;
}

static int get_servo(uint8_t *buf, struct snmp_oid *obj)
{
	uint64_t tmp_uint64;

	switch ((int) obj->p) {
	case (int)SERVO_ASYMMETRY:
		tmp_uint64 = wr_s_state->picos_mu - 2LL * wr_s_state->delta_ms;
		return get_value(buf, obj->asn, &tmp_uint64);
	case (int)SERVO_UPDATE_TIME:
		tmp_uint64 = ((uint64_t) wr_s_state->update_time.secs) *
					1000000000LL
				+ (wr_s_state->update_time.scaled_nsecs >> 16);
		return get_value(buf, obj->asn, &tmp_uint64);
	default:
		break;
	}
	return -1;
}

static int get_port(uint8_t *buf, struct snmp_oid *obj)
{
	int32_t tmp_int32;

	switch ((int) obj->p) {
	case (int)PORT_LINK_STATUS:
		/* overkill, since we need the link to be up to use SNMP */
		tmp_int32 = 1 + ep_link_up(NULL);
		return get_value(buf, obj->asn, &tmp_int32);
	default:
		break;
	}
	return -1;
}

static int get_time(uint8_t *buf, struct snmp_oid *obj)
{
	uint64_t secs;
	uint32_t tics;
	char *datestr;
	int req = (int)obj->p;

	if (req & UPTIME_MASK) {
		/* It will overflow after ~49 days. The counter in FPGA is
		 * implemented as 32bit. */
		tics = timer_get_tics() / 10;
		return get_value(buf, obj->asn, &tics);
	} else if (req & TAI_MASK) {
		shw_pps_gen_get_time(&secs, NULL);
	}

	if ((req & TYPE_MASK) == TIME_STRING) {
		datestr = format_time(secs, TIME_FORMAT_SORTED);
		return get_value(buf, obj->asn, datestr);
	} else /* TAI_NUM */
		return get_value(buf, obj->asn, &secs);
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
	case ASN_UNSIGNED:
	case ASN_INTEGER:
	case ASN_TIMETICKS:
	    tmp = htonl(*(uint32_t *)p);
	    memcpy(oid_data, &tmp, sizeof(tmp));
	    *len = sizeof(tmp);
	    snmp_verbose("%s: 0x%x\n", __func__, *(uint32_t *)p);
	    break;
	case ASN_COUNTER64:
	    if (snmp_version == SNMP_V1) {
		/* There is no support for 64bit counters in SNMPv1 */
		return 0;
	    }
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
	    return 0;
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

static int get_temp(uint8_t *buf, struct snmp_oid *obj)
{
	struct wrc_onetemp *p;
	int l = 0, i = TABLE_FIRST_ROW;
	int32_t t;
	int row;
	int col;
	char buffer[20];

	row = obj->oid_match[TABLE_ROW];
	col = obj->oid_match[TABLE_COL];
	snmp_verbose("%s: row%d, col%d\n", __func__, row, col);
	for (p = wrc_temp_getnext(NULL); p; p = wrc_temp_getnext(p), i++) {
		if (row == i) {
			t = p->t;
			if (col == 2) {
				sprintf(buffer, "%s", p->name);
				break;
			}
			if (col == 3) {
				if (t == TEMP_INVALID) {
					l += sprintf(buffer, "INVALID");
					break;
				}
				if (t < 0) {
					t = -(signed)t;
					l += sprintf(buffer, "-");
				}
				sprintf(buffer, "%d.%04d", t >> 16,
					    ((t & 0xffff) * 10 * 1000 >> 16));
				break;
			}
		}
	}

	if (p) {
		/* data found return it */
		return get_value(buf, obj->asn, buffer);
	}

	return 0;
}


static int get_sfp(uint8_t *buf, struct snmp_oid *obj)
{
	int i;
	struct s_sfpinfo sfp;
	int row;
	int col;
	void *p = NULL;
	int sfpcount = 1;
	int temp;
	char sfp_pn[SFP_PN_LEN + 1];

	row = obj->oid_match[TABLE_ROW];
	col = obj->oid_match[TABLE_COL];
	snmp_verbose("%s: row%d, col%d\n", __func__, row, col);
	for (i = 1; i < sfpcount+1; ++i) {
		sfpcount = storage_get_sfp(&sfp, SFP_GET, i - 1);
		if (sfpcount == 0) {
			snmp_verbose("SFP database empty...\n");
			return 0;
		} else if (sfpcount == -1) {
			snmp_verbose("SFP database corrupted...\n");
			return 0;
		}

		snmp_verbose("%d: PN:", i);
		for (temp = 0; temp < SFP_PN_LEN; ++temp)
			snmp_verbose("%c", sfp.pn[temp]);
		snmp_verbose(" dTx: %8d dRx: %8d alpha: %8d\n", sfp.dTx,
			sfp.dRx, sfp.alpha);
		if (row == i) {
			if (col == 2) {
				/* Use local buffer for sfp PN, since stored
				 * version is without null character at
				 * the end */
				memcpy(sfp_pn, sfp.pn, SFP_PN_LEN);
				sfp_pn[SFP_PN_LEN] = '\0';
				p = sfp_pn;
				break;
			}
			if (col == 3) {
				p = &sfp.dTx;
				break;
			}
			if (col == 4) {
				p = &sfp.dRx;
				break;
			}
			if (col == 5) {
				p = &sfp.alpha;
				break;
			}
		}
	}

	if (p) {
		/* Data found, return it */
		return get_value(buf, obj->asn, p);
	}

	return 0;
}

static int set_aux_diag(uint8_t *buf, struct snmp_oid *obj)
{
	return data_aux_diag(buf, obj, SNMP_SET);
}

static int get_aux_diag(uint8_t *buf, struct snmp_oid *obj)
{
	return data_aux_diag(buf, obj, SNMP_GET);
}

static int data_aux_diag(uint8_t *buf, struct snmp_oid *obj, int mode)
{
	int row;
	int col;
	int bank; /* RO or RW */
	uint32_t val;
	int regs_available;
	int ret;

	bank = (int) obj->p;
	snmp_verbose("%s: bank %d\n", __func__, obj->p);
	if ((void *)bank == AUX_DIAG_RO) {
		regs_available = aux_diag_reg_ro_num;
		snmp_verbose("%s: RO bank\n", __func__);
	} else if ((void *)bank == AUX_DIAG_RW) {
		regs_available = aux_diag_reg_rw_num;
		snmp_verbose("%s: RW bank\n", __func__);
	} else {
		snmp_verbose("%s: wrong AUX bank %d\n", __func__, bank);
		return -SNMP_ERR_BADVALUE;
	}
	row = obj->oid_match[1];
	col = obj->oid_match[0];
	snmp_verbose("%s: row%d, col%d\n", __func__, row, col);
	if (row != 1) {
		/* So far we support only one row in this table. */
		snmp_verbose("%s: wrong row %d, should be 1\n", __func__, row);
		return 0;
	}

	/* Ignore cols < 2 */
	if ((col < 2) || (col >= regs_available + AUX_DIAG_DATA_COL_SHIFT)) {
		snmp_verbose("%s: wrong col %d, max %d\n", __func__,
			     col, regs_available + AUX_DIAG_DATA_COL_SHIFT);
		return 0;
	}
	if (col == 2) {
		/* Col 2 is a register containing the number of registers
		 * available in this bank */
		if (mode == SNMP_SET) {
			/* Register with the number of registers is read
			 * only */
			return -SNMP_ERR_READONLY;
		}
		val = regs_available;
	} else {
		if (mode == SNMP_SET) {
			/* write */
			ret = set_value(buf, obj, &val);
			if (ret <= 0) {
				/* Error while checking SET value,
				 * like wrong ASN */
				return ret;
			}
			ret = diag_write_word(col - AUX_DIAG_DATA_COL_SHIFT,
					      val);
			if (ret < 0) {
				/* Something went wrong during the write */
				return -SNMP_ERR_GENERR;
			}
		}
		/* Read a word, even if we just set it */
		ret = diag_read_word(col - AUX_DIAG_DATA_COL_SHIFT, bank, &val);
		if (ret < 0) {
			/* Something went wrong during the read */
			return -SNMP_ERR_GENERR;
		}
	}

	return get_value(buf, obj->asn, &val);
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
		return -SNMP_ERR_BADVALUE;
	}
	switch (asn_incoming) {
	case ASN_UNSIGNED:
	case ASN_INTEGER:
	    if (len > sizeof(uint32_t))
		return -SNMP_ERR_BADVALUE;
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
	    if (len > obj->data_size) {
		snmp_verbose("%s: string too long (%d), max expected %d\n",
			     len, obj->data_size,__func__);
		return -SNMP_ERR_BADVALUE;
	    }
	    memcpy(p, oid_data, len);
	    *(char *)(p + len) = '\0';
	    snmp_verbose("%s: %s len %d\n", __func__, (char *)p, len);
	    break;
	default:
	    return 0;
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

static int set_ptp_restart(uint8_t *buf, struct snmp_oid *obj)
{
	int ret;
	int32_t *restart_val;

	restart_val = obj->p;
	ret = set_value(buf, obj, restart_val);
	if (ret <= 0)
		return ret;
	switch (*restart_val) {
	case restartPtp:
		snmp_verbose("%s: restart PTP\n", __func__);
		wrc_ptp_stop();
		wrc_ptp_start();

		*restart_val = restartPtpSuccessful;
		break;
	default:
		*restart_val = restartPtpFailed;
	}
	return ret;
}

static int set_ptp_config(uint8_t *buf, struct snmp_oid *obj)
{
	int ret;
	int32_t *apply_mode;
	int temp;

	apply_mode = obj->p;
	ret = set_value(buf, obj, apply_mode);
	if (ret <= 0)
		return ret;
	switch (*apply_mode) {
	case writeToMemoryCurrentSfp:
		sfp_deltaTx = snmp_ptp_config.dTx;
		sfp_deltaRx = snmp_ptp_config.dRx;
		sfp_alpha = snmp_ptp_config.alpha;

		/* Since ppsi does not support update of deltas in runtime,
		 * we need to restart the ppsi */
		pp_printf("SNMP: SFP updated in memory, restart PTP\n");
		wrc_ptp_stop();
		wrc_ptp_start();

		*apply_mode = applySuccessful;
		break;
	case writeToFlashCurrentSfp:
		memcpy(snmp_ptp_config.pn, sfp_pn, SFP_PN_LEN);
		/* continue with writeToFlashGivenSfp */
	case writeToFlashGivenSfp:
		if (snmp_ptp_config.pn[0] == '\0') { /* empty PN */
			snmp_verbose("%s: Invalid PN\n", __func__);
			*apply_mode = applyFailedInvalidPN;
			break;
		}

		temp = strnlen(snmp_ptp_config.pn, SFP_PN_LEN);
		 /* padding with spaces */
		while (temp < SFP_PN_LEN)
			snmp_ptp_config.pn[temp++] = ' ';

		/* add a sfp to the DB */
		temp = storage_get_sfp(&snmp_ptp_config, SFP_ADD, 0);
		if (temp == EE_RET_DBFULL) {
			snmp_verbose("%s: SFP DB is full\n", __func__);
			*apply_mode = applyFailedDBFull;
			break;
		} else if (temp == EE_RET_I2CERR) {
			snmp_verbose("%s: I2C error\n", __func__);
			*apply_mode = applyFailedI2CError;
			break;
		}
		/* perform a sfp match */
		temp = sfp_match();
		if (temp) {
			snmp_verbose("%s: Match error (%d)\n", __func__, temp);
			*apply_mode = applySuccessfulMatchFailed;
			break;
		}

		*apply_mode = applySuccessful;
		break;
	case eraseFlash:
		if (storage_sfpdb_erase() == EE_RET_I2CERR)
			*apply_mode = applyFailed;
		else
			*apply_mode = applySuccessful;
		break;
	default:
		*apply_mode = applyFailed;
	}
	snmp_verbose("%s: delta tx: %d\ndelta rx: %d\nalpha: %d\n", __func__,
		     snmp_ptp_config.dTx, snmp_ptp_config.dRx,
		     snmp_ptp_config.alpha);
	return ret;
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
#define BYTE_REQ_SIZE 0xf9
/* indexes of bytes in snmp packet */
#define BYTE_PACKET_SIZE_i 1
#define BYTE_COMMUNITY_LEN_i 6
/* indexes below are without the length of the community string */
#define BYTE_PDU_i 7
/* below indexes are without the length of request ID */
#define BYTE_REQ_SIZE_i 10
#define BYTE_ERROR_i 13
#define BYTE_ERROR_INDEX_i 16
#define BYTE_OIDLEN_INDEX_i 22

static uint8_t match_array[] = {
	0x30, BYTE_SIZE,
	0x02, 0x01, BYTE_VERSION, /* ASN_INTEGER, size 1 byte, version */
	0x04, 0x06, /* ASN_OCTET_STR, strlen("public") */
		0x70, 0x75, 0x62, 0x6C, 0x69, 0x63, /* "public" */
	BYTE_PDU, BYTE_SIZE, /* SNMP packet type, size of following data */
	0x02, BYTE_REQ_SIZE, /*
	 * ASN_INTEGER, size in bytes, Request/Message ID */
	0x02, 0x01, BYTE_ERROR, /* ASN_INTEGER, size 1 byte, error type */
	0x02, 0x01, BYTE_ERROR_INDEX, /* ASN_INTEGER, size 1 byte,
				       * error index */
	0x30, BYTE_SIZE,
	0x30, BYTE_SIZE,
	0x06, /* ASN_OBJECT_ID */
	/* OID length and oid follows */
};


static void print_oid_verbose(uint8_t *oid, int len)
{
	/*uint8_t * oid_end = oid + len;*/
	int i = 0;

	if (*oid == 0x2b) {/* print first byte */
		snmp_verbose(".1.3");
		i = 1;
	}

	for (; i < len; i++)
		snmp_verbose(".%d", *(oid + i));
}

/* fix sizes of fields in the SNMP packet */
static void snmp_fix_size(uint8_t *buf, int size)
{
	int remain;
	int a_i, h_i;

	size--;
	/* now fix all size fields and change PDU */
	for (a_i = 0, h_i = 0; a_i < sizeof(match_array); a_i++, h_i++) {
		remain = size - h_i;

		if (match_array[a_i] == BYTE_PDU)
			buf[h_i] = SNMP_GET_RESPONSE;
		if (match_array[a_i] == BYTE_REQ_SIZE) {
			h_i += buf[h_i];
			continue;
		}
		if (match_array[a_i] != BYTE_SIZE)
			continue;
		snmp_verbose("%s: offset %i 0x%02x is len %i\n", __func__,
			     h_i, h_i, remain);
		buf[h_i] = remain;
	}
}

/* prepare packet to return error, leave the rest of packet as it was */
static uint8_t snmp_prepare_error(uint8_t *buf, uint8_t error)
{
	uint8_t ret_size;
	uint8_t community_len;
	uint8_t shift;
	uint8_t reqid_size;
	uint8_t oid_len;

	community_len = buf[BYTE_COMMUNITY_LEN_i];
	/* If community_len is too long, it will return malformed
	  * packet, but at least something useful for debugging */
	shift = min(community_len, MAX_COMMUNITY_LEN);
	buf[BYTE_PDU_i + shift] = SNMP_GET_RESPONSE;
	reqid_size = buf[BYTE_REQ_SIZE_i + shift];
	shift += min(reqid_size, MAX_REQID_LEN);
	buf[BYTE_ERROR_i + shift] = error;
	buf[BYTE_ERROR_INDEX_i + shift] = 1;
	oid_len = buf[BYTE_OIDLEN_INDEX_i + shift];
	shift += min(oid_len, MAX_OID_LEN);
	/* write ASN_NULL after the OID */
	buf[BYTE_OIDLEN_INDEX_i + shift + 1] = ASN_NULL;
	buf[BYTE_OIDLEN_INDEX_i + shift + 2] = 0;
	ret_size = BYTE_OIDLEN_INDEX_i + shift + 2 + 1;
	/* recalculate sizes of the fields inside packet */
	snmp_fix_size(buf, ret_size);
	snmp_verbose("%s: error returning %i bytes\n", __func__, ret_size);
	return ret_size;
}

/* And, now, work out your generic frame responder... */
static int snmp_respond(uint8_t *buf)
{
	struct snmp_oid_limb *oid_limb = NULL;
	uint8_t *newbuf = NULL;
	uint8_t *new_oid;
	uint8_t *buf_oid_len;
	int a_i, h_i;
	uint8_t snmp_mode = 0;
	int8_t cmp_result;


	uint8_t oid_branch_matching_len;
	/* Hack to avoid compiler warnings "function defined but not used" for
	 * functions below when SNMP compiled without SET support.
	 * These functions will never be called here. */
	if (0) {
		set_p(NULL, NULL);
		set_pp(NULL, NULL);
		set_ptp_config(NULL, NULL);
		set_ptp_restart(NULL, NULL);
		set_aux_diag(NULL, NULL);
		func_aux_diag(NULL, 0, NULL, 0);
		oid_array_wrpcAuxRwTable[0].oid_len = 0;
		oid_array_wrpcAuxRoTable[0].oid_len = 0;
	}

	for (a_i = 0, h_i = 0; a_i < sizeof(match_array); a_i++, h_i++) {
		switch (match_array[a_i]) {
		case BYTE_SIZE:
		case BYTE_IGNORE:
		case BYTE_ERROR:
		case BYTE_ERROR_INDEX:
			continue;
		case BYTE_REQ_SIZE:
			h_i += buf[h_i];
			continue;
		case BYTE_VERSION:
			snmp_version = buf[h_i];
			if (snmp_version > SNMP_V_MAX)
				return snmp_prepare_error(buf,
							SNMP_ERR_GENERR);
			continue;
		case BYTE_PDU:
			if (SNMP_GET == buf[h_i])
				snmp_mode = MASK_GET;
			if (SNMP_GET_NEXT == buf[h_i])
				snmp_mode = MASK_GET_NEXT;
			if (SNMP_SET_ENABLED && (SNMP_SET == buf[h_i]))
				snmp_mode = MASK_SET;

			if (!(snmp_mode))
				return snmp_prepare_error(buf,
							SNMP_ERR_GENERR);
			break;
		default:
			if (buf[h_i] != match_array[a_i])
				return snmp_prepare_error(buf,
							SNMP_ERR_GENERR);
		}
	}
	snmp_verbose("%s: header match ok\n", __func__);

	buf_oid_len = buf + h_i;
	newbuf = buf + h_i + 1;
	new_oid = buf + h_i + 1;
	/* Save the size of the incoming OID */
	for (oid_limb = oid_limb_array; oid_limb->oid_len; oid_limb++) {
		int res;
		uint8_t flags;

		snmp_verbose("%s: checking oid: ", __func__);
		print_oid_verbose(oid_limb->oid_match, oid_limb->oid_len);
		snmp_verbose("\n");

		oid_branch_matching_len = min(oid_limb->oid_len, *buf_oid_len);

		if ((snmp_mode & MASK_GET || snmp_mode & MASK_SET)
		    && oid_limb->oid_len > *buf_oid_len) {
			 /* For GET and SET incoming OID cannot be shorter than
			  * the one we check now */
			snmp_verbose("%s: continue\n", __func__);
			continue;
		}

		cmp_result = memcmp(oid_limb->oid_match, new_oid,
				    oid_branch_matching_len);

		flags = snmp_mode;
		if ((snmp_mode & MASK_GET_NEXT)
		    && ((cmp_result > 0)
			|| ((oid_limb->oid_len >= *buf_oid_len) && (cmp_result == 0))
		       )) {
			/* return first item in the match within group */
			flags |= RETURN_FIRST;
			/* we want to return the considered oid */
			oid_branch_matching_len = oid_limb->oid_len;
			snmp_verbose("%s: return first oid from the branch\n",
				     __func__);
		}

		if (((oid_limb->oid_len == oid_branch_matching_len) && (cmp_result == 0)) /* exact match */
		    || flags & RETURN_FIRST /* SNMP_GET_NEXT, we're after the OID we got */
		   ) {
			/* branch part of the oid found ! */
			snmp_verbose("%s: group match\n", __func__);
			if (snmp_mode & MASK_GET_NEXT
			    || (flags & RETURN_FIRST)) {
				/* copy new OID */
				memcpy(new_oid, oid_limb->oid_match,
				       oid_limb->oid_len);
			}
			/* pass buf from the oid len, then there is oid itself,
			 * value type, value length and value itself */
			res = oid_limb->twig_func(buf_oid_len,
						  oid_branch_matching_len,
						  oid_limb->obj_array,
						  flags);
			if (res > 0) {
				/* OID found and the return value is filled! */
				if (snmp_mode & MASK_GET_NEXT
				    || (flags & RETURN_FIRST)) {
					/* Update OID len, since it might be
					 * different. Len of the part within
					 * the group was already added in the
					 * twig_func function */
					*buf_oid_len += oid_limb->oid_len;
				}

				newbuf += oid_limb->oid_len;
				newbuf += res;
				break;
			} else if (res == 0) {
				/* oid is not within searched group, take
				 * the next oid */
				continue;
			} else if (res < 0) {
				return snmp_prepare_error(buf,
							  -res);
			}
		}
		if (cmp_result > 0) {
			/* we went too far,no hope to find anything more for
			 * GET and SET, this case for GET_NEXT was considered
			 * earlier */
			oid_limb = NULL;
			break;
		}
		continue;
	}

	if (!oid_limb || !oid_limb->oid_len) {
		snmp_verbose("%s: OID not found! ", __func__);
		print_oid_verbose(new_oid, *buf_oid_len);
		snmp_verbose("\n");
		/* also for last GET_NEXT element */
		return snmp_prepare_error(buf, SNMP_ERR_NOSUCHNAME);
	}

	snmp_fix_size(buf, newbuf - buf);
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

	/* Check the destination IP of SNMP packets. IP version, protocol and
	 * port are checked in the function update_rx_queues, so no need to
	 * check it again */
	if (check_dest_ip(buf)) {
		snmp_verbose("wrong destination IP\n");
		return 0;
	}

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
