
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
#include <linux/sysctl.h>
#include <asm/errno.h>
#include <asm/io.h>
#include <linux/types.h>
#include "smbus.h"
#include "version.h"
#include "isa.h"
#include "sensors.h"
#include "i2c.h"
#include "compat.h"

/* Many LM78 constants needed below */

/* Length of ISA address segment */
#define LM78_EXTENT 8

/* Where are the ISA address/data registers relative to the base address */
#define LM78_ADDR_REG_OFFSET 5
#define LM78_DATA_REG_OFFSET 6

/* The LM78 registers */
#define LM78_REG_IN_MAX(nr) (0x2b + (nr) * 2)
#define LM78_REG_IN_MIN(nr) (0x2c + (nr) * 2)
#define LM78_REG_IN(nr) (0x20 + (nr))

#define LM78_REG_FAN_MIN(nr) (0x3a + (nr))
#define LM78_REG_FAN(nr) (0x27 + (nr))

#define LM78_REG_TEMP 0x27
#define LM78_REG_TEMP_OVER 0x39
#define LM78_REG_TEMP_HYST 0x3a

#define LM78_REG_ALARM1 0x41
#define LM78_REG_ALARM2 0x42

#define LM78_REG_VID_FANDIV 0x47

#define LM78_REG_CONFIG 0x40


/* Conversions */
static int lm78_in_conv[7] = {10000, 10000, 10000, 16892, 38000, 
                              -34768, -15050 };
#define IN_TO_REG(val,nr) ((((val) * 100000 / lm78_in_conv[nr]) + 8) / 16)
#define IN_FROM_REG(val,nr) (((val) *  16 * lm78_in_conv[nr]) / 100000)

#define FAN_TO_REG(val) (((val)==0)?255:((1350000+(val))/((val)*2)))
#define FAN_FROM_REG(val) (((val)==0)?-1:\
                           ((val)==255)?0:(1350000 + (val))/((val)*2))

#define TEMP_TO_REG(val) ((val)<0?(val)&0xff:(val))
#define TEMP_FROM_REG(val) ((val)>0x80?(val)-0x100:(val));

#define VID_FROM_REG(val) ((val) == 0x0f?0:350-(val)*10)

#define ALARMS_FROM_REG(val) (val)

#define DIV_FROM_REG(val) (1 >> (val))
#define DIV_TO_REG(val) ((val)==8?3:(val)==4?2:(val)==1?1:2)

/* Initial limits */
#define LM78_INIT_IN_0 280
#define LM78_INIT_IN_1 280
#define LM78_INIT_IN_2 330
#define LM78_INIT_IN_3 500
#define LM78_INIT_IN_4 1200
#define LM78_INIT_IN_5 -1200
#define LM78_INIT_IN_6 -500

#define LM78_INIT_IN_PERCENTAGE 10

#define LM78_INIT_IN_MIN_0 \
        (LM78_INIT_IN_0 - LM78_INIT_IN_0 * LM78_INIT_IN_PERCENTAGE / 100) 
#define LM78_INIT_IN_MAX_0 \
        (LM78_INIT_IN_0 + LM78_INIT_IN_0 * LM78_INIT_IN_PERCENTAGE / 100) 
#define LM78_INIT_IN_MIN_1 \
        (LM78_INIT_IN_1 - LM78_INIT_IN_1 * LM78_INIT_IN_PERCENTAGE / 100) 
#define LM78_INIT_IN_MAX_1 \
        (LM78_INIT_IN_1 + LM78_INIT_IN_1 * LM78_INIT_IN_PERCENTAGE / 100) 
#define LM78_INIT_IN_MIN_2 \
        (LM78_INIT_IN_2 - LM78_INIT_IN_2 * LM78_INIT_IN_PERCENTAGE / 100) 
#define LM78_INIT_IN_MAX_2 \
        (LM78_INIT_IN_2 + LM78_INIT_IN_2 * LM78_INIT_IN_PERCENTAGE / 100) 
#define LM78_INIT_IN_MIN_3 \
        (LM78_INIT_IN_3 - LM78_INIT_IN_3 * LM78_INIT_IN_PERCENTAGE / 100) 
