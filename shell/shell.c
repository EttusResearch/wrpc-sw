/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Copyright (C) 2012 GSI (www.gsi.de)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 * Author: Wesley W. Terpstra <w.terpstra@gsi.de>
 * Author: Grzegorz Daniluk <grzegorz.daniluk@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <wrc.h>
#include "util.h"
#include "uart.h"
#include "syscon.h"
#include "shell.h"
#include "eeprom.h"

#define SH_MAX_LINE_LEN 80
#define SH_MAX_ARGS 8
#define SH_ENVIRON_SIZE 256

/* interactive shell state definitions */

#define SH_PROMPT 0
#define SH_INPUT 1
#define SH_EXEC 2

#define ESCAPE_FLAG 0x10000

#define KEY_LEFT (ESCAPE_FLAG | 68)
#define KEY_RIGHT (ESCAPE_FLAG | 67)
#define KEY_ENTER (13)
#define KEY_ESCAPE (27)
#define KEY_BACKSPACE (127)
#define KEY_DELETE (126)

struct shell_cmd {
	char *name;
	int (*exec) (const char *args[]);
};

static const struct shell_cmd cmds_list[] = {
	{"pll",		cmd_pll},
	{"gui",		cmd_gui},
	{"ver",		cmd_version},
	{"stat",	cmd_stat},
	{"ptp",		cmd_ptp},
	{"mode",	cmd_mode},
	{"calibration",	cmd_calib},
	{"set",		cmd_set},
	{"env",		cmd_env},
	{"saveenv",	cmd_saveenv},
	{"time",	cmd_time},
	{"sfp",		cmd_sfp},
	{"init",	cmd_init},
#ifdef CONFIG_ETHERBONE
	{"ip",		cmd_ip},
#endif
#if (defined CONFIG_PPSI) && (defined CONFIG_PPSI_RUNTIME_VERBOSITY)
	{"verbose", 	cmd_verbose},
#endif
	{"mac",		cmd_mac},
	{"sdb",		cmd_sdb},

	{NULL,	NULL}
};

static char cmd_buf[SH_MAX_LINE_LEN + 1];
static int cmd_pos = 0, cmd_len = 0;
static int state = SH_PROMPT;
static int current_key = 0;

static int insert(char c)
{
	if (cmd_len >= SH_MAX_LINE_LEN)
		return 0;

	if (cmd_pos != cmd_len)
		memmove(&cmd_buf[cmd_pos + 1], &cmd_buf[cmd_pos],
			cmd_len - cmd_pos);

	cmd_buf[cmd_pos] = c;
	cmd_pos++;
	cmd_len++;

	return 1;
}

static void delete(int where)
{
	memmove(&cmd_buf[where], &cmd_buf[where + 1], cmd_len - where);
	cmd_len--;
}

static void esc(char code)
{
	mprintf("\033[1%c", code);
}

static int _shell_exec()
{
	char *tokptr[SH_MAX_ARGS + 1];
	int n = 0, i = 0;

	memset(tokptr, 0, sizeof(tokptr));

	while (1) {
		if (n >= SH_MAX_ARGS)
			break;

		while (cmd_buf[i] == ' ' && cmd_buf[i])
			cmd_buf[i++] = 0;

		if (!cmd_buf[i])
			break;

		tokptr[n++] = &cmd_buf[i];
		while (cmd_buf[i] != ' ' && cmd_buf[i])
			i++;

		if (!cmd_buf[i])
			break;
	}

	if (!n)
		return 0;

	if (*tokptr[0] == '#')
		return 0;

	for (i = 0; cmds_list[i].name; i++)
		if (!strcasecmp(cmds_list[i].name, tokptr[0])) {
			int rv = cmds_list[i].exec((const char **)tokptr + 1);
			if (rv < 0)
				mprintf("Err %d\n", rv);
			return rv;
		}

	mprintf("Unrecognized command.\n");
	return -EINVAL;
}

int shell_exec(const char *cmd)
{
	strncpy(cmd_buf, cmd, SH_MAX_LINE_LEN);
	cmd_len = strlen(cmd_buf);
	return _shell_exec();
}

void shell_init()
{
	env_init();
	cmd_len = cmd_pos = 0;
	state = SH_PROMPT;
	mprintf("\033[2J\033[1;1H");
}

void shell_interactive()
{
	int c;
	switch (state) {
	case SH_PROMPT:
		mprintf("wrc# ");
		cmd_pos = 0;
		cmd_len = 0;
		state = SH_INPUT;
		break;

	case SH_INPUT:
		c = uart_read_byte();

		if (c < 0)
			return;

		if (c == 27 || ((current_key & ESCAPE_FLAG) && c == 91))
			current_key = ESCAPE_FLAG;
		else
			current_key |= c;

		if (current_key & 0xff) {

			switch (current_key) {
			case KEY_LEFT:
				if (cmd_pos > 0) {
					cmd_pos--;
					esc('D');
				}
				break;
			case KEY_RIGHT:
				if (cmd_pos < cmd_len) {
					cmd_pos++;
					esc('C');
				}
				break;

			case KEY_ENTER:
				mprintf("\n");
				state = SH_EXEC;
				break;

			case KEY_DELETE:
				if (cmd_pos != cmd_len) {
					delete(cmd_pos);
					esc('P');
				}
				break;

			case KEY_BACKSPACE:
				if (cmd_pos > 0) {
					esc('D');
					esc('P');
					delete(cmd_pos - 1);
					cmd_pos--;
				}
				break;

			case '\t':
				break;

			default:
				if (!(current_key & ESCAPE_FLAG)
				    && insert(current_key)) {
					esc('@');
					mprintf("%c", current_key);
				}
				break;

			}
			current_key = 0;
		}
		break;

	case SH_EXEC:
		cmd_buf[cmd_len] = 0;
		_shell_exec();
		state = SH_PROMPT;
		break;
	}
}

const char *fromhex(const char *hex, int *v)
{
	int o = 0;

	for (; *hex; ++hex) {
		if (*hex >= '0' && *hex <= '9') {
			o = (o << 4) + (*hex - '0');
		} else if (*hex >= 'A' && *hex <= 'F') {
			o = (o << 4) + (*hex - 'A') + 10;
		} else if (*hex >= 'a' && *hex <= 'f') {
			o = (o << 4) + (*hex - 'a') + 10;
		} else {
			break;
		}
	}

	*v = o;
	return hex;
}

const char *fromdec(const char *dec, int *v)
{
	int o = 0;

	for (; *dec; ++dec) {
		if (*dec >= '0' && *dec <= '9') {
			o = (o * 10) + (*dec - '0');
		} else {
			break;
		}
	}

	*v = o;
	return dec;
}

int shell_boot_script(void)
{
	uint8_t next = 0;

	//first check if EEPROM is really there
	eeprom_present(WRPC_FMC_I2C, FMC_EEPROM_ADR);
	if (!has_eeprom)
		return -1;

	while (1) {
		cmd_len = eeprom_init_readcmd(WRPC_FMC_I2C, FMC_EEPROM_ADR,
					      (uint8_t *)cmd_buf,
					      SH_MAX_LINE_LEN, next);
		if (cmd_len <= 0) {
			if (next == 0)
				mprintf("Empty init script...\n");
			break;
		}
		cmd_buf[cmd_len - 1] = 0;

		mprintf("executing: %s\n", cmd_buf);
		_shell_exec();
		next = 1;
	}

	return 0;
}
