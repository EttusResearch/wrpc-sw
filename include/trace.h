#ifndef __FREESTANDING_TRACE_H__
#define __FREESTANDING_TRACE_H__

#ifdef CONFIG_NET_VERBOSE
#define NET_IS_VERBOSE 1
#else
#define NET_IS_VERBOSE 0
#endif

#ifdef CONFIG_PLL_VERBOSE
#define PLL_IS_VERBOSE 1
#else
#define PLL_IS_VERBOSE 0
#endif

#ifdef CONFIG_WRC_VERBOSE
#define WRC_IS_VERBOSE 1
#else
#define WRC_IS_VERBOSE 0
#endif

#define pll_verbose(...) \
	({if (PLL_IS_VERBOSE) __debug_printf(__VA_ARGS__);})

#define wrc_verbose(...) \
	({if (WRC_IS_VERBOSE) __debug_printf(__VA_ARGS__);})


#endif /*  __FREESTANDING_TRACE_H__ */
