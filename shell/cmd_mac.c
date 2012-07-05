#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "softpll_ng.h"
#include "shell.h"
#include "../lib/ipv4.h"

static decode_mac(const char *str, unsigned char* mac) {
  int i, x;
  
  /* Don't try to detect bad input; need small code */
  for (i = 0; i < 6; ++i) {
    str = fromhex(str, &x);
    mac[i] = x;
    if (*str == ':') ++str;
  }
}

int cmd_mac(const char *args[])
{
  unsigned char mac[6];
  
  if (!args[0] || !strcasecmp(args[0], "get")) {
    /* get current MAC */
    get_mac_addr(mac);
  } else if (!strcasecmp(args[0], "getp")) {
    /* get persistent MAC */
    get_mac_addr(mac);
    get_persistent_mac(mac);
  } else if (!strcasecmp(args[0], "set") && args[1]) {
    decode_mac(args[1], mac);
    set_mac_addr(mac);
    pfilter_init_default();
  } else if (!strcasecmp(args[0], "setp") && args[1]) {
    decode_mac(args[1], mac);
    set_persistent_mac(mac);
  } else {
    return -EINVAL;
  }
  
  mprintf("MAC-address: %x:%x:%x:%x:%x:%x\n", 
    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}
