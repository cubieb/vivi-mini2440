/*
 * vivi/arch/s3c2410/proc.c
 *
 * Copyright (C) 2002 MIZI Research, Inc.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * 
 * Author: Janghoon Lyu <nandy@mizi.com>
 * Date  : $Date: 2004/02/04 06:22:25 $
 *
 * $Revision: 1.1.1.1 $
 * $Id: proc.c,v 1.1.1.1 2004/02/04 06:22:25 laputa Exp $
 *
 *
 * History
 *
 * 2002-07-12: Janghoon Lyu <nandy@mizi.com>
 *    - Initial code
 *
 */

#include "config.h"
#include "machine.h"
#include "mmu.h"
#include "command.h"
#include "vivi_string.h"
#include "printk.h"
#include "priv_data.h"
#include "time.h"
#include <types.h>
#include <string.h>

#define GET_PCLK		0
#define GET_HCLK		1

struct cpu_clk_t {
	unsigned long clock;
	unsigned long mdiv;
	unsigned long pdiv;
	unsigned long sdiv;
};


struct mem_clk_t {
	unsigned long clock;
	unsigned long bankcon6;
	unsigned long bankcon7;
	unsigned long refresh;
};


/*
 * cpu_clk = FIN 
 */
struct cpu_clk_t s3c2410_clks[] = {
	/*	mdiv	sdiv	pdiv */
	{  50,	0x5c,	0x4,	0x0 },
	{ 200,	0x5c,	0x4,	0x0 },
	{ 203,	 0x3,	0x4,	0x1 },	/* not tested */
	{ 202,	0xa1,	0x3,	0x1 },  /* not tested */
	{ 284,	0x86,	0x1,	0x1 },
	{ 290,	0x89,	0x1,	0x1 },
	{ 400,	0x5c,	0x1,	0x0 },
	{ 474,	0x96,	0x2,	0x0 },
	{ 508,	0x77,	0x1,	0x0 },
	{ 533,	0xd6,	0x3,	0x0 },
	{   0,	   0,	  0,	  0 }
};


/*
 * Memory Clock Change 
 */
struct mem_clk_t s3c2410_memclks[] = {
	/*FClK  Bcon6       Bcon7        Ref */
	{ 284,	0x00018005, 0x00018005, 0x009c0379},
	{ 290,	0x00018005, 0x00018005, 0x009c0379},
	{ 400,	0x00018005, 0x00018005, 0x009c0379},
	{   0,	   0,	  0,	  0 }
};

static inline unsigned long
cal_bus_clk(unsigned long cpu_clk, unsigned long ratio, int who)
{
	unsigned long hclk = 0,pclk = 0;

	switch( (ratio&0x6) >> 1 ) {
		case 0:  
			hclk = cpu_clk;
			break;
		case 1:
			hclk = cpu_clk/2;
			break;
		case 2:
			hclk = cpu_clk/4;
			break;
		case 3:
			hclk = cpu_clk/3;
			break;
	}
	switch ( ratio&0x1) {
		case 0:
			pclk = hclk;
			break;
		case 1:
			pclk = hclk/2;
			break;
	}
	return ( who ? hclk: pclk );
}

/* 
 * Note: this code is not tested.
 *
 * set_cpu_clk():
 *   clk: cpu clock to set. unit is mega hertz 
 *   ratio: 
 */
static int 
set_cpu_clk(unsigned long clk)
{
	struct cpu_clk_t *clks = s3c2410_clks;

	while (clks->clock != 0) {
		if (clk == clks->clock)
		    break;
		clks++;
	}

	if (clks->clock == 0) {
		printk("Can not find cpu clock table\n");
		return -2;
	}
	
	MPLLCON = ((clks->mdiv << 12) | (clks->pdiv << 4) | (clks->sdiv));

	return 0;
}
/*
static int
change_memctl_regs(unsigned long clk) 
{
	struct mem_clk_t * memclks = s3c2410_memclks;

	while ( memclks->clock !=0) {
		if (clk == memclks->clock)
			break;
		memclks++;
	}

	if (memclks->clock == 0) {
		printk("Can not find mem  clock table\n");
		return -2;
	}
	BANKCON6 = memclks->bankcon6;
	BANKCON7 = memclks->bankcon7;
	REFRESH  = memclks->refresh;
	return 0;
}
*/

