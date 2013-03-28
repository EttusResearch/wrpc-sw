#ifndef __SHELL_H
#define __SHELL_H

#define UI_SHELL_MODE 0
#define UI_GUI_MODE 1
#define UI_STAT_MODE 2

extern int wrc_ui_mode;

const char *fromhex(const char *hex, int *v);
const char *fromdec(const char *dec, int *v);

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
void env_init();

int shell_exec(const char *buf);
void shell_interactive();

int shell_boot_script(void);

#endif
