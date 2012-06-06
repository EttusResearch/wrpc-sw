/* SFP Detection / management functions */

#ifndef __SFP_H
#define __SFP_H

#include <stdint.h>

struct sfp_info {
	char part_no[16];
	int32_t delta_tx, delta_rx, alpha;
};

/* Returns 1 if there's a SFP transceiver inserted in the socket. */
int sfp_present();

/* Reads the part ID of the SFP from its configuration EEPROM */
int sfp_read_part_id(char *part_id);

/* SFP Database functions */

/* Adds an SFP to the DB */
int sfp_db_add(struct sfp_info *sinfo);

/* Searches for an SFP matching its' part_id */
struct sfp_info *sfp_db_lookup(const char *part_id);

struct sfp_info *sfp_db_get(int i);

void sfp_db_clear();

#endif