#define LM78_INIT_IN_MAX_3 \
        (LM78_INIT_IN_3 + LM78_INIT_IN_3 * LM78_INIT_IN_PERCENTAGE / 100) 
#define LM78_INIT_IN_MIN_4 \
        (LM78_INIT_IN_4 - LM78_INIT_IN_4 * LM78_INIT_IN_PERCENTAGE / 100) 
#define LM78_INIT_IN_MAX_4 \
        (LM78_INIT_IN_4 + LM78_INIT_IN_4 * LM78_INIT_IN_PERCENTAGE / 100) 
#define LM78_INIT_IN_MIN_5 \
        (LM78_INIT_IN_5 - LM78_INIT_IN_5 * LM78_INIT_IN_PERCENTAGE / 100) 
#define LM78_INIT_IN_MAX_5 \
        (LM78_INIT_IN_5 + LM78_INIT_IN_5 * LM78_INIT_IN_PERCENTAGE / 100) 
#define LM78_INIT_IN_MIN_6 \
        (LM78_INIT_IN_6 - LM78_INIT_IN_6 * LM78_INIT_IN_PERCENTAGE / 100) 
#define LM78_INIT_IN_MAX_6 \
        (LM78_INIT_IN_6 + LM78_INIT_IN_6 * LM78_INIT_IN_PERCENTAGE / 100) 

#define LM78_INIT_FAN_MIN_1 3000
#define LM78_INIT_FAN_MIN_2 3000
#define LM78_INIT_FAN_MIN_3 3000

#define LM78_INIT_TEMP_OVER 60
#define LM78_INIT_TEMP_HYST 50

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

/* For each registered LM78, we need to keep some data in memory. That
   data is pointed to by lm78_list[NR]->data. The structure itself is
   dynamically allocated, at the same time when a new lm78 client is
   allocated. */
struct lm78_data {
         struct semaphore lock;
         int sysctl_id;

         struct semaphore update_lock;
         char valid;                 /* !=0 if following fields are valid */
         unsigned long last_updated; /* In jiffies */

         u8 in[7];                   /* Register value */
         u8 in_max[7];               /* Register value */
         u8 in_min[7];               /* Register value */
         u8 fan[3];                  /* Register value */
         u8 fan_min[3];              /* Register value */
         u8 temp;                    /* Register value */
         u8 temp_over;               /* Register value */
         u8 temp_hyst;               /* Register value */
         u8 fan_div[2];              /* Register encoding, shifted right */
         u8 vid;                     /* Register encoding, combined */
         u16 alarms;                 /* Register encoding, combined */
};


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
static void lm78_remove_client(struct i2c_client *client);
static int lm78_command(struct i2c_client *client, unsigned int cmd, 
                        void *arg);
static void lm78_inc_use (struct i2c_client *client);
static void lm78_dec_use (struct i2c_client *client);

static int lm78_read_value(struct i2c_client *client, u8 register);
static int lm78_write_value(struct i2c_client *client, u8 register, u8 value);
static void lm78_update_client(struct i2c_client *client);
static void lm78_init_client(struct i2c_client *client);

static int lm78_sysctl (ctl_table *table, int *name, int nlen, void *oldval, 
                        size_t *oldlenp, void *newval, size_t newlen,
                        void **context);
static int lm78_proc (ctl_table *ctl, int write, struct file * filp,
                      void *buffer, size_t *lenp);


static void write_in(struct i2c_client *client, int nr, int nrels, 
                     long *results);
static void read_in(struct i2c_client *client, int nr, long *results);
static void write_fan(struct i2c_client *client, int nr, int nrels, 
                      long *results);
static void read_fan(struct i2c_client *client, int nr, long *results);
static void write_temp(struct i2c_client *client, int nrels, long *results);
static void read_temp(struct i2c_client *client, long *results);
static void read_vid(struct i2c_client *client, long *results);
static void read_alarms(struct i2c_client *client, long *results);
static void write_fan_div(struct i2c_client *client, int nrels, long *results);
static void read_fan_div(struct i2c_client *client, long *results);



/* I choose here for semi-static LM78 allocation. Complete dynamic
   allocation could also be used; the code needed for this would probably
   take more memory than the datastructure takes now. */
