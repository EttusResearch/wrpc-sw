/*
 * This includes stuff that is needed to link ppsi into wrpc-sw
 *
 * It is included by the command line (-include) like ptp-noposix's
 * ptpd-wrappers.h is. It is a subset of that file, with only needed stuff
 */
#include <sys/types.h>
#include <stdlib.h>
#include <stdint.h>

#ifndef __IEEE_BIG_ENDIAN
#error "Not big endian, or unknown endianness"
#endif

static inline uint16_t ntohs(uint16_t x) {return x;}
