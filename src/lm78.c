/*
    lm78.c - A Linux module for reading sensor data.
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
#include <linux/malloc.h>
#include <linux/proc_fs.h>
#include <linux/ioport.h>
#include <asm/errno.h>
#include <asm/io.h>
#include <linux/types.h>
#include "lm78.h"
#include "smbus.h"
#include "version.h"
#include "isa.h"
#include "sensors.h"


#ifdef MODULE
extern int init_module(void);
extern int cleanup_module(void);
#endif /* MODULE */

/* There are some complications in a module like this. First off, LM78 chips
   may be both present on the SMBus and the ISA bus, and we have to handle
   those cases separately at some places. Second, there might be several
   LM78 chips available (well, actually, that is probably never done; but
   it is a clean illustration of how to handle a case like that). Finally,
   a specific chip may be attached to *both* ISA and SMBus, and we would
   not like to detect it double. Fortunately, in the case of the LM78 at
   least, a register tells us what SMBus address we are on, so that helps
   a bit - except if there could be more than one SMBus. Groan. No solution
   for this yet. */

/* This module may seem overly long and complicated. In fact, it is not so
   bad. Quite a lot of bookkeeping is done. A real driver can often cut
   some corners. */

static int lm78_init(void);
static int lm78_cleanup(void);

static int lm78_attach_adapter(struct i2c_adapter *adapter);
static int lm78_detect_isa(struct isa_adapter *adapter);
static int lm78_detect_smbus(struct i2c_adapter *adapter);
static int lm78_detach_client(struct i2c_client *client);
static int lm78_detach_isa(struct isa_client *client);
static int lm78_detach_smbus(struct i2c_client *client);
static int lm78_new_client(struct i2c_adapter *adapter,
                           struct i2c_client *new_client);
static void lm78_init_client(struct i2c_client *client);
static void lm78_remove_client(struct i2c_client *client);
static int lm78_command(struct i2c_client *client, unsigned int cmd, 
                        void *arg);
static void lm78_inc_use (struct i2c_client *client);
static void lm78_dec_use (struct i2c_client *client);
static int lm78_read_value(struct i2c_client *client, u8 register);
static int lm78_write_value(struct i2c_client *client, u8 register, u8 value);

/* I choose here for semi-static LM78 allocation. Complete dynamic
   allocation could also be used; the code needed for this would probably
   take more memory than the datastructures take now */
#define MAX_LM78_NR 4
static struct i2c_client *lm78_list[MAX_LM78_NR];
static struct semaphore lm78_semaphores[MAX_LM78_NR];

/* The driver. I choose to use type i2c_driver, as at is identical to both
   smbus_driver and isa_driver, and clients could be of either kind */
static struct i2c_driver lm78_driver = {
  /* name */		"LM78 sensor chip driver",
  /* id */		I2C_DRIVERID_LM78,
  /* flags */		DF_NOTIFY,
  /* attach_adapter */  &lm78_attach_adapter,
  /* detach_client */	&lm78_detach_client,
  /* command */		&lm78_command,
  /* inc_use */		&lm78_inc_use,
  /* dec_use */		&lm78_dec_use
};

/* Used by lm78_init/cleanup */
static int lm78_initialized = 0;

/* This function is called when:
     * lm78_driver is inserted, for each available adapter
     * when a new adapter is inserted (and lm78_driver is still present) */
int lm78_attach_adapter(struct i2c_adapter *adapter)
{
  if (i2c_is_isa_adapter(adapter))
    return lm78_detect_isa((struct isa_adapter *) adapter);
  else
    return lm78_detect_smbus(adapter);
}

/* This function is called whenever a client should be removed */
int lm78_detach_client(struct i2c_client *client)
{
  if (i2c_is_isa_client(client))
    return lm78_detach_isa((struct isa_client *) client);
  else
    return lm78_detach_smbus(client);
}

/* Detect whether there is a LM78 on the ISA bus, register and initialize 
   it. */
