
#include "shell.h"
#include "eeprom.h"
#include "syscon.h"


int cmd_init(const char *args[])
{
  if( !mi2c_devprobe(WRPC_FMC_I2C, FMC_EEPROM_ADR) )
  {
    mprintf("EEPROM not found..\n");
    return -1;
  }

  if(args[0] && !strcasecmp(args[0], "erase"))
  {
    if( eeprom_init_erase(WRPC_FMC_I2C, FMC_EEPROM_ADR) < 0 )
      mprintf("Could not erase init script\n");
  }
  else if(args[0] && !strcasecmp(args[0], "purge"))
  {
    eeprom_init_purge(WRPC_FMC_I2C, FMC_EEPROM_ADR);
  }
  else if(args[1] && !strcasecmp(args[0], "add"))
  {
    if( eeprom_init_add(WRPC_FMC_I2C, FMC_EEPROM_ADR, args) < 0 )
      mprintf("Could not add the command\n");
    else
      mprintf("OK.\n");
  }
  else if(args[0] && !strcasecmp(args[0], "show"))
  {
    eeprom_init_show(WRPC_FMC_I2C, FMC_EEPROM_ADR);
  }
  else if(args[0] && !strcasecmp(args[0], "boot"))
  {
    shell_boot_script();
  }

  return 0;
}

