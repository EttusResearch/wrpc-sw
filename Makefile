PLATFORM = lm32

OBJS_WRC = wrc_main.o wrc_ptp.o dev/uart.o dev/endpoint.o dev/minic.o dev/pps_gen.o dev/syscon.o dev/softpll_ng.o dev/ep_pfilter.o dev/dna.o dev/i2c.o monitor/monitor.o dev/onewire.o dev/eeprom.o 

D = ptp-noposix
PTPD_CFLAGS  = -ffreestanding -DPTPD_FREESTANDING -DWRPC_EXTRA_SLIM -DPTPD_MSBF -DPTPD_DBG 
PTPD_CFLAGS += -Wall -ggdb -I$D/wrsw_hal \
	-I$D/libptpnetif -I$D/PTPWRd \
	-include $D/compat.h -include $D/PTPWRd/dep/trace.h -include $D/libposix/ptpd-wrappers.h
PTPD_CFLAGS += -DPTPD_NO_DAEMON -DNEW_SINGLE_WRFSM -DPTPD_TRACE_MASK=0

OBJS_PTPD = $D/PTPWRd/arith.o
OBJS_PTPD += $D/PTPWRd/bmc.o
OBJS_PTPD += $D/PTPWRd/dep/msg.o
OBJS_PTPD += $D/PTPWRd/dep/net.o
OBJS_PTPD += $D/PTPWRd/dep/servo.o
OBJS_PTPD += $D/PTPWRd/dep/sys.o
OBJS_PTPD += $D/PTPWRd/dep/timer.o
OBJS_PTPD += $D/PTPWRd/dep/wr_servo.o
OBJS_PTPD += $D/PTPWRd/protocol.o
OBJS_PTPD += $D/PTPWRd/wr_protocol.o
OBJS_PTPD_FREE   = $D/libposix/freestanding-startup.o
OBJS_PTPD_FREE	+= $D/libposix/freestanding-display.o
OBJS_PTPD_FREE	+= $D/libposix/wr_nolibs.o
OBJS_PTPD_FREE	+= $D/libposix/freestanding-wrapper.o

CROSS_COMPILE ?= lm32-elf-
CFLAGS_PLATFORM  = -mmultiply-enabled -mbarrel-shift-enabled 
LDFLAGS_PLATFORM = -mmultiply-enabled -mbarrel-shift-enabled   -nostdlib -T target/lm32/ram.ld 
OBJS_PLATFORM=target/lm32/crt0.o target/lm32/irq.o

# Comment this out if you don't want debugging
OBJS_PLATFORM+=target/lm32/debug.o

include shell/shell.mk
include tests/tests.mk
include lib/lib.mk

CC=$(CROSS_COMPILE)gcc
OBJCOPY=$(CROSS_COMPILE)objcopy
OBJDUMP=$(CROSS_COMPILE)objdump
CFLAGS= $(CFLAGS_PLATFORM) -ffunction-sections -fdata-sections -Os -Iinclude -include include/trace.h $(PTPD_CFLAGS) -Iptp-noposix/PTPWRd -I.
LDFLAGS= $(LDFLAGS_PLATFORM) -ffunction-sections -fdata-sections -Os -Iinclude
SIZE = $(CROSS_COMPILE)size
OBJS=$(OBJS_PLATFORM) $(OBJS_WRC) $(OBJS_PTPD) $(OBJS_PTPD_FREE) $(OBJS_SHELL) $(OBJS_TESTS) $(OBJS_LIB)
OUTPUT=wrc
REVISION=$(shell git rev-parse HEAD)


all: 		$(OBJS)
				echo "const char *build_revision = \"$(REVISION)\";" > revision.c
				echo "const char *build_date = __DATE__ \" \" __TIME__;" >> revision.c
				$(CC) $(CFLAGS) -c revision.c
				$(SIZE) -t $(OBJS)
				${CC} -o $(OUTPUT).elf revision.o $(OBJS) $(LDFLAGS) 
				${OBJCOPY} -O binary $(OUTPUT).elf $(OUTPUT).bin
				${OBJDUMP} -d $(OUTPUT).elf > $(OUTPUT)_disasm.S
				./tools/genraminit $(OUTPUT).bin 0 > $(OUTPUT).ram

clean:	
	rm -f $(OBJS) $(OUTPUT).elf $(OUTPUT).bin $(OUTPUT).ram

%.o:		%.c
				${CC} $(CFLAGS) $(PTPD_CFLAGS) $(INCLUDE_DIR) $(LIB_DIR) -c $^ -o $@

load:	#all
		./tools/lm32-loader $(OUTPUT).bin

tools:
			make -C tools
		
fpga:
		- killall -9 vuart_console
		../loadfile ../spec_top.bin
		./tools/zpu-loader $(OUTPUT).bin
