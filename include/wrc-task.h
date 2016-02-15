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
	unsigned long  nrun;
};


#endif /* __WRC_TASK_H__ */
