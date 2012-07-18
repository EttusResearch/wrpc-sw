#include "types.h"
#include "i2c.h"
#include "eeprom.h"
#include "board.h"
#include "syscon.h"

/*
 * The SFP section is placed somewhere inside FMC EEPROM and it really does not 
 * matter where (can be a binary data inside the Board Info section but can be 
 * placed also outside the FMC standardized EEPROM structure. The only requirement
 * is that it starts with 0xdeadbeef pattern. The structure of SFP section is:
 *
 * --------------
 * | count (4B) |
 * --------------------------------------------------------------------------------------------
 * |   SFP(1) part number (16B)       | alpha (4B) | deltaTx (4B) | deltaRx (4B) | chksum(1B) |
 * --------------------------------------------------------------------------------------------
 * |   SFP(2) part number (16B)       | alpha (4B) | deltaTx (4B) | deltaRx (4B) | chksum(1B) |
 * --------------------------------------------------------------------------------------------
 * | (....)                           | (....)     | (....)       | (....)       | (...)      |
 * --------------------------------------------------------------------------------------------
 * |   SFP(count) part number (16B)   | alpha (4B) | deltaTx (4B) | deltaRx (4B) | chksum(1B) |
 * --------------------------------------------------------------------------------------------
 *
 * Fields description:
 * count              - how many SFPs are described in the list (binary)
 * SFP(n) part number - SFP PN as read from SFP's EEPROM (e.g. AXGE-1254-0531) 
 *                      (16 ascii chars)
 * checksum           - low order 8 bits of the sum of all bytes for the SFP(PN,alpha,dTx,dRx)
 *
 */


int eeprom_read(uint8_t i2cif, uint8_t i2c_addr, uint32_t offset, uint8_t *buf, size_t size)
{
	int i;
	unsigned char c;

  mi2c_start(i2cif);
  if(mi2c_put_byte(i2cif, i2c_addr << 1) < 0)
  {
    mi2c_stop(i2cif);
    return -1;
  }
  mi2c_put_byte(i2cif, (offset>>8) & 0xff);
  mi2c_put_byte(i2cif, offset & 0xff);
  mi2c_repeat_start(i2cif);
  mi2c_put_byte(i2cif, (i2c_addr << 1) | 1);
  for(i=0; i<size-1; ++i)
  {
    mi2c_get_byte(i2cif, &c, 0);
    *buf++ = c;
  }
  mi2c_get_byte(i2cif, &c, 1);
  *buf++ = c;
  mi2c_stop(i2cif);

 	return size;
}

int eeprom_write(uint8_t i2cif, uint8_t i2c_addr, uint32_t offset, uint8_t *buf, size_t size)
{
	int i, busy;

	for(i=0;i<size;i++)
	{
	 	mi2c_start(i2cif);

	 	if(mi2c_put_byte(i2cif, i2c_addr << 1) < 0)
	 	{
		 	mi2c_stop(i2cif);
	 	 	return -1;
	  }
		mi2c_put_byte(i2cif, (offset >> 8) & 0xff);
		mi2c_put_byte(i2cif, offset & 0xff);
		mi2c_put_byte(i2cif, *buf++);
		offset++;
		mi2c_stop(i2cif);

		do /* wait until the chip becomes ready */
		{
      mi2c_start(i2cif);
			busy = mi2c_put_byte(i2cif, i2c_addr << 1);
			mi2c_stop(i2cif);
		} while(busy);

	}
 	return size;
}

int32_t eeprom_sfpdb_erase(uint8_t i2cif, uint8_t i2c_addr)
{
  uint8_t sfpcount = 0;

  //just a dummy function that writes '0' to sfp count field of the SFP DB
  if( eeprom_write(i2cif, i2c_addr, EE_BASE_SFP, &sfpcount, sizeof(sfpcount)) != sizeof(sfpcount))
    return EE_RET_I2CERR;
  else
    return sfpcount;
}

int32_t eeprom_get_sfp(uint8_t i2cif, uint8_t i2c_addr, struct s_sfpinfo* sfp, uint8_t add, uint8_t pos)
{
  static uint8_t sfpcount = 0;
  uint8_t i, chksum=0;
  uint8_t* ptr;

  if( pos>=SFPS_MAX )
    return EE_RET_POSERR;  //position in database outside the range

  //read how many SFPs are in the database, but only in the first call (pos==0)
  if( !pos && eeprom_read(i2cif, i2c_addr, EE_BASE_SFP, &sfpcount, sizeof(sfpcount)) != sizeof(sfpcount) )
    return EE_RET_I2CERR;

  if( add && sfpcount==SFPS_MAX )  //no more space in the database to add new SFPs
    return EE_RET_DBFULL;
  else if( !pos && !add && sfpcount==0 )  //there are no SFPs in the database to read
    return sfpcount;

  if(!add)
  {
    if( eeprom_read(i2cif, i2c_addr, EE_BASE_SFP + sizeof(sfpcount) + pos*sizeof(struct s_sfpinfo), sfp, 
          sizeof(struct s_sfpinfo)) != sizeof(struct s_sfpinfo) )
      return EE_RET_I2CERR;

    ptr = (uint8_t*)sfp;
    for(i=0; i<sizeof(struct s_sfpinfo)-1; ++i) //'-1' because we do not include chksum in computation
      chksum = (uint8_t) ((uint16_t)chksum + *(ptr++)) & 0xff;
    if(chksum != sfp->chksum)
      EE_RET_CHKSUM;
  }
  else
  {
    /*count checksum*/
    ptr = (uint8_t*)sfp;
    for(i=0; i<sizeof(struct s_sfpinfo)-1; ++i) //'-1' because we do not include chksum in computation
      chksum = (uint8_t) ((uint16_t)chksum + *(ptr++)) & 0xff;
    sfp->chksum = chksum;
    /*add SFP at the end of DB*/
    eeprom_write(i2cif, i2c_addr, EE_BASE_SFP+sizeof(sfpcount) + sfpcount*sizeof(struct s_sfpinfo), sfp, sizeof(struct s_sfpinfo));
    sfpcount++;
    eeprom_write(i2cif, i2c_addr, EE_BASE_SFP, &sfpcount, sizeof(sfpcount));
  }

  return sfpcount;
}

int8_t eeprom_match_sfp(uint8_t i2cif, uint8_t i2c_addr, struct s_sfpinfo* sfp)
{
  uint8_t sfpcount = 1;
  int8_t i, temp;
  struct s_sfpinfo dbsfp;

  for(i=0; i<sfpcount; ++i)
  {   
    temp = eeprom_get_sfp(WRPC_FMC_I2C, FMC_EEPROM_ADR, &dbsfp, 0, i); 
    if(!i) 
    {   
      sfpcount=temp; //only in first round valid sfpcount is returned from eeprom_get_sfp
      if(sfpcount == 0 || sfpcount == 0xFF)
        return 0;
      else if(sfpcount<0) 
        return sfpcount;
    }   
    if( !strncmp(dbsfp.pn, sfp->pn, 16) )
    {
      sfp->dTx = dbsfp.dTx;
      sfp->dRx = dbsfp.dRx;
      sfp->alpha = dbsfp.alpha;
      return 1;
    }
  } 

  return 0;
}
