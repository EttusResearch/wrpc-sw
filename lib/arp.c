static wr_socket_t arp_socket;
static uint8_t myMAC[6];
static uint8_t myIP[4];

#define ARP_HTYPE	0
#define ARP_PTYPE	(ARP_HTYPE+2)
#define ARP_HLEN	(ARP_PTYPE+2)
#define ARP_PLEN	(ARP_HLEN+1)
#define ARP_OPER	(ARP_PLEN+1)
#define ARP_SHA		(ARP_OPER+2)
#define ARP_SPA		(ARP_SHA+6)
#define ARP_THA		(ARP_SPA+4)
#define ARP_TPA		(ARP_THA+6)
#define ARP_END		(ARP_TPA+4)

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

void arp_init(const char* if_name, uint32_t ip) {
  wr_sockaddr_t saddr;
  uint32_t ip_bigendian;
  
  /* Configure socket filter */
  memset(&saddr, 0, sizeof(saddr));
  strcpy(saddr.if_name, if_name);
  memset(&saddr.mac, 0xFF, 6); /* Broadcast */
  saddr.ethertype = htons(0x0806); /* ARP */
  saddr.family = PTPD_SOCK_RAW_ETHERNET;
  
  /* Configure ARP parameters */
  ip_bigendian = htonl(ip);
  memcpy(myIP, &ip_bigendian, 4);
  get_mac_addr(myMAC);
  
  arp_socket = ptpd_netif_create_socket(PTPD_SOCK_RAW_ETHERNET, 0, &saddr);
}

static int process_arp(char* buffer) {
  uint8_t hisMAC[6];
  uint8_t hisIP[4];
  
  /* Is it ARP request targetting our IP? */
  if (buffer[ARP_OPER+0] != 0 ||
      buffer[ARP_OPER+1] != 1 ||
      memcmp(buffer+ARP_TPA, myIP, 4))
    return 0;

  memcpy(hisMAC, buffer+ARP_SHA, 6);
  memcpy(hisIP,  buffer+ARP_SPA, 4);

  // ------------- ARP ------------  
  // HW ethernet
  buf[ARP_HTYPE+0] = 0;
  buf[ARP_HTYPE+1] = 1;
  // proto IP
  buf[ARP_PTYPE+0] = 8;
  buf[ARP_PTYPE+1] = 0;
  // lengths
  buf[ARP_HLEN] = 6;
  buf[ARP_PLEN] = 4;
  // Response
  buf[ARP_OPER+0] = 0;
  buf[ARP_OPER+1] = 2;
  // my MAC+IP
  memcpy(buf+ARP_SHA, myMAC, 6);
  memcpy(buf+ARP_SPA, myIP,  4);
  // his MAC+IP
  memcpy(buf+ARP_THA, hisMAC, 6);
  memcpy(buf+ARP_TPA, hisIP,  4);
  
  return 1;
}

void arp_poll() {
  char buffer[ARP_END];
  wr_sockaddr_t addr;
  int len;
  
  if ((len = ptpd_netif_recvfrom(&arp_socket, &addr, buffer, sizeof(buffer), 0)) > 0)
    if (process_arp(buffer) > 0)
      ptp_netif_sendto(&arp_socket, &addr, buffer, sizeof(buffer), 0);
}
