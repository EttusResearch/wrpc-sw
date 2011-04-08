/*
 * Public header for the raw I/O interface for PCI or PCI express interfaces
 *
 * Copyright (C) 2010 CERN (www.cern.ch)
 * Author: Alessandro Rubini <rubini@gnudd.com>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 *
 * This work is part of the White Rabbit project, a research effort led
 * by CERN, the European Institute for Nuclear Research.
 */
#ifndef __RAWRABBIT_H__
#define __RAWRABBIT_H__
#include <linux/types.h>
#include <linux/ioctl.h>

#ifdef __KERNEL__ /* The initial part of the file is driver-internal stuff */
#include <linux/pci.h>
#include <linux/completion.h>
#include <linux/workqueue.h>
#include <linux/firmware.h>
#include <linux/wait.h>

struct rr_devsel;

struct rr_dev {
	struct rr_devsel	*devsel;
	struct pci_driver	*pci_driver;
	struct pci_device_id	*id_table;
	struct pci_dev		*pdev;		/* non-null after pciprobe */
	struct mutex		mutex;
	wait_queue_head_t	 q;
	void			*dmabuf;
	struct timespec		 irqtime;
	unsigned long		 irqcount;
	struct completion	 complete;
	struct resource		*area[3];	/* bar 0, 2, 4 */
	void			*remap[3];	/* ioremap of bar 0, 2, 4 */
	unsigned long		 flags;
	struct work_struct	work;
	const struct firmware	*fw;
	int			 usecount;
};

#define RR_FLAG_REGISTERED	0x00000001
#define RR_FLAG_IRQDISABLE	0x00000002
#define RR_FLAG_IRQREQUEST	0x00000002


#define RR_PROBE_TIMEOUT	(HZ/10)		/* for pci_register_drv */

/* These two live in ./loader.c */
extern void rr_ask_firmware(struct rr_dev *dev);
extern void rr_load_firmware(struct work_struct *work);

#endif /* __KERNEL__ */

/* By default, the driver registers for this vendor/devid */
#define RR_DEFAULT_VENDOR	0x1a39
#define RR_DEFAULT_DEVICE	0x0004

#define RR_DEFAULT_FWNAME	"rrabbit-%P-%p@%b"
#define RR_MAX_FWNAME_SIZE	64
#define RR_DEFAULT_BUFSIZE	(1<<20)		/* 1MB */
#define RR_PLIST_SIZE		4096		/* no PAGE_SIZE in user space */
#define RR_PLIST_LEN		(RR_PLIST_SIZE / sizeof(void *))
#define RR_MAX_BUFSIZE		(RR_PLIST_SIZE * RR_PLIST_LEN)


/* This structure is used to select the device to be accessed, via ioctl */
struct rr_devsel {
	__u16 vendor;
	__u16 device;
	__u16 subvendor;	/* RR_DEVSEL_UNUSED to ignore subvendor/dev */
	__u16 subdevice;
	__u16 bus;		/* RR_DEVSEL_UNUSED to ignore bus and devfn */
	__u16 devfn;
};

#define RR_DEVSEL_UNUSED	0xffff

/* Offsets for BAR areas in llseek() and/or ioctl */
#define RR_BAR_0		0x00000000
#define RR_BAR_2		0x20000000
#define RR_BAR_4		0x40000000
#define RR_BAR_BUF		0xc0000000	/* The DMA buffer */
#define RR_IS_DMABUF(addr)	((addr) >= RR_BAR_BUF)
#define __RR_GET_BAR(x)		((x) >> 28)
#define __RR_SET_BAR(x)		((x) << 28)
#define __RR_GET_OFF(x)		((x) & 0x0fffffff)

static inline int rr_is_valid_bar(unsigned long address)
{
	int bar = __RR_GET_BAR(address);
	return bar == 0 || bar == 2 || bar == 4 || bar == 0x0c;
}

static inline int rr_is_dmabuf_bar(unsigned long address)
{
	int bar = __RR_GET_BAR(address);
	return bar == 0x0c;
}

struct rr_iocmd {
	__u32 address; /* bar and offset */
	__u32 datasize; /* 1 or 2 or 4 or 8 */
	union {
		__u8 data8;
		__u16 data16;
		__u32 data32;
		__u64 data64;
	};
};

/* ioctl commands */
#define __RR_IOC_MAGIC '4' /* random or so */

#define RR_DEVSEL	 _IOW(__RR_IOC_MAGIC, 0, struct rr_devsel)
#define RR_DEVGET	 _IOR(__RR_IOC_MAGIC, 1, struct rr_devsel)
#define RR_READ		_IOWR(__RR_IOC_MAGIC, 2, struct rr_iocmd)
#define RR_WRITE	 _IOW(__RR_IOC_MAGIC, 3, struct rr_iocmd)
#define RR_IRQWAIT	  _IO(__RR_IOC_MAGIC, 4)
#define RR_IRQENA	  _IO(__RR_IOC_MAGIC, 5)
#define RR_GETDMASIZE	  _IO(__RR_IOC_MAGIC, 6)
/* #define RR_SETDMASIZE	  _IO(__RR_IOC_MAGIC, 7, unsigned long) */
#define RR_GETPLIST	  _IO(__RR_IOC_MAGIC, 8) /* returns a whole page */


#define VFAT_IOCTL_READDIR_BOTH         _IOR('r', 1, struct dirent [2])

/* Registers from the gennum header files */
enum {
	FCL_BASE	= 0xB00,
	FCL_CTRL	= FCL_BASE,
	FCL_STATUS	= FCL_BASE + 0x4,
	FCL_IODATA_IN	= FCL_BASE + 0x8,
	FCL_IODATA_OUT	= FCL_BASE + 0xC,
	FCL_EN		= FCL_BASE + 0x10,
	FCL_TIMER_0	= FCL_BASE + 0x14,
	FCL_TIMER_1	= FCL_BASE + 0x18,
	FCL_CLK_DIV	= FCL_BASE + 0x1C,
	FCL_IRQ		= FCL_BASE + 0x20,
	FCL_TIMER_CTRL	= FCL_BASE + 0x24,
	FCL_IM		= FCL_BASE + 0x28,
	FCL_TIMER2_0	= FCL_BASE + 0x2C,
	FCL_TIMER2_1	= FCL_BASE + 0x30,
	FCL_DBG_STS	= FCL_BASE + 0x34,
	FCL_FIFO	= 0xE00,
	PCI_SYS_CFG_SYSTEM = 0x800
};

#endif /* __RAWRABBIT_H__ */

