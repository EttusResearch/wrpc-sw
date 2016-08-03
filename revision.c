/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012 - 2015 CERN (www.cern.ch)
 * Author: Alessandro Rubini <rubini@gnudd.com>
 * Author: Adam Wujek <adam.wujek@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include "softpll_ng.h"
#include "revision.h"
const char *build_revision = stats.commit_id;

const char *build_date = stats.build_date;
const char *build_time = stats.build_time;
const char *build_by = stats.build_by;
/*
 * We export softpll internal status to the ARM cpu, for SNMP. Thus,
 * we place this structure at a known address in the linker script
 */
struct spll_stats stats __attribute__((section(".stats"))) = {
	.magic = 0x5b1157a7,
	.ver = SPLL_STATS_VER,
#ifdef CONFIG_DETERMINISTIC_BINARY
	.build_date = "",
	.build_time = "",
	.build_by = "",
#else
	.build_date = __DATE__,
	.build_time = __TIME__,
	.build_by = __GIT_USR__,
#endif
	.commit_id = __GIT_VER__,
};
