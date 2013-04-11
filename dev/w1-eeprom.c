/*
 * Eeprom support
 * Cesar Prados, 2013 GNU GPL2 or later
 */
#include <stdlib.h>
#include <string.h>
#include <wrc.h>
#include <shell.h>
#include <w1.h>

#define LSB_ADDR(X) ((X) & 0xFF)
#define MSB_ADDR(X) (((X) & 0xFF00)>>8)

int w1_write_eeprom(struct w1_dev *dev, int offset, const uint8_t *buffer,
		    int blen)
{
	int i;
	int read_data;

	w1_match_rom(dev);
	w1_write_byte(dev->bus, W1_CMDR_W_SPAD);

	w1_write_byte(dev->bus, LSB_ADDR(offset));
	w1_write_byte(dev->bus, MSB_ADDR(offset));

	for(i = 0; i < 32; i++)
		w1_write_byte(dev->bus, buffer[i]);

	w1_match_rom(dev);
	w1_write_byte(dev->bus, W1_CMDR_C_SPAD);

	w1_write_byte(dev->bus, LSB_ADDR(offset));
	w1_write_byte(dev->bus, MSB_ADDR(offset));

	w1_write_byte(dev->bus, 0x1f);
	usleep(10000);

	read_data = w1_read_byte(dev->bus);
	if (read_data != 0xaa)
		return -1;
	return 0;
}

int w1_read_eeprom(struct w1_dev *dev, int offset, uint8_t *buffer, int blen)
{

	int i;

	w1_match_rom(dev);
	w1_write_byte(dev->bus, W1_CMDR_R_MEMORY);

	w1_write_byte(dev->bus, LSB_ADDR(offset));
	w1_write_byte(dev->bus, MSB_ADDR(offset));

	for(i = 0; i < 32; i++)
		buffer[i] = w1_read_byte(dev->bus);

	return 0;
}


/* A shell command, for testing write*/
static int cmd_w1_w(const char *args[])
{
	struct w1_dev *d;
	int page;
	int blen;
	char pn[32]="\0";

	d = wrpc_w1_bus.devs + 1;

	pp_printf("Writing in device: %08x%08x\n",
		  (int)(d->rom >> 32), (int)d->rom);

	page = atoi(args[0]);

	blen = strlen(args[1]);
	memcpy(pn, args[1], blen);
	if(!w1_write_eeprom(d, page*0x20, (uint8_t *)pn, blen))
		pp_printf("Write success\n");

	return 0;
}

DEFINE_WRC_COMMAND(w1w) = {
	.name = "w1w",
	.exec = cmd_w1_w,
};

/* A shell command, for testing read*/
static int cmd_w1_r(const char *args[])
{
	struct w1_dev *d;
	int page;
	int i;
	char pn[32]="\0";

	d = wrpc_w1_bus.devs + 1;

	pp_printf("Reading from device: %08x%08x\n",
		  (int)(d->rom >> 32), (int)d->rom);

	page = atoi(args[0]);

	w1_read_eeprom(d, page*0x20, (uint8_t *)pn, 32);

	for(i=0;i<32;i++)
		pp_printf("%c",pn[i]);

	pp_printf("\n");

	return 0;
}



DEFINE_WRC_COMMAND(w1r) = {
	.name = "w1r",
	.exec = cmd_w1_r,
};

/* A shell command, for testing write/read*/
static int cmd_w1_test(const char *args[])
{
	struct w1_dev *d;
	int page;
	int errors=0;
	int blen;
	int i;
	char pn[32]="testing";

	blen = strlen(pn);

	d = wrpc_w1_bus.devs + 1;

	pp_printf("Writing in device: %08x%08x\n",
		  (int)(d->rom >> 32), (int)d->rom);

	for(page = 0; page < 80; page++) {
		if(!w1_write_eeprom(d, page * 0x20, (uint8_t *)pn, blen)) {
			pp_printf("Page %i: success\n", page);
		} else {
			pp_printf("Page %i: error\n", page);
			errors++;
		}
	}
	pp_printf("Write Errors: %d \n", errors);

	usleep(1000 * 1000);

	for(page = 0; page < 80; page++) {
		w1_read_eeprom(d, page * 0x20, (uint8_t *)pn, 32);
		for(i = 0; i < 32; i++)
			pp_printf("%c",pn[i]);
		pp_printf("\n");
	}
	return 0;
}

DEFINE_WRC_COMMAND(w1test) = {
	.name = "w1test",
	.exec = cmd_w1_test,
};
