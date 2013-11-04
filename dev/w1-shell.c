/*
 * Onewire generic interface
 * Alessandro Rubini, 2013 GNU GPL2 or later
 */
#include <wrc.h>
#include <shell.h>
#include <w1.h>

#define BLEN 32

/* A shell command, for testing write: "w1w <offset> <byte> [<byte> ...]" */
static int cmd_w1_w(const char *args[])
{
	int offset, i, blen;
	unsigned char buf[BLEN];

	if (!args[0] || !args[1])
		return -1;
	offset = atoi(args[0]);
	for (i = 1, blen = 0; args[i] && blen < BLEN; i++, blen++) {
		buf[blen] = atoi(args[i]);
		pp_printf("offset %4i (0x%03x): %3i (0x%02x)\n",
			  offset + blen, offset + blen, buf[blen], buf[blen]);
	}
	i = w1_write_eeprom_bus(&wrpc_w1_bus, offset, buf, blen);
	pp_printf("write(0x%x, %i): result = %i\n", offset, blen, i);
	return i == blen ? 0 : -1;
}

DEFINE_WRC_COMMAND(w1w) = {
	.name = "w1w",
	.exec = cmd_w1_w,
};

/* A shell command, for testing read: "w1r <offset> <len> */
static int cmd_w1_r(const char *args[])
{
	int offset, i, blen;
	unsigned char buf[BLEN];

	if (!args[0] || !args[1])
		return -1;
	offset = atoi(args[0]);
	blen = atoi(args[1]);
	if (blen > BLEN)
		blen = BLEN;
	i = w1_read_eeprom_bus(&wrpc_w1_bus, offset, buf, blen);
	pp_printf("read(0x%x, %i): result = %i\n", offset, blen, i);
	if (i <= 0 || i > blen) return -1;
	for (blen = 0; blen < i; blen++) {
		pp_printf("offset %4i (0x%03x): %3i (0x%02x)\n",
			  offset + blen, offset + blen, buf[blen], buf[blen]);
	}
	return i == blen ? 0 : -1;
}

DEFINE_WRC_COMMAND(w1r) = {
	.name = "w1r",
	.exec = cmd_w1_r,
};

/* A shell command, for checking */
static int cmd_w1(const char *args[])
{
	int i;
	struct w1_dev *d;
	int32_t temp;

	w1_scan_bus(&wrpc_w1_bus);
	for (i = 0; i < W1_MAX_DEVICES; i++) {
		d = wrpc_w1_bus.devs + i;
		if (d->rom) {
			pp_printf("device %i: %08x%08x\n", i,
				  (int)(d->rom >> 32), (int)d->rom);
		temp = w1_read_temp(d, 0);
		pp_printf("temp: %d.%04d\n", temp >> 16,
			  (int)((temp & 0xffff) * 10 * 1000 >> 16));
		}
	}
	return 0;
}

DEFINE_WRC_COMMAND(w1) = {
	.name = "w1",
	.exec = cmd_w1,
};

