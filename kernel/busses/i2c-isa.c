/*
    i2c-isa.c - Part of lm_sensors, Linux kernel modules for hardware
            monitoring
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

/* This implements an i2c algorithm/adapter for ISA bus. Not that this is
   on first sight very useful; almost no functionality is preserved.
   Except that it makes writing drivers for chips which can be on both
   the SMBus and the ISA bus very much easier. See lm78.c for an example
   of this. */

#include <linux/module.h>
#include <linux/kernel.h>

#include "i2c.h"
#ifdef I2C_SPINLOCK
#include <asm/spinlock.h>
#else
#include <asm/semaphore.h>
#endif

#include "version.h"
#include "i2c-isa.h"

static int isa_master_xfer (struct isa_adapter *adap,
                            struct i2c_msg msgs[], int num);
static int isa_slave_send (struct isa_adapter *adap, char *data, int len);
static int isa_slave_recv (struct isa_adapter *adap, char *data, int len);
static int isa_algo_control (struct isa_adapter *adap, unsigned int cmd,
                             unsigned long arg);
static int isa_client_register (struct isa_client *client);
static int isa_client_unregister (struct isa_client *client);

static int isa_init(void);
static int isa_cleanup(void);

#ifdef MODULE
extern int init_module(void);
extern int cleanup_module(void);
#endif /* MODULE */

/* This is the actual algorithm we define */
static struct isa_algorithm isa_algorithm = {
  /* name */            "ISA bus adapter",
  /* id */              ALGO_ISA,
  /* master_xfer */     &isa_master_xfer,
  /* slave_send */      &isa_slave_send,
  /* slave_rcv */       &isa_slave_recv,
  /* algo_control */    &isa_algo_control,
  /* client_register */ &isa_client_register,
  /* client_unregister*/&isa_client_unregister
};

/* There can only be one... */
static struct isa_adapter isa_adapter;

/* Used in isa_init/cleanup */
static int isa_initialized;

/* Algorithm master_xfer call-back implementation. Can't do that... */
int isa_master_xfer (struct isa_adapter *adap, struct i2c_msg msgs[],
                     int num)
{
#ifdef DEBUG
  printk("isa.o: isa_master_xfer called for adapter `%s' "
         "(no i2c level access possible!)\n",
         adap->name);
#endif
  return -1;
}

/* Algorithm slave_send call-back implementation. Can't do that... */
int isa_slave_send (struct isa_adapter *adap, char *data, int len)
{
#ifdef DEBUG
  printk("isa.o: isa_slave_send called for adapter `%s' "
         "(no i2c level access possible!)\n",
         adap->name);
#endif
  return -1;
}

/* Algorithm slave_recv call-back implementation. Can't do that... */
int isa_slave_recv (struct isa_adapter *adap, char *data, int len)
{
#ifdef DEBUG
  printk("isa.o: isa_slave_recv called for adapter `%s' "
         "(no i2c level access possible!)\n",
         adap->name);
#endif
  return -1;
}

/* Here we can put additional calls to modify the workings of the algorithm.
   But right now, there is no need for that. */
int isa_algo_control (struct isa_adapter *adap, unsigned int cmd,
                       unsigned long arg)
{
  return 0;
}

/* Ehm... This is called when a client is registered to an adapter. We could
   do all kinds of neat stuff here like, ehm - returning success? */
int isa_client_register (struct isa_client *client)
{
  return 0;
}

int isa_client_unregister (struct isa_client *client)
{
  return 0;
}

int isa_init(void)
{
  int res;
  printk("isa.o version %s (%s)\n",LM_VERSION,LM_DATE);
#ifdef DEBUG
  if (isa_initialized) {
    printk("isa.o: Oops, isa_init called a second time!\n");
    return -EBUSY;
  }
#endif
  isa_initialized = 0;
  if ((res = isa_add_algorithm(&isa_algorithm))) {
    printk("isa.o: Algorithm registration failed, module not inserted.\n");
    isa_cleanup();
    return res;
  }
  isa_initialized++;
  strcpy(isa_adapter.name,"ISA main adapter");
  isa_adapter.id = ALGO_ISA | ISA_MAIN;
  isa_adapter.algo = &isa_algorithm;
  if ((res = isa_add_adapter(&isa_adapter))) {
    printk("isa.o: Adapter registration failed, "
           "module isa.o is not inserted\n.");
    isa_cleanup();
    return res;
  }
  isa_initialized++;
  printk("isa.o: ISA bus access for i2c modules initialized.\n");
  return 0;
}

int isa_cleanup(void)
{
  int res;
  if (isa_initialized >= 2)
  {
    if ((res = isa_del_adapter(&isa_adapter))) {
      printk("isa.o: Adapter deregistration failed, module not removed.\n");
      return res;
    } else
      isa_initialized--;
  }
  if (isa_initialized >= 1)
  {
    if ((res = isa_del_algorithm(&isa_algorithm))) {
      printk("isa.o: Algorithm deregistration failed, module not removed.\n");
      return res;
    } else
      isa_initialized--;
  }
  return 0;
}

#ifdef MODULE

MODULE_AUTHOR("Frodo Looijaard <frodol@dds.nl>");
MODULE_DESCRIPTION("ISA bus access through i2c");

int init_module(void)
{
  return isa_init();
}

int cleanup_module(void)
{
  return isa_cleanup();
}

#endif /* MODULE */

