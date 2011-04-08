PLATFORM = lm32

OBJS_WRC = wrc_main.o dev/uart.o dev/endpoint.o dev/minic.o dev/pps_gen.o dev/timer.o dev/softpll.o lib/mprintf.o

ifeq ($(PLATFORM), zpu)
CROSS_COMPILE ?= /opt/gcc-zpu/bin/zpu-elf-
CFLAGS_PLATFORM = -abel -Wl,--relax -Wl,--gc-sections
LDFLAGS_PLATFORM = -abel -Wl,--relax -Wl,--gc-sections
OBJS_PLATFORM=
else
CROSS_COMPILE ?= /opt/gcc-lm32/bin/lm32-elf-
CFLAGS_PLATFORM = -mmultiply-enabled -mbarrel-shift-enabled  
LDFLAGS_PLATFORM = -mmultiply-enabled -mbarrel-shift-enabled   -nostdlib -T target/lm32/ram.ld 
OBJS_PLATFORM=target/lm32/crt0.o
endif


CC=$(CROSS_COMPILE)gcc
OBJCOPY=$(CROSS_COMPILE)objcopy
OBJDUMP=$(CROSS_COMPILE)objdump
CFLAGS= $(CFLAGS_PLATFORM) -ffunction-sections -fdata-sections -Os -Iinclude
LDFLAGS= $(LDFLAGS_PLATFORM) -ffunction-sections -fdata-sections -Os -Iinclude
OBJS=$(OBJS_PLATFORM) $(OBJS_WRC)
OUTPUT=wrc

all: 		$(OBJS)
				${CC} -o $(OUTPUT).elf $(OBJS) $(LDFLAGS) 
				${OBJCOPY} -O binary $(OUTPUT).elf $(OUTPUT).bin
				${OBJDUMP} -d $(OUTPUT).elf > $(OUTPUT)_disasm.S
				./tools/genraminit $(OUTPUT).bin 0 > $(OUTPUT).ram
				
clean:	
	rm -f $(OBJS) $(OUTPUT).elf $(OUTPUT).bin $(OUTPUT).ram

%.o:		%.c
				${CC} $(CFLAGS) $(INCLUDE_DIR) $(LIB_DIR) -c $^ -o $@

load:	all
		./tools/zpu-loader $(OUTPUT).bin

tools:
			make -C tools
		
fpga:
		- killall -9 vuart_console
		../loadfile ../spec_top.bin
		./tools/zpu-loader $(OUTPUT).bin
