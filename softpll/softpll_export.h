/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2010 - 2013 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

/* softpll_export.h: public SoftPLL stats API header */

#ifndef __SOFTPLL_EXPORT_H
#define __SOFTPLL_EXPORT_H

#include <stdint.h>


/* SoftPLL operating modes, for mode parameter of spll_init(). */

/* Grand Master - lock to 10 MHz external reference */
#define SPLL_MODE_GRAND_MASTER 1

/* Free running master - 125 MHz refrence free running, DDMTD locked to it */
#define SPLL_MODE_FREE_RUNNING_MASTER 2

/* Slave mode - 125 MHz reference locked to one of the input clocks */
#define SPLL_MODE_SLAVE 3

/* Disabled mode: SoftPLL inactive */
#define SPLL_MODE_DISABLED 4

#define SEQ_START_EXT 1
#define SEQ_WAIT_EXT 2
#define SEQ_START_HELPER 3
#define SEQ_WAIT_HELPER 4
#define SEQ_START_MAIN 5
#define SEQ_WAIT_MAIN 6
#define SEQ_DISABLED 7
#define SEQ_READY 8
#define SEQ_CLEAR_DACS 9
#define SEQ_WAIT_CLEAR_DACS 10

#define AUX_DISABLED 1
#define AUX_LOCK_PLL 2
#define AUX_ALIGN_PHASE 3
#define AUX_READY 4

#define ALIGN_STATE_EXT_OFF 0
#define ALIGN_STATE_START 1
#define ALIGN_STATE_INIT_CSYNC 2
#define ALIGN_STATE_WAIT_CSYNC 3
#define ALIGN_STATE_START_ALIGNMENT 7
#define ALIGN_STATE_WAIT_SAMPLE 4
#define ALIGN_STATE_COMPENSATE_DELAY 5
#define ALIGN_STATE_LOCKED 6
#define ALIGN_STATE_START_MAIN 8
#define ALIGN_STATE_WAIT_CLKIN 9
#define ALIGN_STATE_WAIT_PLOCK 10

#define SPLL_STATS_VER 2

/* info reported through .stat section */
/* due to endiannes problem strings has to be 4 bytes alligned */
struct spll_stats {
	int magic;	/* 0x5b1157a7 = SPLLSTAT ?;)*/
	int ver;	/* version of the structure */
	int sequence;	/* sequence number, so the host can retry */
	int mode;
	int irq_cnt;
	int seq_state;
	int align_state;
	int H_lock;
	int M_lock;
	int H_y, M_y;
	int del_cnt;
	int start_cnt;
	char commit_id[32];
	char build_date[16];
	char build_time[16];
	char build_by[32];
};

extern struct spll_stats stats;

#endif /* __SOFTPLL_EXPORT_H */

