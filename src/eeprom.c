/*
    eeprom.c - A Linux module for reading sensor data.
    Copyright (c) 1998  Frodo Looijaard <frodol@dds.nl> and
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

#include <linux/module.h>
#include <linux/malloc.h>
#include "smbus.h"
#include "sensors.h"
#include "i2c.h"
#include "version.h"

/* Many constants specified below */


/* Conversions */

/* Initial values */

/* Each client has this additional data */
struct eeprom_data {
         int sysctl_id;

         struct semaphore update_lock;
         char valid;                 /* !=0 if following fields are valid */
         unsigned long last_updated; /* In jiffies */

/* These need to change. (PAE) */
         u16 temp,temp_os,temp_hyst; /* Register values */
};

#ifdef MODULE
extern int init_module(void);
extern int cleanup_module(void);
#endif /* MODULE */

static int eeprom_init(void);
static int eeprom_cleanup(void);

static int eeprom_attach_adapter(struct i2c_adapter *adapter);
static int eeprom_detach_client(struct i2c_client *client);
static int eeprom_command(struct i2c_client *client, unsigned int cmd,
                        void *arg);
			
static void eeprom_inc_use (struct i2c_client *client);
static void eeprom_dec_use (struct i2c_client *client);

static u16 swap_bytes(u16 val);

static int eeprom_read_value(struct i2c_client *client, u8 reg);
static int eeprom_write_value(struct i2c_client *client, u8 reg, u16 value);

static void eeprom_data(struct i2c_client *client, int operation, int ctl_name,
                      int *nrels_mag, long *results);
static void eeprom_update_client(struct i2c_client *client);


/* This is the driver that will be inserted */
static struct i2c_driver eeprom_driver = {
  /* name */            "EEPROM READER",
  /* id */              I2C_DRIVERID_EEPROM,
  /* flags */           DF_NOTIFY,
  /* attach_adapter */  &eeprom_attach_adapter,
  /* detach_client */   &eeprom_detach_client,
  /* command */         &eeprom_command,
  /* inc_use */         &eeprom_inc_use,
  /* dec_use */         &eeprom_dec_use
};

/* These files are created for each detected EEPROM. This is just a template;
   though at first sight, you might think we could use a statically
   allocated list, we need some way to get back to the parent - which
   is done through one of the 'extra' fields which are initialized
   when a new copy is allocated. */
static ctl_table eeprom_dir_table_template[] = {
  { EEPROM_SYSCTL, "EEPROM", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &eeprom_data },
  { 0 }
};

/* Used by init/cleanup */
static int eeprom_initialized = 0;

/* I choose here for semi-static LM78 allocation. Complete dynamic
   allocation could also be used; the code needed for this would probably
   take more memory than the datastructure takes now. */
#define MAX_EEPROM_NR 8
static struct i2c_client *eeprom_list[MAX_EEPROM_NR];


int eeprom_attach_adapter(struct i2c_adapter *adapter)
{
  int address,err,i;
  struct i2c_client *new_client;
  struct eeprom_data *data;

  err = 0;

  /* OK, this is no detection. I know. It will do for now, though.  */

  /* Set err only if a global error would make registering other clients
     impossible too (like out-of-memory). */
     
  /* Serial EEPROMs for SMBus use addresses from 0x28 to 0x2f */
  for (address = 0x28; (! err) && (address <= 0x2f); address ++) {

    /* Later on, we will keep a list of registered addresses for each
       adapter, and check whether they are used here */
    
    if (smbus_read_byte_data(adapter,address,0) < 0)
      continue;

    /* Real detection code goes here */

    /* Allocate space for a new client structure */
    if (! (new_client =  kmalloc(sizeof(struct i2c_client) +
                                sizeof(struct eeprom5_data),
                               GFP_KERNEL))) {
      err = -ENOMEM;
      continue;
    }

    /* Find a place in our global list */
    for (i = 0; i < MAX_EEPROM_NR; i++)
      if (! eeprom_list[i])
         break;
    if (i == MAX_EEPROM_NR) {
      err = -ENOMEM;
      printk("eeprom.o: No empty slots left, recompile and heighten "
             "MAX_EEPROM_NR!\n");
      goto ERROR1;
    }
    eeprom_list[i] = new_client;
    
    /* Fill the new client structure with data */
    data = (struct eeprom_data *) (new_client + 1);
    new_client->data = data;
    new_client->id = i;
    new_client->addr = address;
    new_client->adapter = adapter;
    new_client->driver = &eeprom_driver;
    strcpy(new_client->name,"EEPROM chip");
    data->valid = 0;
    data->update_lock = MUTEX;
    
    /* Tell i2c-core a new client has arrived */
    if ((err = i2c_attach_client(new_client)))
      goto ERROR2;
    
    /* Register a new directory entry with module sensors */
    if ((err = sensors_register_entry(new_client,"eeprom",
                                      eeprom_dir_table_template)) < 0)
      goto ERROR3;
    data->sysctl_id = err;
    err = 0;

    /* Initialize the chip -- No init needed for EEPROMs*/

    continue;
/* OK, this is not exactly good programming practice, usually. But it is
   very code-efficient in this case. */

ERROR3:
    i2c_detach_client(new_client);
ERROR2:
    eeprom_list[i] = NULL;
ERROR1:
    kfree(new_client);
  }
  return err;
}

