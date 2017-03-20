#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <termios.h>
#include <sys/select.h>
#include <sys/time.h>

#include "irq.h"
#include "uart.h"
#include "endpoint.h"
#include "pps_gen.h"
#include "rxts_calibrator.h"

#include "host.h"

uint32_t _endram;

int wrc_stat_running;

/* timer */

void usleep_init(void)
{ printf("%s\n", __func__); }

void timer_init(uint32_t enable)
{ printf("%s\n", __func__); }

void timer_delay(uint32_t tics)
{
	usleep(tics * 1000);
}

uint32_t uptime_sec;

uint32_t timer_get_tics(void)
{
	struct timeval tv;
	uint64_t msecs;

	gettimeofday(&tv, NULL);
	msecs = tv.tv_sec * 1000 + tv.tv_usec / 1000;
	return msecs;
}

void shw_pps_gen_get_time(uint64_t * seconds, uint32_t * nanoseconds)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	if (seconds)
		*seconds = ts.tv_sec;
	if (nanoseconds)
		*nanoseconds = ts.tv_nsec / NS_PER_CLOCK;
}



/* uart */
void uart_init_hw(void)
{
	struct termios t;

	printf("%s\n", __func__);

	tcgetattr(STDIN_FILENO, &t);
	cfmakeraw(&t);
	t.c_oflag |= ONLCR | OPOST;
	tcsetattr(STDIN_FILENO, 0, &t);
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stdin, NULL, _IONBF, 0);
	printf("UART simulator on the host: use ctrl-C or ctrl-D to exit\n");

}

void uart_exit(int i)
{
	system("stty sane");
	exit(i);
}

int uart_read_byte(void)
{
	fd_set set;
	int ret = -1;
	struct timeval tv = {0, 0};

	FD_ZERO(&set);
	FD_SET(STDIN_FILENO, & set);
	if (select(STDIN_FILENO + 1, &set, NULL, NULL, &tv) > 0)
		ret = getchar();
	/* Use ctrl-C and ctrl-D specially (hack!) */
	if (ret == 3 || ret == 4) {
		uart_exit(0);
	}
	return ret;
}

/* unused stuff */

void disable_irq(void)
{ printf("%s\n", __func__); }

void enable_irq(void)
{ printf("%s\n", __func__); }

int wrc_mon_gui(void)
{ printf("%s\n", __func__); return 0;}

void sdb_find_devices(void)
{ printf("%s\n", __func__); }

int calib_t24p(int mode, uint32_t *value)
{ return 0; }

int ep_get_bitslide(void)
{ return 0; }

int ep_enable(int enabled, int autoneg)
{ return 0; }

void ep_init(uint8_t mac_addr[])
{ printf("%s\n", __func__); }

void pfilter_init_default(void)
{}

static int task_relax(void)
{
	usleep(1000);
	return 1; /* we did something */
}
DEFINE_WRC_TASK(relax) = {
	.name = "relax",
	.job = task_relax,
};
