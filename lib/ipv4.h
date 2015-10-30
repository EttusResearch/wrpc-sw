/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#ifndef IPV4_H
#define IPV4_H

#include <inttypes.h>

void ipv4_init(void);
void ipv4_poll(void);

/* Internal to IP stack: */
unsigned int ipv4_checksum(unsigned short *buf, int shorts);

void arp_init(void);
void arp_poll(void);

extern int needIP;
void setIP(unsigned char *IP);
void getIP(unsigned char *IP);

int process_icmp(uint8_t * buf, int len);
int process_bootp(uint8_t * buf, int len);	/* non-zero if IP was set */
int send_bootp(uint8_t * buf, int retry);

#endif
