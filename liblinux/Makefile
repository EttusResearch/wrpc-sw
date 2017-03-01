# If it exists includes Makefile.specific. In this Makefile, you should put
# specific Makefile code that you want to run before this. For example,
# build a particular environment.
-include Makefile.specific

LIB = libwhiterabbit.a
LOBJ := vuart.o

CFLAGS = -Wall -ggdb -O2 -fPIC -Werror -I./ -I../include
LDFLAGS = -L. -lwhiterabbit

%: %.c $(LIB)
	$(CC) $(CFLAGS) $*.c $(LDFLAGS) -o $@

$(LIB): $(LOBJ)
	$(AR) r $@ $^
clean:
	rm -f $(LIB) .depend *.o *~

.depend: Makefile $(wildcard *.c *.h ../*.h)
	$(CC) $(CFLAGS) -M $(LOBJ:.o=.c) -o $@

-include .depend
