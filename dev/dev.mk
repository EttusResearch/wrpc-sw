obj-y += \
	dev/eeprom.o \
	dev/endpoint.o \
	dev/ep_pfilter.o \
	dev/i2c.o \
	dev/minic.o \
	dev/pps_gen.o \
	dev/syscon.o \
	dev/sfp.o \
	dev/sdb.o \
	dev/rxts_calibrator.o

obj-$(CONFIG_SOCKITOWM) +=	dev/onewire.o
obj-$(CONFIG_W1) +=		dev/w1.o	dev/w1-hw.o	dev/w1-temp.o
obj-$(CONFIG_W1) +=		dev/mac.o
obj-$(CONFIG_UART) +=		dev/uart.o
obj-$(CONFIG_UART_SW) +=	dev/uart-sw.o
