/*
 *==============================================================================
 * CERN (BE-CO-HT)
 * Header file for M25P flash memory controller
 *==============================================================================
 *
 * author: Theodor Stana (t.stana@cern.ch)
 *
 * date of creation: 2013-09-25
 *
 * version: 1.0
 *
 * description: 
 *
 * dependencies:
 *
 * references:
 * 
 *==============================================================================
 * GNU LESSER GENERAL PUBLIC LICENSE
 *==============================================================================
 * This source file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation; either version 2.1 of the License, or (at your
 * option) any later version. This source is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details. You should have
 * received a copy of the GNU Lesser General Public License along with this
 * source; if not, download it from http://www.gnu.org/licenses/lgpl-2.1.html
 *==============================================================================
 * last changes:
 *    2013-09-25   Theodor Stana     t.stana@cern.ch     File created
 *==============================================================================
 * TODO: - 
 *==============================================================================
 */
 
#ifndef __FLASH_H_
#define __FLASH_H_
 
#include "types.h"

void flash_init();
void flash_write(int nrbytes, uint32_t addr, uint8_t *dat);
void flash_read(int nrbytes, uint32_t addr, uint8_t *dat);
void flash_serase(uint32_t addr);
uint8_t flash_rsr();

#endif // __FLASH_H_
