/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2010 - 2013 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

/* spll_ptracker.h - data structures & prototypes for phase trackers. */

#ifndef __SPLL_PTRACKER_H
#define __SPLL_PTRACKER_H

#include "spll_common.h"

struct spll_ptracker_state {
	int enabled, id;
	int n_avg, acc, avg_count, preserve_sign;
	int phase_val, ready;
};

void ptracker_init(struct spll_ptracker_state *s, int id, int num_avgs);
void ptracker_start(struct spll_ptracker_state *s);
int ptrackers_update(struct spll_ptracker_state *ptrackers, int tag, int source);

#endif // __SPLL_PTRACKER_H
