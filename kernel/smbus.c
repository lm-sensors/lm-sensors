/*
    smbus.c - Part of lm_sensors, Linux kernel modules for hardware
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/stddef.h>
#include <asm/system.h>

#include "compat.h"

#include "i2c.h"
#ifdef I2C_SPINLOCK
#include <asm/spinlock.h>
#else
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,0,19)
#include <linux/sched.h>
#else
#include <asm/semaphore.h>
#endif
#endif

#include "version.h"
#include "smbus.h"

static s32 smbus_access_i2c (struct i2c_adapter * adapter, u8 addr,
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
s32 smbus_access (struct i2c_adapter * adapter, u8 addr, char read_write,
                  u8 command, int size, union smbus_data * data)
{
  int res;
  if ((adapter->id & ALGO_MASK) == ALGO_SMBUS) {
#ifdef I2C_SPINLOCK
    spin_lock_irqsave(&adapter->lock,adapter->lockflags);
#else
    down(&adapter->lock);
#endif
    res = ((struct smbus_adapter *) adapter) -> 
           smbus_access(addr,read_write,command,size,data);
#ifdef I2C_SPINLOCK
    spin_unlock_irqrestore(&adapter->lock,adapter->lockflags);
#else
    up(&adapter->lock);
#endif
  } else 
    res = smbus_access_i2c(adapter,addr,read_write,command,size,data);
  return res;
}
  
/* Simulate a SMBus command using the i2c protocol 
   No checking of paramters is done!  */
s32 smbus_access_i2c(struct i2c_adapter * adapter, u8 addr, char read_write,
                     u8 command, int size, union smbus_data * data)
{
  /* So we need to generate a series of msgs. In the case of writing, we
     need to use only one message; when reading, we need two. We initialize
     most things with sane defaults, to keep the code below somewhat
     simpler. */
  unsigned char msgbuf0[33];
  unsigned char msgbuf1[33];
  int num = read_write == SMBUS_READ?2:1;
  struct i2c_msg msg[2] = { { addr, 0, 1, msgbuf0 }, 
                            { addr, I2C_M_RD, 0, msgbuf1 }
                          };
  int i;

  msgbuf0[0] = command;
  switch(size) {
  case SMBUS_QUICK:
    msg[0].len = 0;
    num = 1; /* Special case: The read/write field is used as data */
    break;
  case SMBUS_BYTE:
    if (read_write == SMBUS_READ) {
      /* Special case: only a read! */
      msg[0].flags = I2C_M_RD;
      num = 1;
    }
    break;
  case SMBUS_BYTE_DATA:
    if (read_write == SMBUS_READ)
      msg[1].len = 1;
    else {
      msg[0].len = 2;
      msgbuf0[1] = data->byte;
    }
    break;
  case SMBUS_WORD_DATA:
    if (read_write == SMBUS_READ)
      msg[1].len = 2;
    else {
      msg[0].len=3;
      msgbuf0[1] = data->word & 0xff;
      msgbuf0[2] = (data->word >> 8) & 0xff;
    }
    break;
  case SMBUS_PROC_CALL:
    num = 2; /* Special case */
    msg[0].len = 3;
    msg[1].len = 2;
    msgbuf0[1] = data->word & 0xff;
    msgbuf0[2] = (data->word >> 8) & 0xff;
    break;
  case SMBUS_BLOCK_DATA:
    if (read_write == SMBUS_READ) {
      printk("smbus.o: Block read not supported under I2C emulation!\n");
      return -1;
    } else {
      msg[1].len = data->block[0] + 1;
      if (msg[1].len > 32) {
        printk("smbus.o: smbus_access called with invalid block write "
               "size (%d)\n",msg[1].len);
        return -1;
      }
      for (i = 1; i <= msg[1].len; i++)
        msgbuf0[i] = data->block[i];
    }
    break;
  default:
    printk("smbus.o: smbus_access called with invalid size (%d)\n",size);
    return -1;
  }
    
  if (i2c_transfer(adapter, msg, num) < 0)
    return -1;

  if(read_write == SMBUS_READ)
    switch(size) {
    case SMBUS_BYTE:
      data->byte = msgbuf0[0];
      break;
    case SMBUS_BYTE_DATA:
      data->byte = msgbuf1[0];
      break;
    case SMBUS_WORD_DATA: 
    case SMBUS_PROC_CALL:
      data->word = msgbuf1[0] | (msgbuf1[1] << 8);
      break;
  }
  return 0;
}

/* Algorithm master_xfer call-back implementation. Can't do that... */
int smbus_master_xfer (struct smbus_adapter *adap, struct i2c_msg msgs[], 
                       int num)
{
#ifdef DEBUG
  printk("smbus.o: smbus_master_xfer called for adapter `%s' "
         "(no i2c level access possible!)\n",
         adap->name);
#endif
  return -1;
}

/* Algorithm slave_send call-back implementation. Can't do that... */
int smbus_slave_send (struct smbus_adapter *adap, char *data, int len)
{
#ifdef DEBUG
  printk("smbus.o: smbus_slave_send called for adapter `%s' "
         "(no i2c level access possible!)\n",
         adap->name);
#endif
  return -1;
}

/* Algorithm slave_recv call-back implementation. Can't do that... */
int smbus_slave_recv (struct smbus_adapter *adap, char *data, int len)
{
#ifdef DEBUG
  printk("smbus.o: smbus_slave_recv called for adapter `%s' "
         "(no i2c level access possible!)\n",
         adap->name);
#endif
  return -1;
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

int smbus_init(void)
{
  int res;
  printk("smbus.o version %s (%s)\n",LM_VERSION,LM_DATE);
  if ((res = smbus_add_algorithm(&smbus_algorithm)))
    printk("smbus.o: Algorithm registration failed, module not inserted.\n");
  else
    printk("smbus.o initialized\n");
  return res;
}

int smbus_cleanup(void)
{
  int res;
  if ((res = smbus_del_algorithm(&smbus_algorithm)))
    printk("smbus.o: Algorithm deregistration failed, module not removed\n");
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
