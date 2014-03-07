# and don't touch the rest unless you know what you're doing.
CROSS_COMPILE ?= lm32-elf-

CC =		$(CROSS_COMPILE)gcc
LD =		$(CROSS_COMPILE)ld
OBJDUMP =	$(CROSS_COMPILE)objdump
OBJCOPY =	$(CROSS_COMPILE)objcopy
SIZE =		$(CROSS_COMPILE)size

CFLAGS = -Os -I../../include -I../../softpll
OBJS = startup.o main.o
LDS = ram.ld
OUTPUT=wrc_simulation_sw

$(OUTPUT): $(LDS) $(OBJS)
	${CC} -o $(OUTPUT).elf -nostartfiles $(OBJS) -T $(LDS)
	${OBJCOPY} -O binary $(OUTPUT).elf $(OUTPUT).bin
#	${OBJDUMP} -D $(OUTPUT).elf 
	$(SIZE) $(OUTPUT).elf
	../genramvhd -p wrc_simulation_firmware $(OUTPUT).bin >  wrc_simulation_firmware_pkg.vhd
	../genraminit $(OUTPUT).bin 1000 > wrc-simulation.ram
