/* 

Simple Etherbone-VUART bridge.

T.W. 2012, public domain

Todo:
- block polling
- SDB support
- decrappify

WARNING: CRAPPY CODE (as of now).

Building: run make (Etherbone library must be in etherbone subdirectory)

*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <getopt.h>
#include <errno.h>

#include "simple-eb.h"
#include "wb_uart.h"

static int vuart_rx(eb_device_t card)
{
  int rdr = ebs_read(card, 0xe0500 + UART_REG_HOST_RDR);
  if(rdr & UART_HOST_RDR_RDY)
    return UART_HOST_RDR_DATA_R(rdr);
  else
    return -1;
}

static void vuart_tx(eb_device_t card, int c)
{
  while( ebs_read(card, 0xe0500 + UART_REG_SR) & UART_SR_RX_RDY);
  ebs_write(card, 0xe0500 + UART_REG_HOST_TDR,  UART_HOST_TDR_DATA_W(c));
}


size_t spec_vuart_rx(eb_device_t card, char *buffer, size_t size)
{
  size_t s = size, n_rx = 0;
  while(s--)
  {
    int c =  vuart_rx(card);
    if(c < 0)
      return n_rx;
    *buffer++ = c;
    n_rx ++;
  }
  return n_rx;
}

size_t spec_vuart_tx(eb_device_t card, char *buffer, size_t size)
{
  size_t s = size;
  while(s--)
    vuart_tx(card, *buffer++);

  return size;
}

static eb_device_t card;

static int transfer_byte(int from, int is_control) {
	char c;
	int ret;
	do {
		ret = read(from, &c, 1);
	} while (ret < 0 && errno == EINTR);
	if(ret == 1) {
		if(is_control) {
			if(c == '\x01') { // C-a
				return -1;
			}
		}
		spec_vuart_tx(card, &c, 1);
	} else {
		fprintf(stderr, "nothing to read. Port disconnected?\n");
		return -2;
	}
	return 0;
}


uint64_t get_tics()
{
	struct timezone tz = {0, 0};
	struct timeval tv;
	
	gettimeofday(&tv, &tz);
	return tv.tv_sec * 1000000ULL + tv.tv_usec;
}

void term_main(int keep_term)
{
	struct termios oldkey, newkey;
	//above is place for old and new port settings for keyboard teletype
	int need_exit = 0;
	uint64_t last_poll = get_tics();

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
		fd_set fds;
		int ret;
		char rx;
		struct timeval tv = {0, 10000};

		FD_ZERO(&fds);
		FD_SET(STDIN_FILENO, &fds);

		ret = select(STDIN_FILENO+1, &fds, NULL, NULL, &tv);
		if(ret == -1) {
			perror("select");
		} else if (ret > 0) {
			if(FD_ISSET(STDIN_FILENO, &fds)) {
				need_exit = transfer_byte(STDIN_FILENO, 1);
			}
		}


		if(get_tics() - last_poll > 500000)
		{
			while((spec_vuart_rx(card, &rx, 1)) == 1)
				fprintf(stderr,"%c", rx);
				
			last_poll = get_tics();
		}

	}

	if(!keep_term)
		tcsetattr(STDIN_FILENO,TCSANOW,&oldkey);
}

int main(int argc, char **argv)
{
	int bus = -1, dev_fn = -1, c;
	uint32_t vuart_base = 0xe0500;
	int keep_term = 0;

	if(argc < 2)
	{
		printf("usage: %s address\n", argv[0]);
		printf("example: %s udp/192.168.1.101", argv[0]);
		return 0;
	}

	ebs_init();
	ebs_open(&card, argv[1]);

	term_main(0);

	ebs_close(card);
	ebs_shutdown();

	return 0;
}
