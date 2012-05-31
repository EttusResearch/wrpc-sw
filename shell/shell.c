#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "shell.h"

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


struct shell_cmd {
	char *name;
	int (*exec)(const char *args[]);
};

extern int uart_read_byte();

static const struct shell_cmd cmds_list[] = {
		{ "pll", 		cmd_pll },
		{ "gui",		cmd_gui },
		{ "ver",		cmd_version },
		{ "stat",		cmd_stat },
		{ "ptp",		cmd_ptp },
		{ "mode",		cmd_mode },
		{ "measure_t24p",		cmd_measure_t24p },
		{ NULL,			NULL }
	};

static char cmd_buf[SH_MAX_LINE_LEN + 1];
static int cmd_pos = 0, cmd_len = 0;
static int state = SH_PROMPT;
static int current_key = 0;

static int insert(char c)
{
	if(cmd_len >= SH_MAX_LINE_LEN)
		return 0;

	if(cmd_pos != cmd_len)
		memmove(&cmd_buf[cmd_pos+1], &cmd_buf[cmd_pos], cmd_len - cmd_pos);

	cmd_buf[cmd_pos] = c;
	cmd_pos++;
	cmd_len++;

	return 1;
}

static void delete(int where)
{
	memmove(&cmd_buf[where], &cmd_buf[where+1], cmd_len - where);
	cmd_len--;
}

static void esc(char code)
{
	mprintf("\033[1%c", code);
}

static int _shell_exec()
{
	char *tokptr[SH_MAX_ARGS+1];
	int n = 0, i = 0;

	memset(tokptr, 0, sizeof(tokptr));
	
	while(1)
	{
		if(n >= SH_MAX_ARGS)
			break;

		while(cmd_buf[i] == ' ' && cmd_buf[i]) cmd_buf[i++] = 0;

		if(!cmd_buf[i]) 
			break;

		tokptr [n++] = &cmd_buf[i];
		while(cmd_buf[i] != ' ' && cmd_buf[i]) i++;

		if(!cmd_buf[i]) 
			break;
	}

	if(!n)
		return 0;

	if(*tokptr[0] == '#')
		return 0;

	for(i=0; cmds_list[i].name; i++)
		if(!strcasecmp(cmds_list[i].name, tokptr[0]))
		{
			int rv = cmds_list[i].exec(tokptr+1);
			if(rv<0) 
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
	cmd_len = cmd_pos = 0;
	state = SH_PROMPT;
	mprintf("\033[2J\033[1;1H");
}

void shell_interactive()
{
	int c;
	switch(state)
	{
		case SH_PROMPT:
			mprintf("wrc# ");
			cmd_pos = 0;
			cmd_len = 0;
			state = SH_INPUT;
			break;

		case SH_INPUT:
			c = uart_read_byte();
				
			if(c < 0)
				return;

			if(c == 27 || ((current_key & ESCAPE_FLAG) && c == 91))
				current_key = ESCAPE_FLAG;
			else 
				current_key |= c;

			if(current_key & 0xff)
			{ 
			
				switch(current_key)
				{
					case KEY_LEFT:
						if(cmd_pos > 0)
						{
							cmd_pos--;
							esc('D');
						}
						break;
					case KEY_RIGHT:
						if(cmd_pos < cmd_len)
						{
							cmd_pos++;
							esc('C');
						}
						break;
			
					case KEY_ENTER:
						mprintf("\n");
						state = SH_EXEC;
						break;
					
					case KEY_DELETE:
						if(cmd_pos != cmd_len)
						{
							delete(cmd_pos);
							esc('P');
						}
						break;
						
					case KEY_BACKSPACE:
						if(cmd_pos > 0)
						{
							esc('D');
							esc('P');
							delete(cmd_pos-1);
							cmd_pos--;
						}
						break;
					
					case '\t':
						break;
						
					default:
						if(!(current_key & ESCAPE_FLAG) && insert(current_key))
						{
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
