#include <stdio.h>
#include <stdarg.h>

#include "term.h"

void cprintf(int color, const char *fmt, ...)
{
    va_list ap;
    mprintf("\033[0%d;3%dm",color & C_DIM ? 2:1, color&0x7f);
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
}

void pcprintf(int row, int col, int color, const char *fmt, ...)
{
    va_list ap;
    mprintf("\033[%d;%df", row, col);
    mprintf("\033[0%d;3%dm",color & C_DIM ? 2:1, color&0x7f);
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
}

void term_clear()
{
    mprintf("\033[2J\033[1;1H");
}

