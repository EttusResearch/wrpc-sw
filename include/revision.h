/*
* This work is part of the White Rabbit project
*
* Copyright (C) 2017 CERN (www.cern.ch)
* Author: Adam Wujek <adam.wujek@cern.ch>
*
* Released according to the GNU GPL, version 2 or any later version.
*/
#ifndef __REVISION_H__
#define __REVISION_H__

/* Please increment WRPC_SHMEM_VERSION if you change any exported data
 * structure */
#define WRPC_SHMEM_VERSION 2 /* removed some unused fields */

#ifndef __ASSEMBLY__
extern const char *build_revision;
extern const char *build_date;
extern const char *build_time;
extern const char *build_by;
#endif

#endif /* __REVISION_H__ */
