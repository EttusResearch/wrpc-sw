
/*
 * This file includes stuff we don't want to link, so if it is
 * called in error we get a clear error message, rather than a
 * "doesn't fit in ram" error
 */
#include <stdio.h>

extern void __you_should_not_call_printf_from_wrpc_sw(void);
extern void __you_should_not_divide_ll_in_wrpc_sw(void);

#undef printf /* Under ptp-noposix, this is #defined to mprintf */
int printf(const char *fmt, ...)
{
	__you_should_not_call_printf_from_wrpc_sw();
	return 0;
}

#ifdef CONFIG_PPSI /* with ppsi we can avoid libgcc code for division */
/* was used twice in picos_to_ts  */
long long __moddi3 (long long A, long long B)
{
	__you_should_not_divide_ll_in_wrpc_sw();
	return 0;
}

/* was used in set_phase_shift, phase_to_cf_units */
long long __divdi3 (long long A, long long B)
{
	__you_should_not_divide_ll_in_wrpc_sw();
	return 0;
}
#endif /* config_ppsi */
