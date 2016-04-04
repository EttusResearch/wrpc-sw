#include "syscon.h"
#include "spll_common.h"
#include "hw/etherbone-config.h"

static struct SYSCON_WB _syscon;
volatile struct SYSCON_WB *syscon = &_syscon;

static unsigned char _pps[sizeof(struct PPSG_WB)];
unsigned char *BASE_PPS_GEN = (void *)&_pps;

static unsigned char _pll[sizeof(struct SPLL_WB)];
unsigned char *BASE_SOFTPLL = (void *)&_pll;

static unsigned char _ebone[EB_IPV4 + 4];
unsigned char *BASE_ETHERBONE_CFG = (void *)&_ebone;
