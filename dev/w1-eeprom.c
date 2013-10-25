/*
 * Eeprom support (family 0x43)
 * Cesar Prados, Alessandro Rubini, 2013. GNU GPL2 or later
 */
#include <stdlib.h>
#include <string.h>
#include <w1.h>

#ifndef EXTERNAL_W1
#include <wrc.h>
#include <shell.h>
#else
#include <unistd.h>
#endif

#define LSB_ADDR(X) ((X) & 0xFF)
#define MSB_ADDR(X) (((X) & 0xFF00)>>8)

static int w1_write_page(struct w1_dev *dev, int offset, const uint8_t *buffer,
			 int blen)
{
	int i, j, es;

	/* First, write scratchpad */
	w1_match_rom(dev);
	w1_write_byte(dev->bus, W1_CMDR_W_SPAD);
	w1_write_byte(dev->bus, LSB_ADDR(offset));
	w1_write_byte(dev->bus, MSB_ADDR(offset));
	for(i = 0; i < blen; i++)
		w1_write_byte(dev->bus, buffer[i]);

	/* Then, read it back, and remember the return E/S */
	w1_match_rom(dev);
	w1_write_byte(dev->bus, W1_CMDR_R_SPAD);
	if (w1_read_byte(dev->bus) != LSB_ADDR(offset))
		return -1;
	if (w1_read_byte(dev->bus) != MSB_ADDR(offset))
		return -2;
	es = w1_read_byte(dev->bus);
	for(i = 0; i < blen; i++) {
		j = w1_read_byte(dev->bus);
		if (j != buffer[i])
			return -3;
	}

	/* Finally, "copy scratchpad" to actually write */
	w1_match_rom(dev);
	w1_write_byte(dev->bus, W1_CMDR_C_SPAD);
	w1_write_byte(dev->bus, LSB_ADDR(offset));
	w1_write_byte(dev->bus, MSB_ADDR(offset));
	w1_write_byte(dev->bus, es);
	usleep(10000); /* 10ms, in theory */

	/* Don't read back, as nothing useful is there (I get 0xf9, why?) */
	return blen;
}

int w1_write_eeprom(struct w1_dev *dev, int offset, const uint8_t *buffer,
		    int blen)
{
	int i, page, endpage;
	int ret = 0;

	/* Split the write into several page-local writes */
	page = offset / 32;
	endpage = (offset + blen - 1) / 32;

	/* Traling part of first page */
	if (offset % 32) {
		if (endpage != page)
			i = 32 - (offset % 32);
		else
			i = blen;
		ret += w1_write_page(dev, offset, buffer, i);
		if (ret < 0)
			return ret;
		buffer += i;
		offset += i;
		blen -= i;
	}

	/* Whole pages and leading part of last page */
	while (blen > 0 ) {
		i = blen;
		if (blen > 32)
			i = 32;
		i = w1_write_page(dev, offset, buffer, i);
		if (i < 0)
			return i;
		ret += i;
		buffer += 32;
		offset += 32;
		blen -= 32;
	}
	return ret;
}

int w1_read_eeprom(struct w1_dev *dev, int offset, uint8_t *buffer, int blen)
{
	int i;

	w1_match_rom(dev);
	w1_write_byte(dev->bus, W1_CMDR_R_MEMORY);

	w1_write_byte(dev->bus, LSB_ADDR(offset));
	w1_write_byte(dev->bus, MSB_ADDR(offset));

	/* There is no page-size limit in reading, just go on at will */
	for(i = 0; i < blen; i++)
		buffer[i] = w1_read_byte(dev->bus);

	return blen;
}


int w1_read_eeprom_bus(struct w1_bus *bus,
			    int offset, uint8_t *buffer, int blen)
{
	int i, class;

	for (i = 0; i < W1_MAX_DEVICES; i++) {
		class = w1_class(bus->devs + i);
		if (class == 0x43)
			return w1_read_eeprom(bus->devs + i, offset,
					      buffer, blen);
	}
	/* not found */
	return -1;
}

int w1_write_eeprom_bus(struct w1_bus *bus,
			int offset, const uint8_t *buffer, int blen)
{
	int i, class;

	for (i = 0; i < W1_MAX_DEVICES; i++) {
		class = w1_class(bus->devs + i);
		if (class == 0x43)
			return w1_write_eeprom(bus->devs + i, offset,
					       buffer, blen);
	}
	/* not found */
	return -1;
}

#ifndef EXTERNAL_W1
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

/* A shell command, for testing read: "w1w <offset> <len> */
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

#endif
