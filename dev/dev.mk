obj-y += \
	dev/eeprom.o \
	dev/endpoint.o \
	dev/ep_pfilter.o \
	dev/i2c.o \
	dev/minic.o \
	dev/pps_gen.o \
	dev/syscon.o \
	dev/sfp.o \
	dev/onewire.o \
	dev/sdb.o

obj-$(CONFIG_UART) +=		dev/uart.o
obj-$(CONFIG_UART_SW) +=	dev/uart-sw.o