#define REFRESH_PERIOD 78  /* 7.8us */
static int
change_refresh(unsigned long hclk)
{
	hclk = hclk /10000000;
	REFRESH = (REFRESH & ~0x7ff) | (2049-(unsigned int)(hclk * REFRESH_PERIOD));
	return 0;
}
/*
 * cpu clock = (((mdiv + 8) * FIN) / ((pdiv + 2) * (1 << sdiv)))
 *  FIN = Input Frequency (to CPU)
 */
static inline unsigned long
get_cpu_clk(void)
{
	unsigned long val = MPLLCON;
	
	return (((GET_MDIV(val) + 8) * FIN * 2) / \
		((GET_PDIV(val) + 2) * (1 << GET_SDIV(val))));
		
	
}

static unsigned long
get_bus_clk(int who)
{
	unsigned long cpu_clk = get_cpu_clk();
	unsigned long ratio = CLKDIVN;
	return (cal_bus_clk(cpu_clk, ratio, who));
}

static void 
print_cpu_info(void)
{
	long armrev = 0;
	unsigned long mpll = MPLLCON;
	
	__asm__("mrc	p15, 0, %0, c0, c0, 0" : "=r" (armrev));

	printk("\nProcessor Information (Revision: 0x%08lx)\n", armrev);
	printk("--------------------------------------------\n");
	printk("Processor clock: %d Hz\n", get_cpu_clk());
	printk("AHB bus clock  : %d Hz\n", get_bus_clk(GET_HCLK));
	printk("APB bus clock  : %d Hz\n", get_bus_clk(GET_PCLK));
	printk("\nRegister values\n");
	printk("MPLLCON: 0x%08lx", mpll);
	printk("  (MDIV: 0x%04x, PDIV: 0x%02x, SDIV: 0x%02x)\n",
		GET_MDIV(mpll), GET_PDIV(mpll), GET_SDIV(mpll));
	printk("CLKDIVN: 0x%08lx\n\n", CLKDIVN);
}

/*
 * A clock is the PCLK clock.
 */
static void 
change_baudrate(unsigned long clock)
{
	unsigned long baudrate;
	int ret;

	baudrate = get_param_value("baudrate", &ret);
	if (ret) {
		printk("There is no 'baudrate' parameter\n");
		return;
	}
	
#if defined(CONFIG_SERIAL_UART0)
	UBRDIV0 = ((clock / (baudrate * 16)) - 1);
#elif defined(CONFIG_SERIAL_UART1)
	UBRDIV1 = ((clock / (baudrate * 16)) - 1);
#endif
}

void change_clk_divider(int divider )
{
	int hdivn =2, pdivn = 0;

	switch (divider) {
		case 111:hdivn = 1; pdivn = 1; break;
		case 122:hdivn = 2; pdivn = 2; break;
		case 124:hdivn = 2; pdivn = 4; break;
		case 133:hdivn = 3; pdivn = 3; break;
		case 136:hdivn = 3; pdivn = 6; break;
		case 144:hdivn = 4; pdivn = 4; break;
		case 148:hdivn = 8; pdivn = 8; break;
		default:
			 printk(" Wrong divider %d \n");
	}
	//if ( hdivn !=0 ) 
	//	MMU_SetAsyncBusMode();
//	else
//		MMU_SetFastBusMode();
}

/*
 * change cpu clock and bus clock
 * Changed By SW.LEE
 *
 */
int change_sys_clks(unsigned long cpu_clk, unsigned long ratio)
{
	int ret = 0;

	printk("CPU Clock Change From %d Mhz  To %d Mhz \n",
				get_cpu_clk()/1000000,cpu_clk);
	if (ratio > 7) {
		printk("Wrong Clock Ration %d \n",ratio);
	}
	else
	{
		CLKDIVN = ratio;
	}
	
	ret = set_cpu_clk(cpu_clk);
	if (ret) return -1;	

//	change_memctl_regs(cpu_clk);
	change_refresh(get_bus_clk(GET_HCLK)); 

	change_baudrate(get_bus_clk(GET_PCLK));

	{ 		/* To avoid showing broken characters to console */
		int delay;
		for (delay =0; delay < 0x10000; delay ++)
			;
	}

	return 0;
}

