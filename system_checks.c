/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2014 CERN (www.cern.ch)
 * Author: Alessandro Rubini <rubini@gnudd.com>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <wrc.h>
#include <assert.h>

#include "system_checks.h"

extern void _reset_handler(void); /* user to reset again */

void check_stack(void)
{
	assert(_endram == ENDRAM_MAGIC, "Stack overflow! (%x)\n", _endram);
}

#ifdef CONFIG_CHECK_RESET

void check_reset(void)
{
	/* static variables to preserve stack (for dumping it) */
	static uint32_t *p, *save;

	/* _endram is set to ENDRAM_MAGIC after calling this function */
	if (_endram != ENDRAM_MAGIC)
		return;

	/* Before calling anything, find the beginning of the stack */
	p = &_endram + 1;
	while (!*p)
		p++;
	p = (void *)((unsigned long)p & ~0xf); /* align */

	/* Copy it to the beginning of the stack, then reset pointers */
	save = &_endram;
	while (p <= &_fstack)
		*save++ = *p++;
	p -= (save - &_endram);
	save = &_endram;

	/* Ok, now init the devices so we can printf and delay */
	init_hw_after_reset();

	pp_printf("\nWarning: the CPU was reset\nStack trace:\n");
	while (p < &_fstack) {
		pp_printf("%08x: %08x %08x %08x %08x\n",
			  (int)p, save[0], save[1], save[2], save[3]);
		p += 4;
		save += 4;
	}
	pp_printf("Rebooting in 1 second\n\n\n");
	timer_delay_ms(1000);

	/* Zero the stack and start over (so we dump correctly next time) */
	for (p = &_endram; p < &_fstack; p++)
		*p = 0;
	_endram = 0;
	_reset_handler();
}

# else /* no CONFIG_CHECK_RESET */

void check_reset(void) {}

#endif
