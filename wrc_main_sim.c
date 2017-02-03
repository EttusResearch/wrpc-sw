/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2017 CERN (www.cern.ch)
 * Author: Grzegorz Daniluk <grzegorz.daniluk@cern.ch>
 * Author: Maciej Lipinski <maciej.lipinski@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

#include <stdio.h>
#include <inttypes.h>

#include "system_checks.h"
#include "endpoint.h"
#include "minic.h"
#include "pps_gen.h"
#include "softpll_ng.h"
#include <wrpc.h> /*needed for htons()*/

void wrc_run_task(struct wrc_task *t);

static int test_number = 0;

static void wrc_sim_initialize(void)
{
  uint8_t mac_addr[6];

  sdb_find_devices();
  timer_init(1);

  mac_addr[0] = 0xDE;
  mac_addr[1] = 0xAD;
  mac_addr[2] = 0xBE;
  mac_addr[3] = 0xEF;
  mac_addr[4] = 0xBA;
  mac_addr[5] = 0xBE;

  ep_init(mac_addr);
  ep_enable(1, 1);

  minic_init();
  shw_pps_gen_init();
  spll_very_init();
}
/*
 *
 */
int wrpc_test_1(void)
{
  struct hw_timestamp hwts;
  struct wr_ethhdr_vlan tx_hdr;
  struct wr_ethhdr rx_hdr;
  int recvd, i, q_required;
  int j;
  uint8_t tx_payload[NET_MAX_SKBUF_SIZE - 32];
  uint8_t rx_payload[NET_MAX_SKBUF_SIZE - 32];
  int ret;
  int tx_cnt=0, rx_cnt=0, pl_cnt;
  // error code:
  // 0xAA - first
  // 0xBB - normal
  // 0xE* - error code:
  //    0 - Error: returned zero value
  //    1 - Error: wrong seqID
  //    2 - Error: error of rx
  int code=0xAA;

  /// prepare dummy frame
  //payload
  for(j=0; j<80; ++j) {
    tx_payload[j] = 0xC7;//j;
  }
  //address and EtherType
  memcpy(tx_hdr.dstmac, "\x01\x1B\x19\x00\x00\x00", 6);
  tx_hdr.ethtype = htons(0x88f7);

  /// main loop, send test frames
  for (;;)
  {
    // seqID
    tx_payload[0] = 0xFF & (tx_cnt >> 8);
    tx_payload[1] = 0xFF & tx_cnt;

    tx_payload[2] = 0xFF & (code >> 8);
    tx_payload[3] = 0xFF & code;

    // rx return value
    tx_payload[4] = 0xFF & (ret >> 8);
    tx_payload[5] = 0xFF & ret;

    /* A frame is sent out with sequenceID (firt octet) and awaited reception. */
    minic_tx_frame(&tx_hdr, tx_payload, 62, &hwts);
    tx_cnt++;
    ret=minic_rx_frame(&rx_hdr, rx_payload, NET_MAX_SKBUF_SIZE, &hwts);

    /// check whether the received value is OK
    if(ret == 0)
      code = 0xE0; // Error: returned zero value
    else if(ret > 0 ) {
      if(pl_cnt == rx_cnt)
      {
          rx_cnt++;
          code = 0xBB; // OK
      }
      else {
          rx_cnt = pl_cnt+1;
          code = 0xE1; // Error: wrong seqID
      }
    }
    else
          code = 0xE2; // Error: error of rx
  }

}
int main(void)
{
  wrc_sim_initialize();

  switch (test_number) {
    case 0:
      wrpc_test_1();
      break;
    default:
      while(1);
  }
}
