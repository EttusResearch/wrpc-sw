/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2013 CERN (www.cern.ch)
 * Author: Theodor Stana <t.stana@cern.ch>
 *
 * Released according to the GNU LGPL, version 2.1 or any later version.
 */
#ifndef __FLASH_H_
#define __FLASH_H_

#include "types.h"

/* Flash interface functions */
void	flash_init(void);
int	flash_write(uint32_t addr, uint8_t *buf, int count);
int	flash_read(uint32_t addr, uint8_t *buf, int count);
void	flash_serase(uint32_t addr);
void	flash_berase(void);
uint8_t	flash_rsr(void);

/* SDB flash interface functions */
int flash_sdb_check(void);

#endif // __FLASH_H_
