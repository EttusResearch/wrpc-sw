#ifndef __FREESTANDING_TRACE_H__
#define __FREESTANDING_TRACE_H__

#define TRACE_WRAP(...)
#define TRACE_DEV(...) wrc_debug_printf(0, __VA_ARGS__)

#endif
