/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2011 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <inttypes.h>

#include "board.h"
#include "uart.h"

#include <hw/wb_vuart.h>

#define CALC_BAUD(baudrate) \
    ( ((( (unsigned long long)baudrate * 8ULL) << (16 - 7)) + \
      (CPU_CLOCK >> 8)) / (CPU_CLOCK >> 7) )

volatile struct UART_WB *uart;

void uart_init()
{
	uart = (volatile struct UART_WB *)BASE_UART;
	uart->BCR = CALC_BAUD(UART_BAUDRATE);
}

void uart_write_byte(int b)
{
	if (b == '\n')
		uart_write_byte('\r');
	while (uart->SR & UART_SR_TX_BUSY)
		;
	uart->TDR = b;
}

int uart_write_string(const char *s)
{
	const char *t = s;
	while (*s)
		uart_write_byte(*(s++));
	return s - t;
}

int puts(const char *s) __attribute__((alias("uart_write_string")));

static int uart_poll()
{
	return uart->SR & UART_SR_RX_RDY;
}

int uart_read_byte()
{
	if (!uart_poll())
		return -1;

	return uart->RDR & 0xff;
}
