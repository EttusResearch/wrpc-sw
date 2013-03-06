#ifndef __SHELL_H
#define __SHELL_H

#define UI_SHELL_MODE 0
#define UI_GUI_MODE 1
#define UI_STAT_MODE 2

extern int wrc_ui_mode;

const char *fromhex(const char *hex, int *v);
const char *fromdec(const char *dec, int *v);

int cmd_gui(const char *args[]);
int cmd_pll(const char *args[]);
int cmd_sfp(const char *args[]);
int cmd_version(const char *args[]);
int cmd_stat(const char *args[]);
int cmd_ptp(const char *args[]);
int cmd_sfp(const char *args[]);
int cmd_mode(const char *args[]);
int cmd_calib(const char *args[]);
int cmd_time(const char *args[]);
int cmd_ip(const char *args[]);
int cmd_verbose(const char *args[]);
int cmd_sdb(const char *args[]);
int cmd_mac(const char *args[]);
int cmd_init(const char *args[]);

int cmd_env(const char *args[]);
int cmd_saveenv(const char *args[]);
int cmd_set(const char *args[]);

char *env_get(const char *var);
int env_set(const char *var, const char *value);
void env_init();

int shell_exec(const char *buf);
void shell_interactive();

int shell_boot_script(void);

#endif
