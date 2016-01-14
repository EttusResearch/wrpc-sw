/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#ifndef __SHELL_H
#define __SHELL_H

#define UI_SHELL_MODE 0
#define UI_GUI_MODE 1

extern int wrc_ui_mode;
extern int wrc_stat_running;

const char *fromhex(const char *hex, int *v);
const char *fromdec(const char *dec, int *v);
void decode_mac(const char *str, unsigned char *mac);
void print_mac(char *head, unsigned char *mac, char *tail);
void decode_ip(const char *str, unsigned char *ip);
void print_ip(char *head, unsigned char *ip, char *tail);

struct wrc_shell_cmd {
	char *name;
	int (*exec) (const char *args[]);
};
extern struct wrc_shell_cmd __cmd_begin[], __cmd_end[];

/* Put the structures in their own section */
#define DEFINE_WRC_COMMAND(_name) \
	static struct wrc_shell_cmd __wrc_cmd_ ## _name \
	__attribute__((section(".cmd"), __used__))

char *env_get(const char *var);
int env_set(const char *var, const char *value);
void env_init(void);

int shell_exec(const char *buf);
void shell_interactive(void);

int shell_boot_script(void);

#endif
