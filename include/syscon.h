#ifndef __SYSCON_H
#define __SYSCON_H

#include <inttypes.h>

#include "board.h"
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

#define TICS_PER_SECOND 1000
void timer_init(uint32_t enable);
uint32_t timer_get_tics();
void timer_delay(uint32_t how_long);

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

#endif
