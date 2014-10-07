/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2014 CERN (www.cern.ch)
 * Author: Alessandro Rubini <rubini@gnudd.com>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <wrc.h>
#define SDBFS_BIG_ENDIAN
#include <libsdbfs.h>

/* The following pointers are exported */
unsigned char *BASE_MINIC;
unsigned char *BASE_EP;
unsigned char *BASE_SOFTPLL;
unsigned char *BASE_PPS_GEN;
unsigned char *BASE_SYSCON;
unsigned char *BASE_UART;
unsigned char *BASE_ONEWIRE;
unsigned char *BASE_ETHERBONE_CFG;

/* The sdb filesystem itself */
static struct sdbfs wrc_fpga_sdb = {
	.name = "fpga-area",
	.blocksize = 1, /* Not currently used */
	.entrypoint = SDB_ADDRESS,
	.data = 0,
	.flags = SDBFS_F_ZEROBASED,
};

/* called by outside, at boot time */
void sdb_print_devices(void)
{
	struct sdb_device *d;
	int new = 1;
	while ( (d = sdbfs_scan(&wrc_fpga_sdb, new)) != NULL) {
		/*
		 * "%.19s" is not working for XINT printf, and zeroing
		 * d->sdb_component.product.record_type won't work, as
		 * the device is read straight from fpga ROM registers
		 */
		const int namesize = sizeof(d->sdb_component.product.name);
		char name[namesize + 1];

		memcpy(name, d->sdb_component.product.name, sizeof(name));
		name[namesize] = '\0';
		pp_printf("dev  0x%08lx @ %06lx, %s\n",
			  (long)(d->sdb_component.product.device_id),
			  wrc_fpga_sdb.f_offset, name);
		new = 0;
	}
}

struct wrc_device {
	unsigned char **base;
	/* uint64_t vid; -- lazily, we know it's CERN always */
	uint32_t did;
};
#define VID_CERN	0x0000ce42LL
#define VID_GSI		0x00000651LL

struct wrc_device devs[] = {
	{&BASE_MINIC,         0xab28633a},
	{&BASE_EP,            0x650c2d4f},
	{&BASE_SOFTPLL,       0x65158dc0},
	{&BASE_PPS_GEN,       0xde0d8ced},
	{&BASE_SYSCON,        0xff07fc47},
	{&BASE_UART,          0xe2d13d04},
	{&BASE_ONEWIRE,       0x779c5443},
	{&BASE_ETHERBONE_CFG, 0x68202b22},
};

void sdb_find_devices(void)
{
	struct wrc_device *d;
	static int done;
	int i;

	if (!done) {
		sdbfs_dev_create(&wrc_fpga_sdb);
		done++;
	}
	for (d = devs, i = 0; i < ARRAY_SIZE(devs); d++, i++) {
		*(d->base) = (void *)sdbfs_find_id(&wrc_fpga_sdb,
						   VID_CERN, d->did);
	}
}
