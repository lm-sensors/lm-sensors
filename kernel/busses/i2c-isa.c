/*
    i2c-isa.c - Part of lm_sensors, Linux kernel modules for hardware
            monitoring
    Copyright (c) 1998, 1999  Frodo Looijaard <frodol@dds.nl> 

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

/* This implements an i2c algorithm/adapter for ISA bus. Not that this is
   on first sight very useful; almost no functionality is preserved.
   Except that it makes writing drivers for chips which can be on both
   the SMBus and the ISA bus very much easier. See lm78.c for an example
   of this. */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/i2c.h>

#include "compat.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,53)
#include <linux/init.h>
#else
#define __init
#define __initdata
#endif

#include "version.h"
#include "i2c-isa.h"

static void isa_inc_use (struct i2c_adapter *adapter);
static void isa_dec_use (struct i2c_adapter *adapter);
static u32 isa_func(struct i2c_adapter *adapter);

#ifdef MODULE
static
#else
extern
#endif
       int __init i2c_isa_init(void);
static int __init isa_cleanup(void);

#ifdef MODULE
extern int init_module(void);
extern int cleanup_module(void);
#endif /* MODULE */

/* This is the actual algorithm we define */
static struct i2c_algorithm isa_algorithm = {
  /* name */            "ISA bus algorithm",
  /* id */              I2C_ALGO_ISA,
  /* master_xfer */     NULL,
  /* smbus_access */    NULL,
  /* slave_send */      NULL,
  /* slave_rcv */       NULL,
  /* algo_control */    NULL,
  /* functionality */   &isa_func,
};

/* There can only be one... */
static struct i2c_adapter isa_adapter = {
  /* name */            "ISA main adapter",
  /* id */		I2C_ALGO_ISA | I2C_HW_ISA,
  /* algorithm */       &isa_algorithm,
  /* algo_data */       NULL,
  /* inc_use */         &isa_inc_use,
  /* dec_use */         &isa_dec_use,
  /* data */            NULL,
  /* Other fields not initialized */
};

/* Used in isa_init/cleanup */
static int __initdata isa_initialized;

void isa_inc_use (struct i2c_adapter *adapter)
{
#ifdef MODULE
  MOD_INC_USE_COUNT;
#endif
}

void isa_dec_use (struct i2c_adapter *adapter)
{
#ifdef MODULE
  MOD_DEC_USE_COUNT;
#endif
}

/* We can't do a thing... */
static u32 isa_func(struct i2c_adapter *adapter)
{
  return 0;
}

int __init i2c_isa_init(void)
{
  int res;
  printk("i2c-isa.o version %s (%s)\n",LM_VERSION,LM_DATE);
#ifdef DEBUG
  if (isa_initialized) {
    printk("i2c-isa.o: Oops, isa_init called a second time!\n");
    return -EBUSY;
  }
#endif
  isa_initialized = 0;
  if ((res = i2c_add_adapter(&isa_adapter))) {
    printk("i2c-isa.o: Adapter registration failed, "
           "module i2c-isa.o is not inserted\n.");
    isa_cleanup();
    return res;
  }
  isa_initialized++;
  printk("i2c-isa.o: ISA bus access for i2c modules initialized.\n");
  return 0;
}

int __init isa_cleanup(void)
{
  int res;
  if (isa_initialized >= 1)
  {
    if ((res = i2c_del_adapter(&isa_adapter))) {
      printk("i2c-isa.o: Adapter deregistration failed, module not removed.\n");
      return res;
    } else
      isa_initialized--;
  }
  return 0;
}

EXPORT_NO_SYMBOLS;

#ifdef MODULE

MODULE_AUTHOR("Frodo Looijaard <frodol@dds.nl>");
MODULE_DESCRIPTION("ISA bus access through i2c");

int init_module(void)
{
  return i2c_isa_init();
}

int cleanup_module(void)
{
  return isa_cleanup();
}

#endif /* MODULE */

