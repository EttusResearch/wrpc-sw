obj-$(CONFIG_WR_NODE) += \
	shell/shell.o \
	shell/cmd_version.o \
	shell/cmd_ptp.o \
	shell/cmd_help.o \
	shell/cmd_mac.o \
	shell/cmd_ps.o \
	shell/cmd_uptime.o \
	shell/cmd_refresh.o

obj-$(CONFIG_EMBEDDED_NODE) += \
	shell/cmd_stat.o \
	shell/cmd_sfp.o \
	shell/cmd_pll.o \
	shell/cmd_calib.o \
	shell/cmd_time.o \
	shell/cmd_gui.o \
	shell/cmd_sdb.o \
	shell/cmd_ptrack.o \


obj-$(CONFIG_IP) +=				shell/cmd_ip.o
obj-$(CONFIG_PPSI) +=				shell/cmd_verbose.o
obj-$(CONFIG_CMD_CONFIG) +=			shell/cmd_config.o
obj-$(CONFIG_CMD_SLEEP) +=			shell/cmd_sleep.o
obj-$(CONFIG_CMD_LL) +=				shell/cmd_ll.o
obj-$(CONFIG_FLASH_INIT) +=			shell/cmd_init.o
obj-$(CONFIG_VLAN) +=				shell/cmd_vlan.o
obj-$(CONFIG_AUX_DIAG) += 			shell/cmd_diag.o
