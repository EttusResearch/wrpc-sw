/*
 *==============================================================================
 * CERN (BE-CO-HT)
 * Source file for Write data to SPEC flash chip
 *==============================================================================
 *
 * author: Theodor Stana (t.stana@cern.ch)
 *
 * date of creation: 2013-10-04
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
 *    2013-10-04   Theodor Stana     t.stana@cern.ch     File created
 *==============================================================================
 * TODO: Fix flash_rsr() issue
 *==============================================================================
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <unistd.h>

#include "flash.h"

#define SPEC_SYSCON_OFFSET 0x20400 /* from "sdb" on the shell, current gateware */

#define MAX_DEVICES		8		/* no alloc, me lazy */

static struct spec_pci_id {
	unsigned pci_v;
	unsigned pci_d;
} spec_devices[] = {
	{ 0x10dc /* CERN   */, 0x018d /* SPEC   */ },
	{ 0x1a39 /* Gennum */, 0x0004 /* GN4124 */ },
	{ 0, },
};

struct spec_device {
	void *mapaddr;
	int mapsize;
	int busid;
};

static struct spec_device devs[MAX_DEVICES];

char *prgname;
int verbose;

extern void *BASE_SYSCON;


static int spec_write_flash(struct spec_device *spec, int addr, int len)
{
	uint8_t buf[len];
	int i, r;

	BASE_SYSCON = spec->mapaddr + SPEC_SYSCON_OFFSET;

	flash_init();

	if (verbose) {
		fprintf(stderr, "Writing device on bus %i: "
			"offset %i (0x%X), len %i\n", spec->busid,
			addr, addr, len);
	}

	if (isatty(fileno(stdin)))
		fprintf(stderr, "Reading from stdin, please type the data\n");
	i = fread(buf, 1, len, stdin);
	if (i != len) {
		fprintf(stderr, "%s: read error (%i, expeted %i)\n", prgname,
			i, len);
		return 1;
	}

	while (len)
	{
		int j;
		/* Set write length */
		i = len;
		if (len > 256)
			i = 256;

		/* Erase if sector boundary */
		if ((addr % 0x10000) == 0)
		{
			if (verbose)
			{
				fprintf(stderr, "Erasing at address 0x%06X\n",
					addr);
			}
			flash_serase(addr);
			sleep(1);
			/* FIXME: Can't understand why rsr is not working... */
//			r = 0x01;
//			while (r & 0x01)
//			{
//				r = flash_rsr();
//				printf("%d\n", r);
//			}
		}

		/* Write to flash */
		if (verbose)
		{
			fprintf(stderr, "Writing %3i bytes at address 0x%06X\n",
				i, addr);
		}
		flash_write(addr, buf, i);
		sleep(1);

		/* FIXME: As above, RSR is a mistery... */
//		while (flash_rsr() & 0x01)
//			;

		/* Setup next length and address */
		len  -= i;
		addr += i;
		memcpy(buf, buf+i, len % 256);

//		if (i != len) {
//			fprintf(stderr, "Tried to write %i bytes, retval %i\n",
//				len, i);
//			return 1;
//		}
	}

	return 0;

}

/*
 * What follows is mostly generic, should be librarized in a way
 */

/* Access a PCI device, mmap and so on */
static void *spec_access_pci(char *name, int index)
{
	struct spec_device *dev = devs + index;
	char path[PATH_MAX];
	struct stat stbuf;
	int fd;

	memset(dev, 0, sizeof(*dev));
	sprintf(path, "/sys/bus/pci/devices/%s/resource0", name);
	if ((fd = open(path, O_RDWR | O_SYNC)) < 0) {
		fprintf(stderr, "%s: %s: %s\n", prgname, path,
			strerror(errno));
		return NULL;
	}
	fstat(fd, &stbuf);
	dev->mapsize = stbuf.st_size;
	dev->mapaddr = mmap(0, stbuf.st_size,
			    PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);
	if (dev->mapaddr == MAP_FAILED) {
		fprintf(stderr, "%s: mmap(%s): %s\n", prgname, path,
			strerror(errno));
		return NULL;
	}
	if (sscanf(name, "%*x:%x", &dev->busid) != 1)
		return NULL;
	return dev->mapaddr;
}

/* Scan PCI space for vendor and device; return number of successes */
static int spec_scan_pci(struct spec_pci_id *id, struct spec_device *arr,
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
		if (verbose)
			fprintf(stderr, "%s: found device %04x:%04x: %s\n",
				prgname, v, d, namelist[i]->d_name);

		if (ndevs == alen) {
			fprintf(stderr, "%s: array overflow, ignoring card\n",
				prgname);
			continue;
		}
		if (spec_access_pci(namelist[i]->d_name, ndevs) == NULL)
			continue;
		ndevs++;
	}
	return ndevs;
}

static int help(void)
{
	fprintf(stderr, "%s: Use: \"%s [-v] [-b <bus>] <addr> <len>\n",
		prgname, prgname);
	return 1;
}

int main(int argc, char **argv)
{
	int ndev, i, c, bus = -1;
	struct spec_device *spec = NULL;
	prgname = argv[0];

	while ((c = getopt(argc, argv, "b:v")) != -1) {
		switch(c) {
		case 'b':
			sscanf(optarg, "%i", &bus);
			break;
		case 'v':
			verbose++;
			break;
		default:
			exit(help());
		}
	}
	if (optind != argc - 2)
		exit(help());

	/* find which one to use */
	ndev = spec_scan_pci(spec_devices, devs, MAX_DEVICES);
	if (ndev < 1) {
		fprintf(stderr, "%s: no suitable PCI devices\n", prgname);
		exit(1);
	}
	if (bus == -1 && ndev == 1) {
		spec = devs;
	} else if (bus == -1) {
		fprintf(stderr, "%s: several devices found, please choose:\n",
			prgname);
		for (i = 0; i < ndev; i++) {
			fprintf(stderr, "   -b %i\n", devs[i].busid);
		}
		exit(1);
	} else {
		for (i = 0; i < ndev; i++)
			if (bus == devs[i].busid)
				break;
		if (i == ndev) {
			fprintf(stderr, "%s: no device on bus %i\n", prgname,
				bus);
			exit(1);
		}
		spec = devs + i;
	}

	i = spec_write_flash(spec, atoi(argv[optind]), atoi(argv[optind + 1]));
	return i;
}
