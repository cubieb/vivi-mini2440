/*
 * (C) Copyright 2001-2004
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include "stddef.h"
#include "config.h"
#include "printk.h"
#include "net/net.h"
#include <string.h>

static struct net_device *eth_current = NULL; 
static unsigned char CFG_ENET_ADDR[32] = CONFIG_MAC_ADDR;

extern struct net_device *cs8900_probe(void);
extern struct net_device *dm9000_Register(void);

int eth_init(void);

inline static void eth_register(struct net_device* dev)
{
	dev->state = ETH_STATE_INIT;
	dev->next  = NULL;

	eth_current = dev;
}

int eth_dev_init(void)
{	
  struct net_device *dev = NULL;

#ifdef CONFIG_CS8900A
  dev = cs8900_probe();
#else
  dev = dm9000_Register();
#endif
  //printk("Network Device: %s\n", dev->name);

  if(!dev)
    {
      //printk("No Ethernet Adapter Found!\n");
      eth_current = NULL;
      return -1;
    }
  eth_register(dev);
  
  eth_halt();
  if (eth_init() != 0) {
    eth_halt();
    return (-1);
  }
 
  return 0;
}

static int eth_set_enetaddr(struct net_device *dev, char *addr) 
{
	unsigned char enetaddr[6];
	char *end;
	int i;

	if (!dev)
	    return -1;

	for (i=0; i<6; i++) {
	  enetaddr[i] = addr ? simple_strtoul(addr, &end, 16) : 0;
	  if (addr)
	    addr = (*end) ? end+1 : end;
	}

	printk( "Setting HW address on %s\n"
		"New Address is %02X:%02X:%02X:%02X:%02X:%02X\n",
		dev->name,
		enetaddr[0], enetaddr[1],
		enetaddr[2], enetaddr[3],
		enetaddr[4], enetaddr[5]);

	memcpy(dev->enetaddr, enetaddr, 6);
	
	return 0;
}

unsigned char* eth_get_enetaddr(void) 
{
  if(!eth_current)
    {
      //printk("no ethernet adapter found!\n");
      return NULL;
    } 
  
  return eth_current->enetaddr;
}

int eth_init(void)
{
  int ret = 0;

  if (!eth_current)
    {
      //printk("no ethernet adapter found!\n");
      return -1;
    } 
  
  //eth_set_enetaddr(eth_current, CFG_ENET_ADDR);
  
  ret = eth_current->init(eth_current);
  if(!ret)
    eth_current->state = ETH_STATE_ACTIVE;
  
  return ret;
}

int eth_halt(void)
{
  if (!eth_current)
    {
      //printk("no ethernet adapter found!\n");
      return -1;
    }
  
  eth_current->halt(eth_current);
  eth_current->state = ETH_STATE_PASSIVE;

  return 0;
}

int eth_send(volatile void *packet, int len)
{
  if (!eth_current)
    {
      //printk("no ethernet adapter found!\n");
      return -1;
    }

  return eth_current->send(eth_current, packet, len);
}

int eth_rx(void)
{
  if (!eth_current)
    {
      //printk("no ethernet adapter found!\n");
      return -1;
    }
  
  return eth_current->recv(eth_current);
}

char *eth_get_name (void)
{
  return (eth_current ? eth_current->name : "no ethernet adapter found");
}

/*
unsigned long *eth_get_ipaddr(void)
{
  if(!eth_current)
    {
      printk("no ethernet adapter found!\n");
      return NULL;
    }

  return &(eth_current->ip_addr);
}
*/
