EB    ?= no
SDBFS ?= no

CFLAGS = -Wall -ggdb -I../include
LDFLAGS = -lutil
ALL    = genraminit genramvhd genrammif wrpc-uart-sw
ALL   += wrpc-w1-read wrpc-w1-write flash-write

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
	$(CC) $(CFLAGS) -I $(EB) $^ $(LDFLAGS) -o $@ -L $(EB) -letherbone

sdb-wrpc.bin: sdbfs
	$(SDBFS)/gensdbfs $< $@

flash-write: flash-write.c flash-host/libflash.a
	$(CC) $(CFLAGS) -Iflash-host $^ -o $@

flash-host/libflash.a:
	$(MAKE) -C flash-host


clean:
	rm -f $(ALL) *.o *~
	$(MAKE) -C flash-host clean
