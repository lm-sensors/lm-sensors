/*
    smbus.h - Part of lm_sensors, Linux kernel modules for hardware
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

/* Important note: */                                           /* TBD */
/* Lines like these, with the 'TBD' remark (To Be Deleted) */   /* TBD */
/* WILL BE DELETED when this file is installed. */              /* TBD */
/* This allows us to get rid of the ugly LM_SENSORS define */   /* TBD */

#ifndef SENSORS_SMBUS_H
#define SENSORS_SMBUS_H

/* This file must interface with Simon Vogl's i2c driver. Version 19981006 is
   OK, earlier versions are not; later versions will probably give problems
   too. 
*/
#include <asm/types.h>

/* This union is used within smbus_access routines */
union smbus_data { 
        __u8 byte;
        __u16 word;
        __u8 block[32];
};

/* smbus_access read or write markers */
#define SMBUS_READ      1
#define SMBUS_WRITE     0

/* SMBus transaction types (size parameter in the above functions) 
   Note: these no longer correspond to the (arbitrary) PIIX4 internal codes! */
#define SMBUS_QUICK      0
#define SMBUS_BYTE       1
#define SMBUS_BYTE_DATA  2 
#define SMBUS_WORD_DATA  3
#define SMBUS_PROC_CALL  4
#define SMBUS_BLOCK_DATA 5

#ifdef __KERNEL__

/* I2C_SPINLOCK is defined in i2c.h. */
#ifdef I2C_SPINLOCK
#include <asm/spinlock.h>
#else
#include <asm/semaphore.h>
#endif

#ifdef LM_SENSORS					/* TBD */
#include "i2c.h"					/* TBD */
#else							/* TBD */
#include <linux/i2c.h>
#endif							/* TBD */

/* Declarations, to keep the compiler happy */
struct smbus_driver;
struct smbus_client;
struct smbus_algorithm;
struct smbus_adapter;
union smbus_data;

/* A driver tells us how we should handle a specific kind of chip.
   A specific instance of such a chip is called a client.  
   This structure is essentially the same as i2c_driver. */
struct smbus_driver {
  char name[32];
  int id;
  unsigned int flags;
  int (* attach_adapter) (struct smbus_adapter *);
  int (* detach_client) (struct smbus_client *);
  int (* command) (struct smbus_client *, unsigned int cmd, void *arg);
  void (* inc_use) (struct smbus_client *);
  void (* dec_use) (struct smbus_client *);
};

/* A client is a specifc instance of a chip: for each detected chip, there will
   be a client. Its operation is controlled by a driver.
   This structure is essentially the same as i2c_client. */
struct smbus_client {
  char name[32];
  int id;
  unsigned int flags;
  unsigned char addr;
  struct smbus_adapter *adapter;
  struct smbus_driver *driver;
  void *data;
};

/* An algorithm describes how a certain class of busses can be accessed. 
   A specific instance of sucj a bus is called an adapter. 
   This structure is essentially the same as i2c_adapter. */
struct smbus_algorithm {
  char name[32];
  unsigned int id;
  int (* master_xfer) (struct smbus_adapter *adap, struct i2c_msg msgs[],
                       int num);
  int (* slave_send) (struct smbus_adapter *,char *, int);
  int (* slave_recv) (struct smbus_adapter *,char *, int);
  int (* algo_control) (struct smbus_adapter *, unsigned int, unsigned long);
  int (* client_register) (struct smbus_client *);
  int (* client_unregister) (struct smbus_client *);
};

/* An adapter is a specifc instance of a bus: for each detected bus, there will
   be an adapter. Its operation is controlled by an algorithm. 
   I2C_SPINLOCK must be the same as declared in i2c.h.
   This structure is an extension of i2c_algorithm. */
struct smbus_adapter {
  char name[32];
  unsigned int id;
  struct smbus_algorithm *algo;
  void *data;
#ifdef I2C_SPINLOCK
  spinlock_t lock;
  unsigned long lockflags;
#else
  struct semaphore lock;
#endif
  unsigned int flags;
  struct smbus_client *clients[I2C_CLIENT_MAX];
  int client_count;
  int timeout;
  int retries;

  /* Here ended i2c_adapter */
  s32 (* smbus_access) (u8 addr, char read_write,
                        u8 command, int size, union smbus_data * data);
};

/* We need to mark SMBus algorithms in the algorithm structure. 
   Note that any and all adapters using a non-i2c driver use in this
   setup ALGO_SMBUS. Adapters define their own smbus access routine. 
   This also means that adapter->smbus_access is only available if
   this flag is set! */
#define ALGO_SMBUS 0x40000

/* SMBus Adapter ids */
#define SMBUS_PIIX4 1

/* Detect whether we are on an SMBus-only bus. Note that if this returns
   false, you can still use the smbus access routines, as these emulate
   the SMBus on I2C. Unless they are undefined on your algorithm, of
   course. */
#define i2c_is_smbus_client(clientptr) \
        ((clientptr)->adapter->algo->id == ALGO_SMBUS)
#define i2c_is_smbus_adapter(adapptr) \
        ((adapptr)->algo->id == ALGO_SMBUS)


