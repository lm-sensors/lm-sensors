/*
    ltc1710.c - Part of lm_sensors, Linux kernel modules for hardware
             monitoring
    Copyright (c) 1999  Frodo Looijaard <frodol@dds.nl> and
    Philip Edelbrock <phil@netroedge.com>

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

/* A few notes about the LTC1710:

* The LTC1710 is a dual programmable switch.  It can be used to turn
  anything on or off anything which consumes less than 300mA of 
  current and up to 5.5V
  
* The LTC1710 is a very, very simple SMBus device with three possible 
   SMBus addresses (0x58,0x59, or 0x5A).  Only SMBus byte writes
   (command writes) are supported.

* Since only writes are supported, READS DON'T WORK!  The device 
  plays dead in the event of a read, so this makes detection a 
  bit tricky.
  
* BTW- I can safely say that this driver has been tested under
  every possible case, so there should be no bugs. :')
  
  --Phil

*/


#include <linux/module.h>
#include <linux/malloc.h>
#include "smbus.h"
#include "sensors.h"
#include "i2c.h"
#include "i2c-isa.h"
#include "version.h"


/* The LTC1710 registers */

/* (No registers.  [Wow! This thing is SIMPLE!] ) */

/* Initial values */
#define LTC1710_INIT 0 /* Both off */

/* Each client has this additional data */
struct ltc1710_data {
         int sysctl_id;

         struct semaphore update_lock;
         char valid;                 /* !=0 if following fields are valid */
         unsigned long last_updated; /* In jiffies */

         u8 status; /* Register values */
};

#ifdef MODULE
extern int init_module(void);
extern int cleanup_module(void);
#endif /* MODULE */

static int ltc1710_init(void);
static int ltc1710_cleanup(void);
static int ltc1710_attach_adapter(struct i2c_adapter *adapter);
static int ltc1710_detach_client(struct i2c_client *client);
static int ltc1710_command(struct i2c_client *client, unsigned int cmd,
                        void *arg);
static void ltc1710_inc_use (struct i2c_client *client);
static void ltc1710_dec_use (struct i2c_client *client);
static void ltc1710_switch1(struct i2c_client *client, int operation, int ctl_name,
                      int *nrels_mag, long *results);
static void ltc1710_switch2(struct i2c_client *client, int operation, int ctl_name,
                      int *nrels_mag, long *results);
static void ltc1710_update_client(struct i2c_client *client);


/* This is the driver that will be inserted */
static struct i2c_driver ltc1710_driver = {
  /* name */            "LTC1710 sensor chip driver",
  /* id */              I2C_DRIVERID_LTC1710,
  /* flags */           DF_NOTIFY,
  /* attach_adapter */  &ltc1710_attach_adapter,
  /* detach_client */   &ltc1710_detach_client,
  /* command */         &ltc1710_command,
  /* inc_use */         &ltc1710_inc_use,
  /* dec_use */         &ltc1710_dec_use
};

/* These files are created for each detected LTC1710. This is just a template;
   though at first sight, you might think we could use a statically
   allocated list, we need some way to get back to the parent - which
   is done through one of the 'extra' fields which are initialized
   when a new copy is allocated. */
static ctl_table ltc1710_dir_table_template[] = {
  { LTC1710_SYSCTL_SWITCH_1, "switch1", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &ltc1710_switch1 },
  { LTC1710_SYSCTL_SWITCH_2, "switch2", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &ltc1710_switch2 },
  { 0 }
};

/* Used by init/cleanup */
static int ltc1710_initialized = 0;

/* I choose here for semi-static LM78 allocation. Complete dynamic
   allocation could also be used; the code needed for this would probably
   take more memory than the datastructure takes now. */
#define MAX_LTC1710_NR 3
static struct i2c_client *ltc1710_list[MAX_LTC1710_NR];


int ltc1710_attach_adapter(struct i2c_adapter *adapter)
{
  int address,err,i;
  struct i2c_client *new_client;
  struct ltc1710_data *data;

  err = 0;
  /* Make sure we aren't probing the ISA bus!! */
  if (i2c_is_isa_adapter(adapter)) return 0;

  /* OK, this is no detection. I know. It will do for now, though.  */

  /* Set err only if a global error would make registering other clients
     impossible too (like out-of-memory). */
  for (address = 0x58; (! err) && (address <= 0x5A); address ++) {

    /* Later on, we will keep a list of registered addresses for each
       adapter, and check whether they are used here */
    
    /* We must do a write to test because a read fails! */
    if (smbus_write_byte(adapter,address,0) < 0)
      continue;

    /* There is no real detection possible, the device is too simple. */

    /* Allocate space for a new client structure */
    if (! (new_client =  kmalloc(sizeof(struct i2c_client) +
                                sizeof(struct ltc1710_data),
                               GFP_KERNEL))) {
      err = -ENOMEM;
      continue;
    }

    /* Find a place in our global list */
    for (i = 0; i < MAX_LTC1710_NR; i++)
      if (! ltc1710_list[i])
         break;
    if (i == MAX_LTC1710_NR) {
      err = -ENOMEM;
      printk("ltc1710.o: No empty slots left, recompile and heighten "
             "MAX_LTC1710_NR!\n");
      goto ERROR1;
    }
    ltc1710_list[i] = new_client;
    
    /* Fill the new client structure with data */
    data = (struct ltc1710_data *) (new_client + 1);
    new_client->data = data;
    new_client->id = i;
    new_client->addr = address;
    new_client->adapter = adapter;
    new_client->driver = &ltc1710_driver;
    strcpy(new_client->name,"LTC1710 chip");
    data->valid = 0;
    data->update_lock = MUTEX;
    
    /* Tell i2c-core a new client has arrived */
    if ((err = i2c_attach_client(new_client)))
      goto ERROR2;
    
    /* Register a new directory entry with module sensors */
    if ((err = sensors_register_entry(new_client,"ltc1710",
                                      ltc1710_dir_table_template)) < 0)
      goto ERROR3;
    data->sysctl_id = err;
    err = 0;
    continue;
/* OK, this is not exactly good programming practice, usually. But it is
   very code-efficient in this case. */

ERROR3:
    i2c_detach_client(new_client);
ERROR2:
    ltc1710_list[i] = NULL;
ERROR1:
    kfree(new_client);
  }
  return err;
}