#define MAX_LM78_NR 4
static struct i2c_client *lm78_list[MAX_LM78_NR];

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

/* The /proc/sys entries */
/* These files are created for each detected LM78. This is just a template;
   though at first sight, you might think we could use a statically
   allocated list, we need some way to get back to the parent - which
   is done through one of the 'extra' fields which are initialized 
   when a new copy is allocated. */
static ctl_table lm78_dir_table_template[] = {
  { LM78_SYSCTL_IN0, "in0", NULL, 0, 0644, NULL, &lm78_proc, &lm78_sysctl },
  { LM78_SYSCTL_IN1, "in1", NULL, 0, 0644, NULL, &lm78_proc, &lm78_sysctl },
  { LM78_SYSCTL_IN2, "in2", NULL, 0, 0644, NULL, &lm78_proc, &lm78_sysctl },
  { LM78_SYSCTL_IN3, "in3", NULL, 0, 0644, NULL, &lm78_proc, &lm78_sysctl },
  { LM78_SYSCTL_IN4, "in4", NULL, 0, 0644, NULL, &lm78_proc, &lm78_sysctl },
  { LM78_SYSCTL_IN5, "in5", NULL, 0, 0644, NULL, &lm78_proc, &lm78_sysctl },
  { LM78_SYSCTL_IN6, "in6", NULL, 0, 0644, NULL, &lm78_proc, &lm78_sysctl },
  { LM78_SYSCTL_FAN1, "fan1", NULL, 0, 0644, NULL, &lm78_proc, &lm78_sysctl },
  { LM78_SYSCTL_FAN2, "fan2", NULL, 0, 0644, NULL, &lm78_proc, &lm78_sysctl },
  { LM78_SYSCTL_FAN3, "fan3", NULL, 0, 0644, NULL, &lm78_proc, &lm78_sysctl },
  { LM78_SYSCTL_TEMP, "temp", NULL, 0, 0644, NULL, &lm78_proc, &lm78_sysctl },
  { LM78_SYSCTL_VID, "vid", NULL, 0, 0644, NULL, &lm78_proc, &lm78_sysctl },
  { LM78_SYSCTL_FAN_DIV, "fan_div", NULL, 0, 0644, NULL, &lm78_proc, &lm78_sysctl },
  { LM78_SYSCTL_ALARMS, "alarms", NULL, 0, 0644, NULL, &lm78_proc, &lm78_sysctl },
  { 0 }
};


/* This function is called when:
     * lm78_driver is inserted (when this module is loaded), for each
       available adapter
     * when a new adapter is inserted (and lm78_driver is still present) */
int lm78_attach_adapter(struct i2c_adapter *adapter)
{
  if (i2c_is_isa_adapter(adapter))
    return lm78_detect_isa((struct isa_adapter *) adapter);
  else
    return lm78_detect_smbus(adapter);
}

/* This function is called whenever a client should be removed:
    * lm78_driver is removed (when this module is unloaded)
    * when an adapter is removed which has a lm78 client (and lm78_driver
      is still present). */
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

    /* Allocate space for a new client structure */
    if (! (new_client = kmalloc(sizeof(struct isa_client) + 
                                sizeof(struct lm78_data),
                               GFP_KERNEL)))
    {
      err=-ENOMEM;
      goto ERROR1;
    } 

    /* Fill the new client structure with data */
    new_client->data = (struct lm78_data *) (new_client + 1);
    new_client->addr = 0;
    new_client->isa_addr = address;
    if ((err = lm78_new_client((struct i2c_adapter *) adapter,
                               (struct i2c_client *) new_client)))
      goto ERROR2;

    /* Tell i2c-core a new client has arrived */
    if ((err = isa_attach_client(new_client)))
      goto ERROR3;
    
    /* Register a new directory entry with module sensors */
    if ((err = sensors_register_entry((struct i2c_client *) new_client,"lm78",
                                      lm78_dir_table_template)) < 0)
      goto ERROR4;
    ((struct lm78_data *) (new_client->data)) -> sysctl_id = err;

    /* Initialize the LM78 chip */
    lm78_init_client((struct i2c_client *) new_client);
    continue;

/* OK, this is not exactly good programming practice, usually. But it is
   very code-efficient in this case. */

