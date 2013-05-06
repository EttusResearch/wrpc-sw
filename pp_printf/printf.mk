# This is included from ../Makefile, for the wrc build system.
# The Makefile in this directory is preserved from the upstream version

obj-y += pp_printf/printf.o

ppprintf-$(CONFIG_PRINTF_FULL) += pp_printf/vsprintf-full.o
ppprintf-$(CONFIG_PRINTF_MINI) += pp_printf/vsprintf-mini.o
ppprintf-$(CONFIG_PRINTF_NONE) += pp_printf/vsprintf-none.o
ppprintf-$(CONFIG_PRINTF_XINT) += pp_printf/vsprintf-xint.o

ppprintf-y ?= pp_printf/vsprintf-xint.o

obj-y += $(ppprintf-y)


