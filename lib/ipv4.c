#include <string.h>

#include "endpoint.h"
#include "ipv4.h"
#include "ptpd_netif.h"

uint8_t myIP[4];
int needIP = 1;
static wr_socket_t* ipv4_socket;

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

void ipv4_init(const char* if_name) {
  wr_sockaddr_t saddr;
  
  /* Configure socket filter */
  memset(&saddr, 0, sizeof(saddr));
  strcpy(saddr.if_name, if_name);
  get_mac_addr(&saddr.mac[0]); /* Unicast */
  saddr.ethertype = htons(0x0800); /* IPv4 */
  saddr.family = PTPD_SOCK_RAW_ETHERNET;
  
  ipv4_socket = ptpd_netif_create_socket(PTPD_SOCK_RAW_ETHERNET, 0, &saddr);
}

void ipv4_poll(void) {
  static int retry = 0;
  static int timer = 0;
  
  uint8_t buf[400];
  wr_sockaddr_t addr;
  int len;
  
  if ((len = ptpd_netif_recvfrom(ipv4_socket, &addr, buf, sizeof(buf), 0)) > 0) {
    if (needIP && process_bootp(buf, len-14)) {
      retry = 0;
      needIP = 0;
      timer = 0;
    }
    
    if (!needIP && (len = process_icmp(buf, len-14)) > 0)
      ptpd_netif_sendto(ipv4_socket, &addr, buf, len, 0);
  }
  
  if (needIP && timer == 0) {
    len = send_bootp(buf, ++retry);
    
    memset(addr.mac, 0xFF, 6);
    addr.ethertype = htons(0x0800); /* IPv4 */
    ptpd_netif_sendto(ipv4_socket, &addr, buf, len, 0);
  }
  
  if (needIP && ++timer == 100000) timer = 0;
}
