/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
/* SFP Detection / managenent functions */

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>

#include "syscon.h"
#include "i2c.h"
#include "sfp.h"
#include "storage.h"

/* Calibration data (from EEPROM if available) */
int32_t sfp_alpha = 73622176; /* default values if could not read EEPROM */
int32_t sfp_deltaTx = 0;
int32_t sfp_deltaRx = 0;
int32_t sfp_in_db = 0;

char sfp_pn[SFP_PN_LEN];

static int sfp_present(void)
{
	return !gpio_in(GPIO_SFP_DET);
}

static int sfp_read_part_id(char *part_id)
{
	int i;
	uint8_t data, sum;
	mi2c_init(WRPC_SFP_I2C);

	mi2c_start(WRPC_SFP_I2C);
	mi2c_put_byte(WRPC_SFP_I2C, 0xA0);
	mi2c_put_byte(WRPC_SFP_I2C, 0x00);
	mi2c_repeat_start(WRPC_SFP_I2C);
	mi2c_put_byte(WRPC_SFP_I2C, 0xA1);
	mi2c_get_byte(WRPC_SFP_I2C, &data, 1);
	mi2c_stop(WRPC_SFP_I2C);

	sum = data;

	mi2c_start(WRPC_SFP_I2C);
	mi2c_put_byte(WRPC_SFP_I2C, 0xA1);
	for (i = 1; i < 63; ++i) {
		mi2c_get_byte(WRPC_SFP_I2C, &data, 0);
		sum = (uint8_t) ((uint16_t) sum + data) & 0xff;
		if (i >= 40 && i <= 55)	//Part Number
			part_id[i - 40] = data;
	}
	mi2c_get_byte(WRPC_SFP_I2C, &data, 1);	//final word, checksum
	mi2c_stop(WRPC_SFP_I2C);

	if (sum == data)
		return 0;

	return -1;
}

int sfp_match(void)
{
	struct s_sfpinfo sfp;

	sfp_pn[0] = '\0';
	if (!sfp_present()) {
		return -ENODEV;
	}
	if (sfp_read_part_id(sfp_pn)) {
		return -EIO;
	}

	strncpy(sfp.pn, sfp_pn, SFP_PN_LEN);
	if (storage_match_sfp(&sfp) == 0) {
		sfp_in_db = SFP_NOT_MATCHED;
		return -ENXIO;
	}
	sfp_deltaTx = sfp.dTx;
	sfp_deltaRx = sfp.dRx;
	sfp_alpha = sfp.alpha;
	sfp_in_db = SFP_MATCHED;
	return 0;
}
