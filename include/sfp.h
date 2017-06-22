/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

#ifndef __SFP_H
#define __SFP_H

#include <stdint.h>

#define SFP_PN_LEN 16
#define SFP_NOT_MATCHED 1
#define SFP_MATCHED 2

#define SFP_GET 0
#define SFP_ADD 1

extern char sfp_pn[SFP_PN_LEN];

extern int32_t sfp_in_db;
extern int32_t sfp_alpha;
extern int32_t sfp_deltaTx;
extern int32_t sfp_deltaRx;

/* Match plugged SFP with a DB entry */
int sfp_match(void);

#endif
