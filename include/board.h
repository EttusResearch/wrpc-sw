#ifndef __BOARD_H
#define __BOARD_H

#define BASE_MINIC    0x40000
#define BASE_EP   	  0x40100 
#define BASE_SOFTPLL  0x40200 
#define BASE_PPSGEN   0x40300
#define BASE_SYSCON   0x40400
#define BASE_UART 	  0x40500
#define BASE_ONEWIRE  0x40600

#define CPU_CLOCK 62500000ULL

#define UART_BAUDRATE 115200ULL /* not a real UART */

static inline int delay(int x)
{
  while(x--) asm volatile("nop");
}
  
#endif
