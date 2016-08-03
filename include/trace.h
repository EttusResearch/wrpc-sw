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

#ifdef CONFIG_PFILTER_VERBOSE
#define PFILTER_IS_VERBOSE 1
#else
#define PFILTER_IS_VERBOSE 0
#endif

#ifdef CONFIG_WRC_VERBOSE
#define WRC_IS_VERBOSE 1
#else
#define WRC_IS_VERBOSE 0
#endif

#ifdef CONFIG_SNMP_VERBOSE
#define SNMP_IS_VERBOSE 1
#else
#define SNMP_IS_VERBOSE 0
#endif

#define pll_verbose(...) \
	({if (PLL_IS_VERBOSE) __debug_printf(__VA_ARGS__);})

#define pfilter_verbose(...) \
	({if (PFILTER_IS_VERBOSE) __debug_printf(__VA_ARGS__);})

#define wrc_verbose(...) \
	({if (WRC_IS_VERBOSE) __debug_printf(__VA_ARGS__);})

#define net_verbose(...) \
	({if (NET_IS_VERBOSE) __debug_printf(__VA_ARGS__);})

#define snmp_verbose(...) \
	({if (SNMP_IS_VERBOSE) __debug_printf(__VA_ARGS__);})


#ifdef CONFIG_HOST_PROCESS
#define IS_HOST_PROCESS 1
#else
#define IS_HOST_PROCESS 0
#endif

#endif /*  __FREESTANDING_TRACE_H__ */
