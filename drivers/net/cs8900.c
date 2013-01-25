/*
 * Cirrus Logic CS8900A Ethernet
 *
 * (C) Copyright 2002
 * Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Marius Groeger <mgroeger@sysgo.de>
 *
 * Copyright (C) 1999 Ben Williamson <benw@pobox.com>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is loaded into SRAM in bootstrap mode, where it waits
 * for commands on UART1 to read and write memory, jump to code etc.
 * A design goal for this program is to be entirely independent of the
 * target board.  Anything with a CL-PS7111 or EP7211 should be able to run
 * this code in bootstrap mode.  All the board specifics can be handled on
 * the host.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "cpu.h"
#include "machine.h"
#include "net/net.h"
#include "vivi_string.h"
#include "time.h"
#include "heap.h"
#include "memory.h"
#include "printk.h"
#include <string.h>
#include "cs8900.h"


static int cs8900_init(struct net_device *ethdev);
static int cs8900_recv(struct net_device *ethdev);
static int cs8900_send(struct net_device *ethdev, volatile void *packet, int len);
static void cs8900_halt(struct net_device *ethdev);
static void cs8900_reset(struct net_device *ethdev);


char DEVNAME[] = "CS8900A"; 

/* packet page register access functions */

#ifdef CS8900_BUS32
/* we don't need 16 bit initialisation on 32 bit bus */
#define get_reg_init_bus(x) get_reg((x))
#else
static unsigned short get_reg_init_bus(int regno)
{
    /* force 16 bit busmode */
    volatile unsigned char c;
    c = CS8900_BUS16_0;
    c = CS8900_BUS16_1;
    c = CS8900_BUS16_0;
    c = CS8900_BUS16_1;
    c = CS8900_BUS16_0;

    CS8900_PPTR = regno;
    return (unsigned short) CS8900_PDATA;
}
#endif

inline static unsigned short get_reg(int regno)
{
    CS8900_PPTR = regno;
    return (unsigned short) CS8900_PDATA;
}


inline static void put_reg(int regno, unsigned short val)
{
    CS8900_PPTR = regno;
    CS8900_PDATA = val;
}

inline static void set_reg(int regno, u16 val)
{
  put_reg(regno, get_reg(regno) | val);
}

struct net_device *cs8900_probe(void)
{
  int id = 0;
  struct net_device *ethdev = NULL;
 
#ifdef CONFIG_ARCH_S3C2440
  BWSCON = ( BWSCON & ~(0xf<<12)) | (0xd << 12);
  BANKCON3 = ( 3<<11)|(0x7<<8)|(0x1<<6)|(0x3<<4)|(0x3<<2);

#ifdef CONFIG_NET_DEBUG
  printk(" BWSCON   0x%08X   \n",BWSCON );
  printk(" BANKCON3 0x%08X   \n",BANKCON3 );
#endif /* CONFIG_NET_DEBUG */

#else
#error "no arch defined yet!"
#endif /* CONFIG_ARCH_S3C2440 */
    
  id =  get_reg_init_bus(PP_ChipID); 
  /* verify chip id */
  if (id != 0x630e)
    {
      printk( "CS8900 ID 0x%08X Ethernet chip not found?!\n", id);
      return NULL;
    }

  ethdev = (struct net_device *)mmalloc(sizeof(struct net_device)); 
  if(!ethdev)
    {
      printk("mmalloc failed in cs8900_probe!\n");
      return NULL;
    }
  
  /*usage of clear_mem is not clear yet, so skip it now.*/
  //clear_mem((unsigned long)ethdev, sizeof(struct net_device));
  strncpy(ethdev->name, DEVNAME, NAMESIZE);
  ethdev->iobase = CS8900_BASE;

  ethdev->init = cs8900_init;
  ethdev->send = cs8900_send;
  ethdev->recv = cs8900_recv;
  ethdev->halt = cs8900_halt;
  ethdev->reset = cs8900_reset;

  return ethdev;
}

static int cs8900_init(struct net_device *ethdev)
{   
    cs8900_reset(ethdev);

    /* set the ethernet address */
    put_reg(PP_IA + 0, ethdev->enetaddr[0] | (ethdev->enetaddr[1] << 8));
    put_reg(PP_IA + 2, ethdev->enetaddr[2] | (ethdev->enetaddr[3] << 8));
    put_reg(PP_IA + 4, ethdev->enetaddr[4] | (ethdev->enetaddr[5] << 8));
	     
    /* receive only error free packets addressed to this card */
    set_reg(PP_RxCTL, PP_RxCTL_IA | PP_RxCTL_Broadcast | PP_RxCTL_RxOK);

    /* do not generate any interrupts on receive operations */
    set_reg(PP_RxCFG, 0);

    /* do not generate any interrupts on transmit operations */
    set_reg(PP_TxCFG, 0);

    /* do not generate any interrupts on buffer operations */
    set_reg(PP_BufCFG, 0);

    /* enable transmitter/receiver mode */
    set_reg(PP_LineCTL, PP_LineCTL_Rx | PP_LineCTL_Tx);

    return 0;
}

