#include "onewire.h"
#include "../sockitowm/ownet.h"
#include "../sockitowm/findtype.h"

#define DEBUG_PMAC 0

uint8_t FamilySN[MAX_DEV1WIRE][8];
uint8_t devsnum;
uint8_t found_msk;

void own_scanbus(uint8_t portnum)
{
  // Find the device(s)
  found_msk = 0;
  devsnum = 0;
  devsnum += FindDevices(portnum, &FamilySN[devsnum], 0x28, MAX_DEV1WIRE - devsnum); /* Temperature 28 sensor (SPEC) */
  if(devsnum>0) found_msk |= FOUND_DS18B20;
  devsnum += FindDevices(portnum, &FamilySN[devsnum], 0x42, MAX_DEV1WIRE - devsnum); /* Temperature 42 sensor (SCU) */
  devsnum += FindDevices(portnum, &FamilySN[devsnum], 0x43, MAX_DEV1WIRE - devsnum); /* EEPROM */
#if DEBUG_PMAC
  mprintf("Found %d onewire devices\n", devsnum);
#endif
}

int16_t own_readtemp(uint8_t portnum, int16_t *temp, int16_t *t_frac)
{
  if(!(found_msk & FOUND_DS18B20))
    return -1;
  if(ReadTemperature28(portnum, FamilySN[0], temp))
  {
    *t_frac = 5000*(!!(*temp & 0x08)) + 2500*(!!(*temp & 0x04)) + 1250*(!!(*temp & 0x02)) +
              625*(!!(*temp & 0x01));
    *t_frac = *t_frac/100 + (*t_frac%100)/50;
    *temp >>= 4;
    return 0;
  }
  return -1;
}

/* 0 = success, -1 = error */
int8_t get_persistent_mac(uint8_t portnum, uint8_t* mac)
{
  uint8_t read_buffer[32];
  uint8_t i;
  int8_t out;
  
  out = -1;

  if(devsnum == 0) return out;
  
  for (i = 0; i < devsnum; ++i) {
//#if DEBUG_PMAC
    mprintf("Found device: %x:%x:%x:%x:%x:%x:%x:%x\n", 
      FamilySN[i][7], FamilySN[i][6], FamilySN[i][5], FamilySN[i][4],
      FamilySN[i][3], FamilySN[i][2], FamilySN[i][1], FamilySN[i][0]);
//#endif

    /* If there is a temperature sensor, use it for the low three MAC values */
    if (FamilySN[i][0] == 0x28 || FamilySN[i][0] == 0x42) {
      mac[3] = FamilySN[i][3];
      mac[4] = FamilySN[i][2];
      mac[5] = FamilySN[i][1];
      out = 0;
#if DEBUG_PMAC
      mprintf("Using temperature ID for MAC\n");
#endif
    }
    
    /* If there is an EEPROM, read page 0 for the MAC */
    if (FamilySN[i][0] == 0x43) {
      owLevel(portnum, MODE_NORMAL);
      if (ReadMem43(portnum, FamilySN[i], EEPROM_MAC_PAGE, &read_buffer) == TRUE) {
        if (read_buffer[0] == 0 && read_buffer[1] == 0 && read_buffer[2] == 0) {
          /* Skip the EEPROM since it has not been programmed! */
#if DEBUG_PMAC
          mprintf("EEPROM has not been programmed with a MAC\n");
#endif
        } else {
          memcpy(mac, read_buffer, 6);
          out = 0;
#if DEBUG_PMAC
          mprintf("Using EEPROM page: %x:%x:%x:%x:%x:%x\n", 
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
#endif
        }
      }
    }
  }
  
  return out;
}

/* 0 = success, -1 = error */
int8_t set_persistent_mac(uint8_t portnum, uint8_t* mac)
{
  uint8_t FamilySN[1][8];
  uint8_t write_buffer[32];

  // Find the device (only the first one, we won't write MAC to all EEPROMs out there, right?)
  if( FindDevices(portnum, &FamilySN[0], 0x43, 1) == 0) return -1;
  
  memset(write_buffer, 0, sizeof(write_buffer));
  memcpy(write_buffer, mac, 6);
  
#if DEBUG_PMAC
  mprintf("Writing to EEPROM\n");
#endif
  
  /* Write the last EEPROM with the MAC */
  owLevel(portnum, MODE_NORMAL);
  if (Write43(portnum, FamilySN[0], EEPROM_MAC_PAGE, &write_buffer) == TRUE)
    return 0;
  
  return -1;
}
