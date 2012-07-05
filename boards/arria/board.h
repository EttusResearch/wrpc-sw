#ifndef __BOARD_H
#define __BOARD_H

#include <hw/memlayout.h>

/* Board-specific parameters */

/* WR Core system/CPU clock frequency in Hz */
#define CPU_CLOCK 125000000ULL

/* WR Reference clock period (picoseconds) and frequency (Hz) */
#define REF_CLOCK_PERIOD_PS 8000
#define REF_CLOCK_FREQ_HZ 125000000

/* Baud rate of the builtin UART (does not apply to the VUART) */
#define UART_BAUDRATE 115200ULL

/* Maximum number of simultaneously created sockets */
#define NET_MAX_SOCKETS 3

/* Socket buffer size, determines the max. RX packet size */
#define NET_SKBUF_SIZE 512

/* Number of auxillary clock channels - usually equal to the number of FMCs */
#define NUM_AUX_CLOCKS 1

int board_init();
int board_update();

#endif
