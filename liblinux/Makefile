# If it exists includes Makefile.specific. In this Makefile, you should put
# specific Makefile code that you want to run before this. For example,
# build a particular environment.
-include Makefile.specific
SUPPORT_CERN_VMEBRIDGE ?= n

LIB = libdevmap.a
LOBJ := devmap.o

CFLAGS = -Wall -ggdb -fPIC -Werror -I./ -I../include
LDFLAGS = -L.

ifeq ($(SUPPORT_CERN_VMEBRIDGE), y)
LIBVME_A = $(VMEBRIDGE)/lib
LIBVME_H = $(VMEBRIDGE)/include
CFLAGS += -I$(LIBVME_H) -I$(LIBVME_A)
CFLAGS += -DSUPPORT_CERN_VMEBRIDGE
LDFLAGS += -L$(LIBVME_A) -lvmebus
LIB_DEP += $(LIBVME_A)/libvmebus.a
endif

%: %.c $(LIB)
	$(CC) $(CFLAGS) $*.c $(LDFLAGS) -o $@

$(LIB): $(LOBJ) $(LIB_DEP)
ifeq ($(SUPPORT_CERN_VMEBRIDGE), y)
	$(AR) r tmp$@ $(LOBJ)
# transform a thin library into a normal one
	echo -e "create $@\naddlib tmp$@\naddlib $(LIBVME_A)/libvmebus.a\nsave\nend" | $(AR) -M
	$(RM) tmp$@
else
	$(AR) r $@ $^
endif

clean:
	rm -f $(LIB) .depend *.o *~

.depend: Makefile $(wildcard *.c *.h ../*.h)
	$(CC) $(CFLAGS) -M $(LOBJ:.o=.c) -o $@

-include .depend
