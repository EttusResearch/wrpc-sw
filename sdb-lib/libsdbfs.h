#ifndef __LIBSDBFS_H__
#define __LIBSDBFS_H__

/* The library can work in three different environments */
#ifdef __KERNEL__
#  include "libsdbfs-kernel.h"
#elif defined(__unix__)
#  include "libsdbfs-user.h"
#else
#  include "libsdbfs-freestanding.h"
#endif

#include <sdb.h> /* Please point your "-I" to some sensible place */

/*
 * Data structures: please not that the library intself doesn't use
 * malloc, so it's the caller who must deal withallocation/removal.
 * For this reason we can have no opaque structures, but some fields
 * are private
 */

struct sdbfs {

	/* Some fields are informative */
	char *name;			/* may be null */
	void *drvdata;			/* driver may need some detail.. */
	int blocksize;
	unsigned long entrypoint;

	/* The "driver" must offer some methods */
	void *data;			/* Use this if directly mapped */
	unsigned long datalen;		/* Length of the above array */
	int (*read)(struct sdbfs *fs, int offset, void *buf, int count);
	int (*write)(struct sdbfs *fs, int offset, void *buf, int count);
	int (*erase)(struct sdbfs *fs, int offset, int count);

	/* The following fields are library-private */
	struct sdb_device current_record;
	struct sdb_device *currentp;
	int nleft;
	unsigned long f_offset;
	unsigned long f_len;
	unsigned long read_offset;
	unsigned long flags;
	struct sdbfs *next;
};

#define SDBFS_F_VERBOSE		0x0001


/* Defined in glue.c */
int sdbfs_dev_create(struct sdbfs *fs, int verbose);
int sdbfs_dev_destroy(struct sdbfs *fs);
struct sdbfs *sdbfs_dev_find(const char *name);
int sdbfs_open_name(struct sdbfs *fs, const char *name);
int sdbfs_open_id(struct sdbfs *fs, uint64_t vid, uint32_t did);
int sdbfs_close(struct sdbfs *fs);
struct sdb_device *sdbfs_scan(struct sdbfs *fs, int newscan);

/* Defined in access.c */
int sdbfs_fstat(struct sdbfs *fs, struct sdb_device *record_return);
int sdbfs_fread(struct sdbfs *fs, int offset, void *buf, int count);
int sdbfs_fwrite(struct sdbfs *fs, int offset, void *buf, int count);

/* This is needed to convert endianness. Hoping it is not defined elsewhere */
static inline uint64_t htonll(uint64_t ll)
{
        uint64_t res;

        if (htonl(1) == 1)
                return ll;
        res = htonl(ll >> 32);
        res |= (uint64_t)(htonl((uint32_t)ll)) << 32;
        return res;
}
static inline uint64_t ntohll(uint64_t ll)
{
	return htonll(ll);
}

#endif /* __LIBSDBFS_H__ */
