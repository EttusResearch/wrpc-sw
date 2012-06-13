#include <string.h>

#include "endpoint.h"
#include "ipv4.h"

uint8_t myMAC[6];
uint8_t myIP[4];

unsigned int ipv4_checksum(unsigned short* buf, int shorts) {
  int i;
  unsigned int sum;
  
  sum = 0;
  for (i = 0; i < shorts; ++i)
    sum += buf[i];
  
  sum = (sum >> 16) + (sum & 0xffff);
  sum += (sum >> 16);
  
  return (~sum & 0xffff);
}

void ipv4_init(const char* if_name, uint32_t ip) {
  uint32_t ip_bigendian;
  
  ip_bigendian = htonl(ip);
  memcpy(myIP, &ip_bigendian, 4);
  get_mac_addr(myMAC);
  
  arp_init(if_name);
  icmp_init(if_name);
  
  TRACE_DEV("My IP: %d.%d.%d.%d\n", myIP[0], myIP[1], myIP[2], myIP[3]);
}

void ipv4_poll(void) {
  arp_poll();
  icmp_poll();
}
