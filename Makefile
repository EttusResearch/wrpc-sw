# Tomasz Wlostowski for CERN, 2011,2012

CROSS_COMPILE ?= lm32-elf-
export CROSS_COMPILE

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
obj-y = arch/lm32/crt0.o arch/lm32/irq.o
LDS-$(CONFIG_WR_NODE)   = arch/lm32/ram.ld
LDS-$(CONFIG_WR_SWITCH) = arch/lm32/ram-wrs.ld

obj-$(CONFIG_WR_NODE)   += wrc_main.o
obj-$(CONFIG_WR_SWITCH) += wrs_main.o
obj-$(CONFIG_WR_SWITCH) += ipc/minipc-mem-server.o ipc/rt_ipc.o

# our linker script is preprocessed, so have a rule here
%.ld: %.ld.S $(AUTOCONF) .config
	$(CC) -include $(AUTOCONF) -E -P $*.ld.S -o $@


cflags-y =	-ffreestanding -include $(AUTOCONF) -Iinclude/std -Iinclude \
			-I. -Isoftpll -Iipc
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
	-I$(PPSI)/arch-wrpc \
	-I$(PPSI)/arch-wrpc/include \
	-I$(PPSI)/proto-ext-whiterabbit \
	-Iboards/spec

obj-ppsi = \
	$(PPSI)/ppsi.o

obj-$(CONFIG_PPSI) += \
	monitor/monitor_ppsi.o \
	lib/ppsi-wrappers.o \
	$(obj-ppsi)

CFLAGS_PLATFORM  = -mmultiply-enabled -mbarrel-shift-enabled
LDFLAGS_PLATFORM = -mmultiply-enabled -mbarrel-shift-enabled \
	-nostdlib -T $(LDS-y)

include shell/shell.mk
include lib/lib.mk
include pp_printf/printf.mk
include dev/dev.mk
include softpll/softpll.mk

obj-$(CONFIG_WR_NODE) += check-error.o

obj-$(CONFIG_WR_NODE) += sdb-lib/libsdbfs.a
cflags-$(CONFIG_WR_NODE) += -Isdb-lib

CFLAGS = $(CFLAGS_PLATFORM) $(cflags-y) -Wall \
	-ffunction-sections -fdata-sections -Os \
	-include include/trace.h -ggdb

LDFLAGS = $(LDFLAGS_PLATFORM) \
	-Wl,--gc-sections -Os -lgcc -lc

OBJS = $(obj-y)

OUTPUT-$(CONFIG_WR_NODE)   = wrc
OUTPUT-$(CONFIG_WR_SWITCH) = rt_cpu
OUTPUT := $(OUTPUT-y)

REVISION=$(shell git describe --dirty --always)

all: tools $(OUTPUT).ram $(OUTPUT).vhd $(OUTPUT).mif

.PRECIOUS: %.elf %.bin
.PHONY: all tools clean gitmodules $(PPSI)/ppsi.o

# we need to remove "ptpdump" support for ppsi if RAM size is small and
# we include etherbone
ifneq ($(CONFIG_RAMSIZE),131072)
  ifdef CONFIG_ETHERBONE
    PPSI_USER_CFLAGS = -DCONFIG_NO_PTPDUMP
  endif
endif

PPSI_USER_CFLAGS += -DDIAG_PUTS=uart_sw_write_string

$(obj-ppsi):
	test -f $(PPSI)/.config || $(MAKE) -C $(PPSI) wrpc_defconfig
	$(MAKE) -C $(PPSI) WRPCSW_ROOT=.. \
		CROSS_COMPILE=$(CROSS_COMPILE) CONFIG_NO_PRINTF=y \
		USER_CFLAGS="$(PPSI_USER_CFLAGS)"

sdb-lib/libsdbfs.a:
	$(MAKE) -C sdb-lib

$(OUTPUT).elf: $(LDS-y) $(AUTOCONF) gitmodules $(OUTPUT).o config.o
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

clean:
	rm -f $(OBJS) $(OUTPUT).elf $(OUTPUT).bin $(OUTPUT).ram $(LDS)
	$(MAKE) -C $(PPSI) clean
	$(MAKE) -C sdb-lib clean

%.o:		%.c
	${CC} $(CFLAGS) $(PTPD_CFLAGS) $(INCLUDE_DIR) $(LIB_DIR) -c $*.c -o $@

tools:
	$(MAKE) -C tools

# if needed, check out the submodules (first time only), so users
# who didn't read carefully the manual won't get confused
gitmodules:
	@test -d ptp-noposix/libposix || echo "Checking out submodules"
	@test -d ptp-noposix/libposix || git submodule update --init
	@test -d ppsi/arch-wrpc || git submodule update --init


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

# This forces more compilations than needed, but it's useful
# (we depend on .config and not on include/generated/autoconf.h
# because the latter is touched by silentoldconfig at each build)
$(obj-y): .config $(wildcard include/*.h)
