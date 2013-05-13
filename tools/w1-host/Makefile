
all: libw1.a

libw1.a: w1.o w1-eeprom.o w1-hw.o w1-temp.o
	ar r $@ $^

clean:
	rm -f *.o *.a *~