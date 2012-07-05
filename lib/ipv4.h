#ifndef IPV4_H
#define IPV4_H

void ipv4_init(const char* if_name);
void ipv4_poll(void);

/* Internal to IP stack: */
unsigned int ipv4_checksum(unsigned short* buf, int shorts);

void arp_init(const char* if_name);
void arp_poll(void);

extern uint8_t myMAC[6];
extern uint8_t myIP[4];
extern int needIP;

int process_icmp(uint8_t* buf, int len);
int process_bootp(uint8_t* buf, int len); /* non-zero if IP was set */
int send_bootp(uint8_t* buf, int retry);

#endif
