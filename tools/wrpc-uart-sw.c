/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2013 CERN (www.cern.ch)
 * Author: Alessandro Rubini <rubini@gnudd.com>
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pty.h>
#include <arpa/inet.h> /* ntohl etc */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/select.h>

#include "../include/uart-sw.h"

/*
 * The following parameters match the spec, but can be changed on env/cmdline
 */
#define DEFAULT_BUS		-1		/* all */
#define DEFAULT_BAR		0		/* first bar */
#define DEFAULT_RAMADDR		0		/* beginning */
#define DEFAULT_RAMSIZE		0x16000		/* 88k, default in spec */

#define MAX_DEVICES		8		/* no alloc, me lazy */

static struct usw_pci_id {
	unsigned pci_v;
	unsigned pci_d;
} spec_devices[] = {
	{ 0x10dc /* CERN   */, 0x018d /* SPEC   */ },
	{ 0x1a39 /* Gennum */, 0x0004 /* GN4124 */ },
	{ 0, },
};

struct usw_device {
	void *mapaddr;
	int mapsize;
	struct wrc_uart_sw *p;
	uint16_t nreturned;
	int fd;
};

static struct usw_device devs[MAX_DEVICES];

char *prgname;

/* Prepare communication according to user settings (currently pty only) */
static int usw_prepare_io(struct usw_device *dev)
{
	char path[PATH_MAX];
	int fdm, fds;

	if (openpty(&fdm, &fds, path, NULL, NULL) < 0) {
		fprintf(stderr, "%s: openpty(): %s\n", prgname,
			strerror(errno));
		return -1;
	}
	dev->fd = fdm;
	close(fds);
	printf("Opened \"%s\"; use screen or minicom\n", path);
	return 0;
}

/* Open a uart-sw device, with an already-prepared mmap */
static int usw_open_device(char *name, struct usw_device *dev)
{
	int i;
	uint32_t *ptr;

	for (i = 0; i < dev->mapsize; i += 16) {
		ptr = dev->mapaddr + i;
		if (*ptr == UART_SW_MAGIC)
			break;
	}
	if (i == dev->mapsize) {
		fprintf(stderr, "%s: no uart-sw in device %s\n", prgname,
			name);
		return -1;
	}
	fprintf(stderr, "%s: %s: found uart-sw at 0x%x\n", prgname, name, i);
	dev->p = dev->mapaddr + i;

	return usw_prepare_io(dev);
}


