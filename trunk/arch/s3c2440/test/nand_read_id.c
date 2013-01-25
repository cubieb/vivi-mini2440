#include "time.h"
#include "printk.h"

/*registers*/
#define NFCMMD (*(volatile unsigned long *)0x4e000008)
#define NFDATA (*(volatile unsigned long *)0x4e100010)
#define NFADDR (*(volatile unsigned long *)0x4e00000c)
#define NFCONT (*(volatile unsigned long *)0x4e000004)
#define NFSTAT (*(volatile unsigned long *)0x4e000020)

/*commands*/
#define CMD_READID 0x90

int nand_read_id(void)
{
  unsigned char  manufacture_code = 0;
  unsigned char device_code = 0;
  unsigned char multi_plane = 0;

  printk("nand read id command\n");

  NFCONT &= ~(0x2);
  NFSTAT |= (1<<2);
  NFCMMD = (unsigned char)CMD_READID;
  NFADDR = (unsigned char)0x0;

  udelay(15);

  manufacture_code = (unsigned char)NFDATA;
  device_code = (unsigned char)((NFDATA & 0xff00) >> 8);
  multi_plane = NFDATA;
  multi_plane = (unsigned char)((NFDATA & 0xff000000) >> 24);

  printk("manufacture code is 0x%02x\n", manufacture_code);
  printk("device code is 0x%02x\n", device_code);
  printk("support multi plane = 0x%02x\n", multi_plane);

  return 0;
}
