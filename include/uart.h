#ifndef __UART_H
#define __UART_H

void uart_init(void);
void uart_write_byte(int b);
void uart_write_string(char *s);
int uart_read_byte(void);

#endif
