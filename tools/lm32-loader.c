/* 
 * Copyright (c) 2011 Grzegorz Daniluk <g.daniluk@elproma.com.pl>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* 
 * zpu-loader is a simple program created for White Rabbit PTP Core(WRPC).
 * It loads the zpu firmware, given as the argument, directly to a dual-port RAM,
 * which is the part of WRPC. This way, there is no need to synthesize whole
 * FPGA project if one change anything in ZPU firmware.
 * The binary file taken by zpu-loader is the result of the following action:
 *
 *        ${CC} -o $(OUTPUT).elf $(OBJS) $(LDFLAGS) 
 *        ${OBJCOPY} -O binary $(OUTPUT).elf $(OUTPUT).bin
 *
 * zpu-loader uses rawrabbit kernel driver written by Alessandro Rubini.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define BASE_PCIE 0x80000
#define BASE_MBONE 0x00000

#define RST_ADDR 0x20400
#define MEM_ADDR 0x0

#include "rr_io.h"
#include "minibone_lib.h"

static void *mb_handle = NULL;

static void spec_writel(uint32_t value, uint32_t addr)
{
  if(mb_handle)
    mbn_writel(mb_handle, value, (BASE_MBONE + addr)>>2);
  else
    rr_writel(value, BASE_PCIE + addr);
}

static uint32_t spec_readl(uint32_t addr)
{
  uint32_t rval;
  if(mb_handle)
    rval = mbn_readl(mb_handle, (BASE_MBONE + addr)>>2);
  else
    rval = rr_readl(BASE_PCIE + addr);

  return rval;

}

int conv_endian(int x)
{
  return ((x&0xff000000)>>24) + ((x&0x00ff0000)>>8) + ((x&0x0000ff00)<<8) + ((x&0x000000ff)<<24);
}

void rst_lm32(int rst)
{
  spec_writel(rst ? 0x1deadbee : 0x0deadbee, RST_ADDR);
}

void copy_lm32(uint32_t *buf, int buf_nwords, uint32_t base_addr)
{
  int i;
  printf("Writing memory: ");

  for(i=0;i<buf_nwords;i++)
  {
    spec_writel(conv_endian(buf[i]), base_addr + i *4);
    if(!(i & 0xfff)) { printf("."); fflush(stdout); }
  }	

  printf("\nVerifing memory: ");

  for(i=0;i<buf_nwords;i++)
  {
    uint32_t x = spec_readl(base_addr+ i*4);
    if(conv_endian(buf[i]) != x)
    {
      printf("Verify failed (%x vs %x)\n", conv_endian(buf[i]), x);
      return ;
    }

    if(!(i & 0xfff)) { printf("."); fflush(stdout); }
  }	

  printf("OK.\n");
}

int main(int argc, char **argv)
{
  int num_words;
  uint32_t *buf;
  uint8_t target_mac[6];
  FILE *f;
  char if_name[16];

  if(argc<2)
  {
    fprintf(stderr, "No parameters specified !\n");
    fprintf(stderr, "Usage:\n\t%s <binary file> [-m network_if mac_addr]\n", argv[0]);
    fprintf(stderr, "By default the loader assumes that the card is in a PCIe slot. \n");
    fprintf(stderr, "-m option enables remove programming via ethernet. \n");
    return -1;
  }

  if(argc >= 4 && !strcmp(argv[2], "-m"))
  {
    sscanf(argv[3], "%s",if_name);
    sscanf(argv[4], "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx", &target_mac[0], &target_mac[1], &target_mac[2], &target_mac[3], &target_mac[4], &target_mac[5]);
    mb_handle = mbn_open(if_name, target_mac);
    if(!mb_handle)
    {
      fprintf(stderr,"Connection failed....\n");
      return -1;
    }
  } else if( rr_init() < 0)
  {
    fprintf(stderr,"Can't initialize rawrabbit :(\n");
    return -1;
  }


  f=fopen(argv[1],"rb");
  if(!f)
  {
    fprintf(stderr, "Input file not found.\n");
    return -1;
  }	

  fseek(f, 0, SEEK_END);
  int size = ftell(f);
  rewind(f);

  buf = malloc(size + 4);
  fread(buf, 1, size, f);
  fclose(f);

  rst_lm32(1);
  rst_lm32(1);
  rst_lm32(1);
  rst_lm32(1);
  copy_lm32(buf, (size + 3) / 4, 0);
  rst_lm32(0);

  if(mb_handle) 
  {
    mbn_stats(mb_handle);
    mbn_close(mb_handle);
  }

  return 0;
}

