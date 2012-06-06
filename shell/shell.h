#ifndef __SHELL_H
#define __SHELL_H

int cmd_gui(const char *args[]);
int cmd_pll(const char *args[]);
int cmd_sfp(const char *args[]);
int cmd_version(const char *args[]);
int cmd_stat(const char *args[]);
int cmd_ptp(const char *args[]);
int cmd_sfp(const char *args[]);
int cmd_mode(const char *args[]);
int cmd_measure_t24p(const char *args[]);
int cmd_time(const char *args[]);

int cmd_env(const char *args[]);
int cmd_saveenv(const char *args[]);
int cmd_set(const char *args[]);

char *env_get(const char *var);
int env_set(const char *var, const char *value);
void env_init();

int shell_exec(const char *buf);
void shell_interactive();

#endif

