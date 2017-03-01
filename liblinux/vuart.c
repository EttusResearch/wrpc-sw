/**
 * Author: Federico Vaga <federico.vaga@cern.ch>
 */

#include <stdlib.h>
#include <errno.h>

#include "libwhiterabbit.h"


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
	return NULL;
}


/**
 * It releases the resources allocated by wr_vuart_open(). The vuart will
 * not be available anymore
 * @param[in,out] vuart token from wr_vuart_open()
 */
void wr_vuart_close(struct wr_vuart *vuart)
{
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
	return 0;
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
	return 0;
}


/**
 * It receives a single byte
 * @param[in] vuart token from wr_vuart_open()
 *
 *
 */
int wr_vuart_rx(struct wr_vuart *vuart)
{
	return 0;
}


/**
 * It transmits a single byte
 * @param[in] vuart token from wr_vuart_open()
 */
void wr_vuart_tx(struct wr_vuart *vuart, int data)
{
}
