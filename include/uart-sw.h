/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2013 CERN (www.cern.ch)
 * Author: Alessandro Rubini <rubini@gnudd.com>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#ifndef __UART_SW_H
#define __UART_SW_H

/* The host code (tools/wrpc-uart-sw) must include this too, for the struct */
#ifdef __lm32__
#include "uart.h" /* we need to offer the same prototypes */
#endif

/* These are currently static but can become Kconfig items */
#define CONFIG_UART_SW_WSIZE  256
#define CONFIG_UART_SW_RSIZE   32

#define UART_SW_MAGIC 0x752d7377 /* "u-sw" */
struct wrc_uart_sw {
	uint32_t magic;
	uint16_t wsize, nwritten;
	uint16_t rsize, nread;
	unsigned char wbuffer[CONFIG_UART_SW_WSIZE];
	unsigned char rbuffer[CONFIG_UART_SW_RSIZE];
};

#endif /* __UART_SW_H */