/*
 * change a uart baudrate 
 */
void 
change_uart_baudrate(void)
{
	change_baudrate(get_bus_clk(GET_PCLK));
}

static inline int 
get_clk_divider(void)
{
	int val = GET_DIVIDER_TIMER4(TCFG1);
	return (2 << (val));
}

unsigned long 
get_clock_tick_rate(void)
{
  unsigned long prescale = GET_PRESCALE_TIMER4(TCFG0);
  unsigned long divider = (unsigned long)get_clk_divider();
  unsigned freq = get_bus_clk(GET_PCLK);
  
  if (!divider)
    return 0;
  
  return (freq / (prescale + 1) / divider);
}

unsigned long arch_read_timer(void)
{
	return 	TCNTO4;
}

void 
arch_init_time(void)
{
	TCFG0 = (TCFG0_DZONE(0) | TCFG0_PRE1(15) | TCFG0_PRE0(0));

	//added by ayang 2008-10-28
	timer_load_val = get_clock_tick_rate()/100;
	lastdec = TCNTB4 = timer_load_val;
	//printk("timer_load_val = %d, lastdec = %d\n", timer_load_val, lastdec);

	//TCON = (TCON & ~0x700000) | 0x600000;
	//TCON = (TCON & ~0x700000) | 0x500000;
	
	TCON = (TCON_4_AUTO | TCON_4_UPDATE | COUNT_4_OFF);	/* load counter value */

	TCON = (TCON_4_AUTO | COUNT_4_ON);			/* start timer */
}

/*
 * Perform a soft reset of the system.  Put the CPU into the
 * same state as it would be if it had been reset, and branch
 * to what would be the reset vector.
 *
 * loc: location to jump to for soft reset
 */
void processor_reset(unsigned long loc)
{
	cache_clean_invalidate();
	tlb_invalidate();

__asm__(
	"mov	ip, #0\n"
	"mcr	p15, 0, ip, c7, c7, 0\n"	/* invalidate I,D caches */
	"mcr	p15, 0, ip, c7, c10, 4\n"	/* drain WB */
	"mcr	p15, 0, ip, c8, c7, 0\n"	/* invalidate I & D TLBs */
	"mrc	p15, 0, ip, c1, c0, 0\n"	/* ctrl register */
	"bic	ip, ip, #0x000f\n"		/* ............wcam */
	"bic	ip, ip, #0x1100\n"		/* ...i...s........ */
	"mcr	p15, 0, ip, c1, c0, 0\n"	/* ctrl register */
	"mov	pc, %0\n"
	: : "r" (loc) );
}

void command_reset(int argc, const char **argv)
{
	processor_reset(0);
}

user_command_t reset_cmd = {
	"reset",
	command_reset,
	NULL,
	"reset\t\t\t\t-- Reset the system"
};

static void 
display_cpu_help(void)
{
	printk("Usage:\n");
	printk("cpu info\t\t\t-- Display cpu informatin\n");
	printk("cpu set <clock> <ratio>\t\t-- Change cpu clock and bus clock\n");
}

static void command_cpu(int argc, const char **argv)
{
	switch (argc) {
	case 1:
		goto print_usage;
		break;
	case 2:
		if (strncmp("help", argv[1], 4) == 0) {
			display_cpu_help();
			break;
		}
		if (strncmp("info", argv[1], 4) == 0) {
			print_cpu_info();
			break;
		}
		goto print_usage;
	case 4:
		if (strncmp("set", argv[1], 3) == 0) {
			unsigned long clock = strtoul(argv[2], NULL, 0, NULL);
			unsigned long ratio = strtoul(argv[3], NULL, 0, NULL);
			int ret = 0;

			ret = change_sys_clks(clock, ratio);
			if (ret)
				printk("Failed. Can not change cpu clock\n");
			else
				printk("OK\n");
			break;
		}
	default:
		goto print_usage;
		break;
	}

	return;

print_usage:
	display_cpu_help();
}

user_command_t cpu_cmd = {
	"cpu",
	command_cpu,
	NULL,
	"cpu [{cmds}] \t\t\t-- Manage cpu clocks"
};
