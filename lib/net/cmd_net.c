/*
 * (C) Copyright 2000
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

#include <stddef.h>
#include "machine.h"
#include "priv_data.h"
#include "command.h"
#include "printk.h"
#include "config.h"
#include "net/net.h"
#include "mtd/mtd.h"

user_subcommand_t tftp_cmds[];
user_command_t ping_cmd;
#if 0
static int
netcmd_common (proto_t proto, user_subcommand_t *cmd, int argc, const char *argv[])
{
	int   rcode = 0;
	int   size;

	switch (argc) {

	case 2:	/* only one arg - accept two forms:
		 * just load address, or just file name.
		 * The latter form must be written "filename" here.
		 */
		if (argv[1][0] == '"') {	/* just filename */
			copy_filename (LoadFile, argv[1], sizeof(LoadFile));
		} else {			/* load address	*/
			load_addr = simple_strtoul(argv[1], NULL, 16);
		}
		break;

	case 3:	
		load_addr = simple_strtoul(argv[1], NULL, 16);
		copy_filename (LoadFile, argv[2], sizeof(LoadFile));
		break;

	default: 
		printk ("Usage: %s\n", cmd->helpstr);
		return 1;
	}

	if ((size = NetLoop(proto)) < 0)
		return 1;

       	/* done if no file was loaded (no errors though) */
	if (size == 0)
		return 0;

	return rcode;
}
#endif

static void command_help(int argc, const char **argv)
{
  print_usage("tftp", tftp_cmds);
}

#if 0
/* Main command*/
void command_tftp (int argc, const char **argv)
{
  netcmd_common (TFTP, &tftp_cmds, argc, argv)
}
#endif 

static void command_tftp_ram(int argc, const char **argv)
{
  int size = 0;
  int tmp;

  load_addr = get_param_value("default_load_ram_addr", &tmp);
  if(tmp == -1)
      printk("get parameter 'default_load_addr' failed!\n");

  switch(argc) {

  case 2:
    copy_filename(load_file, argv[1], sizeof(load_file));
    break;
  case 3:
    load_addr = simple_strtoul(argv[1], NULL, 16);
    copy_filename(load_file, argv[2], sizeof(load_file));
    break;
  default:
    command_help(0, NULL);
    return ;    
  }

  //if((size = NetLoop(TFTP)) < 0)
   // printk("network error\n");

    return;
}

static void command_tftp_flash(int argc, const char **argv)
{
  int ret;
  loff_t to = 0;
  size_t size = 0, max_size = 0;
  mtd_partition_t *dst_part = NULL;
  int flag;

#ifdef RAM_BASE
  load_addr = RAM_BASE;
#else
#error "RAM_BASE not defined"
#endif  

  switch(argc) {

  case 3:
    dst_part = get_mtd_partition(argv[1]);
    if(!dst_part)
      {
	printk("Could not found \"%s\" partition\n", argv[1]);
	return;
      }
    to = dst_part->offset;
    max_size = dst_part->size;
    flag = dst_part->flag;
    copy_filename(load_file, argv[2], sizeof(load_file));
    break;
  case 4:
    to = simple_strtoul(argv[1], NULL, 16);
    max_size = (size_t)simple_strtoul(argv[2], NULL, 16);
    flag = 0;
    copy_filename(load_file, argv[3], sizeof(load_file));
    break;
  default:
    command_help(0, NULL);
    return ;    
  }

  if((size = NetLoop(TFTP)) < 0)
    {
      printk("network error\n");
      return;
    }

  /* delay for serial output */
  {int i = 0x100000; while(i > 0) i--;}

  if(size == 0)
    {
      printk("get file via TFTP failed\n");
      return;
    }

  if(size > max_size)
    {
      printk("An image size is too large to write to flash.\n");
      printk("wanted = 0x%08lx, loaded = 0x%08lx\n", max_size, size);
      return;
    }

  ret = write_to_flash(to, size, (char *)load_addr, flag);

  return;
}

user_subcommand_t tftp_cmds[] = {
{
	"help",
	command_help,
	"help"
}, {
	"flash",
	command_tftp_flash,
	"flash <mtd_part_name|addr length> <file> \t-- tftp a file to Flash" 
}, {
	"ram",
	command_tftp_ram,
	"ram [addr] <file> \t\t\t\t\t-- tftp a file to RAM" 
}, {
	NULL,
	NULL,
	NULL
}    
};

void command_tftp (int argc, const char **argv)
{
  if(argc <= 2)
    {
      printk("invalid 'tftp' command: too few arguments\n");
      command_help(0, NULL);
      return;
    }

  execsubcmd(tftp_cmds, argc-1, argv+1);
}

user_command_t tftp_cmd = {
	"tftp",
	command_tftp,
	NULL,
	"tftp [{cmds}] \t\t\t-- Load a file to RAM/Flash via TFTP"
};

void command_ping (int argc, const char **argv)
{
	if (argc != 2)
	  {
	    printk("Usage: %s\n", ping_cmd.helpstr);
	    return;
	  }
	NetPingIP = string_to_ip((char *)argv[1]);
	if (NetPingIP == 0) {
		printk ("Usage: %s\n", ping_cmd.helpstr);
		return;
	}

	/*if (NetLoop(PING) < 0) {
		printk ("ping failed; host %s is not alive\n", argv[1]);
		return;
	}*/

	printk("host %s is alive\n", argv[1]);

	return;
}

user_command_t ping_cmd = {
	"ping",
	command_ping,
	NULL,
	"ping <remote IP>\t\t\t-- Send ICMP ECHO_REQUEST to remote host"
};