ERROR4:
    isa_detach_client(new_client);
ERROR3:
    lm78_remove_client((struct i2c_client *) new_client);
ERROR2:
    kfree(new_client);
ERROR1:
    release_region(address, LM78_EXTENT);
  }
  return err;

}

/* Deregister and remove a LM78 client */
int lm78_detach_isa(struct isa_client *client)
{
  int err,i;
  for (i = 0; i < MAX_LM78_NR; i++)
    if ((client == (struct isa_client *) (lm78_list[i])))
      break;
  if (i == MAX_LM78_NR) {
    printk("lm78.o: Client to detach not found.\n");
    return -ENOENT;
  }

  sensors_deregister_entry(((struct lm78_data *)(client->data))->sysctl_id);

  if ((err = isa_detach_client(client))) {
    printk("lm78.o: Client deregistration failed, client not detached.\n");
    return err;
  }
  lm78_remove_client((struct i2c_client *) client);
  release_region(client->isa_addr,LM78_EXTENT);
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

    /* Allocate space for a new client structure */
    if (! (new_client = kmalloc(sizeof(struct i2c_client) + 
                                sizeof(struct lm78_data),
                               GFP_KERNEL))) {
      err = -ENOMEM;
      continue;
    }

    /* Fill the new client structure with data */
    new_client->data = (struct lm78_data *) (new_client + 1);
    new_client->addr = address;
    if ((err = lm78_new_client(adapter,new_client)))
      goto ERROR2;

    /* Tell i2c-core a new client has arrived */
    if ((err = i2c_attach_client(new_client))) 
      goto ERROR3;

    /* Register a new directory entry with module sensors */
    if ((err = sensors_register_entry(new_client,"lm78",
                                      lm78_dir_table_template)) < 0)
      goto ERROR4;
    ((struct lm78_data *) (new_client->data))->sysctl_id = err;

    /* Initialize the LM78 chip */
    lm78_init_client(new_client);
    continue;

/* OK, this is not exactly good programming practice, usually. But it is
   very code-efficient in this case. */
ERROR4:
    i2c_detach_client(new_client);
ERROR3:
    lm78_remove_client((struct i2c_client *) new_client);
ERROR2:
    kfree(new_client);
  }
  return err;
}

int lm78_detach_smbus(struct i2c_client *client)
{
  int err,i;
  for (i = 0; i < MAX_LM78_NR; i++)
    if (client == lm78_list[i])
      break;
  if ((i == MAX_LM78_NR)) {
    printk("lm78.o: Client to detach not found.\n");
    return -ENOENT;
  }

  sensors_deregister_entry(((struct lm78_data *)(client->data))->sysctl_id);

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
  struct lm78_data *data;

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
  strcpy(new_client->name,"LM78 chip");
  new_client->id = i;
  new_client->adapter = adapter;
  new_client->driver = &lm78_driver;
  data = new_client->data;
  data->valid = 0;
  data->lock = MUTEX;
  data->update_lock = MUTEX;
  return 0;
}

