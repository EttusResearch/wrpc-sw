#ifndef IPV4_H
#define IPV4_H

void ipv4_init(const char* if_name, uint32_t ip);
void ipv4_poll(void);

/* Internal to IP stack: */
unsigned int ipv4_checksum(unsigned short* buf, int shorts);
void arp_init(const char* if_name);
void icmp_init(const char* if_name);
void arp_poll(void);
void icmp_poll(void);

extern uint8_t myMAC[6];
extern uint8_t myIP[4];

#endif
