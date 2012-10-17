obj-y += lib/mprintf.o lib/util.o

obj-$(CONFIG_ETHERBONE) += lib/arp.o lib/icmp.o lib/ipv4.o lib/bootp.o
