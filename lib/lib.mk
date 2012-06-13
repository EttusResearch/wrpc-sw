OBJS_LIB= 	lib/mprintf.o \
						lib/util.o
						
ifneq ($(WITH_ETHERBONE), 0)

OBJS_LIB += lib/arp.o lib/icmp.o lib/ipv4.o

endif