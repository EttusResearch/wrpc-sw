#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "wb_uart.h"

#include "rr_io.h"

#define BASE_VUART (0xc0000 + 0x20500)

main(int argc, char *argv[])
{
	if(argc<3)
	{
	 	fprintf(stderr,"Usage: %s <bus> <dev/func>\n", argv[0]);
	 	return 0;
	}

	

	if(rr_init(atoi(argv[1]), atoi(argv[2])))
	{
		perror("rr_init");
		return -1;
	}
	
	for(;;)
	{

		int csr ;
		
		csr = rr_readl(BASE_VUART + UART_REG_HOST_RDR);
//		fprintf(stderr,"csr %x\n", csr);
		if(csr & UART_HOST_RDR_RDY)
		{
			fprintf(stderr, "%c", UART_HOST_RDR_DATA_R(csr));
		} else
		
		usleep(1000);
	}
	
	return 0;
}