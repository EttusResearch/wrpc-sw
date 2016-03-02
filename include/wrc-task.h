/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#ifndef __WRC_TASK_H__
#define __WRC_TASK_H__

/*
 * A task is a data structure, but currently suboptimal.
 * FIXME: init must return int, and both should get a pointer to data
 * FIXME: provide a jiffy-based period.
 */

struct wrc_task {
	char *name;
	int *enable;		/* A global enable variable */
	void (*init)(void);
	int (*job)(void);
	/* And we keep statistics about cpu usage */
	unsigned long nrun;
	unsigned long seconds;
	unsigned long nanos;
};

/* Put the tasks in their own section */
#define DEFINE_WRC_TASK(_name) \
	static struct wrc_task  __task_ ## _name \
	__attribute__((section(".task"), used, aligned(sizeof(unsigned long))))
/* Task 0 must be first, sorry! */
#define DEFINE_WRC_TASK0(_name) \
	static struct wrc_task __task_ ## _name \
	__attribute__((section(".task0"), used, aligned(sizeof(unsigned long))))

extern struct wrc_task __task_begin[];
extern struct wrc_task __task_end[];
#define for_each_task(t) for ((t) = __task_begin; (t) < __task_end; (t)++)

#endif /* __WRC_TASK_H__ */
