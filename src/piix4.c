/*
    smbus.c - A Linux module for reading sensor data.
    Copyright (c) 1998  Frodo Looijaard <frodol@dds.nl> 

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include "smbus.h"
#include "version.h"

static int piix4_init(void);
static int piix4_cleanup(void);
static int piix4_setup(void);
static s32 piix4_access(u8 addr, char read_write,
                        u8 command, int size, union smbus_data * data);

#ifdef MODULE
extern int init_module(void);
extern int cleanup_module(void);
#endif /* MODULE */

static struct smbus_adapter piix4_adapter;
static int piix4_initialized;

/* Detect whether a PIIX4 can be found, and initialize it, where necessary. 
   Return -ENODEV if not found. */
int piix4_setup(void)
{
  return -ENODEV;
  /* TO BE WRITTEN! */
}

/* Return -1 on error. See smbus.h for more information */
s32 piix4_access(u8 addr, char read_write,
                 u8 command, int size, union smbus_data * data)
{
  /* TO BE WRITTEN! */
  return -1;
}


int piix4_init(void)
{
  int res;
  printk("piix4.o version %s (%s)\n",LM_VERSION,LM_DATE);
#ifdef DEBUG
  if (piix4_initialized) {
    printk("piix4.o: Oops, piix4_init called a second time!\n");
    return -EBUSY;
  }
#endif
  piix4_initialized = 0;
  if ((res = piix4_setup())) {
    printk("piix4.o: PIIX4 not detected, module not inserted.\n");
    piix4_cleanup();
    return res;
  }
  piix4_initialized ++;
  strcpy(piix4_adapter.name,"SMBus PIIX4 adapter");
  piix4_adapter.id = ALGO_SMBUS | SMBUS_PIIX4;
  piix4_adapter.algo = &smbus_algorithm;
  piix4_adapter.smbus_access = &piix4_access;
  if ((res = smbus_add_adapter(&piix4_adapter))) {
    printk("piix4.o: Adapter registration failed, module not inserted.\n");
    piix4_cleanup();
    return res;
  }
  piix4_initialized++;
  printk("piix4.o: PIIX4 bus detected and initialized\n");
  return 0;
}

int piix4_cleanup(void)
{
  int res;
  if (piix4_initialized >= 2)
  {
    if ((res = smbus_del_adapter(&piix4_adapter))) {
      printk("piix4.o: smbus_del_adapter failed, module not removed\n");
      return res;
    } else
      piix4_initialized--;
  }
  if (piix4_initialized >= 1) {
    /* Undo anything piix4_setup did */
    piix4_initialized--;
  }
  return 0;
}

#ifdef MODULE

MODULE_AUTHOR("Frodo Looijaard <frodol@dds.nl>");
MODULE_DESCRIPTION("PIIX4 SMBus driver");

int init_module(void)
{
  return piix4_init();
}

int cleanup_module(void)
{
  return piix4_cleanup();
}

#endif /* MODULE */

