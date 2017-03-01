/**
 * Author: Federico Vaga <federico.vaga@cern.ch>
 */


#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <getopt.h>
#include <errno.h>

#include <libwhiterabbit.h>

static void wrpc_vuart_help(char *prog)
{
	fprintf(stderr, "%s [options]\n\n", prog);
	fprintf(stderr, "-a 0x<address>\tAbsolute virtual address for the White Rabbit VUART\n");
}


static void wrpc_vuart_term_main(struct wr_vuart *vuart, int keep_term)
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
	char c, *resource_file;
	int ret, keep_term = 0;
	unsigned int vuart_base;
	struct wr_vuart *vuart;

	while ((c = getopt (argc, argv, "a:f:k")) != -1)
	{
		switch (c)
		{
		case 'a':
			ret = sscanf(optarg, "0x%x", &vuart_base);
			if (ret != 1) {
				wrpc_vuart_help(argv[0]);
				return -1;
			}
			break;
		case 'f':
			resource_file = optarg;
			break;
		case 'k':
			keep_term = 1;
			break;
		default:
			wrpc_vuart_help(argv[0]);
			return -1;
		}
	}

	vuart = wr_vuart_open(resource_file, vuart_base);
	if (!vuart) {
		goto out;
	}

	wrpc_vuart_term_main(vuart, keep_term);
	wr_vuart_close(vuart);

	return 0;
out:
	return -1;
}
