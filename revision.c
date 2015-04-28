#include "softpll_ng.h"
#include "revision.h"
const char *build_revision = stats.commit_id;

const char *build_date = stats.build_date;
const char *build_time = stats.build_time;
/*
 * We export softpll internal status to the ARM cpu, for SNMP. Thus,
 * we place this structure at a known address in the linker script
 */
struct spll_stats stats __attribute__((section(".stats"))) = {
	.magic = 0x5b1157a7,
	.ver = SPLL_STATS_VER,
#ifdef CONFIG_DETERMINISTIC_BINARY
	.build_date = "",
#else
	.build_date = __DATE__,
	.build_time = __TIME__,
#endif
	.commit_id = __GIT_VER__,
};
