/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2010 - 2013 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

/* spll_external.h - definitions & prototypes for the 
   external (10 MHz - Grandmaster mode) reference channel */

#ifndef __SPLL_EXTERNAL_H
#define __SPLL_EXTERNAL_H

#include "spll_common.h"
#include "spll_helper.h"
#include "spll_main.h"

struct spll_external_state {
	struct spll_helper_state *helper;
	struct spll_main_state *main;

  int enabled;

	int align_state;
	int align_timer;
  int align_target;
  int align_step;
  int align_shift;
};

void external_init(volatile struct spll_external_state *s, int ext_ref,
			  int realign_clocks);

void external_start(struct spll_external_state *s);

int external_locked(struct spll_external_state *s);

void external_align_fsm( struct spll_external_state *s );

#endif // __SPLL_EXTERNAL_H
