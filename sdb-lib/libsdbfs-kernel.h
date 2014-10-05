/*
 * This supports both the Linux kernel and barebox, that is similar
 * by design, and defines __KERNEL__ too.
 */
#ifdef __BAREBOX__
#  include <errno.h>
#else /* really linux */
#  include <linux/errno.h>
#endif

#include <linux/types.h>
#include <linux/string.h>
#include <asm/byteorder.h>

/*
 * The default installed /usr/include/linux stuff misses the __KERNEL__ parts.
 * For libsdbfs it means we won't get uint32_t and similar types.
 *
 * So, check if we got the information we need before strange errors happen.
 * The DECLARE_BITMAP macro is in <linux/types.h> since the epoch, but it
 * is not installed in /usr/include/linux/types.h, so use it to check.
 *
 * If building for barebox, we miss the macro, but we are sure that
 * we are picking the correct header, because the library is only built
 * within the barebox source tree.
 */
#if !defined(DECLARE_BITMAP) && !defined(__BAREBOX__)
#  error "Please point LINUX to a source tree if you define __KERNEL__"
#endif

#define SDB_KERNEL	1
#define SDB_USER	0
#define SDB_FREESTAND	0

#define sdb_print(format, ...) printk(format, __VA_ARGS__)
