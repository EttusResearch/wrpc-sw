/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#ifndef __BOARD_WRC_H
#define __BOARD_WRC_H
/*
 * This is meant to be automatically included by the Makefile,
 * when wrpc-sw is build for wrc (node) -- as opposed to wrs (switch)
 */

#include <hw/memlayout.h>

/* Board-specific parameters */
#define TICS_PER_SECOND 1000

/* WR Core system/CPU clock frequency in Hz */
#define CPU_CLOCK 62500000ULL

/* WR Reference clock period (picoseconds) and frequency (Hz) */
#ifdef CONFIG_WR_NODE_PCS16
#  define REF_CLOCK_PERIOD_PS 16000
#  define REF_CLOCK_FREQ_HZ 62500000
#else
#  define REF_CLOCK_PERIOD_PS 8000
#  define REF_CLOCK_FREQ_HZ 125000000
#endif

/* Baud rate of the builtin UART (does not apply to the VUART) */
#define UART_BAUDRATE 115200ULL

/* Maximum number of simultaneously created sockets */
#define NET_MAX_SOCKETS 12

/* Socket buffer size, determines the max. RX packet size */
#define NET_MAX_SKBUF_SIZE 512

/* Number of auxillary clock channels - usually equal to the number of FMCs */
#define NUM_AUX_CLOCKS 1

int board_init(void);
int board_update(void);

/* spll parameter that are board-specific */
#ifdef CONFIG_WR_NODE_PCS16
#  define BOARD_DIVIDE_DMTD_CLOCKS	0
#else
#  define BOARD_DIVIDE_DMTD_CLOCKS	1
#endif
#define BOARD_MAX_CHAN_REF		1
#define BOARD_MAX_CHAN_AUX		2
#define BOARD_MAX_PTRACKERS		1

#ifdef CONFIG_IP
#define HAS_IP 1
#else
#define HAS_IP 0
#endif

#endif /* __BOARD_WRC_H */