int lm78_detect_isa(struct isa_adapter *adapter)
{
  int address,err;
  struct isa_client *new_client;

  /* OK, this is no detection. I know. It will do for now, though.  */

  err = 0;
  for (address = 0x290; (! err) && (address <= 0x290); address += 0x08) {
    if (check_region(address, LM78_EXTENT))
      continue;
    
    if (inb_p(address + LM78_ADDR_REG_OFFSET) == 0xff) {
      outb_p(0x00,address + LM78_ADDR_REG_OFFSET);
      if (inb_p(address + LM78_ADDR_REG_OFFSET) == 0xff)
        continue;
    }
    
    /* Real detection code goes here */
   
    request_region(address, LM78_EXTENT, "lm78");

    if (! (new_client = kmalloc(sizeof(struct isa_client), GFP_KERNEL)))
    {
      release_region(address,LM78_EXTENT);
      err=-ENOMEM;
      continue;
    }
    new_client->addr = 0;
    new_client->isa_addr = address;
    if ((err = lm78_new_client((struct i2c_adapter *) adapter,
                               (struct i2c_client *) new_client))) {
      release_region(address, LM78_EXTENT);
      kfree(new_client);
      continue;
    } 
    if ((err = isa_attach_client(new_client))) {
      release_region(address, LM78_EXTENT);
      lm78_remove_client((struct i2c_client *) new_client);
      kfree(new_client);
      continue;
    }
    lm78_init_client((struct i2c_client *) new_client);
  }
  return err;
}

/* Deregister and remove a LM78 client */
int lm78_detach_isa(struct isa_client *client)
{
  int err;
  if ((err = isa_detach_client(client))) {
    printk("lm78.o: Client deregistration failed, client not detached.\n");
    return err;
  }
  release_region(client->isa_addr,LM78_EXTENT);
  lm78_remove_client((struct i2c_client *) client);
  kfree(client);
  return 0;
}

int lm78_detect_smbus(struct i2c_adapter *adapter)
{
  int address,err;
  struct i2c_client *new_client;

  /* OK, this is no detection. I know. It will do for now, though.  */
  err = 0;
  for (address = 0x20; (! err) && (address <= 0x2f); address ++) {

    /* Later on, we will keep a list of registered addresses for each
       adapter, and check whether they are used here */

    if (smbus_read_byte_data(adapter,address,1) == 0xff) 
      continue;

    /* Real detection code goes here */

    new_client = kmalloc(sizeof(struct i2c_client), GFP_KERNEL);
    new_client->addr = address;
    if ((err = lm78_new_client(adapter,new_client))) {
      kfree(new_client);
      continue;
    }
    if ((err = i2c_attach_client(new_client))) {
      lm78_remove_client(new_client);
      kfree(new_client);
      continue;
    }
    lm78_init_client(new_client);
  }
  return err;
}

int lm78_detach_smbus(struct i2c_client *client)
{
  int err;
  if ((err = i2c_detach_client(client))) {
    printk("lm78.o: Client deregistration failed, client not detached.\n");
    return err;
  }
  lm78_remove_client(client);
  kfree(client);
  return 0;
}


/* Find a free slot, and initialize most of the fields */
int lm78_new_client(struct i2c_adapter *adapter,
                    struct i2c_client *new_client)
{
  int i;

  /* First, seek out an empty slot */
  for(i = 0; i < MAX_LM78_NR; i++)
    if (! lm78_list[i])
      break;
  if (i == MAX_LM78_NR) {
    printk("lm78.o: No empty slots left, recompile and heighten "
           "MAX_LM78_NR!\n");
    return -ENOMEM;
  }
  
  lm78_list[i] = new_client;
  lm78_semaphores[i] = MUTEX;
  new_client->data = &lm78_semaphores[i];
  strcpy(new_client->name,"LM78 chip");
  new_client->id = i;
  new_client->adapter = adapter;
  new_client->driver = &lm78_driver;
  return 0;
}

void lm78_remove_client(struct i2c_client *client)
{
  int i;
  for (i = 0; i < MAX_LM78_NR; i++)
    if (client == lm78_list[i])
      lm78_list[i] = NULL;
}

/* Called when we have found a new LM78. It should set limits, etc. */
void lm78_init_client(struct i2c_client *client)
{
}

/* No commands defined yet */
int lm78_command(struct i2c_client *client, unsigned int cmd, void *arg)
{
  return 0;
}

/* Nothing here yet */
void lm78_inc_use (struct i2c_client *client)
{
}

/* Nothing here yet */
void lm78_dec_use (struct i2c_client *client)
{
}
 

/* The SMBus locks itself, but ISA access must be locked explicitely! 
   We ignore the LM78 BUSY flag at this moment - it could lead to deadlocks,
   would slow down the LM78 access and should not be necessary. 
   There are some ugly typecasts here, but the good new is - they should
   nowhere else be necessary! */