/* Access a PCI device, mmap and so on */
static int usw_access_pci(char *name, int index)
{
	struct usw_device *dev = devs + index;
	char path[PATH_MAX];
	struct stat stbuf;
	int fd;

	memset(dev, 0, sizeof(*dev));
	sprintf(path, "/sys/bus/pci/devices/%s/resource0", name);
	if ((fd = open(path, O_RDWR | O_SYNC)) < 0) {
		fprintf(stderr, "%s: %s: %s\n", prgname, path,
			strerror(errno));
		return -1;
	}
	fstat(fd, &stbuf);
	dev->mapsize = stbuf.st_size;
	dev->mapaddr = mmap(0, stbuf.st_size,
			    PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (dev->mapaddr == MAP_FAILED) {
		fprintf(stderr, "%s: mmap(%s): %s\n", prgname, path,
			strerror(errno));
		return -1;
	}
	return usw_open_device(name, dev);
}


/* Scan PCI space for vendor and device; return number of successes */
static int usw_scan_pci(struct usw_pci_id *id, struct usw_device *arr,
			int alen)
{
	char path[PATH_MAX];
	FILE *f;
	struct dirent **namelist;
	int i, j, n, ndevs;
	unsigned v, d;

	n = scandir("/sys/bus/pci/devices", &namelist, 0, 0);
	if (n < 0) {
		fprintf(stderr, "%s: /sys/bus/pci/devices: %s\n", prgname,
			strerror(errno));
		return -1;
	}

	for (i = ndevs = 0; i < n; i++) {
		if (namelist[i]->d_name[0] == '.')
			continue;
		/* check vendor */
		sprintf(path, "/sys/bus/pci/devices/%s/vendor",
			namelist[i]->d_name);
		f = fopen(path, "r");
		if (!f) {
			fprintf(stderr, "%s: %s: %s\n", prgname, path,
				strerror(errno));
			continue;
		}
		if (fscanf(f, "%i", &v) != 1)
			continue;
		fclose(f);

		/* check device */
		sprintf(path, "/sys/bus/pci/devices/%s/device",
			namelist[i]->d_name);
		f = fopen(path, "r");
		if (!f) {
			fprintf(stderr, "%s: %s: %s\n", prgname, path,
				strerror(errno));
			continue;
		}
		if (fscanf(f, "%i", &d) != 1)
			continue;
		fclose(f);

		for (j = 0; id[j].pci_v; j++)
			if (id[j].pci_v == v && id[j].pci_d == d)
				break;
		if (!spec_devices[j].pci_v)
			continue; /* not found in whole array */

		/* Ok, so this is ours. Celebrate, and open it */
		fprintf(stderr, "%s: found device %04x:%04x: %s\n", prgname,
			v, d, namelist[i]->d_name);

		if (ndevs == alen) {
			fprintf(stderr, "%s: array overflow, ignoring card\n",
				prgname);
			continue;
		}
		if (usw_access_pci(namelist[i]->d_name, ndevs))
			continue;
		ndevs++;
	}
	return ndevs;
}

/* Low-level shmem uart: output if outc >= 0; return -1 or input char */
static int usw_ll(struct usw_device *dev, int outc)
{
	struct wrc_uart_sw local_uart;
	uint32_t *ptr = (void *)&local_uart;
	uint32_t *hwptr = (void *)dev->p;
	int i, n, pos;

	if (outc >= 0) {
		/* FIXME: output */
	}

	/* The shmem is word-mapped but big-endian. So convert (inefficient) */
	for (i = 0; i < sizeof(local_uart) / sizeof(*ptr); i++)
		ptr[i] = ntohl(hwptr[i]);

	if (outc >= 0) {
		/* This is horrible, because of dual endian conversions */
		n = ntohs(local_uart.nread) % CONFIG_UART_SW_RSIZE;
		local_uart.rbuffer[n] = outc;
		pos = (offsetof(struct wrc_uart_sw, rbuffer) + n) / 4;
		hwptr[pos] = htonl(ptr[pos]);
		local_uart.nread = htons(ntohs(local_uart.nread) + 1);
		pos = offsetof(struct wrc_uart_sw, nread) / 4;
		hwptr[pos] = htonl(ptr[pos]);
	}
	if (ntohs(local_uart.nwritten) < dev->nreturned)
		dev->nreturned = 0; /* reloaded */
	if (ntohs(local_uart.nwritten) > dev->nreturned) {
		i = dev->nreturned % CONFIG_UART_SW_WSIZE;
		i = local_uart.wbuffer[i];
		dev->nreturned++;
		return i;
	}
	return -1;
}

/* read/write the file descriptor, polling the low-level uart */
static int usw_do_io(struct usw_device *dev, int ready)
{
	unsigned char buf[1]; /* oh so tiny */
	int char_i, char_o;

	char_o = -1;
	if (ready) {
		if (read(dev->fd, buf, 1) != 1)
			/* error */;
		char_o = buf[0];
	}
	while ((char_i = usw_ll(dev, char_o)) >= 0) {
		buf[0] = char_i;
		write(dev->fd, buf, 1);
		char_o = -1;
	}
	return 0;
}

int main(int argc, char **argv)
{
	int maxfd, ndev, i;
	struct timeval tv = {0,};
	fd_set set, fullset;

	prgname = argv[0];

	/* FIXME: parse commandline arguments */

	ndev = usw_scan_pci(spec_devices, devs, MAX_DEVICES);
	if (ndev < 1) {
		fprintf(stderr, "%s: no suitable PCI devices\n", argv[0]);
		exit(1);
	}

	FD_ZERO(&fullset);
	maxfd = 0;
	for (i = 0; i < ndev; i++) {
		FD_SET(devs[i].fd, &fullset);
		if (devs[i].fd > maxfd)
			maxfd = devs[i].fd;
	}
	while (1) {
		set = fullset;
		tv.tv_usec = 50 * 1000;

		i = select(maxfd + 1, &set, NULL, NULL, &tv);
		if (i < 0)
			continue;
		for (i = 0; i < ndev; i++)
			usw_do_io(devs + i, FD_ISSET(devs[i].fd, &set));
	}
}
