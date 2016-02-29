# Tomasz Wlostowski for CERN, 2011,2012
-include $(CURDIR)/.config

# Until we move CONFIG_LM32 to Kconfig, force it here
CONFIG_LM32 = y
CROSS_COMPILE ?= lm32-elf-

export CROSS_COMPILE

CC =		$(CROSS_COMPILE)gcc
LD =		$(CROSS_COMPILE)ld
OBJDUMP =	$(CROSS_COMPILE)objdump
OBJCOPY =	$(CROSS_COMPILE)objcopy
SIZE =		$(CROSS_COMPILE)size


AUTOCONF = $(CURDIR)/include/generated/autoconf.h

PPSI = ppsi

# we miss CONFIG_ARCH_LM32 as we have no other archs by now
obj-$(CONFIG_LM32) = arch/lm32/crt0.o arch/lm32/irq.o
LDS-$(CONFIG_WR_NODE)   = arch/lm32/ram.ld
LDS-$(CONFIG_WR_SWITCH) = arch/lm32/ram-wrs.ld

obj-$(CONFIG_WR_NODE)   += wrc_main.o
obj-$(CONFIG_WR_SWITCH) += wrs_main.o
obj-$(CONFIG_WR_SWITCH) += ipc/minipc-mem-server.o ipc/rt_ipc.o

obj-y += dump-info.o
# our linker script is preprocessed, so have a rule here
%.ld: %.ld.S $(AUTOCONF) .config
	$(CC) -include $(AUTOCONF) -E -P $*.ld.S -o $@


cflags-y =	-ffreestanding -include $(AUTOCONF) -Iinclude \
			-I. -Isoftpll -Iipc
cflags-y +=	-I$(CURDIR)/pp_printf
cflags-$(CONFIG_LM32) +=  -Iinclude/std

cflags-$(CONFIG_PPSI) += \
	-include include/ppsi-wrappers.h \
	-I$(PPSI)/arch-wrpc \
	-I$(PPSI)/proto-ext-whiterabbit \
	-Iboards/spec

# in order to build tools/wrpc-dump, we need these flags, even for wrs builds
cflags-y += \
	-I$(PPSI)/arch-wrpc/include \
	-I$(PPSI)/include

obj-ppsi = \
	$(PPSI)/ppsi.o

obj-$(CONFIG_PPSI) += \
	monitor/monitor_ppsi.o \
	lib/ppsi-wrappers.o \
	$(obj-ppsi)

cflags-$(CONFIG_LM32) += -mmultiply-enabled -mbarrel-shift-enabled
ldflags-$(CONFIG_LM32) = -mmultiply-enabled -mbarrel-shift-enabled \
	-nostdlib -T $(LDS-y)

# packet-filter rules: for CONFIG_VLAN we use both sets
pfilter-y                     := rules-novlan.bin
pfilter-$(CONFIG_VLAN)        += rules-vlan.bin
export pfilter-y

all:

include shell/shell.mk
include lib/lib.mk
include pp_printf/printf.mk
include dev/dev.mk
include softpll/softpll.mk

# ppsi already has div64 (the same one), so only pick it if not using ppsi.
ifndef CONFIG_PPSI
  obj-y += pp_printf/div64.o
endif
# And always complain if we pick the libgcc division: 64/32 = 32 is enough here.
obj-$(CONFIG_LM32) += check-error.o

# add system check functions like stack overflow and check reset
obj-y += system_checks.o

# WR node has SDB support, WR switch does not
obj-$(CONFIG_WR_NODE) += sdb-lib/libsdbfs.a
cflags-$(CONFIG_WR_NODE) += -Isdb-lib

CFLAGS = $(cflags-y) -Wall -Wstrict-prototypes \
	-ffunction-sections -fdata-sections -Os -Wmissing-prototypes \
	-include include/wrc.h -ggdb

LDFLAGS = $(ldflags-y) \
	-Wl,--gc-sections -Os -lgcc -lc

WRC-O-FLAGS-$(CONFIG_LM32) = --gc-sections -e _start

OBJS = $(obj-y)

OUTPUT-$(CONFIG_WR_NODE)   = wrc
OUTPUT-$(CONFIG_WR_SWITCH) = rt_cpu
OUTPUT := $(OUTPUT-y)

