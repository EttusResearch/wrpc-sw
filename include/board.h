#ifndef __BOARD_H
#define __BOARD_H

#define BASE_UART 	0x60800
#define BASE_GPIO 	0x60400
#define BASE_TIMER 	0x61000
#define BASE_PPSGEN 0x50000
#define BASE_EP   	0x20000 
#define BASE_MINIC  0x10000

#define CPU_CLOCK 62500000ULL

#define UART_BAUDRATE 0 /* not a real UART */

#define GPIO_PIN_LED (0)  

static inline int delay(int x)
{
  while(x--) asm volatile("nop");
}
  
#endif
