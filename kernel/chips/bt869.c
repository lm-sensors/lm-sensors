/*
    bt869.c - Part of lm_sensors, Linux kernel modules for hardware
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


#define DEBUG 1

#include <linux/module.h>
#include <linux/malloc.h>
#include "smbus.h"
#include "sensors.h"
#include "i2c.h"
#include "i2c-isa.h"
#include "version.h"

/* Addresses to scan */
static unsigned short normal_i2c[] = {SENSORS_I2C_END};
static unsigned short normal_i2c_range[] = {0x44,0x45,SENSORS_I2C_END};
static unsigned int normal_isa[] = {SENSORS_ISA_END};
static unsigned int normal_isa_range[] = {SENSORS_ISA_END};

/* Insmod parameters */
SENSORS_INSMOD_1(bt869);

/* Many bt869 constants specified below */

/* The bt869 registers */
/* Coming soon: Many, many registers */

/* Conversions. Rounding and limit checking is only done on the TO_REG
   variants. Note that you should be a bit careful with which arguments
   these macros are called: arguments may be evaluated more than once.
   Fixing this is just not worth it. */
   
   /*none*/

/* Initial values */
/*none*/

/* Each client has this additional data */
struct bt869_data {
         int sysctl_id;

         struct semaphore update_lock;
         char valid;                 /* !=0 if following fields are valid */
         unsigned long last_updated; /* In jiffies */

         u16 status[3]; /* Register values */
};

#ifdef MODULE
extern int init_module(void);
extern int cleanup_module(void);
#endif /* MODULE */

static int bt869_init(void);
static int bt869_cleanup(void);
static int bt869_attach_adapter(struct i2c_adapter *adapter);
static int bt869_detect(struct i2c_adapter *adapter, int address, int kind);
static void bt869_init_client(struct i2c_client *client);
static int bt869_detach_client(struct i2c_client *client);
static int bt869_command(struct i2c_client *client, unsigned int cmd,
                        void *arg);
static void bt869_inc_use (struct i2c_client *client);
static void bt869_dec_use (struct i2c_client *client);
static u16 swap_bytes(u16 val);
static int bt869_read_value(struct i2c_client *client, u8 reg);
static int bt869_write_value(struct i2c_client *client, u8 reg, u16 value);
static void bt869_status(struct i2c_client *client, int operation, int ctl_name,
                      int *nrels_mag, long *results);
static void bt869_update_client(struct i2c_client *client);


/* This is the driver that will be inserted */
static struct i2c_driver bt869_driver = {
  /* name */            "BT869 video-output chip driver",
  /* id */              I2C_DRIVERID_BT869,
  /* flags */           DF_NOTIFY,
  /* attach_adapter */  &bt869_attach_adapter,
  /* detach_client */   &bt869_detach_client,
  /* command */         &bt869_command,
  /* inc_use */         &bt869_inc_use,
  /* dec_use */         &bt869_dec_use
};

/* These files are created for each detected bt869. This is just a template;
   though at first sight, you might think we could use a statically
   allocated list, we need some way to get back to the parent - which
   is done through one of the 'extra' fields which are initialized
   when a new copy is allocated. */
static ctl_table bt869_dir_table_template[] = {
  { BT869_SYSCTL_STATUS, "status", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &bt869_status },
  { 0 }
};

/* Used by init/cleanup */
static int bt869_initialized = 0;

/* I choose here for semi-static bt869 allocation. Complete dynamic
   allocation could also be used; the code needed for this would probably
   take more memory than the datastructure takes now. */
#define MAX_bt869_NR 16
static struct i2c_client *bt869_list[MAX_bt869_NR];


int bt869_attach_adapter(struct i2c_adapter *adapter)
{
  return sensors_detect(adapter,&addr_data,bt869_detect);
}

