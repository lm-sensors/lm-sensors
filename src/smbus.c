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

#ifdef SPINLOCK
#include <asm/spinlock.h>
#else
#include <asm/semaphore.h>
#endif

#include "version.h"
#include "smbus.h"

static s32 smbus_access_i2c (struct smbus_adapter * adapter, u8 addr,
                             char read_write, u8 command, int size,
                             union smbus_data * data);

static int smbus_master_xfer (struct smbus_adapter *adap,
                              struct i2c_msg msgs[], int num);
static int smbus_slave_send (struct smbus_adapter *adap, char *data, int len);
static int smbus_slave_recv (struct smbus_adapter *adap, char *data, int len);
static int smbus_algo_control (struct smbus_adapter *adap, unsigned int cmd, 
                               unsigned long arg);
static int smbus_client_register (struct smbus_client *client);
static int smbus_client_unregister (struct smbus_client *client);

static int smbus_init(void);
static int smbus_cleanup(void);

#ifdef MODULE
extern int init_module(void);
extern int cleanup_module(void);
#endif /* MODULE */

/* This is the actual algorithm we define */
struct smbus_algorithm smbus_algorithm = {
  /* name */		"Non-I2C SMBus adapter",
  /* id */		ALGO_SMBUS,
  /* master_xfer */	&smbus_master_xfer,
  /* slave_send */	&smbus_slave_send,
  /* slave_rcv */	&smbus_slave_recv,
  /* algo_control */	&smbus_algo_control,
  /* client_register */ &smbus_client_register,
  /* client_unregister*/&smbus_client_unregister
};


/* OK, so you want to access a bus using the SMBus protocols. Well, it either
   is registered as a SMBus-only adapter (like the PIIX4), or we need to
   simulate the SMBus commands using the i2c access routines. 
   We do all locking here, so you can ignore that in the adapter-specific
   smbus_accesss routine. */
s32 smbus_access (struct smbus_adapter * adapter, u8 addr, char read_write,
                  u8 command, int size, union smbus_data * data)
{
  int res;
#ifdef SPINLOCK
  spin_lock_irqsave(&adapter->lock,adapter->lockflags);
#else
  down(&adapter->lock);
#endif
  if (adapter->id & ALGO_SMBUS) 
    res = adapter->smbus_access(addr,read_write,command,size,data);
  else
    res = smbus_access_i2c(adapter,addr,read_write,command,size,data);
#ifdef SPINLOCK
  spin_unlock_irqrestore(&adapter->lock,adapter->lockflags);
#else
  up(&adapter->lock);
#endif
  return res;
}
  
/* Simulate a SMBus command using the i2c protocol 
   No checking of paramters is done!
   For SMBUS_QUICK: Use addr, read_write 
   For SMBUS_BYTE: Use addr, read_write, command 
   ....  */
s32 smbus_access_i2c(struct smbus_adapter * adapter, u8 addr, char read_write,
                     u8 command, int size, union smbus_data * data)
{
  /* So we need to generate a series of msgs */
  struct i2c_msg msg[2];
  char msgbuf0[2];
  char msgbuf1[32];
  msg[0].addr = addr;
  msg[0].flags = read_write;
  msg[0].len = 0;
  msg[0].buf = msgbuf0;
  /* WHATEVER */
}

/* Algorithm master_xfer call-back implementation. Can't do that... */
int smbus_master_xfer (struct smbus_adapter *adap, struct i2c_msg msgs[], 
                       int num)
{
  printk("smbus_master_xfer called for adapter `%s' "
         "(no i2c level access possible!)\n",
         adap->name);
  return 0;
}

/* Algorithm slave_send call-back implementation. Can't do that... */
int smbus_slave_send (struct smbus_adapter *adap, char *data, int len)
{
  printk("smbus_slave_send called for adapter `%s' "
         "(no i2c level access possible!)\n",
         adap->name);
  return 0;
}

/* Algorithm slave_recv call-back implementation. Can't do that... */
int smbus_slave_recv (struct smbus_adapter *adap, char *data, int len)
{
  printk("smbus_slave_recv called for adapter `%s' "
         "(no i2c level access possible!)\n",
         adap->name);
  return 0;
}

/* Here we can put additional calls to modify the workings of the algorithm.
   But right now, there is no need for that. */
int smbus_algo_control (struct smbus_adapter *adap, unsigned int cmd, 
                         unsigned long arg)
{
  return 0;
}

/* Ehm... This is called when a client is registered to an adapter. We could
   do all kinds of neat stuff here like, ehm - returning success? */
int smbus_client_register (struct smbus_client *client)
{
  return 0;
}
  
int smbus_client_unregister (struct smbus_client *client)
{
  return 0;
}

/* Next: define SMBus variants of registering. Very boring. To make it possible
   to change these definitions in the future without recompiling all modules,
   we do not define them as inline. */
int smbus_add_algorithm(struct smbus_algorithm *algorithm)
{
  return i2c_add_algorithm( (struct i2c_algorithm *) algorithm);
}

int smbus_del_algorithm(struct smbus_algorithm *algorithm)
{
  return i2c_del_algorithm( (struct i2c_algorithm *) algorithm);
}

int smbus_add_adapter(struct smbus_adapter *adapter)
{
  return i2c_add_adapter( (struct i2c_adapter *) adapter);
}

int smbus_del_adapter(struct smbus_adapter *adapter)
{
  return i2c_del_adapter( (struct i2c_adapter *) adapter);
}

int smbus_add_driver(struct smbus_driver *driver)
{
  return i2c_add_driver( (struct i2c_driver *) driver);
}

int smbus_del_driver(struct smbus_driver *driver)
{
  return i2c_del_driver( (struct i2c_driver *) driver);
}

int smbus_attach_client(struct smbus_client *client)
{
  return i2c_attach_client( (struct i2c_client *) client);
}

int smbus_detach_client(struct smbus_client *client)
{
  return i2c_detach_client( (struct i2c_client *) client);
}

int smbus_init(void)
{
  int res;
  printk("smbus.o version %s (%s)\n",LM_VERSION,LM_DATE);
  if ((res = smbus_add_algorithm(&smbus_algorithm)))
    printk("Module smbus.o not inserted!\n");
  else
    printk("smbus.o initialized\n");
  return res;
}

int smbus_cleanup(void)
{
  int res;
  if ((res = smbus_del_algorithm(&smbus_algorithm)))
    printk("Module smbus.o could not be removed cleanly!\n");
  return res;
}

/* OK, this will for now _only_ compile as a module, but this is neat for
   later, if we want to compile it straight into the kernel */
#ifdef MODULE

MODULE_AUTHOR("Frodo Looijaard <frodol@dds.nl>");
MODULE_DESCRIPTION("System Management Bus (SMBus) access");

int init_module(void)
{
  return smbus_init();
}

int cleanup_module(void)
{
  return smbus_cleanup();
}

#endif /* MODULE */
