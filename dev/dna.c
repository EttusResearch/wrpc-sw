#include "board.h"
#include "syscon.h"

#define DNA_DATA 1
#define DNA_CLK 4
#define DNA_SHIFT 3
#define DNA_READ 2

void dna_read(uint32_t *lo, uint32_t *hi)
{
 	uint64_t dna = 0;
 	int i;
 	
 	gpio_out(DNA_DATA, 0);
 	delay(10);
 	gpio_out(DNA_CLK, 0);
 	delay(10);
 	gpio_out(DNA_READ, 1);
 	delay(10);
 	gpio_out(DNA_SHIFT, 0);
 	delay(10);
 	
 	delay(10);
 	gpio_out(DNA_CLK, 1);
 	delay(10);
 	if(gpio_in(DNA_DATA)) dna |= 1;
 	delay(10);
 	gpio_out(DNA_CLK, 0);
 	delay(10);
 	gpio_out(DNA_READ, 0);
 	gpio_out(DNA_SHIFT, 1);
 	delay(10);

 	
 	for(i=0;i<57;i++)
 	{
	dna <<= 1;
 	delay(10);
 	delay(10);
 	gpio_out(DNA_CLK, 1);
 	delay(10);
 	if(gpio_in(DNA_DATA)) dna |= 1;
 	delay(10);
 	gpio_out(DNA_CLK, 0);
 	delay(10);
 	}
 	
	*hi = (uint32_t) (dna >> 32);
	*lo = (uint32_t) dna;
}
