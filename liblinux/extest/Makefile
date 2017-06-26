# If it exists includes Makefile.specific. In this Makefile, you should put
# specific Makefile code that you want to run before this. For example,
# build a particular environment.
-include Makefile.specific

GIT_VER =? $(shell git describe --always --dirty)
GIT_USR =? $(shell git config --get-all user.name)

LIB = libextest.a
LOBJ := extest.o

CFLAGS = -Wall -ggdb -fPIC -Werror -I./
CFLAGS += -D__GIT_VER__="\"$(GIT_VER)\"" -D__GIT_USR__="\"$(GIT_USR)\""
LDFLAGS = -L. -lextest
ARFLAGS = rc

%: %.c $(LIB)
	$(CC) $(CFLAGS) $*.c $(LDFLAGS) -o $@

$(LIB): $(LOBJ) $(LIB_DEP)
	$(AR) $(ARFLAGS) $@ $^

clean:
	rm -f $(LIB) .depend *.o *~

.depend: Makefile $(wildcard *.c *.h ../*.h)
	$(CC) $(CFLAGS) -M $(LOBJ:.o=.c) -o $@

-include .depend
