/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

#ifndef __SFP_H
#define __SFP_H

#include <stdint.h>

/* Returns 1 if there's a SFP transceiver inserted in the socket. */
int sfp_present(void);

/* Reads the part ID of the SFP from its configuration EEPROM */
int sfp_read_part_id(char *part_id);

#endif
