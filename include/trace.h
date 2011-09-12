#ifndef __FREESTANDING_TRACE_H__
#define __FREESTANDING_TRACE_H__

extern int wrc_extra_debug;

#define TRACE_WRAP(...)
#define TRACE_DEV(...) if(wrc_extra_debug) wrc_debug_printf(0, __VA_ARGS__)

#endif
