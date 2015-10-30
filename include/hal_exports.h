/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#ifndef __HAL_EXPORTS_H
#define __HAL_EXPORTS_H

#include <stdint.h>

#define HAL_MAX_PORTS 32

#define WRSW_HAL_SERVER_ADDR "wrsw_hal"

// checks if the calibration unit is idle
#define HEXP_CAL_CMD_CHECK_IDLE 1

// enables/disables transmission of calibration pattern
#define HEXP_CAL_CMD_TX_PATTERN 2

// requests a measurement of TX delta
#define HEXP_CAL_CMD_TX_MEASURE 4

// requests a measurement of RX delta
#define HEXP_CAL_CMD_RX_MEASURE 5

#define HEXP_CAL_RESP_BUSY 1
#define HEXP_CAL_RESP_OK 0
#define HEXP_CAL_RESP_ERROR -1

#define HEXP_LOCK_CMD_START 1
#define HEXP_LOCK_CMD_CHECK 2
#define HEXP_LOCK_CMD_ENABLE_TRACKING 3

#define HEXP_LOCK_STATUS_LOCKED 0
#define HEXP_LOCK_STATUS_BUSY 1
#define HEXP_LOCK_STATUS_NONE 2

#define HEXP_PPSG_CMD_GET 0
#define HEXP_PPSG_CMD_ADJUST_PHASE 1
#define HEXP_PPSG_CMD_ADJUST_SEC 2
#define HEXP_PPSG_CMD_ADJUST_NSEC 3
#define HEXP_PPSG_CMD_POLL 4
#define HEXP_PPSG_CMD_SET_VALID 5

#define HEXP_ON 1
#define HEXP_OFF 0

#define HEXP_FREQ 0
#define HEXP_PHASE 1

/////////////////added by ML//////////
#define HEXP_EXTSRC_CMD_CHECK 0

#define HEXP_EXTSRC_STATUS_LOCKED 0
#define HEXP_LOCK_STATUS_BUSY	  1
#define HEXP_EXTSRC_STATUS_NOSRC  2
/////////////////////////////////////

#define HAL_TIMING_MODE_GRAND_MASTER 0
#define HAL_TIMING_MODE_FREE_MASTER 1
#define HAL_TIMING_MODE_BC 2

typedef struct {

	char port_name[16];

	int pps_valid;

	uint32_t current_phase_shift;
	int32_t adjust_phase_shift;

	int64_t adjust_sec;
	int32_t adjust_nsec;

	uint64_t current_sec;
	uint32_t current_nsec;

} hexp_pps_params_t;

/* Port modes (hexp_port_state_t.mode) */
#define HEXP_PORT_MODE_WR_M_AND_S 4
#define HEXP_PORT_MODE_WR_MASTER 1
#define HEXP_PORT_MODE_WR_SLAVE 2
#define HEXP_PORT_MODE_NON_WR 3

#define FIX_ALPHA_FRACBITS 40
/*
#define HEXP_PORT_TSC_RISING 1
#define HEXP_PORT_TSC_FALLING 2
*/

typedef struct {
	int timing_mode;	/* Free-running Master/GM/BC */
	int locked_port;

} hexp_timing_state_t;

/* Prototypes of functions that call on rpc */
extern int halexp_check_running(void);
extern int halexp_reset_port(const char *port_name);
extern int halexp_calibration_cmd(const char *port_name, int command, int on_off);
extern int halexp_lock_cmd(const char *port_name, int command, int priority);
extern int halexp_pps_cmd(int cmd, hexp_pps_params_t *params);
extern int halexp_get_timing_state(hexp_timing_state_t *state);

/* Export structures, shared by server and client for argument matching */
#ifdef HAL_EXPORT_STRUCTURES

//int halexp_check_running();
struct minipc_pd __rpcdef_check_running = {
	.name = "check_running",
	.retval = MINIPC_ARG_ENCODE(MINIPC_ATYPE_INT, int),
	.args = {
		 MINIPC_ARG_END,
		 },
};

//int halexp_reset_port(const char *port_name);
struct minipc_pd __rpcdef_reset_port = {
	.name = "reset_port",
	.retval = MINIPC_ARG_ENCODE(MINIPC_ATYPE_INT, int),
	.args = {
		 MINIPC_ARG_ENCODE(MINIPC_ATYPE_STRING, char *),
		 MINIPC_ARG_END,
		 },
};

//int halexp_calibration_cmd(const char *port_name, int command, int on_off);
struct minipc_pd __rpcdef_calibration_cmd = {
	.name = "calibration_cmd",
	.retval = MINIPC_ARG_ENCODE(MINIPC_ATYPE_INT, int),
	.args = {
		 MINIPC_ARG_ENCODE(MINIPC_ATYPE_STRING, char *),
		 MINIPC_ARG_ENCODE(MINIPC_ATYPE_INT, int),
		 MINIPC_ARG_ENCODE(MINIPC_ATYPE_INT, int),
		 MINIPC_ARG_END,
		 },
};

//int halexp_lock_cmd(const char *port_name, int command, int priority);
struct minipc_pd __rpcdef_lock_cmd = {
	.name = "lock_cmd",
	.retval = MINIPC_ARG_ENCODE(MINIPC_ATYPE_INT, int),
	.args = {
		 MINIPC_ARG_ENCODE(MINIPC_ATYPE_STRING, char *),
		 MINIPC_ARG_ENCODE(MINIPC_ATYPE_INT, int),
		 MINIPC_ARG_ENCODE(MINIPC_ATYPE_INT, int),
		 MINIPC_ARG_END,
		 },
};

//int halexp_query_ports(hexp_port_list_t *list);
struct minipc_pd __rpcdef_query_ports = {
	.name = "query_ports",
	.retval = MINIPC_ARG_ENCODE(MINIPC_ATYPE_STRUCT, hexp_port_list_t),
	.args = {
		 MINIPC_ARG_END,
		 },
};

//int halexp_pps_cmd(int cmd, hexp_pps_params_t *params);
struct minipc_pd __rpcdef_pps_cmd = {
	.name = "pps_cmd",
	.retval = MINIPC_ARG_ENCODE(MINIPC_ATYPE_INT, int),
	.args = {
		 MINIPC_ARG_ENCODE(MINIPC_ATYPE_INT, int),
		 MINIPC_ARG_ENCODE(MINIPC_ATYPE_STRUCT, hexp_pps_params_t),
		 MINIPC_ARG_END,
		 },
};

struct minipc_pd __rpcdef_get_timing_state = {
	.name = "get_timing_state",
	.retval = MINIPC_ARG_ENCODE(MINIPC_ATYPE_STRUCT, hexp_timing_state_t),
	.args = {
		 MINIPC_ARG_END,
		 },
};

#endif /* HAL_EXPORT_STRUCTURES */

#endif
