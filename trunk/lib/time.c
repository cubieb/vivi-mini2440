/*
 * vivi/lib/time.c: Simple timers
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
 * $Id: time.c,v 1.1.1.1 2004/02/04 06:22:25 laputa Exp $
 *
 *
 * History
 * 
 * 2002-08-05: Janghoon Lyu <nandy@mizi.com>
 *
 */

#include "config.h"
#include "machine.h"
#include "processor.h"
#include "printk.h"
#include "time.h"
#include "vivi_string.h"
#include <string.h>

#ifdef CONFIG_CMD_SLEEP
#include "command.h"
#endif

//added by ayang 2008-10-28
unsigned long timestamp = 0;
unsigned long lastdec = 0;
unsigned long timer_load_val = 0;

/*
 * timer without interrupts
 */
inline void init_time()
{
	arch_init_time();
	timestamp = 0;
}

static inline unsigned long read_timer(void)
{
	return arch_read_timer();	
}

void reset_timer_masked (void)
{ 
  /* reset time */
  lastdec = read_timer();
  timestamp = 0;
}

static unsigned long get_timer_masked (void)
{
  unsigned long now = read_timer();

  if (lastdec >= now) {
    /* normal mode */
    timestamp += lastdec - now;
  } else {
    /* we have an overflow ... */
    timestamp += lastdec + timer_load_val - now;
  }
  lastdec = now;
  
  return timestamp;
}

void reset_timer (void)
{
  reset_timer_masked ();
}

inline unsigned long get_timer (unsigned long base)
{
  return get_timer_masked () - base;
}

void set_timer (unsigned long t)
{
  timestamp = t;
}

void udelay (unsigned long usec)
{
  unsigned long tmo;
  unsigned long start = get_timer(0);

  tmo = usec;
  if(usec >= 1000)
      tmo = (tmo / 1000) + 1;
  tmo *= (timer_load_val * 100);
  tmo /= 1000;
  if(usec < 1000) 
    tmo /= 1000;

  while ((unsigned long)(get_timer_masked () - start) < tmo)
    /*NOP*/;
}
	  
inline void mdelay(unsigned long msec)
{
	udelay(msec * 1000);	
}

#ifdef CONFIG_CMD_SLEEP
void command_sleep(int argc, const char **argv)
{
	int len;
	unsigned int sec;
	char buf[10];
	char unit;

	if (argc != 2) {
		printk("invalid 'sleep' command: too few or many arguments\n");
		printk("Usage:\n");
		printk("  sleep <sec> -- 1m == mili 1u == micro 1s == secon\n");
		return;
	}

	len = strlen(argv[1]);
	strncpy(buf, argv[1], len-1);
	buf[len-1] = '\0';
	strncpy(&unit, argv[1]+(len-1), 1);
	sec = (unsigned int)strtoul(buf, NULL, 0, NULL);
	if (strncmp("s", (char *)&unit, 1) == 0) {
		printk("sleep %d seconds\n", sec);
		mdelay(sec * 1000);
	} else if (strncmp("m", (char *)&unit, 1) == 0) {
		printk("sleep %d mili-seconds\n", sec);
		mdelay(sec);
	} else if (strncmp("u", (char *)&unit, 1) == 0) {
		printk("sleep %d micro-seconds\n", sec);
		udelay(sec);
	}
}

user_command_t sleep_cmd = {
	"sleep",
	command_sleep,
	NULL,
	"merong"
};
#endif