int eeprom_detach_client(struct i2c_client *client)
{
  int err,i;
  for (i = 0; i < MAX_EEPROM_NR; i++)
    if (client == eeprom_list[i])
      break;
  if ((i == MAX_EEPROM_NR)) {
    printk("eeprom.o: Client to detach not found.\n");
    return -ENOENT;
  }

  sensors_deregister_entry(((struct eeprom_data *)(client->data))->sysctl_id);

  if ((err = i2c_detach_client(client))) {
    printk("eeprom.o: Client deregistration failed, client not detached.\n");
    return err;
  }

  eeprom_list[i] = NULL;
  kfree(client);
  return 0;
}


/* No commands defined yet */
int eeprom_command(struct i2c_client *client, unsigned int cmd, void *arg)
{
  return 0;
}

/* Nothing here yet */
void eeprom_inc_use (struct i2c_client *client)
{
}

/* Nothing here yet */
void eeprom_dec_use (struct i2c_client *client)
{
}

u16 swap_bytes(u16 val)
{
  return (val >> 8) | (val << 8);
}

/* All registers are word-sized, except for the configuration register.
   For some reason, we must swap the low and high byte, but only on 
   reading words?!? */
int eeprom_read_value(struct i2c_client *client, u8 reg)
{
  if (reg == EEPROM_REG_CONF)
    return smbus_read_byte_data(client->adapter,client->addr,reg);
  else
    return swap_bytes(smbus_read_word_data(client->adapter,client->addr,reg));
}

/* No swapping needed here! */
int eeprom_write_value(struct i2c_client *client, u8 reg, u16 value)
{
  if (reg == EEPROM_REG_CONF)
    return smbus_write_byte_data(client->adapter,client->addr,reg,value);
  else
    return smbus_write_word_data(client->adapter,client->addr,reg,value);
}

void eeprom_update_client(struct i2c_client *client)
{
  struct eeprom_data *data = client->data;

  down(&data->update_lock);

  if ((jiffies - data->last_updated > HZ+HZ/2 ) ||
      (jiffies < data->last_updated) || ! data->valid) {

#ifdef DEBUG
    printk("Starting eeprom update\n");
#endif

/* Need to read the data here (PAE) */
    data->eeprom = eeprom_read_value(client,EEPROM_REG_TEMP);
    
    data->last_updated = jiffies;
    data->valid = 1;
  }

  up(&data->update_lock);
}

/* ? (PAE) */
void eeprom_data(struct i2c_client *client, int operation, int ctl_name,
               int *nrels_mag, long *results)
{
  struct eeprom_data *data = client->data;
  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 1;
  else if (operation == SENSORS_PROC_REAL_READ) {
    eeprom_update_client(client);
    results[0] = DATA_FROM_REG(data->temp_os);
    /* ... More needs to go here (PAE) */
    *nrels_mag = 3;
  } else if (operation == SENSORS_PROC_REAL_WRITE) {

/* No writes to the EEPROM (yet, anyway) (PAE) */

  }
}

int eeprom_init(void)
{
  int res;

  printk("eeprom.o version %s (%s)\n",LM_VERSION,LM_DATE);
  eeprom_initialized = 0;
  if ((res = i2c_add_driver(&eeprom_driver))) {
    printk("eeprom.o: Driver registration failed, module not inserted.\n");
    eeprom_cleanup();
    return res;
  }
  eeprom_initialized ++;
  return 0;
}

int eeprom_cleanup(void)
{
  int res;

  if (eeprom_initialized >= 1) {
    if ((res = i2c_del_driver(&eeprom_driver))) {
      printk("eeprom.o: Driver deregistration failed, module not removed.\n");
      return res;
    }
  } else
    eeprom_initialized --;

  return 0;
}


#ifdef MODULE

MODULE_AUTHOR("Frodo Looijaard <frodol@dds.nl> and Philip Edelbrock <phil@netroedge.com>");
MODULE_DESCRIPTION("EEPROM driver");

int init_module(void)
{
  return eeprom_init();
}

int cleanup_module(void)
{
  return eeprom_cleanup();
}

#endif /* MODULE */

