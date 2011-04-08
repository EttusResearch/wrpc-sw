#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "wb_vuart.h"

#include "rr_io.h"

#define BASE_VUART (0x80000 + 0x60800)

main()
{
	if(rr_init())
	{
		perror("rr_init");
		return -1;
	}
	
	for(;;)
	{

		int csr ;
		
		csr = rr_readl(BASE_VUART + UART_REG_DEBUG_CSR);

		if(! (csr & UART_DEBUG_CSR_EMPTY))
		{
			char c = rr_readl(BASE_VUART + UART_REG_DEBUG_R0) & 0xff;
			fprintf(stderr, "%c", c);
		} else
		
		usleep(1000);
	}
	
	return 0;
}