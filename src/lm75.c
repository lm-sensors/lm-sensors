/*
    lm75.c - A Linux module for reading sensor data.
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
#include "smbus.h"
#include "sensors.h"
#include "i2c.h"
#include "version.h"

/* Many LM75 constants specified below */

/* The LM75 registers */
#define LM75_REG_TEMP 0x00
#define LM75_REG_CONF 0x01
#define LM75_REG_TEMP_HYST 0x02
#define LM75_REG_TEMP_OS 0x03

/* Conversions */
#define TEMP_FROM_REG(val) (((val) >> 7) * 5)
#define TEMP_TO_REG(val)   (((((val) + 2) / 5) << 7) & 0xff80)

/* Initial values */
#define LM75_INIT_TEMP_OS 50
#define LM75_INIT_TEMP_HYST 60

/* Each client has this additional data */
struct lm75_data {
         int sysctl_id;

         struct semaphore update_lock;
         char valid;                 /* !=0 if following fields are valid */
         unsigned long last_updated; /* In jiffies */

         u16 temp,temp_os,temp_hyst; /* Register values */
};

#ifdef MODULE
extern int init_module(void);
extern int cleanup_module(void);
#endif /* MODULE */

static int lm75_init(void);
static int lm75_cleanup(void);
static int lm75_attach_adapter(struct i2c_adapter *adapter);
static int lm75_detach_client(struct i2c_client *client);
static int lm75_command(struct i2c_client *client, unsigned int cmd,
                        void *arg);
static void lm75_inc_use (struct i2c_client *client);
static void lm75_dec_use (struct i2c_client *client);
static u16 swap_bytes(u16 val);
static int lm75_read_value(struct i2c_client *client, u8 reg);
static int lm75_write_value(struct i2c_client *client, u8 reg, u16 value);
static void lm75_temp(struct i2c_client *client, int operation, int ctl_name,
                      int *nrels_mag, long *results);
static void lm75_update_client(struct i2c_client *client);


/* This is the driver that will be inserted */
static struct i2c_driver lm75_driver = {
  /* name */            "LM75 sensor chip driver",
  /* id */              I2C_DRIVERID_LM75,
  /* flags */           DF_NOTIFY,
  /* attach_adapter */  &lm75_attach_adapter,
  /* detach_client */   &lm75_detach_client,
  /* command */         &lm75_command,
  /* inc_use */         &lm75_inc_use,
  /* dec_use */         &lm75_dec_use
};

/* These files are created for each detected LM75. This is just a template;
   though at first sight, you might think we could use a statically
   allocated list, we need some way to get back to the parent - which
   is done through one of the 'extra' fields which are initialized
   when a new copy is allocated. */
static ctl_table lm75_dir_table_template[] = {
  { LM75_SYSCTL_TEMP, "temp", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &lm75_temp },
  { 0 }
};

/* Used by init/cleanup */
static int lm75_initialized = 0;

/* I choose here for semi-static LM78 allocation. Complete dynamic
   allocation could also be used; the code needed for this would probably
   take more memory than the datastructure takes now. */
#define MAX_LM75_NR 8
static struct i2c_client *lm75_list[MAX_LM75_NR];


int lm75_attach_adapter(struct i2c_adapter *adapter)
{
  int address,err,i;
  struct i2c_client *new_client;
  struct lm75_data *data;

  err = 0;

  /* OK, this is no detection. I know. It will do for now, though.  */

  /* Set err only if a global error would make registering other clients
     impossible too (like out-of-memory). */
  for (address = 0x48; (! err) && (address <= 0x4f); address ++) {

    /* Later on, we will keep a list of registered addresses for each
       adapter, and check whether they are used here */
    
    if (smbus_read_byte_data(adapter,address,LM75_REG_CONF) < 0)
      continue;

    /* Real detection code goes here */

    /* Allocate space for a new client structure */
    if (! (new_client =  kmalloc(sizeof(struct i2c_client) +
                                sizeof(struct lm75_data),
                               GFP_KERNEL))) {
      err = -ENOMEM;
      continue;
    }

    /* Find a place in our global list */
    for (i = 0; i < MAX_LM75_NR; i++)
      if (! lm75_list[i])
         break;
    if (i == MAX_LM75_NR) {
      err = -ENOMEM;
      printk("lm75.o: No empty slots left, recompile and heighten "
             "MAX_LM75_NR!\n");
      goto ERROR1;
    }
    lm75_list[i] = new_client;
    
    /* Fill the new client structure with data */
    data = (struct lm75_data *) (new_client + 1);
    new_client->data = data;
    new_client->id = i;
    new_client->addr = address;
    new_client->adapter = adapter;
    new_client->driver = &lm75_driver;
    strcpy(new_client->name,"LM75 chip");
    data->valid = 0;
    data->update_lock = MUTEX;
    
    /* Tell i2c-core a new client has arrived */
    if ((err = i2c_attach_client(new_client)))
      goto ERROR2;
    
    /* Register a new directory entry with module sensors */
    if ((err = sensors_register_entry(new_client,"lm75",
                                      lm75_dir_table_template)) < 0)
      goto ERROR3;
    data->sysctl_id = err;
    err = 0;

    /* Initialize the LM75 chip */
    lm75_write_value(new_client,LM75_REG_TEMP_OS,LM75_INIT_TEMP_OS);
    lm75_write_value(new_client,LM75_REG_TEMP_HYST,LM75_INIT_TEMP_HYST);
    lm75_write_value(new_client,LM75_REG_CONF,0);

    continue;
/* OK, this is not exactly good programming practice, usually. But it is
   very code-efficient in this case. */

ERROR3:
    i2c_detach_client(new_client);
ERROR2:
    lm75_list[i] = NULL;
ERROR1:
    kfree(new_client);
  }
  return err;
}

