/*
 *	Copied from Linux Monitor (LiMon) - Networking.
 *
 *	Copyright 1994 - 2000 Neil Russell.
 *	(See License)
 *	Copyright 2000 Roland Borde
 *	Copyright 2000 Paolo Scaffardi
 *	Copyright 2000-2002 Wolfgang Denk, wd@denx.de
 */

/*
 * General Desription:
 *
 * The user interface supports commands for BOOTP, RARP, and TFTP.
 * Also, we support ARP internally. Depending on available data,
 * these interact as follows:
 *
 * BOOTP:
 *
 *	Prerequisites:	- own ethernet address
 *	We want:	- own IP address
 *			- TFTP server IP address
 *			- name of bootfile
 *	Next step:	ARP
 *
 * RARP:
 *
 *	Prerequisites:	- own ethernet address
 *	We want:	- own IP address
 *			- TFTP server IP address
 *	Next step:	ARP
 *
 * ARP:
 *
 *	Prerequisites:	- own ethernet address
 *			- own IP address
 *			- TFTP server IP address
 *	We want:	- TFTP server ethernet address
 *	Next step:	TFTP
 *
 * DHCP:
 *
 *     Prerequisites:	- own ethernet address
 *     We want:		- IP, Netmask, ServerIP, Gateway IP
 *			- bootfilename, lease time
 *     Next step:	- TFTP
 *
 * TFTP:
 *
 *	Prerequisites:	- own ethernet address
 *			- own IP address
 *			- TFTP server IP address
 *			- TFTP server ethernet address
 *			- name of bootfile (if unknown, we use a default name
 *			  derived from our own IP address)
 *	We want:	- load the boot file
 *	Next step:	none
 *
 * NFS:
 *
 *	Prerequisites:	- own ethernet address
 *			- own IP address
 *			- name of bootfile (if unknown, we use a default name
 *			  derived from our own IP address)
 *	We want:	- load the boot file
 *	Next step:	none
 *
 * SNTP:
 *
 *	Prerequisites:	- own ethernet address
 *			- own IP address
 *	We want:	- network time
 *	Next step:	none
 */

#include <stddef.h>
#include "machine.h"
#include "command.h"
#include "net/net.h"
#include "net/tftp.h"
#include "printk.h"
#include "vivi_string.h"
#include "serial.h"
#include "time.h"
#include "priv_data.h"

#define ARP_TIMEOUT		5		/* Seconds before trying ARP again */
#ifndef	CONFIG_NET_RETRY_COUNT
# define ARP_TIMEOUT_COUNT	5		/* # of timeouts before giving up  */
#else
# define ARP_TIMEOUT_COUNT  (CONFIG_NET_RETRY_COUNT)
#endif

#ifdef CONFIG_NET_DEBUG
#define debug(fmt, args...) printk(fmt, ##args)
#else
#define debug(fmt, args...)
#endif

unsigned char		NetOurEther[6];		/* Our ethernet address			*/
unsigned char		NetServerEther[6] =	/* Boot server enet address		*/
			{ 0, 0, 0, 0, 0, 0 };

