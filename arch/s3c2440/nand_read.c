/* 
 * vivi/s3c2410/nand_read.c: Simple NAND read functions for booting from NAND
 *
 * Copyright (C) 2002 MIZI Research, Inc.
 *
 * Author: Hwang, Chideok <hwang@mizi.com>
 * Date  : $Date: 2004/02/04 06:22:24 $
 *
 * $Revision: 1.1.1.1 $
 * $Id: param.c,v 1.9 2002/07/11 06:17:20 nandy Exp 
 *
 * History
 *
 * 2002-05-xx: Hwang, Chideok <hwang@mizi.com>
 *
 * 2002-05-xx: Chan Gyun Jeong <cgjeong@mizi.com>
 *
 * 2002-08-10: Yong-iL Joh <tolkien@mizi.com>
 *
 */

#include <config.h>

#define __REGb(x)	(*(volatile unsigned char *)(x))
#define __REGi(x)	(*(volatile unsigned int *)(x))
#define NF_BASE		0x4e000000

#define NFCONF		__REGi(NF_BASE + 0x0)
#define NFCONT		__REGi(NF_BASE + 0x4)
#define NFCMD		__REGb(NF_BASE + 0x8)
#define NFADDR		__REGb(NF_BASE + 0xC)
#define NFDATA		__REGb(NF_BASE + 0x10)
#define NFSTAT		__REGb(NF_BASE + 0x20)

//#define GPDAT		__REGi(GPIO_CTL_BASE+oGPIO_F+oGPIO_DAT)

#define NAND_CHIP_ENABLE  (NFCONT &= ~(1<<1))
#define NAND_CHIP_DISABLE (NFCONT |=  (1<<1))
#define NAND_CLEAR_RB	  (NFSTAT |=  (1<<2))
#define NAND_DETECT_RB	  { while(! (NFSTAT&(1<<2)) );}

#define BUSY 4
inline void wait_idle(void) {
	while(!(NFSTAT & BUSY));
	NFSTAT |= BUSY;
}

#define NAND_SECTOR_SIZE	512
#define NAND_BLOCK_MASK		(NAND_SECTOR_SIZE - 1)

/* low level nand read function */
int
nand_read_ll(unsigned char *buf, unsigned long start_addr, int size)
{
	int i, j;

	if ((start_addr & NAND_BLOCK_MASK) || (size & NAND_BLOCK_MASK)) {
		return -1;	/* invalid alignment */
	}

	NAND_CHIP_ENABLE;

	for(i=start_addr; i < (start_addr + size);) {
		/* READ0 */
		NAND_CLEAR_RB;	    
		NFCMD = 0;

		/* Write Address */
		NFADDR = i & 0xff;
		NFADDR = (i >> 9) & 0xff;
		NFADDR = (i >> 17) & 0xff;
		NFADDR = (i >> 25) & 0xff;

		NAND_DETECT_RB;

		for(j=0; j < NAND_SECTOR_SIZE; j++, i++) {
			*buf = (NFDATA & 0xff);
			buf++;
		}
	}
	NAND_CHIP_DISABLE;
	return 0;
}
