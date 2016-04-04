EB    ?= no
SDBFS ?= no

CFLAGS = -Wall -ggdb -I../include
LDFLAGS = -lutil
ALL    = genraminit genramvhd genrammif wrpc-uart-sw
ALL   += wrpc-w1-read wrpc-w1-write
ALL   += pfilter-builder
ALL   += wrpc-dump

ifneq ($(EB),no)
ALL += eb-w1-write
endif
ifneq ($(SDBFS),no)
ALL += sdb-wrpc.bin
endif

AS		= as
LD		= ld
CC		= gcc
CPP		= $(CC) -E
AR		= ar
NM		= nm
STRIP		= strip
OBJCOPY		= objcopy
OBJDUMP		= objdump

all:	$(ALL)

%:	%.c
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@

wrpc-w1-read: wrpc-w1-read.c ../dev/w1.c ../dev/w1-eeprom.c ../dev/w1-hw.c
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

wrpc-w1-write: wrpc-w1-write.c ../dev/w1.c ../dev/w1-eeprom.c ../dev/w1-hw.c
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

eb-w1-write: eb-w1-write.c ../dev/w1.c ../dev/w1-eeprom.c eb-w1.c
	$(CC) $(CFLAGS) -I $(EB) $^ $(LDFLAGS) -o $@ -L $(EB)/.libs -letherbone

sdb-wrpc.bin: sdbfs
	$(SDBFS)/gensdbfs $< $@

wrpc-dump: wrpc-dump.c dump-info-host.o
	$(CC) $(CFLAGS) -I../ppsi/include -I../ppsi/arch-wrpc/include \
	-I ../softpll \
	$^ -o $@

pfilter-builder: pfilter-builder.c
	$(CC) $(CFLAGS)  -include ../include/generated/autoconf.h \
		$@.c $(LDFLAGS) -o $@


dump-info-host.o: ../dump-info.o
	$(CROSS_COMPILE)objcopy --only-section=.data.dump_info \
		-O binary $^ dump-info-host.bin
	$(AS) makebin.S -o $@

../dump-info.o: ../dump-info.c
	$(MAKE) -C .. dump-info.o
clean:
	rm -f $(ALL) *.o *~

