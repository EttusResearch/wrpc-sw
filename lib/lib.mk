obj-y += lib/util.o

obj-$(CONFIG_LM32) += \
	lib/atoi.o \
	lib/usleep.o

obj-$(CONFIG_WR_NODE) += lib/net.o

obj-$(CONFIG_IP) += lib/ipv4.o lib/arp.o lib/icmp.o lib/udp.o lib/bootp.o
obj-$(CONFIG_SYSLOG) += lib/syslog.o