/* Declare an algorithm structure. All SMBus derived adapters should use this
   algorithm! */
extern struct smbus_algorithm smbus_algorithm;

/* This is the very generalized SMBus access routine. You probably do not
   want to use this, though; one of the functions below may be much easier,
   and probably just as fast. 
   Note that we use i2c_adapter here, because you do not need a specific
   smbus adapter to call this function. */
extern s32 smbus_access (struct i2c_adapter * adapter, u8 addr, 
                         char read_write, u8 command, int size,
                         union smbus_data * data);

/* Now follow the 'nice' access routines. These also document the calling
   conventions of smbus_access. */

extern inline s32 smbus_write_quick(struct i2c_adapter * adapter, u8 addr, 
                                    u8 value)
{
  return smbus_access(adapter,addr,value,0,SMBUS_QUICK,NULL);
}

extern inline s32 smbus_read_byte(struct i2c_adapter * adapter,u8 addr)
{
  union smbus_data data;
  if (smbus_access(adapter,addr,SMBUS_READ,0,SMBUS_BYTE,&data))
    return -1;
  else
    return 0x0FF & data.byte;
}

extern inline s32 smbus_write_byte(struct i2c_adapter * adapter, u8 addr, 
                                   u8 value)
{
  return smbus_access(adapter,addr,SMBUS_WRITE,value, SMBUS_BYTE,NULL);
}

extern inline s32 smbus_read_byte_data(struct i2c_adapter * adapter,
                                       u8 addr, u8 command)
{
  union smbus_data data;
  if (smbus_access(adapter,addr,SMBUS_READ,command,SMBUS_BYTE_DATA,&data))
    return -1;
  else
    return 0x0FF & data.byte;
}

extern inline s32 smbus_write_byte_data(struct i2c_adapter * adapter,
                                        u8 addr, u8 command, u8 value)
{
  union smbus_data data;
  data.byte = value;
  return smbus_access(adapter,addr,SMBUS_WRITE,command,SMBUS_BYTE_DATA,&data);
}

extern inline s32 smbus_read_word_data(struct i2c_adapter * adapter,
                                       u8 addr, u8 command)
{
  union smbus_data data;
  if (smbus_access(adapter,addr,SMBUS_READ,command,SMBUS_WORD_DATA,&data))
    return -1;
  else
    return 0x0FFFF & data.word;
}

extern inline s32 smbus_write_word_data(struct i2c_adapter * adapter,
                                        u8 addr, u8 command, u16 value)
{
  union smbus_data data;
  data.word = value;
  return smbus_access(adapter,addr,SMBUS_WRITE,command,SMBUS_WORD_DATA,&data);
}

extern inline s32 smbus_process_call(struct i2c_adapter * adapter,
                                     u8 addr, u8 command, u16 value)
{
  union smbus_data data;
  data.word = value;
  if (smbus_access(adapter,addr,SMBUS_WRITE,command,SMBUS_PROC_CALL,&data))
    return -1;
  else
    return 0x0FFFF & data.word;
}

/* Returns the number of read bytes */
extern inline s32 smbus_read_block_data(struct i2c_adapter * adapter,
                                        u8 addr, u8 command, u8 *values)
{
  union smbus_data data;
  int i;
  if (smbus_access(adapter,addr,SMBUS_READ,command,SMBUS_BLOCK_DATA,&data))
    return -1;
  else {
    for (i = 1; i <= data.block[0]; i++)
      values[i-1] = data.block[i];
    return data.block[0];
  }
}

extern inline s32 smbus_write_block_data(struct i2c_adapter * adapter,
                                         u8 addr, u8 command, u8 length,
                                         u8 *values)
{
  union smbus_data data;
  int i;
  if (length > 32)
    length = 32;
  for (i = 1; i <= length; i++)
    data.block[i] = values[i-1];
  data.block[0] = length;
  return smbus_access(adapter,addr,SMBUS_WRITE,command,SMBUS_BLOCK_DATA,&data);
}


/* Next: define SMBus variants of registering. */

#define smbus_add_algorithm(algoptr) \
	i2c_add_algorithm((struct i2c_algorithm *) (algoptr))
#define smbus_del_algorithm(algoptr) \
	i2c_del_algorithm((struct i2c_algorithm *) (algoptr))

#define smbus_add_adapter(adapptr) \
	i2c_add_adapter((struct i2c_adapter *) (adapptr))
#define smbus_del_adapter(adapptr) \
	i2c_del_adapter((struct i2c_adapter *) (adapptr))

#define smbus_add_driver(driverptr) \
	i2c_add_driver((struct i2c_driver *) (driverptr))
#define smbus_del_driver(driverptr) \
	i2c_add_driver((struct i2c_driver *) (driverptr))

#define smbus_attach_client(clientptr) \
	i2c_attach_client((struct i2c_client *) (clientptr))
#define smbus_detach_client(clientptr) \
	i2c_detach_client((struct i2c_client *) (clientptr))


#endif /* def __KERNEL__ */

#endif /* ndef SENSORS_SMBUS_H */

