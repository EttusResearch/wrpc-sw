#ifndef __SHELL_H
#define __SHELL_H

int cmd_gui(const char *args[]);
int cmd_pll(const char *args[]);
int cmd_version(const char *args[]);
int cmd_stat(const char *args[]);
int cmd_ptp(const char *args[]);
int cmd_sfp(const char *args[]);
int cmd_mode(const char *args[]);
int cmd_measure_t24p(const char *args[]);

int shell_exec(const char *buf);
void shell_interactive();

#endif
