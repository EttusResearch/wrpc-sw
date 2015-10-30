/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#ifndef __SYSTEM_CHECKS_H__
#define __SYSTEM_CHECKS_H__

#define ENDRAM_MAGIC 0xbadc0ffe

extern uint32_t _endram;
extern uint32_t _fstack;

void check_stack(void);
void check_reset(void);
void init_hw_after_reset(void);

#endif /* __SYSTEM_CHECKS_H__ */
