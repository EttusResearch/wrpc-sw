
/* Though freestanding, some minimal headers are expected to exist */
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#define SDB_KERNEL	0
#define SDB_USER	0
#define SDB_FREESTAND	1

#ifdef SDBFS_BIG_ENDIAN
#  define ntohs(x) (x)
#  define htons(x) (x)
#  define ntohl(x) (x)
#  define htonl(x) (x)
#else
#  error "No support, yet, for little-endian freestanding library"
#endif
