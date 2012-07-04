#include "persistent_mac.h"
#include "../sockitowm/ownet.h"
#include "../sockitowm/findtype.h"

#define MAX_DEV1WIRE 8
#define DEBUG_PMAC 0

/* 0 = success, -1 = error */
int get_persistent_mac(unsigned char* mac)
{
  unsigned char FamilySN[MAX_DEV1WIRE][8];
  unsigned char read_buffer[32];
  unsigned char portnum = ONEWIRE_PORT;
  int i, devices, out;

  // Find the device(s)
  devices = 0;
  devices += FindDevices(portnum, &FamilySN[devices], 0x28, MAX_DEV1WIRE - devices); /* Temperature 28 sensor (SPEC) */
  devices += FindDevices(portnum, &FamilySN[devices], 0x42, MAX_DEV1WIRE - devices); /* Temperature 42 sensor (SCU) */
  devices += FindDevices(portnum, &FamilySN[devices], 0x43, MAX_DEV1WIRE - devices); /* EEPROM */
  
  out = -1;
  
  for (i = 0; i < devices; ++i) {
    /* If there is a temperature sensor, use it for the low three MAC values */
    if (FamilySN[i][0] == 0x28 || FamilySN[i][0] == 0x42) {
      mac[3] = FamilySN[i][3];
      mac[4] = FamilySN[i][2];
      mac[5] = FamilySN[i][1];
      out = 0;
#ifdef DEBUG_PMAC
      mprintf("Using temperature sensor ID: %x:%x:%x:%x:%x:%x:%x:%x\n", 
        FamilySN[i][7], FamilySN[i][6], FamilySN[i][5], FamilySN[i][4],
        FamilySN[i][3], FamilySN[i][2], FamilySN[i][1], FamilySN[i][0]);
#endif
    }
    
    /* If there is an EEPROM, read page 0 for the MAC */
    if (FamilySN[i][0] == 0x43) {
      owLevel(portnum, MODE_NORMAL);
      if (ReadMem43(portnum, FamilySN[i], EEPROM_MAC_PAGE, &read_buffer) == TRUE) {
        if (read_buffer[0] == 0 && read_buffer[1] == 0 && read_buffer[2] == 0) {
          /* Skip the EEPROM since it has not been programmed! */
#ifdef DEBUG_PMAC
        mprintf("EEPROM %x:%x:%x:%x:%x:%x:%x:%x has not been programmed with a MAC\n", 
          FamilySN[i][7], FamilySN[i][6], FamilySN[i][5], FamilySN[i][4],
          FamilySN[i][3], FamilySN[i][2], FamilySN[i][1], FamilySN[i][0]);
#endif
        } else {
          memcpy(mac, read_buffer, 6);
          out = 0;
#ifdef DEBUG_PMAC
          mprintf("Using EEPROM page: %x:%x:%x:%x:%x:%x\n", 
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        }
#endif
      }
    }
  }
  
  return out;
}

/* 0 = success, -1 = error */
int set_persistent_mac(unsigned char* mac)
{
  unsigned char FamilySN[MAX_DEV1WIRE][8];
  unsigned char write_buffer[32];
  unsigned char portnum = ONEWIRE_PORT;
  int i, devices;

  // Find the device(s)
  devices = FindDevices(portnum, &FamilySN[0], 0x43, MAX_DEV1WIRE); /* EEPROM */
  
  if (devices == 0) return -1;
  
  memset(write_buffer, 0, sizeof(write_buffer));
  memcpy(write_buffer, mac, 6);
  
  /* Write the last EEPROM with the MAC */
  owLevel(portnum, MODE_NORMAL);
  if (Write43(portnum, FamilySN[devices-1], EEPROM_MAC_PAGE, &write_buffer) == TRUE)
    return 0;
  
  return -1;
}
