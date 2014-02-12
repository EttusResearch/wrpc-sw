#ifndef __SYSCON_H
#define __SYSCON_H

#include <inttypes.h>
#include <sys/types.h>
#include "board.h"

uint32_t timer_get_tics(void);
void timer_delay(uint32_t tics);

/* The following ones come from the kernel, but simplified */
#ifndef time_after
#define time_after(a,b)		\
	((long)(b) - (long)(a) < 0)
#define time_before(a,b)	time_after(b,a)
#define time_after_eq(a,b)	\
	 ((long)(a) - (long)(b) >= 0)
#define time_before_eq(a,b)	time_after_eq(b,a)
#endif

/* This can be used for up to 2^32 / TICS_PER_SECONDS == 42 seconds in wrs */
static inline void timer_delay_ms(int ms)
{
	timer_delay(ms * TICS_PER_SECOND / 1000);
}


#ifdef CONFIG_WR_NODE

#undef PACKED /* if we already included a regs file, we'd get a warning */
#include <hw/wrc_syscon_regs.h>

struct SYSCON_WB {
	uint32_t RSTR;		/*Syscon Reset Register */
	uint32_t GPSR;		/*GPIO Set/Readback Register */
	uint32_t GPCR;		/*GPIO Clear Register */
	uint32_t HWFR;		/*Hardware Feature Register */
	uint32_t TCR;		/*Timer Control Register */
	uint32_t TVR;		/*Timer Counter Value Register */
};

/*GPIO pins*/
#define GPIO_LED_LINK SYSC_GPSR_LED_LINK
#define GPIO_LED_STAT SYSC_GPSR_LED_STAT
#define GPIO_BTN1     SYSC_GPSR_BTN1
#define GPIO_BTN2     SYSC_GPSR_BTN2
#define GPIO_SFP_DET  SYSC_GPSR_SFP_DET

#define WRPC_FMC_I2C  0
#define WRPC_SFP_I2C  1

struct s_i2c_if {
	uint32_t scl;
	uint32_t sda;
};

extern struct s_i2c_if i2c_if[2];

void timer_init(uint32_t enable);

/* usleep.c */
extern void usleep_init(void);
extern int usleep(useconds_t usec);



extern volatile struct SYSCON_WB *syscon;

/****************************
 *        GPIO
 ***************************/
static inline void gpio_out(int pin, int val)
{
	if (val)
		syscon->GPSR = pin;
	else
		syscon->GPCR = pin;
}

static inline int gpio_in(int pin)
{
	return syscon->GPSR & pin ? 1 : 0;
}

static inline int sysc_get_memsize()
{
	return (SYSC_HWFR_MEMSIZE_R(syscon->HWFR) + 1) * 16;
}

#endif /* CONFIG_WR_NODE */
#endif
