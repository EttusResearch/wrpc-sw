#include <wrc.h>
#include <string.h>
#include <uart.h>
#include <shell.h>
#include <lib/ipv4.h>

int puts(const char *s)
{
	char new_s[CONFIG_PRINT_BUFSIZE + 4];
	int l, ret;

	ret = uart_write_string(s);
	l = strlen(s);

	/* avoid shell-interation stuff */
	if (shell_is_interacting)
		return ret;
	if (l < 2 || s[0] == '\e')
		return ret;
	if (!strncmp(s, "wrc#", 4))
		return ret;

	/* if not terminating with newline, add a trailing "...\n" */
	strcpy(new_s, s);
	if (s[l-1] != '\n') {
		new_s[l++] = '.';
		new_s[l++] = '.';
		new_s[l++] = '.';
		new_s[l++] = '\n';
		new_s[l++] = '\0';
	}
	syslog_report(new_s);
	return ret;
}
