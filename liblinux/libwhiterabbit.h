/**
 * Author: Federico Vaga <federico.vaga@cern.ch>
 */

#ifndef __LIBWHITERABBIT_H__
#define __LIBWHITERABBIT_H__

struct wr_vuart {
	int fd;
	void *mmap;
	volatile struct UART_WB *base;
};

/**
 * @defgroup vuart Virtual UART
 *@{
 */
extern struct wr_vuart *wr_vuart_open(char *resource_path, unsigned int offset);
extern void wr_vuart_close(struct wr_vuart *vuart);

extern size_t wr_vuart_read(struct wr_vuart *vuart, char *buf, size_t size);
extern size_t wr_vuart_write(struct wr_vuart *vuart, char *buf, size_t size);

extern int wr_vuart_rx(struct wr_vuart *vuart);
extern void wr_vuart_tx(struct wr_vuart *vuart, int data);
/** @}*/
#endif
