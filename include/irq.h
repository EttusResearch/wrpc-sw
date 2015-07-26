#ifndef __IRQ_H
#define __IRQ_H

static inline void clear_irq(void)
{
	unsigned int val = 1;
	asm volatile ("wcsr ip, %0"::"r" (val));
}

void disable_irq(void);
void enable_irq(void);

#endif