/* This function is called by sensors_detect */
int bt869_detect(struct i2c_adapter *adapter, int address, int kind)
{
  int i,cur,conf,hyst,os;
  struct i2c_client *new_client;
  struct bt869_data *data;
  int err=0;
  const char *type_name,*client_name;


printk("bt869.o:  probing address %d .\n",address);
  /* Make sure we aren't probing the ISA bus!! This is just a safety check
     at this moment; sensors_detect really won't call us. */
#ifdef DEBUG
  if (i2c_is_isa_adapter(adapter)) {
    printk("bt869.o: bt869_detect called for an ISA bus adapter?!?\n");
    return 0;
  }
#endif

  /* Here, we have to do the address registration check for the I2C bus.
     But that is not yet implemented. */

  /* OK. For now, we presume we have a valid client. We now create the
     client structure, even though we cannot fill it completely yet.
     But it allows us to access bt869_{read,write}_value. */
  if (! (new_client = kmalloc(sizeof(struct i2c_client) +
                              sizeof(struct bt869_data),
                              GFP_KERNEL))) {
    err = -ENOMEM;
    goto ERROR0;
  }

  data = (struct bt869_data *) (((struct i2c_client *) new_client) + 1);
  new_client->addr = address;
  new_client->data = data;
  new_client->adapter = adapter;
  new_client->driver = &bt869_driver;

  /* Now, we do the remaining detection. It is lousy. */
  smbus_write_byte_data(new_client->adapter,
	new_client->addr,0xC4,0);         /* set status bank 0 */
  cur = smbus_read_byte_data(adapter,address,0);
  printk("bt869.o: address 0x%X testing-->0x%X\n",address,cur);
  if ((cur | 0x27) != 0x27)
      goto ERROR1;
      
  /* Determine the chip type */
  kind = ((cur & 0x20)>>5);

  if (kind) {
    type_name = "bt869";
    client_name = "bt869 chip";
    printk("bt869.o: BT869 detected\n");
  } else {
    type_name = "bt868";
    client_name = "bt868 chip";
    printk("bt869.o: BT868 detected\n");
  }
  
  /* Fill in the remaining client fields and put it into the global list */
  strcpy(new_client->name,client_name);

  /* Find a place in our global list */
  for (i = 0; i < MAX_bt869_NR; i++)
    if (! bt869_list[i])
       break;
  if (i == MAX_bt869_NR) {
    err = -ENOMEM;
    printk("bt869.o: No empty slots left, recompile and heighten "
           "MAX_bt869_NR!\n");
    goto ERROR2;
  }
  bt869_list[i] = new_client;
  new_client->id = i;
  data->valid = 0;
  data->update_lock = MUTEX;
    
  /* Tell the I2C layer a new client has arrived */
  if ((err = i2c_attach_client(new_client)))
    goto ERROR3;

  /* Register a new directory entry with module sensors */
  if ((i = sensors_register_entry(new_client,type_name,
                                  bt869_dir_table_template)) < 0) {
    err = i;
    goto ERROR4;
  }
  data->sysctl_id = i;

  bt869_init_client((struct i2c_client *) new_client);
  return 0;

/* OK, this is not exactly good programming practice, usually. But it is
   very code-efficient in this case. */

ERROR4:
  i2c_detach_client(new_client);
ERROR3:
  for (i = 0; i < MAX_bt869_NR; i++)
    if (new_client == bt869_list[i])
      bt869_list[i] = NULL;
ERROR2:
ERROR1:
  kfree(new_client);
ERROR0:
  return err;
}

int bt869_detach_client(struct i2c_client *client)
{
  int err,i;

  sensors_deregister_entry(((struct bt869_data *)(client->data))->sysctl_id);

  if ((err = i2c_detach_client(client))) {
    printk("bt869.o: Client deregistration failed, client not detached.\n");
    return err;
  }

  for (i = 0; i < MAX_bt869_NR; i++)
    if (client == bt869_list[i])
      break;
  if ((i == MAX_bt869_NR)) {
    printk("bt869.o: Client to detach not found.\n");
    return -ENOENT;
  }
  bt869_list[i] = NULL;

  kfree(client);

  return 0;
}


/* No commands defined yet */
int bt869_command(struct i2c_client *client, unsigned int cmd, void *arg)
{
  return 0;
}

