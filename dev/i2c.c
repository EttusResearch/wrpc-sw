#include "types.h"
#include "board.h"
#include "syscon.h"

#define I2C_DELAY 100

static void mi2c_delay()
{
    int i;
    for(i=0;i<I2C_DELAY;i++) asm volatile ("nop");
}

#define M_SDA_OUT(x) { gpio_out(GPIO_SDA, x); mi2c_delay(); }
#define M_SCL_OUT(x) { gpio_out(GPIO_SCL, x); mi2c_delay(); }
#define M_SDA_IN gpio_in(GPIO_SDA)

void mi2c_start()
{
  M_SDA_OUT(0);
  M_SCL_OUT(0);
}

void mi2c_repeat_start()
{
  M_SDA_OUT(1);
  M_SCL_OUT(1);
  M_SDA_OUT(0);
  M_SCL_OUT(0);
}

void mi2c_stop()
{
  M_SDA_OUT(0);
  M_SCL_OUT(1);
  M_SDA_OUT(1);
}

unsigned char mi2c_put_byte(unsigned char data)
{
  char i;
  unsigned char ack;

  for (i=0;i<8;i++, data<<=1)
    {
      M_SDA_OUT(data&0x80);
      M_SCL_OUT(1);
      M_SCL_OUT(0);
    }

  M_SDA_OUT(1);
  M_SCL_OUT(1);

  ack = M_SDA_IN;	/* ack: sda is pulled low ->success.	 */

  M_SCL_OUT(0);
  M_SDA_OUT(0);

  return ack!=0;
}

void mi2c_get_byte(unsigned char *data)
{

  int i;
  unsigned char indata = 0;

  /* assert: scl is low */
  M_SCL_OUT(0);

  for (i=0;i<8;i++)
    {
      M_SCL_OUT(1);
      indata <<= 1;
      if ( M_SDA_IN ) indata |= 0x01;
      M_SCL_OUT(0);
    }

  M_SDA_OUT(1);
  *data= indata;
}

void mi2c_init()
{
  M_SCL_OUT(1);
  M_SDA_OUT(1);
}

void mi2c_scan()
{
    int i;
    
    for(i=0;i<0x80;i++)
    {
	mi2c_start();
	mprintf("scan %d\n",i);
	if(!mi2c_put_byte(i<<1)) mprintf("found : %x\n", i);
	mi2c_stop();
	
    }    
}
