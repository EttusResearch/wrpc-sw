#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include <rawrabbit.h>
#include "rr_io.h"

#define DEVNAME "/dev/rawrabbit"

static int fd;

int rr_init()
{
	struct rr_devsel devsel;

	int ret = -EINVAL;

	fd = open(DEVNAME, O_RDWR);
	if (fd < 0) {
		return -1;
	}

	return 0;
}

int rr_writel(uint32_t data, uint32_t addr)
{
	struct rr_iocmd iocmd;
	iocmd.datasize = 4;
	iocmd.address = addr;
	iocmd.address |= __RR_SET_BAR(0);
	iocmd.data32 = data;
	ioctl(fd, RR_WRITE, &iocmd);
}

uint32_t rr_readl(uint32_t addr)
{
	struct rr_iocmd iocmd;
	iocmd.datasize = 4;
	iocmd.address = addr;
	iocmd.address |= __RR_SET_BAR(0);
	if(ioctl(fd, RR_READ, &iocmd) < 0) perror("ioctl");
	return iocmd.data32;
}

static void gennum_writel(uint32_t data, uint32_t addr)
{
	struct rr_iocmd iocmd;
	iocmd.datasize = 4;
	iocmd.address = addr;
	iocmd.address |= __RR_SET_BAR(4);
	iocmd.data32 = data;
	ioctl(fd, RR_WRITE, &iocmd);
}

static uint32_t gennum_readl(uint32_t addr)
{
	struct rr_iocmd iocmd;
	iocmd.datasize = 4;
	iocmd.address = addr;
	iocmd.address |= __RR_SET_BAR(4);
	ioctl(fd, RR_READ, &iocmd);
	return iocmd.data32;
}

static inline int64_t get_tics()
{
    struct timezone tz= {0,0};
    struct timeval tv;
    gettimeofday(&tv, &tz);
    return (int64_t)tv.tv_sec * 1000000LL + (int64_t) tv.tv_usec;
}

int rr_load_bitstream(const void *data, int size8)
{
	int i, ctrl, done = 0, wrote = 0;
	unsigned long j;
	uint8_t val8;
	const uint8_t *data8 = data;
	const uint32_t *data32 = data;
	int size32 = (size8 + 3) >> 2;


//        fprintf(stderr,"Loading %d bytes...\n", size8);

	if (1) {
		/*
		 * Hmmm.... revers bits for xilinx images?
		 * We can't do in kernel space anyways, as the pages are RO
		 */
		uint8_t *d8 = (uint8_t *)data8; /* Horrible: kill const */
		for (i = 0; i < size8; i++) {
			val8 = d8[i];
			d8[i] =  0
				| ((val8 & 0x80) >> 7)
				| ((val8 & 0x40) >> 5)
				| ((val8 & 0x20) >> 3)
				| ((val8 & 0x10) >> 1)
				| ((val8 & 0x08) << 1)
				| ((val8 & 0x04) << 3)
				| ((val8 & 0x02) << 5)
				| ((val8 & 0x01) << 7);
		}
	}

	/* Do real stuff */
	gennum_writel(0x00, FCL_CLK_DIV);
	gennum_writel(0x40, FCL_CTRL); /* Reset */
	i = gennum_readl( FCL_CTRL);
	if (i != 0x40) {
		fprintf(stderr, "%s: %i: error\n", __func__, __LINE__);
		return;
	}
	gennum_writel(0x00,  FCL_CTRL);

	gennum_writel(0x00,  FCL_IRQ); /* clear pending irq */

	switch(size8 & 3) {
	case 3: ctrl = 0x116; break;
	case 2: ctrl = 0x126; break;
	case 1: ctrl = 0x136; break;
	case 0: ctrl = 0x106; break;
	}
	gennum_writel(ctrl,  FCL_CTRL);

	gennum_writel(0x00,  FCL_CLK_DIV); /* again? maybe 1 or 2? */

	gennum_writel(0x00,  FCL_TIMER_CTRL); /* "disable FCL timer func" */

	gennum_writel(0x10,  FCL_TIMER_0); /* "pulse width" */
	gennum_writel(0x00,  FCL_TIMER_1);

	/* Set delay before data and clock is applied by FCL after SPRI_STATUS is
		detected being assert.
	*/
	gennum_writel(0x08,  FCL_TIMER2_0); /* "delay before data/clock..." */
	gennum_writel(0x00,  FCL_TIMER2_1);
	gennum_writel(0x17,  FCL_EN); /* "output enable" */

	ctrl |= 0x01; /* "start FSM configuration" */
	gennum_writel(ctrl,  FCL_CTRL);

	while(size32 > 0)
	{


		/* Check to see if FPGA configuation has error */
		i = gennum_readl( FCL_IRQ);
		if ( (i & 8) && wrote) {
			done = 1;
			fprintf(stderr,"EarlyDone");
//			fprintf(stderr,"%s: %idone after %i\n", __func__, __LINE__,				wrote);
		} else if ( (i & 0x4) && !done) {
			fprintf(stderr,"Error after %i\n", wrote);
			return -1;
		}

   // fprintf(stderr,".");
    
		while(gennum_readl(FCL_IRQ) & (1<<5)); // wait until at least 1/2 of the FIFO is empty
		
		/* Write 64 dwords into FIFO at a time. */
		for (i = 0; size32 && i < 32; i++) {
			gennum_writel(*data32,  FCL_FIFO);
			data32++; size32--; wrote++;
//			udelay(20);
		}
	}

	gennum_writel(0x186,  FCL_CTRL); /* "last data written" */
	int64_t tstart = get_tics();
	//j = jiffies + 2 * HZ;
	/* Wait for DONE interrupt  */
	while(!done) {
	//	fprintf(stderr,stderr, "Wait!");
	
		i = gennum_readl( FCL_IRQ);
		printf("irqr: %x %x\n", FCL_IRQ, i);
		if (i & 0x8) {
			fprintf(stderr,"done after %i\n",       wrote);
			done = 1;
		} else if( (i & 0x4) && !done) {
			fprintf(stderr,"Error after %i\n",   wrote);
			return -1;
		}
		usleep(10000);
		
		if(get_tics() - tstart > 1000000LL)
		{
		    fprintf(stderr,"Loader: DONE timeout. Did you choose proper bitgen options?\n");
		    return;
		}
		/*if (time_after(jiffies, j)) {
			printk("%s: %i: tout after %i\n", __func__, __LINE__,
			       wrote);
			return;
		} */
	}
	return done?0:-1;
}

int rr_load_bitstream_from_file(const char *file_name)
{
    uint8_t *buf;
    FILE *f;
    uint32_t size;
    
    f=fopen(file_name,"rb");
    if(!f) return -1;
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    buf = malloc(size);
    if(!buf)
    {
        fclose(f);
        return -1;
    }
    fseek(f, 0, SEEK_SET);
    fread(buf, 1, size, f);
    fclose(f);
    int rval = rr_load_bitstream(buf, size);
    free(buf);
    return rval;
}