int lm78_read_value(struct i2c_client *client, u8 reg)
{
  int res;
  if (i2c_is_isa_client(client)) {
    down((struct semaphore *) (client->data));
    outb_p(reg,(((struct isa_client *) client)->isa_addr) + 
               LM78_ADDR_REG_OFFSET);
    res = inb_p((((struct isa_client *) client)->isa_addr) + 
                LM78_DATA_REG_OFFSET);
    up((struct semaphore *) (client->data));
    return res;
  } else
    return smbus_read_byte_data(client->adapter,client->addr, reg);
}

/* The SMBus locks itself, but ISA access muse be locked explicitely! 
   We ignore the LM78 BUSY flag at this moment - it could lead to deadlocks,
   would slow down the LM78 access and should not be necessary. 
   There are some ugly typecasts here, but the good new is - they should
   nowhere else be necessary! */
int lm78_write_value(struct i2c_client *client, u8 reg, u8 value)
{
  if (i2c_is_isa_client(client)) {
    down((struct semaphore *) (client->data));
    outb_p(reg,((struct isa_client *) client)->isa_addr + LM78_ADDR_REG_OFFSET);
    outb_p(value,((struct isa_client *) client)->isa_addr + LM78_DATA_REG_OFFSET);
    up((struct semaphore *) (client->data));
    return 0;
  } else
    return smbus_write_byte_data(client->adapter, client->addr, reg,value);
}

/* ANYTHING BELOW IS JUST AN EXAMPLE. IGNORE IF YOU WANT TO BASE A DRIVER
   ON THE CODE IN THIS FILE */

/* Stupid entry in /proc */
static int proc_function(char *buf, char **start, off_t offset, int len,
                         int unused)
{
  int i;
  len = 0;
  for (i = 0; i < MAX_LM78_NR; i++)
    if(lm78_list[i]) {
      if(i2c_is_isa_client(lm78_list[i])) 
        len += sprintf(buf+len,"(isa) %d: address=%x\n",i,
                       ((struct isa_client *) (lm78_list[i]))->isa_addr);
      else
        len += sprintf(buf+len,"(i2c) %d: address=%x\n",i,
                       lm78_list[i]->addr);
    }
  return len;
}

/* OK, this is a test entry. Just ignore, it is not important. */
static struct proc_dir_entry proc_entry =
	{
	  0,12,"sensors-test",
	  S_IFREG | S_IRUGO, 1, 0, 0,
	  0, NULL,
	  &proc_function
	};

    
int lm78_init(void)
{
  int res;

  printk("lm78.o version %s (%s)\n",LM_VERSION,LM_DATE);
  lm78_initialized = 0;

  /* OK, we register some stupid proc file here. This is *just* *temporary*,
    for test purposes. Ignore if you want. Only works for kernels 2.0.x. */
  if ((res = proc_register_dynamic(&proc_root,&proc_entry))) {
    printk("lm78.o: Couldn't create /proc/sensors-test, "
           "module not inserted.\n");
    lm78_cleanup();
    return res;
  }
  lm78_initialized ++;

  if ((res =i2c_add_driver(&lm78_driver))) {
    printk("lm78.o: Driver registration failed, module not inserted.\n");
    lm78_cleanup();
    return res;
  }
  lm78_initialized ++;
  return 0;
}

int lm78_cleanup(void)
{
  int res;

  if (lm78_initialized >= 2) {
    if ((res = i2c_del_driver(&lm78_driver))) {
      printk("lm78.o: Driver deregistration failed, module not removed.\n");
      return res;
    }
  } else
    lm78_initialized --;

  if (lm78_initialized >= 1) {
    if((res = proc_unregister(&proc_root,proc_entry.low_ino))) {
      printk("lm78.o: Deregistration of /proc/sensors_test failed, "
              "module not removed.\n");
      return res;
    }
  } else
    lm78_initialized --;
  return 0;
}


#ifdef MODULE

MODULE_AUTHOR("Frodo Looijaard <frodol@dds.nl>");
MODULE_DESCRIPTION("LM78 driver");

int init_module(void)
{
  return lm78_init();
}

int cleanup_module(void)
{
  return lm78_cleanup();
}

#endif /* MODULE */

