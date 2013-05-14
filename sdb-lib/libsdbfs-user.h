#ifndef __LIBSDBFS_USER_H__
#define __LIBSDBFS_USER_H__

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h> /* htonl */

#define SDB_KERNEL	0
#define SDB_USER	1
#define SDB_FREESTAND	0

#define sdb_print(format, ...) fprintf(stderr, format, __VA_ARGS__)

#endif /* __LIBSDBFS_USER_H__ */
