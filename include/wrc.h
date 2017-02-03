/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#ifndef __WRC_H__
#define __WRC_H__

/*
 * This header includes all generic prototypes that were missing
 * in earlier implementations. For example, the monitor is only
 * one function and doesn't deserve an header of its own.
 * Also, this brings in very common and needed headers
 */
#include <inttypes.h>
#include <syscon.h>
#include <pp-printf.h>
#include <util.h>
#include <trace.h>
#include <wrc-task.h>
#define vprintf pp_vprintf
#define sprintf pp_sprintf

#ifndef min
#define min(a, b) \
	({ __typeof__ (a) _a = (a); \
	  __typeof__ (b) _b = (b); \
	  _a < _b ? _a : _b; })
#endif

#undef offsetof
#define offsetof(TYPE, MEMBER) ((long) &((TYPE *)0)->MEMBER)
#undef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

/* Allow "if" at C language level, to avoid ifdef */
#ifdef CONFIG_WR_SWITCH
#  define is_wr_switch 1
#  define is_wr_node 0
#else
#  define is_wr_switch 0
#  define is_wr_node 1
#endif

#ifdef CONFIG_WR_NODE_SIM
#  define IS_WR_NODE_SIM 1
#else
#  define IS_WR_NODE_SIM 0
#endif

extern int wrc_vlan_number;

int wrc_mon_gui(void);
void shell_init(void);

/* This header is included by softpll: manage wrc/wrs difference */
#ifdef CONFIG_WR_NODE
#define NS_PER_CLOCK 8
#else /* CONFIG_WR_SWITCH */
#define NS_PER_CLOCK 16
#endif

/* Default width (in 8ns/16ns units) of the pulses on the PPS output */
#define PPS_WIDTH (10 * 1000 * 1000 / NS_PER_CLOCK) /* 10ms */

/* This is in the library, somewhere */
extern int abs(int val);

/* refresh period for _gui_ and _stat_ commands */
extern int wrc_ui_refperiod;

/* Init functions and defaults for the wrs build */
int ad9516_init(int scb_ver);
void rts_init(void);
int rtipc_init(void);
void rts_update(void);
void rtipc_action(void);

/* div64.c, lifted from the linux kernel through pp_printf or ppsi */
extern uint32_t __div64_32(uint64_t *n, uint32_t base);

#endif /* __WRC_H__ */
