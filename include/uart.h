#ifndef __UART_H
#define __UART_H

void uart_init_sw(void);
void uart_init_hw(void);
void uart_write_byte(int b);
int uart_write_string(const char *s);
int puts(const char *s);
int uart_read_byte(void);

/* uart-sw is used by ppsi (but may be wrapped to normal uart) */
int uart_sw_write_string(const char *s);


#endif
