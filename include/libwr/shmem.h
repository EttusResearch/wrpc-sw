/*
 * This is the shared memory interface for multi-process cooperation
 * within the whiterabbit switch. Everyone exports status information.
 */
#ifndef __WRS_SHM_H__
#define __WRS_SHM_H__
#include <stdint.h>

#define WRS_SHM_FILE  "/dev/shm/wrs-shmem-%i"
#define WRS_SHM_MIN_SIZE    (4*1024)
#define WRS_SHM_MAX_SIZE  (256*1024)

/* Each process "name" (i.e. id) is added to the filename above */
enum wrs_shm_name {
	wrs_shm_ptp,
	wrs_shm_rtu,
	wrs_shm_hal,
	wrs_shm_vlan,
	WRS_SHM_N_NAMES,	/* must be last */
};

/* Each area starts with this process identifier */
struct wrs_shm_head {
	void *mapbase;		/* In writer's addr space (to track ptrs) */
	char name[7 * sizeof(void *)];

	unsigned long stamp;	/* Last modified, w/ CLOCK_MONOTONIC */
	unsigned long data_off;	/* Where the structure lives */
	int fd;			/* So we can enlarge it using fd */
	int pid;		/* The current pid owning the area */

	unsigned pidsequence;	/* Each new pid must increments this */
	unsigned sequence;	/* If we need consistency, this is it */
	unsigned version;	/* Version of the data structure */
	unsigned data_size;	/* Size of it (for binary dumps) */
};

/* flags */
#define WRS_SHM_READ   0x0000
#define WRS_SHM_WRITE  0x0001
#define WRS_SHM_LOCKED 0x0002 /* at init time: writers locks, readers wait  */

/* get vs. put, like in the kernel. Errors are in errno (see source) */
void *wrs_shm_get(enum wrs_shm_name name_id, char *name, unsigned long flags);
int wrs_shm_put(void *headptr);

/* The writer can allocate structures that live in the area itself */
void *wrs_shm_alloc(void *headptr, size_t size);

/* The reader can track writer's pointers, if they are in the area */
void *wrs_shm_follow(void *headptr, void *ptr);

/* Before and after writing a chunk of data, act on sequence and stamp */
#define WRS_SHM_WRITE_BEGIN	1
#define WRS_SHM_WRITE_END	0
extern void wrs_shm_write(void *headptr, int flags);

/* A reader can rely on the sequence number (in the <linux/seqlock.h> way) */
extern unsigned wrs_shm_seqbegin(void *headptr);
extern int wrs_shm_seqretry(void *headptr, unsigned start);

/* A reader can check wether information is current enough */
extern int wrs_shm_age(void *headptr);

/* A reader can get the information pointer, for a specific version, or NULL */
extern void *wrs_shm_data(void *headptr, unsigned version);

#endif /* __WRS_SHM_H__ */
