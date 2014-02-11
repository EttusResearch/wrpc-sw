/*
 * We build for both wr-switch and wr-node (core).
 *
 * Unfortunately, our submodules include <board.h> without using
 * our own Kconfig defines. Thus, assume wr-node unless building
 * specifically for wr-switch (which doesn't refer to submodules).
 * Same appplies to ./tools/, where we can avoid a Makefile
 * patch for add "-include ../include/generated/autoconf.h"
 */
#if defined(CONFIG_WR_SWITCH)
#  include "board-wrs.h"
#else
#  include "board-wrc.h"
#endif
