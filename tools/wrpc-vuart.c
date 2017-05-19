/**
 * Author: Federico Vaga <federico.vaga@cern.ch>
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>
#include <termios.h>
#include <getopt.h>
#include <errno.h>

#include <hw/wb_uart.h>
#include <libdevmap.h>

static void wrpc_vuart_help(char *prog)
{
	const char *mapping_help_str;

	mapping_help_str = dev_mapping_help();
	fprintf(stderr, "%s [options]\n", prog);
	fprintf(stderr, "%s\n", mapping_help_str);
	fprintf(stderr, "Vuart specific option: [-k(keep terminal)]\n");
}

/**
 * It receives a single byte
 * @param[in] vuart token from dev_map()
 *
 *
 */
static int8_t wr_vuart_rx(struct mapping_desc *vuart)
{
	int rdr = ((volatile struct UART_WB *)vuart->base)->HOST_RDR;
	if (vuart->is_be)
		rdr = ntohl(rdr);

	return (rdr & UART_HOST_RDR_RDY) ? UART_HOST_RDR_DATA_R(rdr) : -1;
}

/**
 * It transmits a single byte
 * @param[in] vuart token from dev_map()
 */
static void wr_vuart_tx(struct mapping_desc *vuart, char data)
{
	volatile struct UART_WB *ptr = (volatile struct UART_WB *)vuart->base;
	int sr = (vuart->is_be) ? ntohl(ptr->SR) : ptr->SR;
	uint32_t val;

	while(sr & UART_SR_RX_RDY)
		sr = (vuart->is_be) ? ntohl(ptr->SR) : ptr->SR;
	val = (vuart->is_be) ? htonl(UART_HOST_TDR_DATA_W(data)) :
		UART_HOST_TDR_DATA_W(data);
	ptr->HOST_TDR =  val;
}

/**
 * It reads a number of bytes and it stores them in a given buffer
 * @param[in] vuart token from dev_map()
 * @param[out] buf destination for read bytes
 * @param[in] size numeber of bytes to read
 *
 * @return the number of read bytes
 */
static size_t wr_vuart_read(struct mapping_desc *vuart, char *buf, size_t size)
{
	size_t s = size, n_rx = 0;
	int8_t c;

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
 * @param[in] vuart token from dev_map()
 * @param[in] buf buffer to write
 * @param[in] size numeber of bytes to write
 *
 * @return the number of written bytes
 */
static size_t wr_vuart_write(struct mapping_desc *vuart, char *buf, size_t size)
{

	size_t s = size;

	while(s--)
		wr_vuart_tx(vuart, *buf++);

	return size;
}

static void wrpc_vuart_term_main(struct mapping_desc *vuart, int keep_term)
{
	struct termios oldkey, newkey;
	//above is place for old and new port settings for keyboard teletype
	int need_exit = 0;
	fd_set fds;
	int ret;
	char rx, tx;

	fprintf(stderr, "[press C-a to exit]\n");

	if(!keep_term) {
		tcgetattr(STDIN_FILENO,&oldkey);
		newkey.c_cflag = B9600 | CS8 | CLOCAL | CREAD;
		newkey.c_iflag = IGNPAR;
		newkey.c_oflag = 0;
		newkey.c_lflag = 0;
		newkey.c_cc[VMIN]=1;
		newkey.c_cc[VTIME]=0;
		tcflush(STDIN_FILENO, TCIFLUSH);
		tcsetattr(STDIN_FILENO,TCSANOW,&newkey);
	}
	while(!need_exit) {
		struct timeval tv = {0, 10000};

		FD_ZERO(&fds);
		FD_SET(STDIN_FILENO, &fds);

		/*
		 * Check if the STDIN has characters to read
		 * (what the user writes)
		 */
		ret = select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
		switch (ret) {
		case -1:
			perror("select");
			break;
		case 0: /* timeout */
			break;
		default:
			if(!FD_ISSET(STDIN_FILENO, &fds))
				break;
			/* The user wrote something */
			do {
				ret = read(STDIN_FILENO, &tx, 1);
			} while (ret < 0 && errno == EINTR);
			if (ret != 1) {
				fprintf(stderr, "nothing to read. Port disconnected?\n");
				need_exit = 1; /* kill */
			}
			/* If the user character is C-a, then kill */
			if(tx == '\x01')
				need_exit = 1;

			ret = wr_vuart_write(vuart, &tx, 1);
			if (ret != 1) {
				fprintf(stderr, "Unable to write (errno: %d)\n", errno);
				need_exit = 1;
			}
			break;
		}

		/* Print all the incoming charactes */
		while((wr_vuart_read(vuart, &rx, 1)) == 1)
			fprintf(stderr,"%c", rx);

	}

	if(!keep_term)
		tcsetattr(STDIN_FILENO, TCSANOW, &oldkey);
}


int main(int argc, char *argv[])
{
	char c;
	int keep_term = 0;
	struct mapping_args *map_args;
	struct mapping_desc *vuart = NULL;

	map_args = dev_parse_mapping_args(argc, argv);
	if (!map_args) {
		wrpc_vuart_help(argv[0]);
		goto out;
	}

	/* Parse specific args */
	while ((c = getopt (argc, argv, "k")) != -1) {
		switch (c) {
		case 'k':
			keep_term = 1;
			break;
		case 'h':
			wrpc_vuart_help(argv[0]);
			break;
		case '?':
			break;
		}
	}

	vuart = dev_map(map_args, sizeof(struct UART_WB));
	if (!vuart) {
		fprintf(stderr, "%s: vuart_open() failed: %s\n", argv[0],
			strerror(errno));
		goto out;
	}

	wrpc_vuart_term_main(vuart, keep_term);
	dev_unmap(vuart);

	return 0;
out:
	return -1;
}
