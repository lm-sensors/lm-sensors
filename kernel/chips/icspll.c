/*
    icspll.c - Part of lm_sensors, Linux kernel modules for hardware
               monitoring
    Copyright (c) 1998, 1999  Frodo Looijaard <frodol@dds.nl>,
    Philip Edelbrock <phil@netroedge.com>,
    and Mark Studebaker <mds@eng.paradyne.com>

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

/*
    ** WARNING  **
    Supports limited combinations of clock chips and busses.
    Use on unsupported chips may crash your system.
    See doc/chips/icspll for details.
*/


#include <linux/version.h>
#include <linux/module.h>
#include <linux/malloc.h>
#include <linux/i2c.h>
#include "sensors.h"
#include "i2c-isa.h"
#include "version.h"
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,53)
#include <linux/init.h>
#else
#define __init
#define __initdata
#endif


/* Many constants specified below */

#define ICSPLL_SIZE 7
#define MAXBLOCK_SIZE 32

/* Each client has this additional data */
struct icspll_data {
         int sysctl_id;

         struct semaphore update_lock;
         char valid;                 /* !=0 if following fields are valid */
         unsigned long last_updated; /* In jiffies */

         u8 data[ICSPLL_SIZE];     /* Register values */
	 int memtype;
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
       int __init sensors_icspll_init(void);
static int __init icspll_cleanup(void);

static int icspll_attach_adapter(struct i2c_adapter *adapter);
static int icspll_detach_client(struct i2c_client *client);
static int icspll_command(struct i2c_client *client, unsigned int cmd,
                        void *arg);
			
static void icspll_inc_use (struct i2c_client *client);
static void icspll_dec_use (struct i2c_client *client);

#if 0
static int icspll_write_value(struct i2c_client *client, u8 reg, u16 value);
#endif

static void icspll_contents(struct i2c_client *client, int operation, int ctl_name,
                      int *nrels_mag, long *results);
static void icspll_update_client(struct i2c_client *client);


/* This is the driver that will be inserted */
static struct i2c_driver icspll_driver = {
  /* name */            "ICS/PLL clock reader",
  /* id */              I2C_DRIVERID_ICSPLL,
  /* flags */           I2C_DF_NOTIFY,
  /* attach_adapter */  &icspll_attach_adapter,
  /* detach_client */   &icspll_detach_client,
  /* command */         &icspll_command,
  /* inc_use */         &icspll_inc_use,
  /* dec_use */         &icspll_dec_use
};

/* These files are created for each detected ICSPLL. This is just a template;
   though at first sight, you might think we could use a statically
   allocated list, we need some way to get back to the parent - which
   is done through one of the 'extra' fields which are initialized
   when a new copy is allocated. */
static ctl_table icspll_dir_table_template[] = {
  { ICSPLL_SYSCTL1, "reg0-6", NULL, 0, 0444, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &icspll_contents },
  { 0 }
};

/* holding place for data - block read could be as much as 32 */
static u8 tempdata[MAXBLOCK_SIZE];

/* Used by init/cleanup */
static int __initdata icspll_initialized = 0;

static int icspll_id = 0;
int icspll_attach_adapter(struct i2c_adapter *adapter)
{
  int address,err,i;
  struct i2c_client *new_client;
  struct icspll_data *data;

  err = 0;
  /* Make sure we aren't probing the ISA bus!! */
  if (i2c_is_isa_adapter(adapter)) return 0;
  
  /* OK, this is no detection. I know. It will do for now, though.  */

  /* Set err only if a global error would make registering other clients
     impossible too (like out-of-memory). */
     
  /* ICS and PLL clock chips seem to all be at 0x69.
     Don't know what is out there at 0x6A, don't scan that yet. */

  for (address = 0x69; (! err) && (address <= 0x69); address ++) {

    /* Later on, we will keep a list of registered addresses for each
       adapter, and check whether they are used here */
    
    /* these chips only support block transfers so use that for detection */
    if(i2c_smbus_read_block_data(adapter, address, 0x00, tempdata) < 0) {
#ifdef DEBUG
      printk("icspll.o: No icspll found at: 0x%X\n",address);
#endif
      continue;
    }

    /* Real detection code goes here */

    /* Allocate space for a new client structure */
    if (! (new_client =  kmalloc(sizeof(struct i2c_client) +
                                sizeof(struct icspll_data),
                               GFP_KERNEL))) {
      err = -ENOMEM;
      continue;
    }

    /* Fill the new client structure with data */
    data = (struct icspll_data *) (new_client + 1);
    new_client->data = data;
    new_client->id = icspll_id++;
    new_client->addr = address;
    new_client->adapter = adapter;
    new_client->driver = &icspll_driver;
  new_client->flags = 0;
    strcpy(new_client->name,"ICSPLL chip");
    data->valid = 0;
    init_MUTEX(&data->update_lock);
/* fill data structure so unknown registers are 0xFF */
    data->data[0] = ICSPLL_SIZE;
    for(i = 1; i <= ICSPLL_SIZE; i++)
      data->data[i] = 0xFF;
    
    /* Tell i2c-core a new client has arrived */
    if ((err = i2c_attach_client(new_client)))
      goto ERROR2;
    
    /* Register a new directory entry with module sensors */
    if ((err = sensors_register_entry(new_client,"icspll",
                                      icspll_dir_table_template
				      THIS_MODULE)) < 0)
      goto ERROR3;
    data->sysctl_id = err;
    err = 0;

    continue;
/* OK, this is not exactly good programming practice, usually. But it is
   very code-efficient in this case. */

ERROR3:
    i2c_detach_client(new_client);
ERROR2:
ERROR1:
    kfree(new_client);
  }
  return err;
}

int icspll_detach_client(struct i2c_client *client)
{
  int err,i;

  sensors_deregister_entry(((struct icspll_data *)(client->data))->sysctl_id);

  if ((err = i2c_detach_client(client))) {
    printk("icspll.o: Client deregistration failed, client not detached.\n");
    return err;
  }

  kfree(client);
  return 0;
}


/* No commands defined yet */
int icspll_command(struct i2c_client *client, unsigned int cmd, void *arg)
{
  return 0;
}

void icspll_inc_use (struct i2c_client *client)
{
#ifdef MODULE
  MOD_INC_USE_COUNT;
#endif
}

void icspll_dec_use (struct i2c_client *client)
{
#ifdef MODULE
  MOD_DEC_USE_COUNT;
#endif
}

#if 0
/* No writes yet (PAE) */
int icspll_write_value(struct i2c_client *client, u8 reg, u16 value)
{
  return i2c_smbus_write_block_data(client->adapter,client->addr,reg,value);
}
#endif

void icspll_update_client(struct i2c_client *client)
{
  struct icspll_data *data = client->data;
  int i, len;

  down(&data->update_lock);

  if ((jiffies - data->last_updated > HZ+HZ/2 ) ||
      (jiffies < data->last_updated) || ! data->valid) {

    len = i2c_smbus_read_block_data(client->adapter, client->addr, 0x00, tempdata);
#ifdef DEBUG
    printk("icspll.o: read returned %d values\n", len);
#endif
    if(len > ICSPLL_SIZE)
      len = ICSPLL_SIZE;
    for(i = 0; i < len; i++)
      data->data[i] = tempdata[i];
    
    data->last_updated = jiffies;
    data->valid = 1;
  }

  up(&data->update_lock);
}


void icspll_contents(struct i2c_client *client, int operation, int ctl_name,
               int *nrels_mag, long *results)
{
  int i;
  struct icspll_data *data = client->data;
  
  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 0;
  else if (operation == SENSORS_PROC_REAL_READ) {
    icspll_update_client(client);
    for (i=0; i< ICSPLL_SIZE; i++) {
    	results[i]=data->data[i];
    }
#ifdef DEBUG
    printk("icspll.o: 0x%X ICSPLL Contents: ",client->addr);
    for (i=0; i< ICSPLL_SIZE; i++) {
      printk(" 0x%X",data->data[i]);
    }
    printk(" .\n");
#endif
    *nrels_mag = ICSPLL_SIZE;
  } else if (operation == SENSORS_PROC_REAL_WRITE) {

/* No writes to the ICSPLL (yet, anyway) (PAE) */
    printk("icspll.o: No writes to ICSPLLs supported!\n");
  }
}

int __init sensors_icspll_init(void)
{
  int res;

  printk("icspll.o version %s (%s)\n",LM_VERSION,LM_DATE);
  icspll_initialized = 0;
  if ((res = i2c_add_driver(&icspll_driver))) {
    printk("icspll.o: Driver registration failed, module not inserted.\n");
    icspll_cleanup();
    return res;
  }
  icspll_initialized ++;
  return 0;
}

int __init icspll_cleanup(void)
{
  int res;

  if (icspll_initialized >= 1) {
    if ((res = i2c_del_driver(&icspll_driver))) {
      printk("icspll.o: Driver deregistration failed, module not removed.\n");
      return res;
    }
  } else
    icspll_initialized --;

  return 0;
}

EXPORT_NO_SYMBOLS;

#ifdef MODULE

MODULE_AUTHOR("Frodo Looijaard <frodol@dds.nl>, Philip Edelbrock <phil@netroedge.com>, and Mark Studebaker <mds@eng.paradyne.com>");
MODULE_DESCRIPTION("ICSPLL driver");

int init_module(void)
{
  return sensors_icspll_init();
}

int cleanup_module(void)
{
  return icspll_cleanup();
}

#endif /* MODULE */

