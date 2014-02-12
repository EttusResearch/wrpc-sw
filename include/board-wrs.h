#ifndef __BOARD_H
#define __BOARD_H

#define TICS_PER_SECOND 100000

#define CPU_CLOCK             62500000
#define REF_CLOCK_FREQ_HZ     62500000
#define REF_CLOCK_PERIOD_PS   16000

#define UART_BAUDRATE 115200

/* RT CPU Memory layout */
#define BASE_UART 0x10000
#define BASE_SOFTPLL 0x10100
#define BASE_SPI 0x10200
#define BASE_GPIO 0x10300
#define BASE_TIMER 0x10400
#define BASE_PPS_GEN 0x10500

/* spll parameter that are board-specific */
#define BOARD_DIVIDE_DMTD_CLOCKS	0
#define BOARD_MAX_CHAN_REF		18
#define BOARD_MAX_CHAN_AUX		1
#define BOARD_MAX_PTRACKERS		18

#endif
