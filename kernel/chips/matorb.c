/*
    matorb.c - Part of lm_sensors, Linux kernel modules for hardware
             monitoring
    Copyright (c) 1998, 1999  Frodo Looijaard <frodol@dds.nl>
    and Philip Edelbrock <phil@netroedge.com>

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


#define DEBUG 1

#include <linux/module.h>
#include <linux/malloc.h>
#include <linux/i2c.h>
#include "sensors.h"
#include "i2c-isa.h"
#include "version.h"
#include "compat.h"

/* Addresses to scan */
static unsigned short normal_i2c[] = {0x2E,SENSORS_I2C_END};
static unsigned short normal_i2c_range[] = {SENSORS_I2C_END};
static unsigned int normal_isa[] = {SENSORS_ISA_END};
static unsigned int normal_isa_range[] = {SENSORS_ISA_END};

/* Insmod parameters */
SENSORS_INSMOD_1(matorb);

/* Many MATORB constants specified below */


/* Each client has this additional data */
struct matorb_data {
         int sysctl_id;

         struct semaphore update_lock;
         char valid;                 /* !=0 if following fields are valid */
         unsigned long last_updated; /* In jiffies */

};

#ifdef MODULE
extern int init_module(void);
extern int cleanup_module(void);
#endif /* MODULE */

static int matorb_init(void);
static int matorb_cleanup(void);
static int matorb_attach_adapter(struct i2c_adapter *adapter);
static int matorb_detect(struct i2c_adapter *adapter, int address, int kind);
static void matorb_init_client(struct i2c_client *client);
static int matorb_detach_client(struct i2c_client *client);
static int matorb_command(struct i2c_client *client, unsigned int cmd,
                        void *arg);
static void matorb_inc_use (struct i2c_client *client);
static void matorb_dec_use (struct i2c_client *client);
static int matorb_write_value(struct i2c_client *client, u8 reg, u16 value);
static void matorb_disp(struct i2c_client *client, int operation, int ctl_name,
                      int *nrels_mag, long *results);
static void matorb_update_client(struct i2c_client *client);


/* This is the driver that will be inserted */
static struct i2c_driver matorb_driver = {
  /* name */            "Matrix Orbital LCD driver",
  /* id */              I2C_DRIVERID_MATORB,
  /* flags */           DF_NOTIFY,
  /* attach_adapter */  &matorb_attach_adapter,
  /* detach_client */   &matorb_detach_client,
  /* command */         &matorb_command,
  /* inc_use */         &matorb_inc_use,
  /* dec_use */         &matorb_dec_use
};

/* These files are created for each detected MATORB. This is just a template;
   though at first sight, you might think we could use a statically
   allocated list, we need some way to get back to the parent - which
   is done through one of the 'extra' fields which are initialized
   when a new copy is allocated. */
static ctl_table matorb_dir_table_template[] = {
  { MATORB_SYSCTL_DISP, "disp", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &matorb_disp },
  { 0 }
};

/* Used by init/cleanup */
static int matorb_initialized = 0;

/* I choose here for semi-static MATORB allocation. Complete dynamic
   allocation could also be used; the code needed for this would probably
   take more memory than the datastructure takes now. */
#define MAX_MATORB_NR 16
static struct i2c_client *matorb_list[MAX_MATORB_NR];


int matorb_attach_adapter(struct i2c_adapter *adapter)
{
  return sensors_detect(adapter,&addr_data,matorb_detect);
}

/* This function is called by sensors_detect */
int matorb_detect(struct i2c_adapter *adapter, int address, int kind)
{
  int i,cur;
  struct i2c_client *new_client;
  struct matorb_data *data;
  int err=0;
  const char *type_name="matorb";
  const char *client_name="matorb";

  /* Make sure we aren't probing the ISA bus!! This is just a safety check
     at this moment; sensors_detect really won't call us. */
#ifdef DEBUG
  if (i2c_is_isa_adapter(adapter)) {
    printk("matorb.o: matorb_detect called for an ISA bus adapter?!?\n");
    return 0;
  }
#endif

  /* Here, we have to do the address registration check for the I2C bus.
     But that is not yet implemented. */

  /* OK. For now, we presume we have a valid client. We now create the
     client structure, even though we cannot fill it completely yet.
     But it allows us to access matorb_{read,write}_value. */
  if (! (new_client = kmalloc(sizeof(struct i2c_client) +
                              sizeof(struct matorb_data),
                              GFP_KERNEL))) {
    err = -ENOMEM;
    goto ERROR0;
  }

  data = (struct matorb_data *) (((struct i2c_client *) new_client) + 1);
  new_client->addr = address;
  new_client->data = data;
  new_client->adapter = adapter;
  new_client->driver = &matorb_driver;

  /* Now, we do the remaining detection. It is lousy. */
  cur = smbus_write_byte_data(adapter,address,0x0FE, 0x58); /* clear screen */
  
  printk("matorb.o: debug detect 0x%X\n",cur);
  
  /* Fill in the remaining client fields and put it into the global list */
  strcpy(new_client->name,client_name);

  /* Find a place in our global list */
  for (i = 0; i < MAX_MATORB_NR; i++)
    if (! matorb_list[i])
       break;
  if (i == MAX_MATORB_NR) {
    err = -ENOMEM;
    printk("matorb.o: No empty slots left, recompile and heighten "
           "MAX_MATORB_NR!\n");
    goto ERROR2;
  }
  matorb_list[i] = new_client;
  new_client->id = i;
  data->valid = 0;
  init_MUTEX(&data->update_lock);
    
  /* Tell the I2C layer a new client has arrived */
  if ((err = i2c_attach_client(new_client)))
    goto ERROR3;

  /* Register a new directory entry with module sensors */
  if ((i = sensors_register_entry(new_client,type_name,
                                  matorb_dir_table_template)) < 0) {
    err = i;
    goto ERROR4;
  }
  data->sysctl_id = i;

  matorb_init_client((struct i2c_client *) new_client);
  return 0;

/* OK, this is not exactly good programming practice, usually. But it is
   very code-efficient in this case. */

ERROR4:
  i2c_detach_client(new_client);
ERROR3:
  for (i = 0; i < MAX_MATORB_NR; i++)
    if (new_client == matorb_list[i])
      matorb_list[i] = NULL;
ERROR2:
  kfree(new_client);
ERROR0:
  return err;
}

