#include "board.h"

volatile int irq_cnt = 0;
__attribute__((interrupt)) void softpll_irq()
{
    irq_cnt++;
    uart_write_byte('x');
}