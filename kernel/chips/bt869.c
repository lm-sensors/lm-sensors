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

#include <linux/version.h>
#include <linux/module.h>
#include <linux/malloc.h>
#include <linux/i2c.h>
#include "sensors.h"
#include "i2c-isa.h"
#include "version.h"
#include "compat.h"
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,53)
#include <linux/init.h>
#else
#define __init
#define __initdata
#endif


/* Addresses to scan */
static unsigned short normal_i2c[] = {SENSORS_I2C_END};

/* found only at 0x44 or 0x45 */
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

         u8 status[3]; /* Register values */
         u16 res[2]; /* Resolution XxY */
         u8 ntsc; /* 1=NTSC, 0=PAL */
         u8 half; /* go half res */
         u8 depth; /* screen depth */
         u8 colorbars; /* turn on/off colorbar calibration screen */
};

#ifdef MODULE
extern int init_module(void);
extern int cleanup_module(void);
#endif /* MODULE */

#ifdef MODULE
static
#else
extern
#endif
       int __init sensors_bt869_init(void);
static int __init bt869_cleanup(void);
static int bt869_attach_adapter(struct i2c_adapter *adapter);
static int bt869_detect(struct i2c_adapter *adapter, int address, 
                        unsigned short flags, int kind);
static void bt869_init_client(struct i2c_client *client);
static int bt869_detach_client(struct i2c_client *client);
static int bt869_command(struct i2c_client *client, unsigned int cmd,
                        void *arg);
static void bt869_inc_use (struct i2c_client *client);
static void bt869_dec_use (struct i2c_client *client);
static int bt869_read_value(struct i2c_client *client, u8 reg);
static int bt869_write_value(struct i2c_client *client, u8 reg, u16 value);
static void bt869_status(struct i2c_client *client, int operation, int ctl_name,
                      int *nrels_mag, long *results);
static void bt869_ntsc(struct i2c_client *client, int operation, int ctl_name,
                      int *nrels_mag, long *results);
static void bt869_res(struct i2c_client *client, int operation, int ctl_name,
                      int *nrels_mag, long *results);
static void bt869_half(struct i2c_client *client, int operation, int ctl_name,
                      int *nrels_mag, long *results);
static void bt869_colorbars(struct i2c_client *client, int operation, int ctl_name,
                      int *nrels_mag, long *results);
static void bt869_depth(struct i2c_client *client, int operation, int ctl_name,
                      int *nrels_mag, long *results);
static void bt869_update_client(struct i2c_client *client);


/* This is the driver that will be inserted */
static struct i2c_driver bt869_driver = {
  /* name */            "BT869 video-output chip driver",
  /* id */              I2C_DRIVERID_BT869,
  /* flags */           I2C_DF_NOTIFY,
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
  { BT869_SYSCTL_STATUS, "status", NULL, 0, 0444, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &bt869_status },
  { BT869_SYSCTL_NTSC, "ntsc", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &bt869_ntsc },
  { BT869_SYSCTL_RES, "res", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &bt869_res },
  { BT869_SYSCTL_HALF, "half", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &bt869_half },
  { BT869_SYSCTL_COLORBARS, "colorbars", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &bt869_colorbars },
  { BT869_SYSCTL_DEPTH, "depth", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &bt869_depth },
  { 0 }
};

/* Used by init/cleanup */
static int __initdata bt869_initialized = 0;

int bt869_id = 0;

int bt869_attach_adapter(struct i2c_adapter *adapter)
{
  return sensors_detect(adapter,&addr_data,bt869_detect);
}

