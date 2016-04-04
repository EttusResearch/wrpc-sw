/*
 * This includes stuff that is needed to link ppsi into wrpc-sw
 *
 * It is included by the command line (-include).
 * It is a subset of the older "ptpd-wrappers.h", in ptp-noposix
 */
#include <sys/types.h>
#include <stdlib.h>
#include <stdint.h>

#ifdef CONFIG_HOST_PROCESS
#  include <arpa/inet.h>
#else

#  ifndef __IEEE_BIG_ENDIAN
#    error "Not big endian, or unknown endianness"
#  endif

  static inline uint16_t ntohs(uint16_t x) {return x;}

#endif
