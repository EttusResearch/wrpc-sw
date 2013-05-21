OBJS = simple-eb.o lm32-etheruart.o 
CFLAGS = -g -Ietherbone -I.
LDFLAGS = -Letherbone -letherbone

all:	$(OBJS) 
			gcc -o lm32-etheruart $(OBJS) $(LDFLAGS)
			
clean:
			rm -f lm32-etheruart *.o