GIT_VER = $(shell git describe --always --dirty | sed  's;^wr-switch-sw-;;')

all: tools $(OUTPUT).ram $(OUTPUT).vhd $(OUTPUT).mif

.PRECIOUS: %.elf %.bin
.PHONY: all tools clean gitmodules $(PPSI)/ppsi.o

# we need to remove "ptpdump" support for ppsi if RAM size is small and
# we include etherbone
ifneq ($(CONFIG_RAMSIZE),131072)
  ifdef CONFIG_IP
    PPSI_USER_CFLAGS = -DCONFIG_NO_PTPDUMP
  endif
endif

PPSI_USER_CFLAGS += -DDIAG_PUTS=uart_sw_write_string

PPSI-CFG-y = wrpc_defconfig
PPSI-FLAGS-$(CONFIG_LM32) = CONFIG_NO_PRINTF=y

$(obj-ppsi):
	test -f $(PPSI)/.config || $(MAKE) -C $(PPSI) $(PPSI-CFG-y)
	$(MAKE) -C $(PPSI) ppsi.o WRPCSW_ROOT=.. \
		CROSS_COMPILE=$(CROSS_COMPILE) CONFIG_NO_PRINTF=y
		USER_CFLAGS="$(PPSI_USER_CFLAGS)"

sdb-lib/libsdbfs.a:
	$(MAKE) -C sdb-lib

$(OUTPUT).elf: $(LDS-y) $(AUTOCONF) gitmodules $(OUTPUT).o config.o
	$(CC) $(CFLAGS) -D__GIT_VER__="\"$(GIT_VER)\"" -c revision.c
	${CC} -o $@ revision.o config.o $(OUTPUT).o $(LDFLAGS)
	${OBJDUMP} -d $(OUTPUT).elf > $(OUTPUT)_disasm.S
	$(SIZE) $@

$(OUTPUT).o: $(OBJS)
	$(LD) $(WRC-O-FLAGS-y) -r $(OBJS) -T bigobj.lds -o $@

OBJCOPY-TARGET-$(CONFIG_LM32) = -O elf32-lm32 -B lm32

config.o: .config
	sed '1,3d' .config > .config.bin
	dd bs=1 count=1 if=/dev/zero 2> /dev/null >> .config.bin
	$(OBJCOPY) -I binary $(OBJCOPY-TARGET-y) \
		--rename-section .data=.data.config  .config.bin $@
	rm -f .config.bin

%.bin: %.elf
	${OBJCOPY} -O binary $^ $@

%.ram: tools %.bin
	./tools/genraminit $*.bin 0 > $@

%.vhd: tools %.bin
	./tools/genramvhd -s $(CONFIG_RAMSIZE) $*.bin > $@

%.mif: tools %.bin
	./tools/genrammif $*.bin $(CONFIG_RAMSIZE) > $@

$(AUTOCONF): silentoldconfig

clean:
	rm -f $(OBJS) $(OUTPUT).elf $(OUTPUT).bin $(OUTPUT).ram \
		$(LDS)  rules-*.bin
	$(MAKE) -C $(PPSI) clean
	$(MAKE) -C sdb-lib clean
	$(MAKE) -C tools clean

%.o:		%.c
	${CC} $(CFLAGS) $(PTPD_CFLAGS) $(INCLUDE_DIR) $(LIB_DIR) -c $*.c -o $@

tools: .config
	$(MAKE) -C tools

# if needed, check out the submodules (first time only), so users
# who didn't read carefully the manual won't get confused
gitmodules:
	@test -d ppsi/arch-wrpc || echo "Checking out submodules"
	@test -d ppsi/arch-wrpc || git submodule update --init


# following targets from Makefile.kconfig
silentoldconfig:
	@mkdir -p include/config
	$(MAKE) quiet=quiet_ -f Makefile.kconfig $@

scripts_basic config:
	$(MAKE) quiet=quiet_ -f Makefile.kconfig $@

%config:
	$(MAKE) quiet=quiet_ -f Makefile.kconfig $@

defconfig:
	$(MAKE) quiet=quiet_ -f Makefile.kconfig spec_defconfig

.config: silentoldconfig

# This forces more compilations than needed, but it's useful
# (we depend on .config and not on include/generated/autoconf.h
# because the latter is touched by silentoldconfig at each build)
$(obj-y): .config $(wildcard include/*.h)
