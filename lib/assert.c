#include <stdarg.h>
#include <assert.h>
#include <wrc.h>

void panic(const char *fmt, ...)
{
	va_list args;

	while (1) {
		pp_printf("Panic: ");
		va_start(args, fmt);
		vprintf(fmt, args);
		va_end(args);
		usleep(1000 * 1000);
	}
}

void __assert(const char *func, int line, int forever,
		     const char *fmt, ...)
{
	va_list args;

	while (1) {
		pp_printf("Assertion failed (%s:%i)", func, line);
		if (fmt && fmt[0]) {
			pp_printf(": ");
			va_start(args, fmt);
			vprintf(fmt, args);
			va_end(args);
		} else {
			pp_printf(".\n");
		}
		if (!forever)
			break;
		usleep(1000 * 1000);
	}
}