unsigned char		NetBcastAddr[6] =	/* Ethernet bcast address		*/
			{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
unsigned char		NetEtherNullAddr[6] =
			{ 0, 0, 0, 0, 0, 0 };

#define IPSTRINGSIZE 32

unsigned char CFG_NetOurIP[IPSTRINGSIZE] = CONFIG_HOST_IP;
unsigned char CFG_NetServerIP[IPSTRINGSIZE] = CONFIG_TFTP_SERVER;
unsigned char CFG_NetOurSubnetMask[IPSTRINGSIZE] = CONFIG_NET_MASK;
unsigned char CFG_NetOurGatewayIP[IPSTRINGSIZE] = CONFIG_GATEWAY_IP;
unsigned char LocalHostIP[IPSTRINGSIZE] = "127.0.0.1";
IPaddr_t	NetOurIP = 0;		/* Our IP addr (0 = unknown)		*/
IPaddr_t	NetServerIP = 0;	/* Our IP addr (0 = unknown)		*/
IPaddr_t        NetOurSubnetMask = 0;
IPaddr_t        NetOurGatewayIP = 0;

/* not actually used by now*/
//unsigned short  NetFileSize = 0;

unsigned long	NetFileXferSize;/* The actual transferred size of the bootfile (in bytes) */


volatile unsigned char *NetRxPkt;      	/* Current receive packet		*/
int		NetRxPktLen;		/* Current rx packet length		*/
unsigned	NetIPID;		/* IP packet ID				*/



int		NetState;		/* Network loop state			*/

char		load_file[128];		/* the name of file to loaded 			*/

IPaddr_t	NetPingIP;		/* the ip address to ping 		*/

static void PingStart(void);

volatile unsigned char	PktBuf[(PKTBUFSRX+1) * PKTSIZE_ALIGN + PKTALIGN];

volatile unsigned char *NetRxPackets[PKTBUFSRX]; /* Receive packets			*/

static rxhand_f *packetHandler;		/* Current RX packet handler		*/
static thand_f *timeHandler;		/* Current timeout handler		*/
static unsigned long	timeStart;		/* Time base value			*/
static unsigned long	timeDelta;		/* Current timeout value		*/
volatile unsigned char *NetTxPacket = 0;	/* THE transmit packet			*/

static int net_check_prereq (proto_t protocol);

/**********************************************************************/

IPaddr_t	NetArpWaitPacketIP;
IPaddr_t	NetArpWaitReplyIP;
unsigned char	       *NetArpWaitPacketMAC;	/* MAC address of waiting packet's destination	*/
unsigned char	       *NetArpWaitTxPacket;	/* THE transmit packet			*/
int		NetArpWaitTxPacketSize;
unsigned char 		NetArpWaitPacketBuf[PKTSIZE_ALIGN + PKTALIGN];
unsigned long		NetArpWaitTimerStart;
int		NetArpWaitTry;

unsigned long   load_addr;

void ArpRequest (void)
{
	int i;
	volatile unsigned char *pkt;
	ARP_t *arp;

#ifdef CONFIG_NET_DEBUG
	printk ("ARP broadcast %d\n", NetArpWaitTry);
#endif
	pkt = NetTxPacket;

	pkt += NetSetEther (pkt, NetBcastAddr, PROT_ARP);

	arp = (ARP_t *) pkt;

	arp->ar_hrd = htons (ARP_ETHER);
	arp->ar_pro = htons (PROT_IP);
	arp->ar_hln = 6;
	arp->ar_pln = 4;
	arp->ar_op = htons (ARPOP_REQUEST);

	memcpy (&arp->ar_data[0], NetOurEther, 6);		/* source ET addr	*/
	NetWriteIP ((unsigned char *) & arp->ar_data[6], NetOurIP);	/* source IP addr	*/
	for (i = 10; i < 16; ++i) {
		arp->ar_data[i] = 0;				/* dest ET addr = 0     */
	}

	if ((NetArpWaitPacketIP & NetOurSubnetMask) !=
	    (NetOurIP & NetOurSubnetMask)) {
		if (NetOurGatewayIP == 0) {
			putstr ("## Warning: gatewayip needed but not set\n");
			NetArpWaitReplyIP = NetArpWaitPacketIP;
		} else {
			NetArpWaitReplyIP = NetOurGatewayIP;
		}
	} else {
		NetArpWaitReplyIP = NetArpWaitPacketIP;
	}

	NetWriteIP ((unsigned char *) & arp->ar_data[16], NetArpWaitReplyIP);
	(void) eth_send (NetTxPacket, (pkt - NetTxPacket) + ARP_HDR_SIZE);
}

void ArpTimeoutCheck(void)
{
	unsigned long t;

	if (!NetArpWaitPacketIP)
		return;

	t = get_timer(0);

	/* check for arp timeout */
	if ((t - NetArpWaitTimerStart) > ARP_TIMEOUT * HZ) {
		NetArpWaitTry++;

		if (NetArpWaitTry >= ARP_TIMEOUT_COUNT) {
			putstr ("\nARP Retry count exceeded; starting again\n");
			NetArpWaitTry = 0;
			NetStartAgain();
		} else {
			NetArpWaitTimerStart = t;
			ArpRequest();
		}
	}
}

/**********************************************************************/
/*
 *	Main network processing loop.
 */

int
NetLoop(proto_t protocol)
{
        int ret;

	/* XXX problem with bss workaround */
	NetArpWaitPacketMAC = NULL;
	NetArpWaitTxPacket = NULL;
	NetArpWaitPacketIP = 0;
	NetArpWaitReplyIP = 0;
	NetTxPacket = NULL;

	if (!NetTxPacket) {
		int	i;
		/*
		 *	Setup packet buffers, aligned correctly.
		 */
		NetTxPacket = &PktBuf[0] + (PKTALIGN - 1);
		NetTxPacket -= (unsigned long)NetTxPacket % PKTALIGN;
		for (i = 0; i < PKTBUFSRX; i++) {
			NetRxPackets[i] = NetTxPacket + (i+1)*PKTSIZE_ALIGN;
		}
	}

	if (!NetArpWaitTxPacket) {
		NetArpWaitTxPacket = &NetArpWaitPacketBuf[0] + (PKTALIGN - 1);
		NetArpWaitTxPacket -= (unsigned long)NetArpWaitTxPacket % PKTALIGN;
		NetArpWaitTxPacketSize = 0;
	}

restart:
	memcpy (NetOurEther, eth_get_enetaddr(), 6);
#ifdef CONFIG_NET_DEBUG
	printk("NetOurEther = %x\n", NetOurEther);
#endif
	NetState = NETLOOP_CONTINUE;

	/*
	 *	Start the ball rolling with the given start function.  From
	 *	here on, this code is a state machine driven by received
	 *	packets and timer events.
	 */
          NetOurIP = (IPaddr_t)get_param_value("net_host", &ret);
          if(NetOurIP == 0)
	    {
	      NetOurIP = get_IPaddr(CFG_NetOurIP);
	      printk("net_host is set to %s: \n", CFG_NetOurIP);
	      if(set_param_value("net_host", (param_value_t)NetOurIP) < 0)
		printk("set param failed!\n");
	    }

          NetOurGatewayIP = (IPaddr_t)get_param_value("net_gateway", &ret);
	  if(NetOurGatewayIP == 0)
	    {
	      NetOurGatewayIP = get_IPaddr(CFG_NetOurGatewayIP);
	      printk("net_gateway is set to %s: \n", CFG_NetOurGatewayIP);
	      if(set_param_value("net_gateway", (param_value_t)NetOurGatewayIP) < 0)
		printk("set param failed!\n");
	    }
	  NetOurSubnetMask = (IPaddr_t)get_param_value("net_mask", &ret);
	  if(NetOurSubnetMask == 0)
	    {
	      NetOurSubnetMask = get_IPaddr(CFG_NetOurSubnetMask);
	      printk("net_mask is set to %s: \n", CFG_NetOurSubnetMask);
	      if(set_param_value("net_mask", (param_value_t)NetOurSubnetMask) < 0)
		printk("set param failed!\n");
	    }

	  switch (protocol) {

	  case TFTP:
	    NetServerIP = (IPaddr_t)get_param_value("net_tftp_server", &ret);
	    if(NetServerIP == 0)
	    {
	      NetServerIP = get_IPaddr(CFG_NetServerIP);
	      printk("net_tftp_server is set to %s: \n", CFG_NetServerIP);
	      if(set_param_value("net_tftp_server", (param_value_t)NetServerIP) < 0)
		printk("set param failed!\n");
	    }
	    break;

	  case PING:
	    if(NetPingIP == NetOurIP || NetPingIP == get_IPaddr(LocalHostIP))
	      return 0;
	    else
	      break;

	  default:
	    break;
	  }
	  
	switch (net_check_prereq (protocol)) {
	case 1:
		/* network not configured */
		eth_halt();
		return (-1);
	case 0:
		switch (protocol) {
		case TFTP:
			/* always use ARP to get server ethernet address */
			TftpStart();
			break;

		case PING:
			PingStart();
			break;

		default:
			break;
		}

		NetFileXferSize = 0;
		break;
	}

	/*
	 *	Main packet reception loop.  Loop receiving packets until
	 *	someone sets `NetState' to a state that terminates.
	 */
	for (;;) {
	  //WATCHDOG_RESET();
#ifdef CONFIG_SHOW_ACTIVITY
		{
			extern void show_activity(int arg);
			show_activity(1);
		}
#endif
		/*
		 *	Check the ethernet for a new packet.  The ethernet
		 *	receive routine will process it.
		 */
			eth_rx();

		/*
		 *	Abort if ctrl-c was pressed.
		 *      It is seemed that CTRL-C has not implemented yet.
		 */

			
		if (ctrlc()) {
			eth_halt();
			printk("\nAbort\n");
			return (-1);
		}
		
	
		ArpTimeoutCheck();		

		/*
		 *	Check for a timeout, and run the timeout handler
		 *	if we have one.
		 */
		if (timeHandler && ((get_timer(0) - timeStart) > timeDelta)) 
		{
			thand_f *x;
			x = timeHandler;
			timeHandler = (thand_f *)0;
			(*x)();
		}


		switch (NetState) {

		case NETLOOP_RESTART:
			goto restart;

		case NETLOOP_SUCCESS:
			if (NetFileXferSize > 0) {
				printk("Bytes transferred = %ld (%lx hex)\n",
					NetFileXferSize,
					NetFileXferSize);
			}
			eth_halt();
			return NetFileXferSize;

		case NETLOOP_FAIL:
			return (-1);
		}
	}
}

/**********************************************************************/

static void
startAgainTimeout(void)
{
	NetState = NETLOOP_RESTART;
}

static void
startAgainHandler(unsigned char * pkt, unsigned dest, unsigned src, unsigned len)
{
	/* Totally ignore the packet */
}

void NetStartAgain (void)
{
#ifdef NET_NORETRY
	char *nretry;
	int noretry = 0;
	
	
	if ((nretry = getenv ("netretry")) != NULL) {
		noretry = (strcmp (nretry, "no") == 0);
		once = (strcmp (nretry, "once") == 0);
	}
	
	if (noretry) {
		eth_halt ();
		NetState = NETLOOP_FAIL;
		return;
	}
#endif
	NetSetTimeout (10 * HZ, startAgainTimeout);
	NetSetHandler (startAgainHandler);
}

/**********************************************************************/
/*
 *	Miscelaneous bits.
 */

void
NetSetHandler(rxhand_f * f)
{
	packetHandler = f;
}


void
NetSetTimeout(unsigned long iv, thand_f * f)
{
	if (iv == 0) {
		timeHandler = (thand_f *)0;
	} else {
		timeHandler = f;
		timeStart = get_timer(0);
		timeDelta = iv;
	}
}

#if 0
void
NetSendPacket(volatile unsigned char * pkt, int len)
{
	(void) eth_send(pkt, len);
}
#endif

int
NetSendUDPPacket(unsigned char *ether, IPaddr_t dest, int dport, int sport, int len)
{
	unsigned char *pkt;

	/* convert to new style broadcast */
	if (dest == 0)
		dest = 0xFFFFFFFF;

	/* if broadcast, make the ether address a broadcast and don't do ARP */
	if (dest == 0xFFFFFFFF)
		ether = NetBcastAddr;

	/* if MAC address was not discovered yet, save the packet and do an ARP request */
	if (memcmp(ether, NetEtherNullAddr, 6) == 0) {

#ifdef CONFIG_NET_DEBUG
		printk("sending ARP for %08lx\n", dest);
#endif
		NetArpWaitPacketIP = dest;
		NetArpWaitPacketMAC = ether;

		pkt = NetArpWaitTxPacket;
		pkt += NetSetEther (pkt, NetArpWaitPacketMAC, PROT_IP);

		NetSetIP (pkt, dest, dport, sport, len);
		memcpy(pkt + IP_HDR_SIZE, (unsigned char *)NetTxPacket + (pkt - (unsigned char *)NetArpWaitTxPacket) + IP_HDR_SIZE, len);

		/* size of the waiting packet */
		NetArpWaitTxPacketSize = (pkt - NetArpWaitTxPacket) + IP_HDR_SIZE + len;

		/* and do the ARP request */
		NetArpWaitTry = 1;
		NetArpWaitTimerStart = get_timer(0);
		ArpRequest();
		return 1;	/* waiting */
	}

#ifdef CONFIG_NET_DEBUG
	printk("sending UDP to %08lx/%02x:%02x:%02x:%02x:%02x:%02x\n",
		dest, ether[0], ether[1], ether[2], ether[3], ether[4], ether[5]);
#endif

	pkt = (unsigned char *)NetTxPacket;
	pkt += NetSetEther (pkt, ether, PROT_IP);
	NetSetIP (pkt, dest, dport, sport, len);
	(void) eth_send(NetTxPacket, (pkt - NetTxPacket) + IP_HDR_SIZE + len);

	return 0;	/* transmitted */
}

static unsigned short PingSeqNo;

int PingSend(void)
{
	static unsigned char mac[6];
	//volatile IP_t *ip;
	IP_t *ip;
	volatile unsigned short *s;
	unsigned char *pkt;

	/* XXX always send arp request */
	memcpy(mac, NetEtherNullAddr, 6);

#ifdef CONFIG_NET_DEBUG
	printk("sending ARP for %08lx\n", NetPingIP);
#endif


	NetArpWaitPacketIP = NetPingIP;
	NetArpWaitPacketMAC = mac;

	pkt = NetArpWaitTxPacket;
	pkt += NetSetEther(pkt, mac, PROT_IP);
	
	//ip = (volatile IP_t *)pkt;
	ip = (IP_t *)pkt;

	/*
	 *	Construct an IP and ICMP header.  (need to set no fragment bit - XXX)
	 */
	ip->ip_hl_v  = 0x45;		/* IP_HDR_SIZE / 4 (not including UDP) */
	ip->ip_tos   = 0;
	ip->ip_len   = htons(IP_HDR_SIZE_NO_UDP + 8);
	ip->ip_id    = htons(NetIPID++);
	ip->ip_off   = htons(0x4000);	/* No fragmentation */
	ip->ip_ttl   = 255;
	ip->ip_p     = 0x01;		/* ICMP */
	ip->ip_sum   = 0;
	NetCopyIP((void*)&ip->ip_src, &NetOurIP); /* already in network byte order */
	NetCopyIP((void*)&ip->ip_dst, &NetPingIP);	   /* - "" - */
	ip->ip_sum   = ~NetCksum((unsigned char *)ip, IP_HDR_SIZE_NO_UDP / 2);
	s = &ip->udp_src;		/* XXX ICMP starts here */
	s[0] = htons(0x0800);		/* echo-request, code */
	s[1] = 0;			/* checksum */
	s[2] = 0; 			/* identifier */
	s[3] = htons(PingSeqNo++);	/* sequence number */
	s[1] = ~NetCksum((unsigned char *)s, 8/2);
	/* size of the waiting packet */
	NetArpWaitTxPacketSize = (pkt - NetArpWaitTxPacket) + IP_HDR_SIZE_NO_UDP + 8;

	/* and do the ARP request */
	NetArpWaitTry = 1;
	NetArpWaitTimerStart = get_timer(0);
	ArpRequest();

	return 1;	/* waiting */
}

static void
PingTimeout (void)
{
	eth_halt();
	NetState = NETLOOP_FAIL;	/* we did not get the reply */
}

static void
PingHandler (unsigned char * pkt, unsigned dest, unsigned src, unsigned len)
{
	IPaddr_t tmp;
	volatile IP_t *ip = (volatile IP_t *)pkt;

	tmp = NetReadIP((void *)&ip->ip_src);
	if (tmp != NetPingIP)
		return;

	NetState = NETLOOP_SUCCESS;
}

static void PingStart(void)
{
  NetSetTimeout (10 * HZ, PingTimeout);
  NetSetHandler (PingHandler);

  PingSend();
}

void NetReceive(volatile unsigned char * inpkt, int len)
{
	Ethernet_t *et;
	IP_t	*ip = NULL;
	ARP_t	*arp;
	IPaddr_t tmp;
	int	x;
	unsigned char *pkt = NULL;
#if 0
	int i = 0;
	unsigned short *tpkt = NULL;
#endif	


#ifdef CONFIG_NET_DEBUG
	printk("\npacket received");
	printk("\npacket len = %d\n", len);
#endif

	NetRxPkt = inpkt;
	NetRxPktLen = len;
	et = (Ethernet_t *)inpkt;

	/* too small packet? */
	if (len < ETHER_HDR_SIZE)
	  return;

#if 0
	tpkt = (unsigned short *)inpkt;
	for(i = 0; i < (len/2 + 1); i++)
	  printk("%.4X ", tpkt[i]);
#endif

	x = ntohs(et->et_protlen);
       
	if (x < 1514) {
		/*
		 *	Got a 802 packet.  Check the other protocol field.
		 */
		x = ntohs(et->et_prot);

		ip = (IP_t *)(inpkt + E802_HDR_SIZE);
		len -= E802_HDR_SIZE;

	} else if (x != PROT_VLAN) {	/* normal packet */
		ip = (IP_t *)(inpkt + ETHER_HDR_SIZE);
		len -= ETHER_HDR_SIZE;
	} 

#ifdef CONFIG_NET_DEBUG
	printk("Receive from protocol 0x%x\n", x);
#endif

	switch (x) {

	case PROT_ARP:
		/*
		 * We have to deal with two types of ARP packets:
		 * - REQUEST packets will be answered by sending  our
		 *   IP address - if we know it.
		 * - REPLY packates are expected only after we asked
		 *   for the TFTP server's or the gateway's ethernet
		 *   address; so if we receive such a packet, we set
		 *   the server ethernet address
		 */
#ifdef CONFIG_NET_DEBUG
		putstr ("Got ARP\n");
#endif

		arp = (ARP_t *)ip;
		if (len < ARP_HDR_SIZE) {
			printk("bad length %d < %d\n", len, ARP_HDR_SIZE);
			return;
		}
		if (ntohs(arp->ar_hrd) != ARP_ETHER) {
			return;
		}
		if (ntohs(arp->ar_pro) != PROT_IP) {
			return;
		}
		if (arp->ar_hln != 6) {
			return;
		}
		if (arp->ar_pln != 4) {
			return;
		}
		if (NetOurIP == 0) {
			return;
		}
		if (NetReadIP(&arp->ar_data[16]) != NetOurIP) {
			return;
		}
		switch (ntohs(arp->ar_op)) {
		case ARPOP_REQUEST:		/* reply with our IP address	*/
#ifdef CONFIG_NET_DEBUG
			putstr ("Got ARP REQUEST, return our IP\n");
#endif
			pkt = (unsigned char *)et;
			pkt += NetSetEther(pkt, et->et_src, PROT_ARP);
			arp->ar_op = htons(ARPOP_REPLY);
			memcpy   (&arp->ar_data[10], &arp->ar_data[0], 6);
			NetCopyIP(&arp->ar_data[16], &arp->ar_data[6]);
			memcpy   (&arp->ar_data[ 0], NetOurEther, 6);
			NetCopyIP(&arp->ar_data[ 6], &NetOurIP);
			(void) eth_send((unsigned char *)et, (pkt - (unsigned char *)et) + ARP_HDR_SIZE);
			return;

		case ARPOP_REPLY:		/* arp reply */
			/* are we waiting for a reply */
			if (!NetArpWaitPacketIP || !NetArpWaitPacketMAC)
				break;
#ifdef CONFIG_NET_DEBUG
			printk("Got ARP REPLY, set server/gtwy eth addr (%02x:%02x:%02x:%02x:%02x:%02x)\n",
				arp->ar_data[0], arp->ar_data[1],
				arp->ar_data[2], arp->ar_data[3],
				arp->ar_data[4], arp->ar_data[5]);
#endif

			tmp = NetReadIP(&arp->ar_data[6]);

			/* matched waiting packet's address */
			if (tmp == NetArpWaitReplyIP) {
#ifdef CONFIG_NET_DEBUG
				putstr ("Got it\n");
#endif
				/* save address for later use */
				memcpy(NetArpWaitPacketMAC, &arp->ar_data[0], 6);

				/* modify header, and transmit it */
				memcpy(((Ethernet_t *)NetArpWaitTxPacket)->et_dest, NetArpWaitPacketMAC, 6);
				(void) eth_send(NetArpWaitTxPacket, NetArpWaitTxPacketSize);

				/* no arp request pending now */
				NetArpWaitPacketIP = 0;
				NetArpWaitTxPacketSize = 0;
				NetArpWaitPacketMAC = NULL;

			}
			return;
		default:
#ifdef CONFIG_NET_DEBUG
			printk("Unexpected ARP opcode 0x%x\n", ntohs(arp->ar_op));
#endif
			return;
		}
		break;

	case PROT_IP:
#ifdef CONFIG_NET_DEBUG
		putstr ("Got IP\n");
#endif
		if (len < IP_HDR_SIZE) {
			debug ("len bad %d < %d\n", len, IP_HDR_SIZE);
			return;
		}
		if (len < ntohs(ip->ip_len)) {
			printk("len bad %d < %d\n", len, ntohs(ip->ip_len));
			return;
		}
		len = ntohs(ip->ip_len);
		if ((ip->ip_hl_v & 0xf0) != 0x40) {
			return;
		}
		if (ip->ip_off & htons(0x1fff)) { /* Can't deal w/ fragments */
			return;
		}
		if (!NetCksumOk((unsigned char *)ip, IP_HDR_SIZE_NO_UDP / 2)) {
			putstr ("checksum bad\n");
			return;
		}
		tmp = NetReadIP(&ip->ip_dst);
		if (NetOurIP && tmp != NetOurIP && tmp != 0xFFFFFFFF) {
			return;
		}
		/*
		 * watch for ICMP host redirects
		 *
		 * There is no real handler code (yet). We just watch
		 * for ICMP host redirect messages. In case anybody
		 * sees these messages: please contact me
		 * (wd@denx.de), or - even better - send me the
		 * necessary fixes :-)
		 *
		 * Note: in all cases where I have seen this so far
		 * it was a problem with the router configuration,
		 * for instance when a router was configured in the
		 * BOOTP reply, but the TFTP server was on the same
		 * subnet. So this is probably a warning that your
		 * configuration might be wrong. But I'm not really
		 * sure if there aren't any other situations.
		 */
		if (ip->ip_p == IPPROTO_ICMP) {
			ICMP_t *icmph = (ICMP_t *)&(ip->udp_src);

			switch (icmph->type) {
			case ICMP_REDIRECT:
				if (icmph->code != ICMP_REDIR_HOST)
					return;
				putstr(" ICMP Host Redirect to ");
				print_IPaddr(icmph->un.gateway);
				putc(' ');
				return;

			case ICMP_ECHO_REPLY:
				/*
				 *	IP header OK.  Pass the packet to the current handler.
				 */
				/* XXX point to ip packet */
				(*packetHandler)((unsigned char *)ip, 0, 0, 0);
				return;

			default:
				return;
			}
		} else if (ip->ip_p != IPPROTO_UDP) {	/* Only UDP packets */
			return;
		}

#ifdef CONFIG_UDP_CHECKSUM
		if (ip->udp_xsum != 0) {
			unsigned long   xsum;
			unsigned short *sumptr;
			unsigned short  sumlen;

			xsum  = ip->ip_p;
			xsum += (ntohs(ip->udp_len));
			xsum += (ntohl(ip->ip_src) >> 16) & 0x0000ffff;
			xsum += (ntohl(ip->ip_src) >>  0) & 0x0000ffff;
			xsum += (ntohl(ip->ip_dst) >> 16) & 0x0000ffff;
			xsum += (ntohl(ip->ip_dst) >>  0) & 0x0000ffff;

			sumlen = ntohs(ip->udp_len);
			sumptr = (unsigned short *) &(ip->udp_src);

			while (sumlen > 1) {
				unsigned short sumdata;

				sumdata = *sumptr++;
				xsum += ntohs(sumdata);
				sumlen -= 2;
			}
			if (sumlen > 0) {
				unsigned short sumdata;

				sumdata = *(unsigned char *) sumptr;
				sumdata = (sumdata << 8) & 0xff00;
				xsum += sumdata;
			}
			while ((xsum >> 16) != 0) {
				xsum = (xsum & 0x0000ffff) + ((xsum >> 16) & 0x0000ffff);
			}
			if ((xsum != 0x00000000) && (xsum != 0x0000ffff)) {
				printk(" UDP wrong checksum %08x %08x\n", xsum, ntohs(ip->udp_xsum));
				return;
			}
		}
#endif

		/*
		 *	IP header OK.  Pass the packet to the current handler.
		 */
		(*packetHandler)((unsigned char *)ip +IP_HDR_SIZE,
						ntohs(ip->udp_dst),
						ntohs(ip->udp_src),
						ntohs(ip->udp_len) - 8);
		break;
	}
}


/**********************************************************************/

static int net_check_prereq (proto_t protocol)
{
	switch (protocol) {
		/* Fall through */

	case PING:
		if (NetPingIP == 0) {
			putstr ("*** ERROR: ping address not given\n");
			return (1);
		}
		goto common;

	case TFTP:
		if (NetServerIP == 0) {
			putstr ("*** ERROR: `serverip' not set\n");
			return (1);
		}

	common:
		if (NetOurIP == 0) {
			putstr ("*** ERROR: `ipaddr' not set\n");
			return (1);
		}
		/* Fall through */

		if (memcmp (NetOurEther, "\0\0\0\0\0\0", 6) == 0) {
			putstr ("*** ERROR: `ethaddr' not set\n");
			return (1);
		}
		/* Fall through */
	default:
		return (0);
	}

	return (0);		/* OK */
}
/**********************************************************************/

int
NetCksumOk(unsigned char * ptr, int len)
{
	return !((NetCksum(ptr, len) + 1) & 0xfffe);
}


unsigned
NetCksum(unsigned char * ptr, int len)
{
	unsigned long	xsum;
	unsigned short *p = (unsigned short *)ptr;

	xsum = 0;
	while (len-- > 0)
		xsum += *p++;
	xsum = (xsum & 0xffff) + (xsum >> 16);
	xsum = (xsum & 0xffff) + (xsum >> 16);
	return (xsum & 0xffff);
}

inline int NetEthHdrSize(void)
{
	return ETHER_HDR_SIZE;
}

int
NetSetEther(volatile unsigned char * xet, unsigned char * addr, unsigned int prot)
{
	Ethernet_t *et = (Ethernet_t *)xet;

	memcpy (et->et_dest, addr, 6);
	memcpy (et->et_src, NetOurEther, 6);
	
	et->et_protlen = htons(prot);
	
	return ETHER_HDR_SIZE;
}

void
NetSetIP(volatile unsigned char * xip, IPaddr_t dest, int dport, int sport, int len)
{
  
        //volatile IP_t *ip = (IP_t *)xip;
	IP_t *ip = (IP_t *)xip;

	/*
	 *	If the data is an odd number of bytes, zero the
	 *	byte after the last byte so that the checksum
	 *	will work.
	 */
	if (len & 1)
		xip[IP_HDR_SIZE + len] = 0;

	/*
	 *	Construct an IP and UDP header.
	 *	(need to set no fragment bit - XXX)
	 */
	ip->ip_hl_v  = 0x45;		/* IP_HDR_SIZE / 4 (not including UDP) */
	ip->ip_tos   = 0;
	ip->ip_len   = htons(IP_HDR_SIZE + len);
	ip->ip_id    = htons(NetIPID++);
	ip->ip_off   = htons(0x4000);	/* No fragmentation */
	ip->ip_ttl   = 255;
	ip->ip_p     = 17;		/* UDP */
	ip->ip_sum   = 0;
	NetCopyIP((void*)&ip->ip_src, &NetOurIP); /* already in network byte order */
	NetCopyIP((void*)&ip->ip_dst, &dest);	   /* - "" - */
	ip->udp_src  = htons(sport);
	ip->udp_dst  = htons(dport);
	ip->udp_len  = htons(8 + len);
	ip->udp_xsum = 0;
	ip->ip_sum   = ~NetCksum((unsigned char *)ip, IP_HDR_SIZE_NO_UDP / 2);
}

void copy_filename (char *dst, const char *src, int size)
{
	if (*src && (*src == '"')) {
		++src;
		--size;
	}

	while ((--size > 0) && *src && (*src != '"')) {
		*dst++ = *src++;
	}
	*dst = '\0';
}

char* ip_to_string (IPaddr_t x, char *s)
{
	if(s == NULL)
		return NULL;

	x = ntohl (x);
	sprintf (s, "%d.%d.%d.%d",
		 (int) ((x >> 24) & 0xff),
		 (int) ((x >> 16) & 0xff),
		 (int) ((x >> 8) & 0xff), (int) ((x >> 0) & 0xff)
	);

	return s;
}

IPaddr_t string_to_ip(char *s)
{
	IPaddr_t addr;
	/*char *e;
	int i;

	if (s == NULL)
		return 0;

	for (addr=0, i=0; i<4; ++i) {
		unsigned long val = s ? simple_strtoul(s, &e, 10) : 0;
		addr <<= 8;
		addr |= (val & 0xFF);
		if (s) {
			s = (*e) ? e+1 : e;
		}
	}

	return (htonl(addr));
	*/
	return addr;
}

void print_IPaddr (IPaddr_t x)
{
	char tmp[16];

	ip_to_string (x, tmp);

	putstr (tmp);
}

IPaddr_t get_IPaddr (char *var)
{
  return (string_to_ip(var));
}

int net_parameter_init(void)
{
  int ret = NET_PARAM_SAVED;
  int tmp;

  NetOurIP = (IPaddr_t)get_param_value("net_host", &tmp);
  if(NetOurIP == 0)
    {
      NetOurIP = get_IPaddr(CFG_NetOurIP);
      ret = NET_PARAM_NULL;
      if(set_param_value("net_host", (param_value_t)NetOurIP) < 0)
	{
	  printk("set param error!\n");
	  ret = NET_PARAM_SET_FAILED;
	}
    }

  NetOurGatewayIP = (IPaddr_t)get_param_value("net_gateway", &tmp);
  if(NetOurGatewayIP == 0)
    {
      NetOurGatewayIP = get_IPaddr(CFG_NetOurGatewayIP);
      ret = NET_PARAM_NULL;
      if(set_param_value("net_gateway", (param_value_t)NetOurGatewayIP) < 0)
	{
	  printk("set param error!\n");
	  ret = NET_PARAM_SET_FAILED;
	}
    }

  NetOurSubnetMask = (IPaddr_t)get_param_value("net_mask", &tmp);
  if(NetOurSubnetMask == 0)
    {
      NetOurSubnetMask = get_IPaddr(CFG_NetOurSubnetMask);
      ret = NET_PARAM_NULL;
      if(set_param_value("net_mask", (param_value_t)NetOurSubnetMask) < 0)
	{
	  printk("set param error!\n");
	  ret = NET_PARAM_SET_FAILED;
	}
    }

  NetServerIP = (IPaddr_t)get_param_value("net_tftp_server", &tmp);
  if(NetServerIP == 0)
    {
      NetServerIP = get_IPaddr(CFG_NetServerIP);
      ret = NET_PARAM_NULL;
      if(set_param_value("net_tftp_server", (param_value_t)NetServerIP) < 0)
	{
	  printk("set param error!\n");
	  ret = NET_PARAM_SET_FAILED;
	}
    }

  return ret;
}

