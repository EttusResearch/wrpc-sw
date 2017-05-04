/**
 * Author: Federico Vaga <federico.vaga@cern.ch>
 */

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <hw/wb_uart.h>

#include "libwhiterabbit.h"


#define MAX_SIZE 0x100000
/**
 * It opens a White Rabbit Virtual UART
 * @parma[in] resource_path path to the resource file which can be mmapped
 * @param[in] offset virtual address where the UART is available within
 *            the resource
 *
 * @todo make the open more user firendly
 *
 * @return NULL on error and errno is approproately set.
 *         On success a vuart token.
 */
struct wr_vuart *wr_vuart_open(char *resource_path, unsigned int offset)
{
	struct wr_vuart *vuart;

	vuart = malloc(sizeof(struct wr_vuart));
	if (!vuart)
		goto out_alloc;

	vuart->fd = open(resource_path, O_RDWR | O_SYNC);
	if (vuart->fd <=0)
		goto out_open;

	vuart->mmap = mmap(NULL, MAX_SIZE & ~(getpagesize() - 1),
			   PROT_READ | PROT_WRITE,
			   MAP_SHARED, vuart->fd, 0);
	if ((long)vuart->mmap == -1)
		goto out_mmap;

	vuart->base = vuart->mmap + offset;

	return vuart;

out_mmap:
	close(vuart->fd);
out_open:
	free(vuart);
out_alloc:
	return NULL;
}


/**
 * It releases the resources allocated by wr_vuart_open(). The vuart will
 * not be available anymore and the token will be invalidated
 * @param[in,out] vuart token from wr_vuart_open()
 */
void wr_vuart_close(struct wr_vuart *vuart)
{
	close(vuart->fd);
	free(vuart);
}


/**
 * It reads a number of bytes and it stores them in a given buffer
 * @param[in] vuart token from wr_vuart_open()
 * @param[out] buf destination for read bytes
 * @param[in] size numeber of bytes to read
 *
 * @return the number of read bytes
 */
size_t wr_vuart_read(struct wr_vuart *vuart, char *buf, size_t size)
{
	size_t s = size, n_rx = 0;
	int c;

	while(s--) {
		c =  wr_vuart_rx(vuart);
		if(c < 0)
			return n_rx;
		*buf++ = c;
		n_rx ++;
	}
	return n_rx;
}


/**
 * It writes a number of bytes from a given buffer
 * @param[in] vuart token from wr_vuart_open()
 * @param[in] buf buffer to write
 * @param[in] size numeber of bytes to write
 *
 * @return the number of written bytes
 */
size_t wr_vuart_write(struct wr_vuart *vuart, char *buf, size_t size)
{

	size_t s = size;

	while(s--)
		wr_vuart_tx(vuart, *buf++);

	return size;
}


/**
 * It receives a single byte
 * @param[in] vuart token from wr_vuart_open()
 *
 *
 */
int wr_vuart_rx(struct wr_vuart *vuart)
{
	int rdr = vuart->base->RDR;

	return (rdr & UART_HOST_RDR_RDY) ? UART_HOST_RDR_DATA_R(rdr) : -1;
}


/**
 * It transmits a single byte
 * @param[in] vuart token from wr_vuart_open()
 */
void wr_vuart_tx(struct wr_vuart *vuart, int data)
{
	while(vuart->base->SR & UART_SR_RX_RDY)
		;
	vuart->base->HOST_TDR =  UART_HOST_TDR_DATA_W(data);
}
