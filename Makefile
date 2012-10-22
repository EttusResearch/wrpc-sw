# Tomasz Wlostowski for CERN, 2011,2012

# choose your board here.
BOARD = spec

# and don't touch the rest unless you know what you're doing.
CROSS_COMPILE ?= lm32-elf-

CC =		$(CROSS_COMPILE)gcc
LD =		$(CROSS_COMPILE)ld
OBJDUMP =	$(CROSS_COMPILE)objdump
OBJCOPY =	$(CROSS_COMPILE)objcopy
SIZE =		$(CROSS_COMPILE)size

-include $(CURDIR)/.config

AUTOCONF = $(CURDIR)/include/generated/autoconf.h

PTP_NOPOSIX = ptp-noposix

# we miss CONFIG_ARCH_LM32 as we have no other archs by now
obj-y = arch/lm32/crt0.o arch/lm32/irq.o arch/lm32/debug.o
obj-y += wrc_main.o wrc_ptp.o monitor/monitor.o
LDS = arch/lm32/ram.ld

# our linker script is preprocessed, so have a rule here
%.ld: %.ld.S $(AUTOCONF)
	$(CC) -include $(AUTOCONF) -E -P $*.ld.S -o $@


cflags-y = -include $(AUTOCONF)	-Iinclude -I.

cflags-$(CONFIG_PP_PRINTF) += -I$(CURDIR)/pp_printf

cflags-$(CONFIG_PTP_NOPOSIX) += \
	-ffreestanding \
	-DPTPD_FREESTANDING \
	-DWRPC_EXTRA_SLIM \
	-DPTPD_MSBF \
	-DPTPD_DBG \
	-DPTPD_NO_DAEMON \
	-DNEW_SINGLE_WRFSM \
	-DPTPD_TRACE_MASK=0 \
	-include $(PTP_NOPOSIX)/compat.h \
	-include $(PTP_NOPOSIX)/PTPWRd/dep/trace.h \
	-include $(PTP_NOPOSIX)/libposix/ptpd-wrappers.h \
	-I$(PTP_NOPOSIX)/wrsw_hal \
	-I$(PTP_NOPOSIX)/libptpnetif \
	-I$(PTP_NOPOSIX)/softpll \
	-I$(PTP_NOPOSIX)/PTPWRd

obj-$(CONFIG_PTP_NOPOSIX) += $(PTP_NOPOSIX)/PTPWRd/arith.o \
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
	$(PTP_NOPOSIX)/libposix/net.o \
	$(PTP_NOPOSIX)/softpll/softpll_ng.o

CFLAGS_PLATFORM  = -mmultiply-enabled -mbarrel-shift-enabled
LDFLAGS_PLATFORM = -mmultiply-enabled -mbarrel-shift-enabled \
	-nostdlib -T $(LDS)

include shell/shell.mk
include tests/tests.mk
include lib/lib.mk
include pp_printf/printf.mk
include sockitowm/sockitowm.mk
include dev/dev.mk


CFLAGS = $(CFLAGS_PLATFORM) $(cflags-y) -Wall \
	-ffunction-sections -fdata-sections -Os \
	-include include/trace.h

LDFLAGS = $(LDFLAGS_PLATFORM) \
	-Wl,--gc-sections -Os -lgcc -lc

OBJS = $(obj-y)

OUTPUT = wrc

REVISION=$(shell git describe --dirty --always)

all: tools $(OUTPUT).ram $(OUTPUT).vhd

.PRECIOUS: %.elf %.bin
.PHONY: all tools clean gitmodules

$(OUTPUT).elf: $(LDS) $(AUTOCONF) gitmodules $(OUTPUT).o
	$(CC) $(CFLAGS) -DGIT_REVISION=\"$(REVISION)\" -c revision.c
	${CC} -o $@ revision.o $(OUTPUT).o $(LDFLAGS)
	${OBJDUMP} -d $(OUTPUT).elf > $(OUTPUT)_disasm.S
	$(SIZE) $@

$(OUTPUT).o: $(OBJS)
	$(LD) --gc-sections -e _start -r $(OBJS) -o $@

%.bin: %.elf
	${OBJCOPY} -O binary $^ $@

%.ram: tools %.bin
	./tools/genraminit $*.bin 0 > $@

%.vhd: tools %.bin
	./tools/genramvhd -s 90112 $*.bin > $@

$(AUTOCONF): silentoldconfig

$(OBJS): include/board.h

include/board.h:
	ln -sf ../boards/$(BOARD)/board.h include/board.h


clean:
	rm -f $(OBJS) $(OUTPUT).elf $(OUTPUT).bin $(OUTPUT).ram include/board.h

%.o:		%.c
	${CC} $(CFLAGS) $(PTPD_CFLAGS) $(INCLUDE_DIR) $(LIB_DIR) -c $*.c -o $@

tools:
	$(MAKE) -C tools

# if needed, check out the submodules (first time only), so users
# who didn't read carefully the manual won't get confused
gitmodules:
	@test -d ptp-noposix/libposix || echo "Checking out submodules"
	@test -d ptp-noposix/libposix || git submodule update --init
	@test -d ppsi/arch-spec || git submodule update --init


# following targets from Makefile.kconfig
silentoldconfig:
	@mkdir -p include/config
	$(MAKE) -f Makefile.kconfig $@

scripts_basic config %config:
	$(MAKE) -f Makefile.kconfig $@

.config: silentoldconfig

