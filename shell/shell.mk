OBJS_SHELL = 	shell/shell.o \
							shell/environ.o \
							shell/cmd_version.o \
							shell/cmd_pll.o \
							shell/cmd_sfp.o \
							shell/cmd_stat.o \
							shell/cmd_ptp.o \
							shell/cmd_mode.o \
							shell/cmd_calib.o \
							shell/cmd_time.o \
							shell/cmd_gui.o \
							shell/cmd_sdb.o \
							shell/cmd_mac.o \
							shell/cmd_init.o

ifneq ($(WITH_ETHERBONE), 0)
OBJS_SHELL += shell/cmd_ip.o
endif
