/*
 *==============================================================================
 * CERN (BE-CO-HT)
 * Source file for M25P flash controller
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
 
#include "flash.h"
#include "types.h"
#include "syscon.h"

static void delay()
{
  int i;
  for (i = 0; i < 1; i++)
    asm volatile ("nop");
}

/*
 * Bit-bang SPI transfer function
 */
static uint8_t bbspi_transfer(uint8_t cspin, uint8_t val)
{
  uint8_t i, retval = 0;
  gpio_out(GPIO_SPI_NCS, cspin);
  for (i = 0; i < 8; i++)
  {
    gpio_out(GPIO_SPI_SCLK, 0);
    if (val & 0x80)
    {
      gpio_out(GPIO_SPI_MOSI, 1);
    }
    else
    {
      gpio_out(GPIO_SPI_MOSI, 0);
    }
    delay();
    gpio_out(GPIO_SPI_SCLK, 1);
    retval <<= 1;
    retval |= gpio_in(GPIO_SPI_MISO);
    delay();
    val <<= 1;
  }

  gpio_out(GPIO_SPI_SCLK, 0);

  return retval;
}

void flash_init()
{
  gpio_out(GPIO_SPI_NCS, 1);
  gpio_out(GPIO_SPI_SCLK, 0);
  gpio_out(GPIO_SPI_MOSI, 0);
}

void flash_write(int nrbytes, uint32_t addr, uint8_t *dat)
{
  int i;

  bbspi_transfer(1,0);
  bbspi_transfer(0,0x06);
  bbspi_transfer(1,0);
  bbspi_transfer(0,0x02);
  bbspi_transfer(0,(addr & 0xFF0000) >> 16);
  bbspi_transfer(0,(addr & 0xFF00) >> 8);
  bbspi_transfer(0,(addr & 0xFF));
  for ( i = 0; i < nrbytes; i++ )
  {
    bbspi_transfer(0,dat[i]);
  }
  bbspi_transfer(1,0);
}

void flash_read(int nrbytes, uint32_t addr, uint8_t *dat)
{
  int cnt = 0;
  int i;
//  d = 1;
  bbspi_transfer(1,0);
//  mprintf("cmd\n");
  bbspi_transfer(0,0x0b);
//  mprintf("addr0\n");
  bbspi_transfer(0,(addr & 0xFF0000) >> 16);
//  mprintf("addr1\n");
  bbspi_transfer(0,(addr & 0xFF00) >> 8);
//  mprintf("addr2\n");
  bbspi_transfer(0,(addr & 0xFF));
  bbspi_transfer(0,0);

  for ( i = 0; i < nrbytes; i++ )
  {
    dat[i] = bbspi_transfer(0, 0);
  }
  bbspi_transfer(1,0);
}

void flash_serase(uint32_t addr)
{
  bbspi_transfer(1,0);
  bbspi_transfer(0,0x06);
  bbspi_transfer(1,0);
  bbspi_transfer(0,0xD8);
  bbspi_transfer(0,(addr & 0xFF0000) >> 16);
  bbspi_transfer(0,(addr & 0xFF00) >> 8);
  bbspi_transfer(0,(addr & 0xFF));
  bbspi_transfer(1,0);
}

uint8_t flash_rsr()
{
  uint8_t retval;
  bbspi_transfer(1,0);
  bbspi_transfer(0,0x05);
  retval = bbspi_transfer(0,0);
  bbspi_transfer(1,0);
  return retval;
}