int lm75_detach_client(struct i2c_client *client)
{
  int err,i;
  for (i = 0; i < MAX_LM75_NR; i++)
    if (client == lm75_list[i])
      break;
  if ((i == MAX_LM75_NR)) {
    printk("lm75.o: Client to detach not found.\n");
    return -ENOENT;
  }

  sensors_deregister_entry(((struct lm75_data *)(client->data))->sysctl_id);

  if ((err = i2c_detach_client(client))) {
    printk("lm75.o: Client deregistration failed, client not detached.\n");
    return err;
  }

  lm75_list[i] = NULL;
  kfree(client);
  return 0;
}


/* No commands defined yet */
int lm75_command(struct i2c_client *client, unsigned int cmd, void *arg)
{
  return 0;
}

/* Nothing here yet */
void lm75_inc_use (struct i2c_client *client)
{
#ifdef MODULE
  MOD_INC_USE_COUNT;
#endif
}

/* Nothing here yet */
void lm75_dec_use (struct i2c_client *client)
{
#ifdef MODULE
  MOD_DEC_USE_COUNT;
#endif
}

u16 swap_bytes(u16 val)
{
  return (val >> 8) | (val << 8);
}

/* All registers are word-sized, except for the configuration register.
   For some reason, we must swap the low and high byte, but only on 
   reading words?!? */
int lm75_read_value(struct i2c_client *client, u8 reg)
{
  if (reg == LM75_REG_CONF)
    return smbus_read_byte_data(client->adapter,client->addr,reg);
  else
    return swap_bytes(smbus_read_word_data(client->adapter,client->addr,reg));
}

/* No swapping needed here! */
int lm75_write_value(struct i2c_client *client, u8 reg, u16 value)
{
  if (reg == LM75_REG_CONF)
    return smbus_write_byte_data(client->adapter,client->addr,reg,value);
  else
    return smbus_write_word_data(client->adapter,client->addr,reg,value);
}

void lm75_update_client(struct i2c_client *client)
{
  struct lm75_data *data = client->data;

  down(&data->update_lock);

  if ((jiffies - data->last_updated > HZ+HZ/2 ) ||
      (jiffies < data->last_updated) || ! data->valid) {

#ifdef DEBUG
    printk("Starting lm75 update\n");
#endif

    data->temp = lm75_read_value(client,LM75_REG_TEMP);
    data->temp_os = lm75_read_value(client,LM75_REG_TEMP_OS);
    data->temp_hyst = lm75_read_value(client,LM75_REG_TEMP_HYST);
    data->last_updated = jiffies;
    data->valid = 1;
  }

  up(&data->update_lock);
}


void lm75_temp(struct i2c_client *client, int operation, int ctl_name,
               int *nrels_mag, long *results)
{
  struct lm75_data *data = client->data;
  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 1;
  else if (operation == SENSORS_PROC_REAL_READ) {
    lm75_update_client(client);
    results[0] = TEMP_FROM_REG(data->temp_os);
    results[1] = TEMP_FROM_REG(data->temp_hyst);
    results[2] = TEMP_FROM_REG(data->temp);
    *nrels_mag = 3;
  } else if (operation == SENSORS_PROC_REAL_WRITE) {
    if (*nrels_mag >= 1) {
      data->temp_os = TEMP_TO_REG(results[0]);
      lm75_write_value(client,LM75_REG_TEMP_OS,data->temp_os);
    }
    if (*nrels_mag >= 2) {
      data->temp_os = TEMP_TO_REG(results[1]);
      lm75_write_value(client,LM75_REG_TEMP_HYST,data->temp_os);
    }
  }
}

int lm75_init(void)
{
  int res;

  printk("lm75.o version %s (%s)\n",LM_VERSION,LM_DATE);
  lm75_initialized = 0;
  if ((res = i2c_add_driver(&lm75_driver))) {
    printk("lm75.o: Driver registration failed, module not inserted.\n");
    lm75_cleanup();
    return res;
  }
  lm75_initialized ++;
  return 0;
}

int lm75_cleanup(void)
{
  int res;

  if (lm75_initialized >= 1) {
    if ((res = i2c_del_driver(&lm75_driver))) {
      printk("lm75.o: Driver deregistration failed, module not removed.\n");
      return res;
    }
  } else
    lm75_initialized --;

  return 0;
}


#ifdef MODULE

MODULE_AUTHOR("Frodo Looijaard <frodol@dds.nl>");
MODULE_DESCRIPTION("LM75 driver");

int init_module(void)
{
  return lm75_init();
}

int cleanup_module(void)
{
  return lm75_cleanup();
}

#endif /* MODULE */