/* Inverse of lm78_new_client */
void lm78_remove_client(struct i2c_client *client)
{
  int i;
  for (i = 0; i < MAX_LM78_NR; i++)
    if (client == lm78_list[i]) 
      lm78_list[i] = NULL;
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

/* Called when we have found a new LM78. It should set limits, etc. */
void lm78_init_client(struct i2c_client *client)
{
  /* Reset all except Watchdog values and last conversion values
     This sets fan-divs to 2, among others */
  lm78_write_value(client,LM78_REG_CONFIG,0x80);

  lm78_write_value(client,LM78_REG_IN_MIN(0),IN_TO_REG(LM78_INIT_IN_MIN_0,0));
  lm78_write_value(client,LM78_REG_IN_MAX(0),IN_TO_REG(LM78_INIT_IN_MAX_0,0));
  lm78_write_value(client,LM78_REG_IN_MIN(1),IN_TO_REG(LM78_INIT_IN_MIN_1,1));
  lm78_write_value(client,LM78_REG_IN_MAX(1),IN_TO_REG(LM78_INIT_IN_MAX_1,1));
  lm78_write_value(client,LM78_REG_IN_MIN(2),IN_TO_REG(LM78_INIT_IN_MIN_2,2));
  lm78_write_value(client,LM78_REG_IN_MAX(2),IN_TO_REG(LM78_INIT_IN_MAX_2,2));
  lm78_write_value(client,LM78_REG_IN_MIN(3),IN_TO_REG(LM78_INIT_IN_MIN_3,3));
  lm78_write_value(client,LM78_REG_IN_MAX(3),IN_TO_REG(LM78_INIT_IN_MAX_3,3));
  lm78_write_value(client,LM78_REG_IN_MIN(4),IN_TO_REG(LM78_INIT_IN_MIN_4,4));
  lm78_write_value(client,LM78_REG_IN_MAX(4),IN_TO_REG(LM78_INIT_IN_MAX_4,4));
  lm78_write_value(client,LM78_REG_IN_MIN(5),IN_TO_REG(LM78_INIT_IN_MIN_5,5));
  lm78_write_value(client,LM78_REG_IN_MAX(5),IN_TO_REG(LM78_INIT_IN_MAX_5,5));
  lm78_write_value(client,LM78_REG_IN_MIN(6),IN_TO_REG(LM78_INIT_IN_MIN_6,6));
  lm78_write_value(client,LM78_REG_IN_MAX(6),IN_TO_REG(LM78_INIT_IN_MAX_6,6));
  lm78_write_value(client,LM78_REG_FAN_MIN(1),FAN_TO_REG(LM78_INIT_FAN_MIN_1));
  lm78_write_value(client,LM78_REG_FAN_MIN(2),FAN_TO_REG(LM78_INIT_FAN_MIN_2));
  lm78_write_value(client,LM78_REG_FAN_MIN(3),FAN_TO_REG(LM78_INIT_FAN_MIN_3));
  lm78_write_value(client,LM78_REG_TEMP_OVER,TEMP_TO_REG(LM78_INIT_TEMP_OVER));
  lm78_write_value(client,LM78_REG_TEMP_HYST,TEMP_TO_REG(LM78_INIT_TEMP_HYST));

  /* Start monitoring */
  lm78_write_value(client,LM78_REG_CONFIG,
                   (lm78_read_value(client,LM78_REG_CONFIG) & 0xf7) | 0x01);
  
}

void lm78_update_client(struct i2c_client *client)
{
  struct lm78_data *data = client->data;
  int i;

  down(&data->update_lock);

  if ((jiffies - data->last_updated > HZ+HZ/2 ) ||
      (jiffies < data->last_updated) || ! data->valid) {

#ifdef DEBUG
    printk("Starting lm78 update\n");
#endif
    for (i = 0; i <= 6; i++) {
      data->in[i]     = lm78_read_value(client,LM78_REG_IN(i));
      data->in_min[i] = lm78_read_value(client,LM78_REG_IN_MIN(i));
      data->in_max[i] = lm78_read_value(client,LM78_REG_IN_MAX(i));
    }
    for (i = 1; i <= 3; i++) {
      data->fan[i-1] = lm78_read_value(client,LM78_REG_FAN(i));
      data->fan_min[i-1] = lm78_read_value(client,LM78_REG_FAN_MIN(i));
    }
    data->temp = lm78_read_value(client,LM78_REG_TEMP);
    data->temp_over = lm78_read_value(client,LM78_REG_TEMP_OVER);
    data->temp_hyst = lm78_read_value(client,LM78_REG_TEMP_HYST);
    i = lm78_read_value(client,LM78_REG_VID_FANDIV);
    data->vid = i & 0x0f;
    data->fan_div[0] = (i >> 4) & 0x03;
    data->fan_div[1] = i >> 6;
    data->alarms = lm78_read_value(client,LM78_REG_ALARM1) +
                   (lm78_read_value(client,LM78_REG_ALARM2) >> 8);
    data->last_updated = jiffies;
    data->valid = 1;
  }

  up(&data->update_lock);
}

/* This function is called when /proc/sys/dev/lm78-???/... is accessed */
int lm78_proc (ctl_table *ctl, int write, struct file * filp,
               void *buffer, size_t *lenp)
{
  int nrels,mag;
  long results[7];
  struct i2c_client *client = ctl -> extra1;

  /* If buffer is size 0, or we try to read when not at the start, we 
     return nothing. Note that I think writing when not at the start
     does not work either, but anyway, this is straight from the kernel
     sources. */
  if (!*lenp || (filp->f_pos && !write)) {
    *lenp = 0;
    return 0;
  }

  /* How many numbers are found within these files, and how to scale them? */
  switch (ctl->ctl_name) {
    case LM78_SYSCTL_IN0: case LM78_SYSCTL_IN1: case LM78_SYSCTL_IN2: 
    case LM78_SYSCTL_IN3: case LM78_SYSCTL_IN4: case LM78_SYSCTL_IN5:
    case LM78_SYSCTL_IN6:
      nrels=3;
      mag=2;
      break;
    case LM78_SYSCTL_TEMP:
      nrels=3;
      mag=0;
      break;
    case LM78_SYSCTL_FAN1: case LM78_SYSCTL_FAN2: case LM78_SYSCTL_FAN3:
    case LM78_SYSCTL_FAN_DIV:
      nrels=2;
      mag=0;
      break;
    case LM78_SYSCTL_VID:
      nrels=1;
      mag=2;
      break;
    case LM78_SYSCTL_ALARMS:
      nrels=1;
      mag=0;
      break;
    default: /* Should never be called */
      return -EINVAL;
  }
 
  /* OK, try writing stuff. */
  if (write) {
    sensors_parse_reals(&nrels,buffer,*lenp,results,mag);
    if (nrels == 0)
      return 0;
    switch (ctl->ctl_name) {
      case LM78_SYSCTL_IN0: write_in(client,0,nrels,results); break;
      case LM78_SYSCTL_IN1: write_in(client,1,nrels,results); break;
      case LM78_SYSCTL_IN2: write_in(client,2,nrels,results); break;
      case LM78_SYSCTL_IN3: write_in(client,3,nrels,results); break;
      case LM78_SYSCTL_IN4: write_in(client,4,nrels,results); break;
      case LM78_SYSCTL_IN5: write_in(client,5,nrels,results); break;
      case LM78_SYSCTL_IN6: write_in(client,6,nrels,results); break;
      case LM78_SYSCTL_FAN1: write_fan(client,1,nrels,results); break;
      case LM78_SYSCTL_FAN2: write_fan(client,2,nrels,results); break;
      case LM78_SYSCTL_FAN3: write_fan(client,3,nrels,results); break;
      case LM78_SYSCTL_FAN_DIV: write_fan_div(client,nrels,results);break;
      case LM78_SYSCTL_TEMP: write_temp(client,nrels,results);break;
      case LM78_SYSCTL_VID: case LM78_SYSCTL_ALARMS: break;
      default: /* Should never be called */ *lenp=0; return -EINVAL; break;
    }
    filp->f_pos += *lenp;
    return 0;
  } else { /* read */
    /* Update all values in LM_Sensor_Data */

    lm78_update_client((struct i2c_client *) (ctl->extra1));

    /* Read the values to print into results */
    switch (ctl->ctl_name) {
      case LM78_SYSCTL_IN0: read_in(client,0,results);break;
      case LM78_SYSCTL_IN1: read_in(client,1,results);break;
      case LM78_SYSCTL_IN2: read_in(client,2,results);break;
      case LM78_SYSCTL_IN3: read_in(client,3,results);break;
      case LM78_SYSCTL_IN4: read_in(client,4,results);break;
      case LM78_SYSCTL_IN5: read_in(client,5,results);break;
      case LM78_SYSCTL_IN6: read_in(client,6,results);break;
      case LM78_SYSCTL_FAN1: read_fan(client,1,results);break;
      case LM78_SYSCTL_FAN2: read_fan(client,2,results);break;
      case LM78_SYSCTL_FAN3: read_fan(client,3,results);break;
      case LM78_SYSCTL_TEMP: read_temp(client,results);break;
      case LM78_SYSCTL_FAN_DIV: read_fan_div(client,results);break;
      case LM78_SYSCTL_VID: read_vid(client,results);break;
      case LM78_SYSCTL_ALARMS: read_alarms(client,results);break;
      default: /* Should never be called */ return -EINVAL;
    }
    /* OK, print it now */
    sensors_write_reals(nrels,buffer,lenp,results,mag);
    filp->f_pos += *lenp;
    return 0;
  }
}

/* This function is called when a sysctl on a lm78 file is done */
int lm78_sysctl (ctl_table *table, int *name, int nlen, void *oldval, 
               size_t *oldlenp, void *newval, size_t newlen,
               void **context)
{
  long results[7];
  int nrels,oldlen;
  struct i2c_client *client = table -> extra1;
 
  /* How many numbers are found within these files, and how to scale them? */
  switch (table->ctl_name) {
    case LM78_SYSCTL_IN0: case LM78_SYSCTL_IN1: case LM78_SYSCTL_IN2: 
    case LM78_SYSCTL_IN3: case LM78_SYSCTL_IN4: case LM78_SYSCTL_IN5: 
    case LM78_SYSCTL_IN6: case LM78_SYSCTL_TEMP: 
      nrels=3;
      break;
    case LM78_SYSCTL_FAN1: case LM78_SYSCTL_FAN2: case LM78_SYSCTL_FAN3:
    case LM78_SYSCTL_FAN_DIV:
      nrels=2;
      break;
    case LM78_SYSCTL_VID: case LM78_SYSCTL_ALARMS:
      nrels=1;
      break;
    default: /* Should never be called */
      return -EINVAL;
  }

  /* Check if we need to output the old values */
  if (oldval && oldlenp && ! get_user_data(oldlen,oldlenp) && oldlen) {

    /* Update all values in LM_Sensor_Data */
    lm78_update_client((struct i2c_client *) (table->extra1));
    switch (table->ctl_name) {
      case LM78_SYSCTL_IN0: read_in(client,0,results);break;
      case LM78_SYSCTL_IN1: read_in(client,1,results);break;
      case LM78_SYSCTL_IN2: read_in(client,2,results);break;
      case LM78_SYSCTL_IN3: read_in(client,3,results);break;
      case LM78_SYSCTL_IN4: read_in(client,4,results);break;
      case LM78_SYSCTL_IN5: read_in(client,5,results);break;
      case LM78_SYSCTL_IN6: read_in(client,6,results);break;
      case LM78_SYSCTL_FAN1: read_fan(client,1,results);break;
      case LM78_SYSCTL_FAN2: read_fan(client,2,results);break;
      case LM78_SYSCTL_FAN3: read_fan(client,3,results);break;
      case LM78_SYSCTL_TEMP: read_temp(client,results);break;
      case LM78_SYSCTL_FAN_DIV: read_fan_div(client,results);break;
      case LM78_SYSCTL_VID: read_vid(client,results);break;
      case LM78_SYSCTL_ALARMS: read_alarms(client,results);break;
      default: /* Should never be called */ return -EINVAL;
    }
    
    /* Note the rounding factor! */
    if (nrels * sizeof(long) < oldlen)
      oldlen = nrels * sizeof(long);
    oldlen = (oldlen / sizeof(long)) * sizeof(long);
    copy_to_user(oldval,results,oldlen);
    put_user(oldlen,oldlenp);
  }

  /* Check to see whether we need to read the new values */
  if (newval && newlen) {
    if (nrels * sizeof(long) < newlen)
      newlen = nrels * sizeof(long);
    nrels = newlen / sizeof(long);
    newlen = (newlen / sizeof(long)) * sizeof(long);
    copy_from_user(results,newval,newlen);
    
    switch (table->ctl_name) {
      case LM78_SYSCTL_IN0: write_in(client,0,nrels,results); break;
      case LM78_SYSCTL_IN1: write_in(client,1,nrels,results); break;
      case LM78_SYSCTL_IN2: write_in(client,2,nrels,results); break;
      case LM78_SYSCTL_IN3: write_in(client,3,nrels,results); break;
      case LM78_SYSCTL_IN4: write_in(client,4,nrels,results); break;
      case LM78_SYSCTL_IN5: write_in(client,5,nrels,results); break;
      case LM78_SYSCTL_IN6: write_in(client,6,nrels,results); break;
      case LM78_SYSCTL_FAN1: write_fan(client,1,nrels,results); break;
      case LM78_SYSCTL_FAN2: write_fan(client,2,nrels,results); break;
      case LM78_SYSCTL_FAN3: write_fan(client,3,nrels,results); break;
      case LM78_SYSCTL_TEMP: write_temp(client,nrels,results); break;
      case LM78_SYSCTL_FAN_DIV: write_fan_div(client,nrels,results);break;
      case LM78_SYSCTL_VID: case LM78_SYSCTL_ALARMS: break;
      default: /* Should never be called */ return -EINVAL; break;
    }
  }
  return 1; /* We have done all the work */
}

void write_in(struct i2c_client *client, int nr, int nrels, long *results)
{
  struct lm78_data *data = client->data;
  if (nrels >= 1) {
    data->in_min[nr] = IN_TO_REG(results[0],nr);
    lm78_write_value(client,LM78_REG_IN_MIN(nr),data->in_min[nr]);
  }
  if (nrels >= 2) {
    data->in_max[nr] = IN_TO_REG(results[1],nr);
    lm78_write_value(client,LM78_REG_IN_MAX(nr),data->in_max[nr]);
  }
}

void read_in(struct i2c_client *client, int nr, long *results)
{
  struct lm78_data *data = client->data;
  results[0] = IN_FROM_REG(data->in_min[nr],nr);
  results[1] = IN_FROM_REG(data->in_max[nr],nr);
  results[2] = IN_FROM_REG(data->in[nr],nr);
}

void write_fan(struct i2c_client *client, int nr, int nrels, long *results)
{
  struct lm78_data *data = client->data;
  if (nrels >= 1) {
    data->fan_min[nr-1] = FAN_TO_REG(results[0]);
    lm78_write_value(client,LM78_REG_FAN_MIN(nr),data->fan_min[nr-1]);
  }
}

void read_fan(struct i2c_client *client, int nr, long *results)
{
  struct lm78_data *data = client->data;
  results[0] = FAN_FROM_REG(data->fan_min[nr-1]);
  results[1] = FAN_FROM_REG(data->fan[nr-1]);
}

void write_temp(struct i2c_client *client, int nrels, long *results)
{
  struct lm78_data *data = client->data;
  if (nrels >= 1) {
    data->temp_over = TEMP_TO_REG(results[0]);
    lm78_write_value(client,LM78_REG_TEMP_OVER,data->temp_over);
  }
  if (nrels >= 2) {
    data->temp_hyst = TEMP_TO_REG(results[0]);
    lm78_write_value(client,LM78_REG_TEMP_HYST,data->temp_hyst);
  }
}

void read_temp(struct i2c_client *client, long *results)
{
  struct lm78_data *data = client->data;
  results[0] = TEMP_FROM_REG(data->temp_over);
  results[1] = TEMP_FROM_REG(data->temp_hyst);
  results[2] = TEMP_FROM_REG(data->temp);
}

void read_vid(struct i2c_client *client, long *results)
{
  struct lm78_data *data = client->data;
  results[0] = VID_FROM_REG(data->vid);
}

void read_alarms(struct i2c_client *client, long *results)
{
  struct lm78_data *data = client->data;
  results[0] = ALARMS_FROM_REG(data->alarms);
}

void read_fan_div(struct i2c_client *client, long *results)
{
  struct lm78_data *data = client->data;
  results[0] = DIV_FROM_REG(data->fan_div[0]);
  results[1] = DIV_FROM_REG(data->fan_div[1]);
}
 
void write_fan_div(struct i2c_client *client, int nrels, long *results)
{
  struct lm78_data *data = client->data;
  if (nrels >= 2)
    data->fan_div[1] = DIV_TO_REG(results[1]);
  if (nrels >= 1) {
    data->fan_div[0] = DIV_TO_REG(results[0]);
    lm78_write_value(client,LM78_REG_VID_FANDIV,
                     (data->fan_div[0] >> 4) | (data->fan_div[1] >> 6));
  }
}

int lm78_init(void)
{
  int res;

  printk("lm78.o version %s (%s)\n",LM_VERSION,LM_DATE);
  lm78_initialized = 0;

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

  if (lm78_initialized >= 1) {
    if ((res = i2c_del_driver(&lm78_driver))) {
      printk("lm78.o: Driver deregistration failed, module not removed.\n");
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

