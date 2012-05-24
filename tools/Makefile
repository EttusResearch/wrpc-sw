CFLAGS = -Wall -ggdb -I. -I../include -Imini_bone

AS		= $(CROSS_COMPILE)as
LD		= $(CROSS_COMPILE)ld
CC		= $(CROSS_COMPILE)gcc
CPP		= $(CC) -E
AR		= $(CROSS_COMPILE)ar
NM		= $(CROSS_COMPILE)nm
STRIP		= $(CROSS_COMPILE)strip
OBJCOPY		= $(CROSS_COMPILE)objcopy
OBJDUMP		= $(CROSS_COMPILE)objdump

all: lm32-loader genraminit vuart_console

OBJS_LOADER = lm32-loader.o rr_io.o mini_bone/ptpd_netif.o mini_bone/minibone_lib.o

lm32-loader:	$(OBJS_LOADER)
				make -C mini_bone
				${CC} -o lm32-loader $(OBJS_LOADER)
				
genraminit: genraminit.o
				${CC} -o genraminit genraminit.o

vuart_console: vuart_console.o rr_io.o
				${CC} -o vuart_console vuart_console.o rr_io.o

%.o: 		%.c
				${CC} -c $^ $(CFLAGS) -O0

clean:
	rm -f $(ALL) *.o *~ mini_bone/*.o mini_bone/*~
