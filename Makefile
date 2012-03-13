CROSS_COMPILE = lm32-elf-

OBJS_WRC = 	wrc_main.o \
						wrc_ptp.o \
						monitor/monitor.o

PTP_NOPOSIX = ptp-noposix

INCLUDE_DIRS = -I$(PTP_NOPOSIX)/wrsw_hal -I$(PTP_NOPOSIX)/libptpnetif -Isoftpll -Iinclude
					 
CFLAGS_PTPD  = -ffreestanding -DPTPD_FREESTANDING -DWRPC_EXTRA_SLIM -DPTPD_MSBF -DPTPD_DBG \
							 -DPTPD_NO_DAEMON -DNEW_SINGLE_WRFSM -DPTPD_TRACE_MASK=0 \
							 -include $(PTP_NOPOSIX)/compat.h \
							 -include $(PTP_NOPOSIX)/PTPWRd/dep/trace.h \
							 -include $(PTP_NOPOSIX)/libposix/ptpd-wrappers.h

OBJS_PTPD = $(PTP_NOPOSIX)/PTPWRd/arith.o \
						$(PTP_NOPOSIX)/PTPWRd/bmc.o \
						$(PTP_NOPOSIX)/PTPWRd/dep/msg.o \
						$(PTP_NOPOSIX)/PTPWRd/dep/net.o \
						$(PTP_NOPOSIX)/PTPWRd/dep/sys.o \
						$(PTP_NOPOSIX)/PTPWRd/dep/timer.o \
						$(PTP_NOPOSIX)/PTPWRd/dep/wr_servo.o \
						$(PTP_NOPOSIX)/PTPWRd/dep/servo.o \
						$(PTP_NOPOSIX)/PTPWRd/protocol.o \
						$(PTP_NOPOSIX)/PTPWRd/wr_protocol.o \
						$(PTP_NOPOSIX)/libposix/freestanding-startup.o \
						$(PTP_NOPOSIX)/libposix/freestanding-wrapper.o \
						$(PTP_NOPOSIX)/libposix/net.o

CFLAGS_PLATFORM  = -mmultiply-enabled -mbarrel-shift-enabled 
LDFLAGS_PLATFORM = -mmultiply-enabled -mbarrel-shift-enabled   -nostdlib -T target/lm32/ram.ld 

OBJS_PLATFORM=target/lm32/crt0.o target/lm32/irq.o target/lm32/debug.o

include shell/shell.mk
include tests/tests.mk
include lib/lib.mk
include softpll/softpll.mk
include dev/dev.mk

CC=$(CROSS_COMPILE)gcc
OBJDUMP=$(CROSS_COMPILE)objdump
OBJCOPY=$(CROSS_COMPILE)objcopy
SIZE=$(CROSS_COMPILE)size
					 
CFLAGS= $(CFLAGS_PLATFORM) $(CFLAGS_PTPD) $(INCLUDE_DIRS) -ffunction-sections -fdata-sections -Os -Iinclude -include include/trace.h $(PTPD_CFLAGS) -I$(PTP_NOPOSIX)/PTPWRd -I. -Isoftpll
LDFLAGS= $(LDFLAGS_PLATFORM) -ffunction-sections -fdata-sections --gc-sections -Os -Iinclude
OBJS=$(OBJS_PLATFORM) $(OBJS_WRC) $(OBJS_PTPD) $(OBJS_SHELL) $(OBJS_TESTS) $(OBJS_LIB) $(OBJS_SOFTPLL) $(OBJS_DEV)
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
				./tools/genramvhd -s 65536 $(OUTPUT).bin > $(OUTPUT).vhd

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
