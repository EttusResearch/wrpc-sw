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
PPSI = ppsi

# we miss CONFIG_ARCH_LM32 as we have no other archs by now
obj-y = arch/lm32/crt0.o arch/lm32/irq.o arch/lm32/debug.o
LDS = arch/lm32/ram.ld

obj-y += wrc_main.o
obj-y += softpll/softpll_ng.o


# our linker script is preprocessed, so have a rule here
%.ld: %.ld.S $(AUTOCONF)
	$(CC) -include $(AUTOCONF) -E -P $*.ld.S -o $@


cflags-y =	-ffreestanding -include $(AUTOCONF) -Iinclude -I. -Isoftpll
cflags-y +=	-I$(CURDIR)/pp_printf

cflags-$(CONFIG_PTP_NOPOSIX) += \
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
	-I$(PTP_NOPOSIX)/libptpnetif \
	-I$(PTP_NOPOSIX)/PTPWRd

obj-$(CONFIG_PTP_NOPOSIX) += wrc_ptp_noposix.o \
	monitor/monitor.o \
	lib/ptp-noposix-wrappers.o \
	$(PTP_NOPOSIX)/PTPWRd/arith.o \
	$(PTP_NOPOSIX)/PTPWRd/bmc.o \
	$(PTP_NOPOSIX)/PTPWRd/dep/msg.o \
	$(PTP_NOPOSIX)/PTPWRd/dep/net.o \
	$(PTP_NOPOSIX)/PTPWRd/dep/sys.o \
	$(PTP_NOPOSIX)/PTPWRd/dep/timer.o \
	$(PTP_NOPOSIX)/PTPWRd/dep/wr_servo.o \
	$(PTP_NOPOSIX)/PTPWRd/dep/servo.o \
	$(PTP_NOPOSIX)/PTPWRd/protocol.o \
	$(PTP_NOPOSIX)/PTPWRd/wr_protocol.o \
	$(PTP_NOPOSIX)/libposix/freestanding-startup.o

cflags-$(CONFIG_PPSI) += \
	-ffreestanding \
	-include include/ppsi-wrappers.h \
	-Iinclude \
	-I$(PPSI)/include \
	-I$(PPSI)/arch-spec \
	-I$(PPSI)/arch-spec/include \
	-I$(PPSI)/proto-ext-whiterabbit \
	-Iboards/spec

# FIXM: The following it temporary, untile we clean up
cflags-$(CONFIG_PPSI) += \
	-I$(PTP_NOPOSIX)/PTPWRd \
	-include $(PTP_NOPOSIX)/PTPWRd/dep/trace.h \

obj-$(CONFIG_PPSI) += wrc_ptp_ppsi.o \
	monitor/monitor_ppsi.o \
	lib/ppsi-wrappers.o \
	$(PPSI)/ppsi.o \
	$(PPSI)/arch-spec/libarch.a

CFLAGS_PLATFORM  = -mmultiply-enabled -mbarrel-shift-enabled
LDFLAGS_PLATFORM = -mmultiply-enabled -mbarrel-shift-enabled \
	-nostdlib -T $(LDS)

include shell/shell.mk
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

all: tools $(OUTPUT).ram $(OUTPUT).vhd $(OUTPUT).mif

.PRECIOUS: %.elf %.bin
.PHONY: all tools clean gitmodules $(PPSI)/ppsi.o

$(PPSI)/ppsi.o:
	$(MAKE) -C $(PPSI) ARCH=spec PROTO_EXT=whiterabbit \
		CROSS_COMPILE=$(CROSS_COMPILE) CONFIG_NO_PRINTF=y

$(OUTPUT).elf: $(LDS) $(AUTOCONF) gitmodules $(OUTPUT).o config.o
	$(CC) $(CFLAGS) -DGIT_REVISION=\"$(REVISION)\" -c revision.c
	${CC} -o $@ revision.o config.o $(OUTPUT).o $(LDFLAGS)
	${OBJDUMP} -d $(OUTPUT).elf > $(OUTPUT)_disasm.S
	$(SIZE) $@

$(OUTPUT).o: $(OBJS)
	$(LD) --gc-sections -e _start -r $(OBJS) -T bigobj.lds -o $@

config.o: .config
	sed '1,3d' .config > .config.bin
	dd bs=1 count=1 if=/dev/zero 2> /dev/null >> .config.bin
	$(OBJCOPY) -I binary -O elf32-lm32 -B lm32 \
		--rename-section .data=.data.config  .config.bin $@
	rm -f .config.bin

%.bin: %.elf
	${OBJCOPY} -O binary $^ $@

%.ram: tools %.bin
	./tools/genraminit $*.bin 0 > $@

%.vhd: tools %.bin
	./tools/genramvhd -s `. ./.config; echo $$CONFIG_RAMSIZE` $*.bin > $@

%.mif: tools %.bin
	./tools/genrammif $*.bin `. ./.config; echo $$CONFIG_RAMSIZE` > $@

$(AUTOCONF): silentoldconfig

$(OBJS): include/board.h

include/board.h:
	ln -sf ../boards/$(BOARD)/board.h include/board.h


clean:
	rm -f $(OBJS) $(OUTPUT).elf $(OUTPUT).bin $(OUTPUT).ram include/board.h
	$(MAKE) -C $(PPSI) clean

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

scripts_basic config:
	$(MAKE) -f Makefile.kconfig $@

%config:
	$(MAKE) -f Makefile.kconfig $@

defconfig:
	$(MAKE) -f Makefile.kconfig spec_defconfig

.config: silentoldconfig

