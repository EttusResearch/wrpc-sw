CFLAGS = -Wall -ggdb -I. -I../include -Imini_bone
ALL    = genraminit genramvhd genrammif

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
	${CC} -o $@ $<

%.o:	%.c
	${CC} -c $^ $(CFLAGS) -O0

clean:
	rm -f $(ALL) *.o