/* Get a data block via Ethernet */
static int cs8900_recv(struct net_device *ethdev)
{
    int i;
    unsigned short rxlen = 0;
    unsigned short *addr = NULL;
    unsigned char *pkt = NULL;
    unsigned short status = 0;
    unsigned short *tpkt = NULL;


    status = get_reg(PP_RER);
#ifdef CONFIG_NET_DEBUG
    printk("%s: Rx Event = %x\n", __FUNCTION__, status);
#endif
    if ((status & PP_RER_RxOK) == 0)
      return 0;

    //status = CS8900_RTDATA;       /* stat */
    //rxlen = CS8900_RTDATA;        /* len */
    status = get_reg(PP_RxStatus);
    rxlen = get_reg(PP_RxLength);
#ifdef CONFIG_NET_DEBUG
    printk("RxStatus = %x, Rxlength = %d\n", status, rxlen);
#endif

    if(rxlen > PKTSIZE_ALIGN)
      printk("packet too big!\n");
    
    for(addr = (unsigned short *)NetRxPackets[0], i = rxlen >> 1; i > 0; i-- )
      {
	*addr++ = CS8900_RTDATA;
      }
    if(rxlen & 1)
      *addr++ = CS8900_RTDATA;

    pkt = (unsigned char *)(NetRxPackets[0] + 1);

#ifdef CONFIG_QQ2440
    tpkt = (unsigned short *)pkt;
    for(i = 0; i < (rxlen%2 ? rxlen/2+1 : rxlen/2); i++)
      {
	tpkt[i] = htons(tpkt[i]);
      }
#endif

#ifdef CONFIG_NET_DEBUG
    tpkt = (unsigned short *)pkt;
    for(i = 0; i < rxlen/2; i++)
      printk("%.4X ", tpkt[i]);
#endif

    /* Pass the packet up to the protocol layers. */
    NetReceive(pkt, rxlen);

    return rxlen;
}

/* Send a data block via Ethernet. */
static int 
cs8900_send(struct net_device *ethdev, volatile void *packet, int len)
{
    volatile unsigned short *addr;
    int tmo = 0;
    unsigned short s;

#ifdef CONFIG_NET_DEBUG
    printk("%s:\n", __FUNCTION__);
    printk("%d\n", len);
#endif

retry:
    /* initiate a transmit sequence */
    CS8900_TxCMD = PP_TxCmd_TxStart_Full;
    CS8900_TxLEN = len;
    
    /* Test to see if the chip has allocated memory for the packet */
    if ((get_reg(PP_BusSTAT) & PP_BusSTAT_TxRDY) == 0) {    
	/* Oops... this should not happen! */
	printk("cs: unable to send packet; retrying...\n");
	for (tmo = get_timer(0) + 5 * HZ; get_timer(0) < tmo; )
	  /*NOP*/;
	cs8900_reset(ethdev);
	goto retry; 
    }

    /* Write the contents of the packet */
    /* assume even number of bytes */
    for(addr = packet; len > 0; len -= 2 )
      CS8900_RTDATA = *addr++;

    /* wait for transfer to succeed */
    tmo = get_timer(0) + 5 * HZ;
    while((s = get_reg(PP_TER) & ~0x1F) == 0) 
    {
	if (get_timer(0) >= tmo)
	  break;
    }
     
      /* nothing */ ;
    if ((s & (PP_TER_CRS | PP_TER_TxOK)) != PP_TER_TxOK) {
	printk("\ntransmission error %#x\n", s);
    }

    return 0;
}

static void cs8900_reset(struct net_device *ethdev)
{
    int tmo = 0;
    unsigned short us;

    /* reset NIC */
    set_reg(PP_SelfCTL, get_reg(PP_SelfCTL) | PP_SelfCTL_Reset );

    /* wait for 200ms */
    udelay(200000);
    /* Wait until the chip is reset */

    tmo = get_timer(0) + 1 * HZ;
    while ((((us = get_reg_init_bus(PP_SelfSTAT)) & PP_SelfSTAT_InitD) == 0)
	   && tmo < get_timer(0))
      /*NOP*/;
}

static void cs8900_halt(struct net_device *ethdev)
{
    /* disable transmitter/receiver mode */
    set_reg(PP_LineCTL, 0);

    /* "shutdown" to show ChipID or kernel wouldn't find he cs8900 ... */
    get_reg_init_bus(PP_ChipID);
}


