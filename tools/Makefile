CFLAGS = -Wall -ggdb -I. -I../include

AS		= $(CROSS_COMPILE)as
LD		= $(CROSS_COMPILE)ld
CC		= $(CROSS_COMPILE)gcc
CPP		= $(CC) -E
AR		= $(CROSS_COMPILE)ar
NM		= $(CROSS_COMPILE)nm
STRIP		= $(CROSS_COMPILE)strip
OBJCOPY		= $(CROSS_COMPILE)objcopy
OBJDUMP		= $(CROSS_COMPILE)objdump

all: zpu-loader genraminit vuart_console

OBJS_LOADER = zpu-loader.o rr_io.o

zpu-loader:	$(OBJS_LOADER)
				${CC} -o zpu-loader $(OBJS_LOADER)
				
genraminit: genraminit.o
				${CC} -o genraminit genraminit.o

vuart_console: vuart_console.o rr_io.o
				${CC} -o vuart_console vuart_console.o rr_io.o

%.o: 		%.c
				${CC} -c $^ $(CFLAGS) -O0

clean:
	rm -f $(ALL) *.o *~
