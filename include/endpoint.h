#ifndef __ENDPOINT_H
#define __ENDPOINT_H

#define DMTD_AVG_SAMPLES 256
#define DMTD_MAX_PHASE 16384

#include <stdint.h>

typedef enum {
AND=0,
NAND=4,
OR=1,
NOR=5,
XOR=2,
XNOR=6,
MOV=3,
NOT=7
} pfilter_op_t;

void ep_init(uint8_t mac_addr[]);
void get_mac_addr(uint8_t dev_addr[]);
int ep_enable(int enabled, int autoneg);
int ep_link_up(uint16_t *lpa);
int ep_get_deltas(uint32_t *delta_tx, uint32_t *delta_rx);
int ep_get_psval(int32_t *psval);
int ep_cal_pattern_enable();
int ep_cal_pattern_disable();

void pfilter_new();
void pfilter_cmp(int offset, int value, int mask, pfilter_op_t op, int rd);
void pfilter_btst(int offset, int bit_index, pfilter_op_t op, int rd);
void pfilter_nop();
void pfilter_logic2(int rd, int ra, pfilter_op_t op, int rb);
static void pfilter_logic3(int rd, int ra, pfilter_op_t op, int rb, pfilter_op_t op2, int rc);
void pfilter_load();
void pfilter_init_default();

uint16_t pcs_read(int location);
void pcs_write(int location, int value);

#endif
