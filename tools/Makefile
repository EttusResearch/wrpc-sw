CFLAGS = -Wall -ggdb -I. -I../include -Imini_bone
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

%:	%.o
	${CC} $< $(LDFLAGS) -o $@

%.o:	%.c
	${CC} -c $^ $(CFLAGS) -O0

clean:
	rm -f $(ALL) *.o
