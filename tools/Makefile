CFLAGS = -Wall -ggdb
LDFLAGS = -lutil
ALL    = genraminit genramvhd genrammif wrpc-uart-sw

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

clean:
	rm -f $(ALL) *.o *~