int ltc1710_detach_client(struct i2c_client *client)
{
  int err,i;
  for (i = 0; i < MAX_LTC1710_NR; i++)
    if (client == ltc1710_list[i])
      break;
  if ((i == MAX_LTC1710_NR)) {
    printk("ltc1710.o: Client to detach not found.\n");
    return -ENOENT;
  }

  sensors_deregister_entry(((struct ltc1710_data *)(client->data))->sysctl_id);

  if ((err = i2c_detach_client(client))) {
    printk("ltc1710.o: Client deregistration failed, client not detached.\n");
    return err;
  }

  ltc1710_list[i] = NULL;
  kfree(client);
  return 0;
}


/* No commands defined yet */
int ltc1710_command(struct i2c_client *client, unsigned int cmd, void *arg)
{
  return 0;
}

/* Nothing here yet */
void ltc1710_inc_use (struct i2c_client *client)
{
#ifdef MODULE
  MOD_INC_USE_COUNT;
#endif
}

/* Nothing here yet */
void ltc1710_dec_use (struct i2c_client *client)
{
#ifdef MODULE
  MOD_DEC_USE_COUNT;
#endif
}


void ltc1710_update_client(struct i2c_client *client)
{
  struct ltc1710_data *data = client->data;

  down(&data->update_lock);

  if ((jiffies - data->last_updated > HZ+HZ/2 ) ||
      (jiffies < data->last_updated) || ! data->valid) {

#ifdef DEBUG
    printk("Starting ltc1710 update\n");
#endif

    /* data->status = smbus_read_byte(client->adapter,client->addr); 
    	Unfortunately, reads always fail!  */
    data->last_updated = jiffies;
    data->valid = 1;
  }

  up(&data->update_lock);
}


void ltc1710_switch1(struct i2c_client *client, int operation, int ctl_name,
               int *nrels_mag, long *results)
{
  struct ltc1710_data *data = client->data;
  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 0;
  else if (operation == SENSORS_PROC_REAL_READ) {
    ltc1710_update_client(client);
    results[0] = data->status & 1;
    *nrels_mag = 1;
  } else if (operation == SENSORS_PROC_REAL_WRITE) {
    if (*nrels_mag >= 1) {
      data->status = (data->status & 2) | results[0];
      smbus_write_byte(client->adapter,client->addr,data->status);
    }
  }
}

void ltc1710_switch2(struct i2c_client *client, int operation, int ctl_name,
               int *nrels_mag, long *results)
{
  struct ltc1710_data *data = client->data;
  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 0;
  else if (operation == SENSORS_PROC_REAL_READ) {
    ltc1710_update_client(client);
    results[0] = (data->status & 2) >> 1;
    *nrels_mag = 1;
  } else if (operation == SENSORS_PROC_REAL_WRITE) {
    if (*nrels_mag >= 1) {
      data->status = (data->status & 1) | (results[0] << 1);
      smbus_write_byte(client->adapter,client->addr,data->status);
    }
  }
}

int ltc1710_init(void)
{
  int res;

  printk("ltc1710.o version %s (%s)\n",LM_VERSION,LM_DATE);
  ltc1710_initialized = 0;
  if ((res = i2c_add_driver(&ltc1710_driver))) {
    printk("ltc1710.o: Driver registration failed, module not inserted.\n");
    ltc1710_cleanup();
    return res;
  }
  ltc1710_initialized ++;
  return 0;
}

int ltc1710_cleanup(void)
{
  int res;

  if (ltc1710_initialized >= 1) {
    if ((res = i2c_del_driver(&ltc1710_driver))) {
      printk("ltc1710.o: Driver deregistration failed, module not removed.\n");
      return res;
    }
    ltc1710_initialized --;
  }

  return 0;
}


#ifdef MODULE

MODULE_AUTHOR("Frodo Looijaard <frodol@dds.nl> and Philip Edelbrock <phil@netroedge.com>");
MODULE_DESCRIPTION("LTC1710 driver");

int init_module(void)
{
  return ltc1710_init();
}

int cleanup_module(void)
{
  return ltc1710_cleanup();
}

#endif /* MODULE */

