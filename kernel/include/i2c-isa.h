/*
    i2c-isa.h - Part of lm_sensors, Linux kernel modules for hardware
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

/* Important note: */                                           /* TBD */
/* Lines like these, with the 'TBD' remark (To Be Deleted) */   /* TBD */
/* WILL BE DELETED when this file is installed. */              /* TBD */
/* This allows us to get rid of the ugly LM_SENSORS define */   /* TBD */


#ifndef SENSORS_SENSOR_H
#define SENSORS_SENSOR_H

#ifdef __KERNEL__

/* This file must interface with Simon Vogl's i2c driver. Version 19981006 is
   OK, earlier versions are not; later versions will probably give problems
   too. 
*/
#include <asm/types.h>

/* SPINLOCK is defined in i2c.h. */
#ifdef SPINLOCK
#include <asm/spinlock.h>
#else
#include <asm/semaphore.h>
#endif

#ifdef LM_SENSORS					/* TBD */
#include "i2c.h"					/* TBD */
#else /* ndef LM_SENSORS */				/* TBD */
#include <linux/i2c.h>
#endif /* def LM_SENSORS */				/* TBD */

/* Note that this driver is *not* built upon smbus.c, but is parallel to it.
   We do not need SMBus facilities if we are on the ISA bus, after all */

/* Declarations, to keep the compiler happy */
struct isa_driver;
struct isa_client;
struct isa_algorithm;
struct isa_adapter;

/* A driver tells us how we should handle a specific kind of chip.
   A specific instance of such a chip is called a client.  
   This structure is essentially the same as i2c_driver. */
struct isa_driver {
  char name[32];
  int id;
  unsigned int flags;
  int (* attach_adapter) (struct isa_adapter *);
  int (* detach_client) (struct isa_client *);
  int (* command) (struct isa_client *, unsigned int cmd, void *arg);
  void (* inc_use) (struct isa_client *);
  void (* dec_use) (struct isa_client *);
};

/* A client is a specifc instance of a chip: for each detected chip, there will
   be a client. Its operation is controlled by a driver.
   This structure is an extension of i2c_client. */
struct isa_client {
  char name[32];
  int id;
  unsigned int flags;
  unsigned char addr;
  struct isa_adapter *adapter;
  struct isa_driver *driver;
  void *data;

  /* Here ended i2c_client */
  unsigned int isa_addr;
};

/* An algorithm describes how a certain class of busses can be accessed.
   A specific instance of sucj a bus is called an adapter.
   This structure is essentially the same as i2c_adapter. */
struct isa_algorithm {
  char name[32];
  unsigned int id;
  int (* master_xfer) (struct isa_adapter *adap, struct i2c_msg msgs[],
                       int num);
  int (* slave_send) (struct isa_adapter *,char *, int);
  int (* slave_recv) (struct isa_adapter *,char *, int);
  int (* algo_control) (struct isa_adapter *, unsigned int, unsigned long);
  int (* client_register) (struct isa_client *);
  int (* client_unregister) (struct isa_client *);
};

/* An adapter is a specifc instance of a bus: for each detected bus, there will
   be an adapter. Its operation is controlled by an algorithm.
   SPINLOCK must be the same as declared in i2c.h.
   This structure is essentially the same as i2c_algorithm. */
struct isa_adapter {
  char name[32];
  unsigned int id;
  struct isa_algorithm *algo;
  void *data;
#ifdef SPINLOCK
  spinlock_t lock;
  unsigned long lockflags;
#else
  struct semaphore lock;
#endif
  unsigned int flags;
  struct isa_client *clients[I2C_CLIENT_MAX];
  int client_count;
  int timeout;
  int retries;
};


/* Detect whether we are on the isa bus. If this returns true, all i2c
  access will fail! */
#define i2c_is_isa_client(clientptr) \
        ((clientptr)->adapter->algo->id == ALGO_ISA)
#define i2c_is_isa_adapter(adapptr) \
        ((adapptr)->algo->id == ALGO_ISA)

/* Next: define ISA variants of registering. */
#define isa_add_algorithm(algoptr) \
        i2c_add_algorithm((struct i2c_algorithm *) (algoptr))
#define isa_del_algorithm(algoptr) \
        i2c_del_algorithm((struct i2c_algorithm *) (algoptr))

#define isa_add_adapter(adapptr) \
        i2c_add_adapter((struct i2c_adapter *) (adapptr))
#define isa_del_adapter(adapptr) \
        i2c_del_adapter((struct i2c_adapter *) (adapptr))

#define isa_add_driver(driverptr) \
        i2c_add_driver((struct i2c_driver *) (driverptr))
#define isa_del_driver(driverptr) \
        i2c_add_driver((struct i2c_driver *) (driverptr))

#define isa_attach_client(clientptr) \
        i2c_attach_client((struct i2c_client *) (clientptr))
#define isa_detach_client(clientptr) \
        i2c_detach_client((struct i2c_client *) (clientptr))

#endif /* def __KERNEL__ */

/* We need to mark ISA algorithms in the algorithm structure. */
#define ALGO_ISA 0x50000

/* ISA Adapter ids */
#define ISA_MAIN 1

#endif /* ndef SENSORS_ISA_H */
