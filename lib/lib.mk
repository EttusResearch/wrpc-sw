obj-y += lib/util.o lib/atoi.o
obj-y += lib/usleep.o
obj-$(CONFIG_WR_NODE) += lib/net.o

obj-$(CONFIG_ETHERBONE) += lib/arp.o lib/icmp.o lib/ipv4.o lib/bootp.o
