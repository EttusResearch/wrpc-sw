
LINUX ?= /lib/modules/$(shell uname -r)/build

# If we compile for the kernel, we need to include real kernel headers.
# The thing is enough a mess that I moved it to a different file
include Makefile.arch


AS              = $(CROSS_COMPILE)as
LD              = $(CROSS_COMPILE)ld
CC              = $(CROSS_COMPILE)gcc
CPP             = $(CC) -E
AR              = $(CROSS_COMPILE)ar
NM              = $(CROSS_COMPILE)nm
STRIP           = $(CROSS_COMPILE)strip
OBJCOPY         = $(CROSS_COMPILE)objcopy
OBJDUMP         = $(CROSS_COMPILE)objdump

# calculate endianness at compile time
ENDIAN := $(shell ./check-endian $(CC))

CFLAGS = -Wall -ggdb -O2
CFLAGS += -I../include/linux -I../include # for <sdb.h>
CFLAGS += -ffunction-sections -fdata-sections
CFLAGS += -Wno-pointer-sign
CFLAGS += $(ENDIAN) $(LINUXINCLUDE)


LIB = libsdbfs.a
OBJS = glue.o access.o

all: $(LIB)

$(OBJS): $(wildcard *.h)

$(LIB): $(OBJS)
	$(AR) r $@ $(OBJS)

clean:
	rm -f $(OBJS) $(LIB) *~ core

# add the other unused targets, so the rule in ../Makefile works
modules install modules_install:
