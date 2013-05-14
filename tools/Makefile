CFLAGS = -Wall -ggdb
LDFLAGS = -lutil
ALL    = genraminit genramvhd genrammif wrpc-uart-sw
ALL   += wrpc-w1-read #wrpc-w1-write

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

wrpc-w1-read: wrpc-w1-read.c w1-host/libw1.a
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

w1-host/libw1.a:
	$(MAKE) -C w1-host

clean:
	rm -f $(ALL) *.o *~
	$(MAKE) -C w1-host clean
