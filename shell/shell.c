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
#include "uart.h"
#include "syscon.h"
#include "shell.h"
#include "storage.h"

#define SH_MAX_LINE_LEN 80
#define SH_MAX_ARGS 8

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

static char cmd_buf[SH_MAX_LINE_LEN + 1];
static int cmd_pos = 0, cmd_len = 0;
static int state = SH_PROMPT;
static int current_key = 0;

int shell_is_interacting;

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
	pp_printf("\033[1%c", code);
}

static int _shell_exec(void)
{
	char *tokptr[SH_MAX_ARGS + 1];
	struct wrc_shell_cmd *p;
	int n = 0, i = 0, rv;

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

	for (p = __cmd_begin; p < __cmd_end; p++)
		if (!strcasecmp(p->name, tokptr[0])) {
			rv = p->exec((const char **)(tokptr + 1));
			if (rv < 0)
				pp_printf("Command \"%s\": error %d\n",
					p->name, rv);
			return rv;
		}

	pp_printf("Unrecognized command \"%s\".\n", tokptr[0]);
	return -EINVAL;
}

int shell_exec(const char *cmd)
{
	int i;

	if (cmd != cmd_buf)
		strncpy(cmd_buf, cmd, SH_MAX_LINE_LEN);
	cmd_len = strlen(cmd_buf);
	shell_is_interacting = 1;
	i = _shell_exec();
	shell_is_interacting = 0;
	return i;
}

void shell_init()
{
	cmd_len = cmd_pos = 0;
	state = SH_PROMPT;
}

int shell_interactive()
{
	int c;

	switch (state) {
	case SH_PROMPT:
		pp_printf("wrc# ");
		cmd_pos = 0;
		cmd_len = 0;
		state = SH_INPUT;
		return 1;

	case SH_INPUT:
		c = uart_read_byte();

		if (c < 0)
			return 0;

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
				pp_printf("\n");
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
					pp_printf("%c", current_key);
				}
				break;

			}
			current_key = 0;
		}
		return 1;

	case SH_EXEC:
		cmd_buf[cmd_len] = 0;
		_shell_exec();
		state = SH_PROMPT;
		return 1;
	}
	return 0;
}


const char *fromhex64(const char *hex, int64_t *v)
{
	int64_t o = 0;
	int sign = 1;

	if (hex && *hex == '-') {
		sign = -1;
		hex++;
	}
	for (; hex && *hex; ++hex) {
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

	*v = o * sign;
	return hex;
}

const char *fromhex(const char *hex, int *v)
{
	const char *ret;
	int64_t v64;

	ret = fromhex64(hex, &v64);
	*v = (int)v64;
	return ret;
}

const char *fromdec(const char *dec, int *v)
{
	int o = 0, sign = 1;

	if (dec && *dec == '-') {
		sign = -1;
		dec++;
	}
	for (; dec && *dec; ++dec) {
		if (*dec >= '0' && *dec <= '9') {
			o = (o * 10) + (*dec - '0');
		} else {
			break;
		}
	}

	*v = o * sign;
	return dec;
}

static char shell_init_cmd[] = CONFIG_INIT_COMMAND;

static int build_init_readcmd(uint8_t *cmd, int maxlen)
{
	static char *p = shell_init_cmd;
	int i;

	/* use semicolon as separator */
	for (i = 0; i < maxlen && p[i] && p[i] != ';'; i++)
		cmd[i] = p[i];
	cmd[i] = '\0';
	p += i;
	if (*p == ';')
		p++;
	if (i == 0) {
		/* it's the last call, roll-back *p to be ready for the next
		 * call */
		p = shell_init_cmd;
	}
	return i;
}

void shell_boot_script(void)
{
	uint8_t next = 0;

	if (!has_eeprom)
		return;

	while (CONFIG_HAS_BUILD_INIT) {
		cmd_len = build_init_readcmd((uint8_t *)cmd_buf,
					SH_MAX_LINE_LEN);
		if (!cmd_len)
			break;
		pp_printf("executing: %s\n", cmd_buf);
		shell_exec(cmd_buf);
	}

	while (CONFIG_HAS_FLASH_INIT) {
		cmd_len = storage_init_readcmd((uint8_t *)cmd_buf,
					      SH_MAX_LINE_LEN, next);
		if (cmd_len <= 0) {
			if (next == 0)
				pp_printf("Empty init script...\n");
			break;
		}
		cmd_buf[cmd_len - 1] = 0;

		pp_printf("executing: %s\n", cmd_buf);
		shell_exec(cmd_buf);
		next = 1;
	}

	return;
}

void shell_show_build_init(void)
{
	uint8_t i = 0;

	pp_printf("-- built-in script --\n");
	while (CONFIG_HAS_BUILD_INIT) {
		cmd_len = build_init_readcmd((uint8_t *)cmd_buf,
					SH_MAX_LINE_LEN);
		if (!cmd_len)
			break;
		pp_printf("%s\n", cmd_buf);
		++i;
	}
	if (!i)
		pp_printf("(empty)\n");
}
