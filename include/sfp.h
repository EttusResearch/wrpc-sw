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

extern int32_t sfp_in_db;
extern int32_t sfp_alpha;
extern int32_t sfp_deltaTx;
extern int32_t sfp_deltaRx;

/* Returns 1 if there's a SFP transceiver inserted in the socket. */
int sfp_present(void);

/* Reads the part ID of the SFP from its configuration EEPROM */
int sfp_read_part_id(char *part_id);

#endif
