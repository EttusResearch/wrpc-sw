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

#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<sys/ioctl.h>
#include"rawrabbit.h"

#define DEVNAME "/dev/rawrabbit"
#define RST_ADDR 0xE2000

int rst_zpu(int spec, int rst);
int copy(int spec, int srcbin, unsigned int baseaddr);
int verify(int spec, int srcbin, unsigned int baseaddr);
int conv_endian(int x);
int dump_to_file(int spec, char *filename, unsigned int baseaddr);

int main(int argc, char **argv)
{
  unsigned int addr = 0x80000;
  int spec, srcbin;
  unsigned int bytes;
  char *dumpfile =NULL;

  if(argc<2)
  {
    fprintf(stderr, "No parameters specified !\n");
    fprintf(stderr, "Usage:\n\t./%s [-r] <binary file> [base address]\n", argv[0]);
    fprintf(stderr, "-r options dumps the memory contents to a given file.\n\n");
    return -1;
  }

  if(!strcmp(argv[1], "-r"))
   		dumpfile = argv[2];
	else if(argc==3)
    addr = atoi(argv[2]);
   		


  spec = open(DEVNAME, O_RDWR);
  if(spec < 0)
  {
    perror("Could not open device");
    return -1;
  }

	if(dumpfile)
	{
	 	dump_to_file(spec, dumpfile, addr);
	 	return 0;
	}

  srcbin = open(argv[1], O_RDONLY);
  if(srcbin < 0)
  {
    perror("Could not open binary file");
    close(spec);
    return -1;
  }

  rst_zpu(spec, 1);
  bytes = copy(spec, srcbin, addr);
  if(bytes < 0)
  {
    close(srcbin);
    close(spec);
    return -1;
  }
  printf("Wrote %u bytes\n", bytes);
  verify(spec, srcbin, addr);

  rst_zpu(spec, 0);

  close(srcbin);
  close(spec);

  return 0;
}

int copy(int spec, int srcbin, unsigned int baseaddr)
{
  unsigned int bytes, word;
  struct rr_iocmd iocmd;
  int ret;

  printf("Writing memory: ");
  bytes=0;
  while(1)
  {
    ret = read(srcbin, &word, 4); /*read 32-bit word*/
    if(ret<0)
    {
      perror("Error while reading binary file");
      return -1;
    }
    else if(ret==0)
    {
      printf("Done\n");
      break;
    }

    iocmd.address = baseaddr+bytes;
    bytes += ret;                     //address shift for next write
    iocmd.address |= __RR_SET_BAR(0); //bar0
    iocmd.datasize = 4;
    iocmd.data32 = conv_endian(word);
    ret = ioctl(spec, RR_WRITE, &iocmd);
    if(ret<0)
    {
      perror("Error while writing to SPEC");
      return -1;
    }
    printf(".");
  }

  return bytes;
}


int dump_to_file(int spec, char *filename, unsigned int baseaddr)
{
  unsigned int bytes, word;
  struct rr_iocmd iocmd;
  int ret;
  FILE *f= fopen(filename,"wb");
  
  if(!f)
  return -1;

  bytes=0;
  while(bytes < 0x10000)
  {

    iocmd.address = baseaddr+bytes;
  //  bytes += ret;                     //address shift for next write
    iocmd.address |= __RR_SET_BAR(0); //bar0
    iocmd.datasize = 4;
    iocmd.data32 = 0;
    ret = ioctl(spec, RR_READ, &iocmd);
	word = conv_endian(iocmd.data32);
	fwrite(&word, 4, 1, f);
	
	bytes+=4;

  }

	fclose(f);
}


int verify(int spec, int srcbin, unsigned int baseaddr)
{
  unsigned int wbin;
  struct rr_iocmd iocmd;
  unsigned int bytes;
  int ret;

  printf("Verifing: ");

  bytes = 0;
  while(1)
  {
    ret = read(srcbin, &wbin, 4); /*read 32-bit word*/
    if(ret<0)
    {
      perror("Error while reading binary file");
      return -1;
    }
    else if(ret==0)
    {
      printf("OK !\n");
      break;
    }

    iocmd.address = baseaddr+bytes;
    bytes += ret;
    iocmd.address |= __RR_SET_BAR(0); //bar0
    iocmd.datasize = 4;
    ret = ioctl(spec, RR_READ, &iocmd);
    if(ret<0)
    {
      perror("Error while reading SPEC memory");
      return -1;
    }

    if(iocmd.data32 != conv_endian(wbin))
    {
      printf("Error (@word %u)\n", bytes/4);
      return -1;
    }
    printf(".");

    return 0;
  }

  return 0;
}

int rst_zpu(int spec, int rst)
{
  struct rr_iocmd iocmd;

  iocmd.address = RST_ADDR;
  iocmd.address |= __RR_SET_BAR(0); //bar0
  iocmd.datasize = 4;
  iocmd.data32 = rst;
  
  if( ioctl(spec, RR_WRITE, &iocmd) < 0)
  {
    perror("Could not reset ZPU");
    return -1;
  }

  return 0;
}

int conv_endian(int x)
{
  return ((x&0xff000000)>>24) + ((x&0x00ff0000)>>8) + ((x&0x0000ff00)<<8) + ((x&0x000000ff)<<24);
}
