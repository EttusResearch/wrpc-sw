obj-y += \
	dev/endpoint.o \
	dev/ep_pfilter.o \
	dev/i2c.o \
	dev/minic.o \
	dev/pps_gen.o \
	dev/syscon.o \
	dev/sfp.o \
	dev/sdb.o \
	dev/rxts_calibrator.o

obj-$(CONFIG_LEGACY_EEPROM) += dev/eeprom.o
obj-$(CONFIG_SDB_EEPROM) += dev/sdb-eeprom.o

obj-$(CONFIG_W1) +=		dev/w1.o	dev/w1-hw.o
obj-$(CONFIG_W1) +=		dev/w1-temp.o	dev/w1-eeprom.o
obj-$(CONFIG_UART) +=		dev/uart.o
obj-$(CONFIG_UART_SW) +=	dev/uart-sw.o