/* Nothing here yet */
void bt869_inc_use (struct i2c_client *client)
{
#ifdef MODULE
  MOD_INC_USE_COUNT;
#endif
}

/* Nothing here yet */
void bt869_dec_use (struct i2c_client *client)
{
#ifdef MODULE
  MOD_DEC_USE_COUNT;
#endif
}

u16 swap_bytes(u16 val)
{
  return (val >> 8) | (val << 8);
}

/* All registers are byte-sized.
   bt869 uses a high-byte first convention, which is exactly opposite to
   the usual practice. */
int bt869_read_value(struct i2c_client *client, u8 reg)
{
    return smbus_read_byte(client->adapter,client->addr);
}

/* All registers are byte-sized.
   bt869 uses a high-byte first convention, which is exactly opposite to
   the usual practice. */
int bt869_write_value(struct i2c_client *client, u8 reg, u16 value)
{
    return smbus_write_byte_data(client->adapter,client->addr,reg,value);
}

void bt869_init_client(struct i2c_client *client)
{
    /* Initialize the bt869 chip */
    bt869_write_value(client,0xC4,0);
    
    /* Todo: setup the proper modes */
}

void bt869_update_client(struct i2c_client *client)
{
  struct bt869_data *data = client->data;

  down(&data->update_lock);

  if ((jiffies - data->last_updated > HZ+HZ/2 ) ||
      (jiffies < data->last_updated) || ! data->valid) {

#ifdef DEBUG
    printk("Starting bt869 update\n");
#endif

    bt869_write_value(client,0xC4,0);
    data->status[0] = bt869_read_value(client,1);
    bt869_write_value(client,0xC4,0x40);
    data->status[1] = bt869_read_value(client,1);
    bt869_write_value(client,0xC4,0x80);
    data->status[2] = bt869_read_value(client,1);
    bt869_write_value(client,0xC4,0x0C0);
    data->last_updated = jiffies;
    data->valid = 1;
  }

  up(&data->update_lock);
}


void bt869_status(struct i2c_client *client, int operation, int ctl_name,
               int *nrels_mag, long *results)
{
  struct bt869_data *data = client->data;
  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 0;
  else if (operation == SENSORS_PROC_REAL_READ) {
    bt869_update_client(client);
    results[0] = data->status[0];
    results[1] = data->status[1];
    results[2] = data->status[2];
    *nrels_mag = 3;
  } else if (operation == SENSORS_PROC_REAL_WRITE) {
    if (*nrels_mag >= 1) {
 //     data->temp_os = TEMP_TO_REG(results[0]);
//      bt869_write_value(client,bt869_REG_TEMP_OS,data->temp_os);
    }
    if (*nrels_mag >= 2) {
//      data->temp_os = TEMP_TO_REG(results[1]);
//      bt869_write_value(client,bt869_REG_TEMP_HYST,data->temp_os);
    }
  }
}

int bt869_init(void)
{
  int res;

  printk("bt869.o version %s (%s)\n",LM_VERSION,LM_DATE);
  bt869_initialized = 0;
  if ((res = i2c_add_driver(&bt869_driver))) {
    printk("bt869.o: Driver registration failed, module not inserted.\n");
    bt869_cleanup();
    return res;
  }
  bt869_initialized ++;
  return 0;
}

int bt869_cleanup(void)
{
  int res;

  if (bt869_initialized >= 1) {
    if ((res = i2c_del_driver(&bt869_driver))) {
      printk("bt869.o: Driver deregistration failed, module not removed.\n");
      return res;
    }
    bt869_initialized --;
  }

  return 0;
}


#ifdef MODULE

MODULE_AUTHOR("Frodo Looijaard <frodol@dds.nl> and Philip Edelbrock <phil@netroedge.com>");
MODULE_DESCRIPTION("bt869 driver");

int init_module(void)
{
  return bt869_init();
}

int cleanup_module(void)
{
  return bt869_cleanup();
}

#endif /* MODULE */

