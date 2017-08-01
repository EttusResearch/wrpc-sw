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
const char *fromhex64(const char *hex, int64_t *v);
const char *fromdec(const char *dec, int *v);
void decode_mac(const char *str, unsigned char *mac);
char *format_mac(char *s, const unsigned char *mac);
void decode_ip(const char *str, unsigned char *ip);
char *format_ip(char *s, const unsigned char *ip);

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
int shell_interactive(void);
extern int shell_is_interacting;

void shell_boot_script(void);
void shell_show_build_init(void);

#endif
