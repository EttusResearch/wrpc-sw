obj-y += \
	shell/shell.o \
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
	shell/cmd_init.o \
	shell/cmd_ptrack.o \
	shell/cmd_help.o \
	shell/cmd_refresh.o

obj-$(CONFIG_ETHERBONE) +=			shell/cmd_ip.o
obj-$(CONFIG_PPSI) +=				shell/cmd_verbose.o
obj-$(CONFIG_CMD_CONFIG) +=			shell/cmd_config.o
obj-$(CONFIG_CMD_SLEEP) +=			shell/cmd_sleep.o