int matorb_detach_client(struct i2c_client *client)
{
  int err,i;

  sensors_deregister_entry(((struct matorb_data *)(client->data))->sysctl_id);

  if ((err = i2c_detach_client(client))) {
    printk("matorb.o: Client deregistration failed, client not detached.\n");
    return err;
  }

  for (i = 0; i < MAX_MATORB_NR; i++)
    if (client == matorb_list[i])
      break;
  if ((i == MAX_MATORB_NR)) {
    printk("matorb.o: Client to detach not found.\n");
    return -ENOENT;
  }
  matorb_list[i] = NULL;

  kfree(client);

  return 0;
}


/* No commands defined yet */
int matorb_command(struct i2c_client *client, unsigned int cmd, void *arg)
{
  return 0;
}

/* Nothing here yet */
void matorb_inc_use (struct i2c_client *client)
{
#ifdef MODULE
  MOD_INC_USE_COUNT;
#endif
}

/* Nothing here yet */
void matorb_dec_use (struct i2c_client *client)
{
#ifdef MODULE
  MOD_DEC_USE_COUNT;
#endif
}

#if 0
/* All registers are word-sized, except for the configuration register.
   MATORB uses a high-byte first convention, which is exactly opposite to
   the usual practice. */
int matorb_read_value(struct i2c_client *client, u8 reg)
{
    return -1;  /* Doesn't support reads */
}
#endif

/* All registers are word-sized, except for the configuration register.
   MATORB uses a high-byte first convention, which is exactly opposite to
   the usual practice. */
int matorb_write_value(struct i2c_client *client, u8 reg, u16 value)
{
  if (reg==0) {
    return smbus_write_byte(client->adapter,client->addr,value);
  } else {
    return smbus_write_byte_data(client->adapter,client->addr,reg,value);
  }
}

void matorb_init_client(struct i2c_client *client)
{
    /* Initialize the MATORB chip */
}

void matorb_update_client(struct i2c_client *client)
{
  struct matorb_data *data = client->data;

  down(&data->update_lock);

  if ((jiffies - data->last_updated > HZ+HZ/2 ) ||
      (jiffies < data->last_updated) || ! data->valid) {

#ifdef DEBUG
    printk("Starting matorb update\n");
#endif

/* nothing yet */
    data->last_updated = jiffies;
    data->valid = 1;
  }

  up(&data->update_lock);
}


void matorb_disp(struct i2c_client *client, int operation, int ctl_name,
               int *nrels_mag, long *results)
{
int i;

  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 0;
  else if (operation == SENSORS_PROC_REAL_READ) {
    matorb_update_client(client);
    results[0] = 0;
    *nrels_mag = 3;
  } else if (operation == SENSORS_PROC_REAL_WRITE) {
    for (i=1; i<=*nrels_mag;i++) {
      matorb_write_value(client,0,results[i-1]);
    }
  }
}

int matorb_init(void)
{
  int res;

  printk("matorb.o version %s (%s)\n",LM_VERSION,LM_DATE);
  matorb_initialized = 0;
  if ((res = i2c_add_driver(&matorb_driver))) {
    printk("matorb.o: Driver registration failed, module not inserted.\n");
    matorb_cleanup();
    return res;
  }
  matorb_initialized ++;
  return 0;
}

int matorb_cleanup(void)
{
  int res;

  if (matorb_initialized >= 1) {
    if ((res = i2c_del_driver(&matorb_driver))) {
      printk("matorb.o: Driver deregistration failed, module not removed.\n");
      return res;
    }
    matorb_initialized --;
  }

  return 0;
}


#ifdef MODULE

MODULE_AUTHOR("Frodo Looijaard <frodol@dds.nl> and Philip Edelbrock <phil@netroedge.com>");
MODULE_DESCRIPTION("MATORB driver");

int init_module(void)
{
  return matorb_init();
}

int cleanup_module(void)
{
  return matorb_cleanup();
}

#endif /* MODULE */

