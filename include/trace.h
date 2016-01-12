#ifndef __FREESTANDING_TRACE_H__
#define __FREESTANDING_TRACE_H__

#ifdef CONFIG_NET_VERBOSE
#define NET_IS_VERBOSE 1
#else
#define NET_IS_VERBOSE 0
#endif

#ifdef CONFIG_WR_NODE
#include <wrc.h>

#define TRACE_DEV(...) wrc_debug_printf(0, __VA_ARGS__)

#else /* WR_SWITCH */

#include <pp-printf.h>
#define TRACE_DEV(...) pp_printf(__VA_ARGS__)

#endif /* node/switch */

#endif
