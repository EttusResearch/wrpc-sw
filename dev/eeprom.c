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

int8_t eeprom_get_sfpinfo(uint8_t i2cif, uint8_t i2c_addr, uint32_t offset, struct s_sfpinfo *sfpinfo, uint16_t section_sz)
{
  uint8_t *buf;
  uint32_t i;
  uint8_t checksum, sum;
  
  buf = (uint8_t *)sfpinfo;
  eeprom_read(i2cif, i2c_addr, offset, buf, section_sz * sizeof(struct s_sfpinfo));

  //read checksum
  eeprom_read(i2cif, i2c_addr, offset+section_sz*sizeof(struct s_sfpinfo), &checksum, 1);

  //count checksum
  sum = (uint8_t) (section_sz>>8 & 0xff);
  sum = (uint8_t) ((uint16_t) sum + (section_sz & 0xff)) & 0xff;

  for(i=0; i<section_sz*sizeof(struct s_sfpinfo); ++i)
    sum = (uint8_t) ((uint16_t)sum + *(buf+i)) & 0xff;
  
  if(sum == checksum)
  {
    mprintf("%s: checksum match\n", __FUNCTION__);
    return 0;
  }
  else
  {
    mprintf("%s: checksum error, %x | %x\n", __FUNCTION__, sum, checksum);
    return -1;
  }
} 

//int8_t access_eeprom(char *sfp_pn, int32_t *alpha, int32_t *deltaTx, int32_t *deltaRx)
//{
//  uint16_t i;
//  uint8_t j;
//  uint16_t sfp_sz;
//  int32_t sfp_adr;
//  struct s_sfpinfo sfpinfo[SFPS_MAX];
//
//  mi2c_init(WRPC_FMC_I2C);
//  
//  sfp_adr = eeprom_sfp_section(WRPC_FMC_I2C, FMC_EEPROM_ADR, 64*1024, &sfp_sz);
//  if(sfp_adr == -1)
//  {
//    mprintf("FMC EEPROM not found\n");
//    return -1;
//  }
//  else if(sfp_sz > SFPINFO_MAX)
//  {
//    //Ooops, there are too many of them, print warning
//    mprintf("! Warning ! too many SFP entries (%d)\n", sfp_sz);
//    sfp_sz = SFPINFO_MAX;
//  }
//  else if(sfp_sz == 0)
//  {
//    mprintf("EEPROM: could no find SFP section, staring with defaults\n");
//    return -1;
//  }
//  mprintf("EEPROM: found SFP section at %d size %d\n", (uint32_t)sfp_adr, (uint32_t)sfp_sz);
//
//  if( eeprom_get_sfpinfo(WRPC_FMC_I2C, FMC_EEPROM_ADR, sfp_adr, sfpinfo, sfp_sz))
//  {
//    mprintf("EEPROM ERROR\n");
//    return -1;
//  }
//
//  for(i=0; i<sfp_sz; ++i)
//  {
//    for(j=0; j<16; ++j)
//    {
//      if(sfp_pn[j] != sfpinfo[i].pn[j])
//        break;
//    }
//
//    if( j==16 ) //which means sfp_pn = sfpinfo[i].pn
//    {
//      mprintf("match SFP%d: pn=", i+1);
//      for(j=0; j<16; ++j)
//        mprintf("%c", sfpinfo[i].pn[j]);
//      //mprintf(" alpha=%x deltaTx=%x deltaRx=%x\n", sfpinfo[i].alpha, sfpinfo[i].deltaTx, sfpinfo[i].deltaRx);
//
//      *alpha   = sfpinfo[i].alpha;
//      *deltaTx = sfpinfo[i].deltaTx;
//      *deltaRx = sfpinfo[i].deltaRx;
//    }
//  }
//
//  return 0;
//}