/* This function is called by sensors_detect */
int bt869_detect(struct i2c_adapter *adapter, int address, 
                 unsigned short flags, int kind)
{
  int i,cur;
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

  if (! i2c_check_functionality(adapter,I2C_FUNC_SMBUS_READ_BYTE| 
                                        I2C_FUNC_SMBUS_WRITE_BYTE_DATA))
    goto ERROR0;

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
  new_client->flags = 0;

  /* Now, we do the remaining detection. It is lousy. */
  i2c_smbus_write_byte_data(new_client,0xC4,0);      /* set status bank 0 */
  cur = i2c_smbus_read_byte(new_client);
  printk("bt869.o: address 0x%X testing-->0x%X\n",address,cur);
  if ((cur & 0xE0) != 0x20)
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

  new_client->id = bt869_id++;
  data->valid = 0;
  init_MUTEX(&data->update_lock);
    
  /* Tell the I2C layer a new client has arrived */
  if ((err = i2c_attach_client(new_client)))
    goto ERROR3;

  /* Register a new directory entry with module sensors */
  if ((i = sensors_register_entry(new_client,type_name,
                                  bt869_dir_table_template,
				  THIS_MODULE)) < 0) {
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
ERROR1:
  kfree(new_client);
ERROR0:
  return err;
}

int bt869_detach_client(struct i2c_client *client)
{
  int err;

  sensors_deregister_entry(((struct bt869_data *)(client->data))->sysctl_id);

  if ((err = i2c_detach_client(client))) {
    printk("bt869.o: Client deregistration failed, client not detached.\n");
    return err;
  }

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

/* All registers are byte-sized.
   bt869 uses a high-byte first convention, which is exactly opposite to
   the usual practice. */
int bt869_read_value(struct i2c_client *client, u8 reg)
{
    return i2c_smbus_read_byte(client);
}

/* All registers are byte-sized.
   bt869 uses a high-byte first convention, which is exactly opposite to
   the usual practice. */
int bt869_write_value(struct i2c_client *client, u8 reg, u16 value)
{
    return i2c_smbus_write_byte_data(client,reg,value);
}

void bt869_init_client(struct i2c_client *client)
{
  struct bt869_data *data = client->data;
  
    /* Initialize the bt869 chip */
    bt869_write_value(client,0x0ba,0x80);
 //   bt869_write_value(client,0x0D6, 0x00);
    /* Be a slave to the clock on the Voodoo3 */
    bt869_write_value(client,0xa0,0x80);
    bt869_write_value(client,0xba,0x20);
    /* depth =16bpp */
    bt869_write_value(client,0x0C6, 0x001);
    bt869_write_value(client,0xC4,1);
    /* Flicker free enable and config */
    bt869_write_value(client,0xC8,0);
    data->res[0]=640;
    data->res[1]=480;
    data->ntsc=1;
    data->half=0;
    data->colorbars=0;
    data->depth=16;
    
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
/* Set values of device */
    if ((data->res[0] == 640) && (data->res[1] == 480)) {
      bt869_write_value(client,0xB8,(! data->ntsc));
      bt869_write_value(client,0xa0,0x80 + 0x0C);
      printk("bt869.o: writing into config -->0x%X\n",(0 + (! data->ntsc)));
    } else if ((data->res[0] == 800) && (data->res[1] == 600)) {
      bt869_write_value(client,0xB8,(2 + (! data->ntsc)));
      bt869_write_value(client,0xa0,0x80 + 0x11);
      printk("bt869.o: writing into config -->0x%X\n",(2 + (! data->ntsc)));
    } else {
      bt869_write_value(client,0xB8,(! data->ntsc));
      bt869_write_value(client,0xa0,0x80 + 0x0C);
      printk("bt869.o: writing into config -->0x%X\n",(0 + (! data->ntsc)));
      printk("bt869.o:  Warning: arbitrary resolutions not supported yet.  Using 640x480.\n");
      data->res[0] = 640;
      data->res[1] = 480;
    }
    if ((data->depth!=24) && (data->depth!=16))
      data->depth=16;
    if (data->depth==16)
      bt869_write_value(client,0x0C6, 0x001);
    if (data->depth==24)
      bt869_write_value(client,0x0C6, 0x000);
    bt869_write_value(client,0xD4,data->half<<6);
    /* Be a slave to the clock on the Voodoo3 */
    bt869_write_value(client,0xba,0x20);
    /* depth =16bpp */
    bt869_write_value(client,0x0C6, 0x001);
    bt869_write_value(client,0xC4,1);

/* Get status */
    bt869_write_value(client,0xC4,1 | (data->colorbars << 2));
    data->status[0] = bt869_read_value(client,1);
    bt869_write_value(client,0xC4,0x41 | (data->colorbars << 2));
    data->status[1] = bt869_read_value(client,1);
    bt869_write_value(client,0xC4,0x81 | (data->colorbars << 2));
    data->status[2] = bt869_read_value(client,1);
    bt869_write_value(client,0xC4,0x0C1 | (data->colorbars << 2));
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
	printk("bt869.o: Warning: write was requested on read-only proc file: status\n");
  }
}


void bt869_ntsc(struct i2c_client *client, int operation, int ctl_name,
               int *nrels_mag, long *results)
{
  struct bt869_data *data = client->data;
  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 0;
  else if (operation == SENSORS_PROC_REAL_READ) {
    bt869_update_client(client);
    results[0] = data->ntsc;
    *nrels_mag = 1;
  } else if (operation == SENSORS_PROC_REAL_WRITE) {
    if (*nrels_mag >= 1) {
      data->ntsc = (results[0] > 0);
    }
    bt869_update_client(client);
  }
}


void bt869_res(struct i2c_client *client, int operation, int ctl_name,
               int *nrels_mag, long *results)
{
  struct bt869_data *data = client->data;
  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 0;
  else if (operation == SENSORS_PROC_REAL_READ) {
    bt869_update_client(client);
    results[0] = data->res[0];
    results[1] = data->res[1];
    *nrels_mag = 2;
  } else if (operation == SENSORS_PROC_REAL_WRITE) {
    if (*nrels_mag >= 1) {
      data->res[0] = results[0];
    }
    if (*nrels_mag >= 2) {
      data->res[1] = results[1];
    }
    bt869_update_client(client);
  }
}


void bt869_half(struct i2c_client *client, int operation, int ctl_name,
               int *nrels_mag, long *results)
{
  struct bt869_data *data = client->data;
  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 0;
  else if (operation == SENSORS_PROC_REAL_READ) {
    bt869_update_client(client);
    results[0] = data->half;
    *nrels_mag = 1;
  } else if (operation == SENSORS_PROC_REAL_WRITE) {
    if (*nrels_mag >= 1) {
      data->half = (results[0] > 0);
      bt869_update_client(client);
    }
  }
}

void bt869_colorbars(struct i2c_client *client, int operation, int ctl_name,
               int *nrels_mag, long *results)
{
  struct bt869_data *data = client->data;
  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 0;
  else if (operation == SENSORS_PROC_REAL_READ) {
    bt869_update_client(client);
    results[0] = data->colorbars;
    *nrels_mag = 1;
  } else if (operation == SENSORS_PROC_REAL_WRITE) {
    if (*nrels_mag >= 1) {
      data->colorbars = (results[0] > 0);
      bt869_update_client(client);
    }
  }
}

void bt869_depth(struct i2c_client *client, int operation, int ctl_name,
               int *nrels_mag, long *results)
{
  struct bt869_data *data = client->data;
  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 0;
  else if (operation == SENSORS_PROC_REAL_READ) {
    bt869_update_client(client);
    results[0] = data->depth;
    *nrels_mag = 1;
  } else if (operation == SENSORS_PROC_REAL_WRITE) {
    if (*nrels_mag >= 1) {
      data->depth = results[0];
      bt869_update_client(client);
    }
  }
}

int __init sensors_bt869_init(void)
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

int __init bt869_cleanup(void)
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

EXPORT_NO_SYMBOLS;

#ifdef MODULE

MODULE_AUTHOR("Frodo Looijaard <frodol@dds.nl> and Philip Edelbrock <phil@netroedge.com>");
MODULE_DESCRIPTION("bt869 driver");

int init_module(void)
{
  return sensors_bt869_init();
}

int cleanup_module(void)
{
  return bt869_cleanup();
}

#endif /* MODULE */

