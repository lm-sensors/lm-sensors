/*
    lm80.c - Part of lm_sensors, Linux kernel modules for hardware
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
#include "i2c-isa.h"
#include "sensors.h"
#include "i2c.h"
#include "compat.h"

/* Many LM80 constants specified below */

/* The LM80 registers */
#define LM80_REG_IN_MAX(nr) (0x2a + (nr) * 2)
#define LM80_REG_IN_MIN(nr) (0x2b + (nr) * 2)
#define LM80_REG_IN(nr) (0x20 + (nr))

#define LM80_REG_FAN1_MIN 0x3c
#define LM80_REG_FAN2_MIN 0x3d
#define LM80_REG_FAN1 0x28
#define LM80_REG_FAN2 0x29

#define LM80_REG_TEMP 0x27
#define LM80_REG_TEMP_HOT_MAX 0x38
#define LM80_REG_TEMP_HOT_HYST 0x39
#define LM80_REG_TEMP_OS_MAX 0x3a
#define LM80_REG_TEMP_OS_HYST 0x3b

#define LM80_REG_CONFIG 0x00
#define LM80_REG_ALARM1 0x01
#define LM80_REG_ALARM2 0x02
#define LM80_REG_MASK1 0x03
#define LM80_REG_MASK2 0x04
#define LM80_REG_FANDIV 0x05
#define LM80_REG_RES 0x06


/* Conversions. Rounding is only done on the TO_REG variants. */
#define IN_TO_REG(val,nr) ((val) & 0xff)
#define IN_FROM_REG(val,nr) (val)

#define FAN_TO_REG(val,div) ((val)==0?255:\
                             ((1350000+(val)*(div)/2)/((val)*(div))) & 0xff)
#define FAN_FROM_REG(val,div) ((val)==0?-1:\
                               (val)==255?0:1350000/((div)*(val)))

#define TEMP_FROM_REG(val) lm80_temp_from_reg(val)

#define TEMP_LIMIT_TO_REG(val) (((val)<0?(((val)-50)/100)&0xff:\
                                           ((val)+50)/100) & 0xff)
#define TEMP_LIMIT_FROM_REG(val) (((val)>0x80?(val)-0x100:(val))*100)

#define ALARMS_FROM_REG(val) (val) 

#define DIV_FROM_REG(val) (1 << (val))
#define DIV_TO_REG(val) ((val)==8?3:(val)==4?2:(val)==1?0:1)

/* Initial limits */
#define LM80_INIT_IN_0 190
#define LM80_INIT_IN_1 190
#define LM80_INIT_IN_2 190
#define LM80_INIT_IN_3 190
#define LM80_INIT_IN_4 190
#define LM80_INIT_IN_5 190
#define LM80_INIT_IN_6 190

#define LM80_INIT_IN_PERCENTAGE 10

#define LM80_INIT_IN_MIN_0 \
        (LM80_INIT_IN_0 - LM80_INIT_IN_0 * LM80_INIT_IN_PERCENTAGE / 100) 
#define LM80_INIT_IN_MAX_0 \
        (LM80_INIT_IN_0 + LM80_INIT_IN_0 * LM80_INIT_IN_PERCENTAGE / 100) 
#define LM80_INIT_IN_MIN_1 \
        (LM80_INIT_IN_1 - LM80_INIT_IN_1 * LM80_INIT_IN_PERCENTAGE / 100) 
#define LM80_INIT_IN_MAX_1 \
        (LM80_INIT_IN_1 + LM80_INIT_IN_1 * LM80_INIT_IN_PERCENTAGE / 100) 
#define LM80_INIT_IN_MIN_2 \
        (LM80_INIT_IN_2 - LM80_INIT_IN_2 * LM80_INIT_IN_PERCENTAGE / 100) 
#define LM80_INIT_IN_MAX_2 \
        (LM80_INIT_IN_2 + LM80_INIT_IN_2 * LM80_INIT_IN_PERCENTAGE / 100) 
#define LM80_INIT_IN_MIN_3 \
        (LM80_INIT_IN_3 - LM80_INIT_IN_3 * LM80_INIT_IN_PERCENTAGE / 100) 
#define LM80_INIT_IN_MAX_3 \
        (LM80_INIT_IN_3 + LM80_INIT_IN_3 * LM80_INIT_IN_PERCENTAGE / 100) 
#define LM80_INIT_IN_MIN_4 \
        (LM80_INIT_IN_4 - LM80_INIT_IN_4 * LM80_INIT_IN_PERCENTAGE / 100) 
#define LM80_INIT_IN_MAX_4 \
        (LM80_INIT_IN_4 + LM80_INIT_IN_4 * LM80_INIT_IN_PERCENTAGE / 100) 
#define LM80_INIT_IN_MIN_5 \
        (LM80_INIT_IN_5 - LM80_INIT_IN_5 * LM80_INIT_IN_PERCENTAGE / 100) 
#define LM80_INIT_IN_MAX_5 \
        (LM80_INIT_IN_5 + LM80_INIT_IN_5 * LM80_INIT_IN_PERCENTAGE / 100) 
#define LM80_INIT_IN_MIN_6 \
        (LM80_INIT_IN_6 - LM80_INIT_IN_6 * LM80_INIT_IN_PERCENTAGE / 100) 
#define LM80_INIT_IN_MAX_6 \
        (LM80_INIT_IN_6 + LM80_INIT_IN_6 * LM80_INIT_IN_PERCENTAGE / 100) 

#define LM80_INIT_FAN_MIN_1 3000
#define LM80_INIT_FAN_MIN_2 3000

#define LM80_INIT_TEMP_OS_MAX 600
#define LM80_INIT_TEMP_OS_HYST 500
#define LM80_INIT_TEMP_HOT_MAX 700
#define LM80_INIT_TEMP_HOT_HYST 600

#ifdef MODULE
extern int init_module(void);
extern int cleanup_module(void);
#endif /* MODULE */

/* For each registered LM80, we need to keep some data in memory. That
   data is pointed to by lm80_list[NR]->data. The structure itself is
   dynamically allocated, at the same time when a new lm80 client is
   allocated. */
struct lm80_data {
         int sysctl_id;

         struct semaphore update_lock;
         char valid;                 /* !=0 if following fields are valid */
         unsigned long last_updated; /* In jiffies */

         u8 in[7];                   /* Register value */
         u8 in_max[7];               /* Register value */
         u8 in_min[7];               /* Register value */
         u8 fan[2];                  /* Register value */
         u8 fan_min[2];              /* Register value */
         u8 fan_div[2];              /* Register encoding, shifted right */
         u16 temp;                   /* Register values, shifted right */
         u8 temp_hot_max;            /* Register value */
         u8 temp_hot_hyst;           /* Register value */
         u8 temp_os_max;             /* Register value */
         u8 temp_os_hyst;            /* Register value */
         u16 alarms;                 /* Register encoding, combined */
};


static int lm80_init(void);
static int lm80_cleanup(void);

static int lm80_attach_adapter(struct i2c_adapter *adapter);
static int lm80_detach_client(struct i2c_client *client);
static int lm80_new_client(struct i2c_adapter *adapter,
                           struct i2c_client *new_client);
static void lm80_remove_client(struct i2c_client *client);
static int lm80_command(struct i2c_client *client, unsigned int cmd, 
                        void *arg);
static void lm80_inc_use (struct i2c_client *client);
static void lm80_dec_use (struct i2c_client *client);

static long lm80_temp_from_reg(u16 regs);

static int lm80_read_value(struct i2c_client *client, u8 register);
static int lm80_write_value(struct i2c_client *client, u8 register, u8 value);
static void lm80_update_client(struct i2c_client *client);
static void lm80_init_client(struct i2c_client *client);


static void lm80_in(struct i2c_client *client, int operation, int ctl_name,
                    int *nrels_mag, long *results);
static void lm80_fan(struct i2c_client *client, int operation, int ctl_name,
                     int *nrels_mag, long *results);
static void lm80_temp(struct i2c_client *client, int operation, int ctl_name,
                      int *nrels_mag, long *results);
static void lm80_alarms(struct i2c_client *client, int operation, int ctl_name,
                        int *nrels_mag, long *results);
static void lm80_fan_div(struct i2c_client *client, int operation, int ctl_name,
                         int *nrels_mag, long *results);

/* I choose here for semi-static LM80 allocation. Complete dynamic
   allocation could also be used; the code needed for this would probably
   take more memory than the datastructure takes now. */
#define MAX_LM80_NR 4
static struct i2c_client *lm80_list[MAX_LM80_NR];

/* The driver. I choose to use type i2c_driver, as at is identical to both
   smbus_driver and isa_driver, and clients could be of either kind */
static struct i2c_driver lm80_driver = {
  /* name */		"LM80 sensor driver",
  /* id */		I2C_DRIVERID_LM80,
  /* flags */		DF_NOTIFY,
  /* attach_adapter */  &lm80_attach_adapter,
  /* detach_client */	&lm80_detach_client,
  /* command */		&lm80_command,
  /* inc_use */		&lm80_inc_use,
  /* dec_use */		&lm80_dec_use
};

/* Used by lm80_init/cleanup */
static int lm80_initialized = 0;

/* The /proc/sys entries */
/* These files are created for each detected LM80. This is just a template;
   though at first sight, you might think we could use a statically
   allocated list, we need some way to get back to the parent - which
   is done through one of the 'extra' fields which are initialized 
   when a new copy is allocated. */
static ctl_table lm80_dir_table_template[] = {
  { LM80_SYSCTL_IN0, "in0", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &lm80_in },
  { LM80_SYSCTL_IN1, "in1", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &lm80_in },
  { LM80_SYSCTL_IN2, "in2", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &lm80_in },
  { LM80_SYSCTL_IN3, "in3", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &lm80_in },
  { LM80_SYSCTL_IN4, "in4", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &lm80_in },
  { LM80_SYSCTL_IN5, "in5", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &lm80_in },
  { LM80_SYSCTL_IN6, "in6", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &lm80_in },
  { LM80_SYSCTL_FAN1, "fan1", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &lm80_fan },
  { LM80_SYSCTL_FAN2, "fan2", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &lm80_fan },
  { LM80_SYSCTL_TEMP, "temp", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &lm80_temp },
  { LM80_SYSCTL_FAN_DIV, "fan_div", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &lm80_fan_div },
  { LM80_SYSCTL_ALARMS, "alarms", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &lm80_alarms },
  { 0 }
};


int lm80_attach_adapter(struct i2c_adapter *adapter)
{
  int address,err;
  struct i2c_client *new_client;
  const char *type_name,*client_name;

  /* OK, this is no detection. I know. It will do for now, though.  */
  err = 0;
  
  /* Make sure we aren't probing the ISA bus!! */
  if (i2c_is_isa_adapter(adapter)) return 0;
  
  for (address = 0x20; (! err) && (address <= 0x2f); address ++) {

    /* Later on, we will keep a list of registered addresses for each
       adapter, and check whether they are used here */

    if (smbus_read_byte_data(adapter,address,LM80_REG_CONFIG) < 0) 
      continue;

    /* Real detection code goes here */

    printk("lm80.o: LM80 detected\n");
    type_name = "lm80";
    client_name = "LM80 chip";


    /* Allocate space for a new client structure. To counter memory
       fragmentation somewhat, we only do one kmalloc. */
    if (! (new_client = kmalloc(sizeof(struct i2c_client) + 
                                sizeof(struct lm80_data),
                               GFP_KERNEL))) {
      err = -ENOMEM;
      continue;
    }

    /* Fill the new client structure with data */
    new_client->data = (struct lm80_data *) (new_client + 1);
    new_client->addr = address;
    strcpy(new_client->name,client_name);
    if ((err = lm80_new_client(adapter,new_client)))
      goto ERROR2;

    /* Tell i2c-core a new client has arrived */
    if ((err = i2c_attach_client(new_client))) 
      goto ERROR3;

    /* Register a new directory entry with module sensors */
    if ((err = sensors_register_entry(new_client,type_name,
                                      lm80_dir_table_template)) < 0)
      goto ERROR4;
    ((struct lm80_data *) (new_client->data))->sysctl_id = err;
    err = 0;

    /* Initialize the LM80 chip */
    lm80_init_client(new_client);
    continue;

/* OK, this is not exactly good programming practice, usually. But it is
   very code-efficient in this case. */
ERROR4:
    i2c_detach_client(new_client);
ERROR3:
    lm80_remove_client((struct i2c_client *) new_client);
ERROR2:
    kfree(new_client);
  }
  return err;
}

int lm80_detach_client(struct i2c_client *client)
{
  int err,i;
  for (i = 0; i < MAX_LM80_NR; i++)
    if (client == lm80_list[i])
      break;
  if ((i == MAX_LM80_NR)) {
    printk("lm80.o: Client to detach not found.\n");
    return -ENOENT;
  }

  sensors_deregister_entry(((struct lm80_data *)(client->data))->sysctl_id);

  if ((err = i2c_detach_client(client))) {
    printk("lm80.o: Client deregistration failed, client not detached.\n");
    return err;
  }
  lm80_remove_client(client);
  kfree(client);
  return 0;
}


/* Find a free slot, and initialize most of the fields */
int lm80_new_client(struct i2c_adapter *adapter,
                    struct i2c_client *new_client)
{
  int i;
  struct lm80_data *data;

  /* First, seek out an empty slot */
  for(i = 0; i < MAX_LM80_NR; i++)
    if (! lm80_list[i])
      break;
  if (i == MAX_LM80_NR) {
    printk("lm80.o: No empty slots left, recompile and heighten "
           "MAX_LM80_NR!\n");
    return -ENOMEM;
  }
  
  lm80_list[i] = new_client;
  new_client->id = i;
  new_client->adapter = adapter;
  new_client->driver = &lm80_driver;
  data = new_client->data;
  data->valid = 0;
  data->update_lock = MUTEX;
  return 0;
}

/* Inverse of lm80_new_client */
void lm80_remove_client(struct i2c_client *client)
{
  int i;
  for (i = 0; i < MAX_LM80_NR; i++)
    if (client == lm80_list[i]) 
      lm80_list[i] = NULL;
}

/* No commands defined yet */
int lm80_command(struct i2c_client *client, unsigned int cmd, void *arg)
{
  return 0;
}

void lm80_inc_use (struct i2c_client *client)
{
#ifdef MODULE
  MOD_INC_USE_COUNT;
#endif
}

void lm80_dec_use (struct i2c_client *client)
{
#ifdef MODULE
  MOD_DEC_USE_COUNT;
#endif
}
 

long lm80_temp_from_reg(u16 temp)
{
  long res;
  res = ((temp & 0xff00) >> 8) * 10000 + ((temp & 0x10) >> 4) * 5000 +
        ((temp & 0x20) >> 5) * 2500 + ((temp & 0x40) >> 6) * 1250 +
        ((temp & 0x80) >> 7) * 625;
  temp /= 100;
  if (temp >= 0x80 * 100)
    temp = - (0x100 * 100 - temp);
  return temp;
}

int lm80_read_value(struct i2c_client *client, u8 reg)
{
  return smbus_read_byte_data(client->adapter,client->addr, reg);
}

int lm80_write_value(struct i2c_client *client, u8 reg, u8 value)
{
  return smbus_write_byte_data(client->adapter, client->addr, reg,value);
}

/* Called when we have found a new LM80. It should set limits, etc. */
void lm80_init_client(struct i2c_client *client)
{
  /* Reset all except Watchdog values and last conversion values
     This sets fan-divs to 2, among others. This makes most other
     initializations unnecessary */
  lm80_write_value(client,LM80_REG_CONFIG,0x80);
  /* Set 11-bit temperature resolution */
  lm80_write_value(client,LM80_REG_RES,0x08);

  lm80_write_value(client,LM80_REG_IN_MIN(0),IN_TO_REG(LM80_INIT_IN_MIN_0,0));
  lm80_write_value(client,LM80_REG_IN_MAX(0),IN_TO_REG(LM80_INIT_IN_MAX_0,0));
  lm80_write_value(client,LM80_REG_IN_MIN(1),IN_TO_REG(LM80_INIT_IN_MIN_1,1));
  lm80_write_value(client,LM80_REG_IN_MAX(1),IN_TO_REG(LM80_INIT_IN_MAX_1,1));
  lm80_write_value(client,LM80_REG_IN_MIN(2),IN_TO_REG(LM80_INIT_IN_MIN_2,2));
  lm80_write_value(client,LM80_REG_IN_MAX(2),IN_TO_REG(LM80_INIT_IN_MAX_2,2));
  lm80_write_value(client,LM80_REG_IN_MIN(3),IN_TO_REG(LM80_INIT_IN_MIN_3,3));
  lm80_write_value(client,LM80_REG_IN_MAX(3),IN_TO_REG(LM80_INIT_IN_MAX_3,3));
  lm80_write_value(client,LM80_REG_IN_MIN(4),IN_TO_REG(LM80_INIT_IN_MIN_4,4));
  lm80_write_value(client,LM80_REG_IN_MAX(4),IN_TO_REG(LM80_INIT_IN_MAX_4,4));
  lm80_write_value(client,LM80_REG_IN_MIN(5),IN_TO_REG(LM80_INIT_IN_MIN_5,5));
  lm80_write_value(client,LM80_REG_IN_MAX(5),IN_TO_REG(LM80_INIT_IN_MAX_5,5));
  lm80_write_value(client,LM80_REG_IN_MIN(6),IN_TO_REG(LM80_INIT_IN_MIN_6,6));
  lm80_write_value(client,LM80_REG_IN_MAX(6),IN_TO_REG(LM80_INIT_IN_MAX_6,6));
  lm80_write_value(client,LM80_REG_FAN1_MIN,FAN_TO_REG(LM80_INIT_FAN_MIN_1,2));
  lm80_write_value(client,LM80_REG_FAN2_MIN,FAN_TO_REG(LM80_INIT_FAN_MIN_2,2));
  lm80_write_value(client,LM80_REG_TEMP_HOT_MAX,
                   TEMP_LIMIT_TO_REG(LM80_INIT_TEMP_OS_MAX));
  lm80_write_value(client,LM80_REG_TEMP_HOT_HYST,
                   TEMP_LIMIT_TO_REG(LM80_INIT_TEMP_OS_HYST));
  lm80_write_value(client,LM80_REG_TEMP_OS_MAX,
                   TEMP_LIMIT_TO_REG(LM80_INIT_TEMP_OS_MAX));
  lm80_write_value(client,LM80_REG_TEMP_OS_HYST,
                   TEMP_LIMIT_TO_REG(LM80_INIT_TEMP_OS_HYST));

  /* Start monitoring */
  lm80_write_value(client,LM80_REG_CONFIG,0x01);
}

void lm80_update_client(struct i2c_client *client)
{
  struct lm80_data *data = client->data;
  int i;

  down(&data->update_lock);

  if ((jiffies - data->last_updated > 2*HZ ) ||
      (jiffies < data->last_updated) || ! data->valid) {

#ifdef DEBUG
    printk("Starting lm80 update\n");
#endif
    for (i = 0; i <= 6; i++) {
      data->in[i]     = lm80_read_value(client,LM80_REG_IN(i));
      data->in_min[i] = lm80_read_value(client,LM80_REG_IN_MIN(i));
      data->in_max[i] = lm80_read_value(client,LM80_REG_IN_MAX(i));
    }
    data->fan[0] = lm80_read_value(client,LM80_REG_FAN1);
    data->fan_min[0] = lm80_read_value(client,LM80_REG_FAN1_MIN);
    data->fan[1] = lm80_read_value(client,LM80_REG_FAN2);
    data->fan_min[1] = lm80_read_value(client,LM80_REG_FAN2_MIN);
   
    data->temp = (lm80_read_value(client,LM80_REG_TEMP) << 8) |
                 (lm80_read_value(client,LM80_REG_RES) & 0xf0);
    data->temp_os_max = lm80_read_value(client,LM80_REG_TEMP_OS_MAX);
    data->temp_os_hyst = lm80_read_value(client,LM80_REG_TEMP_OS_HYST);
    data->temp_hot_max = lm80_read_value(client,LM80_REG_TEMP_HOT_MAX);
    data->temp_hot_hyst = lm80_read_value(client,LM80_REG_TEMP_HOT_HYST);

    i = lm80_read_value(client,LM80_REG_FANDIV);
    data->fan_div[0] = (i >> 2) & 0x03;
    data->fan_div[1] = (i >> 4) & 0x03;
    data->alarms = lm80_read_value(client,LM80_REG_ALARM1) +
                   (lm80_read_value(client,LM80_REG_ALARM2) << 8);
    data->last_updated = jiffies;
    data->valid = 1;
  }

  up(&data->update_lock);
}


/* The next few functions are the call-back functions of the /proc/sys and
   sysctl files. Which function is used is defined in the ctl_table in
   the extra1 field.
   Each function must return the magnitude (power of 10 to divide the date
   with) if it is called with operation==SENSORS_PROC_REAL_INFO. It must
   put a maximum of *nrels elements in results reflecting the data of this
   file, and set *nrels to the number it actually put in it, if operation==
   SENSORS_PROC_REAL_READ. Finally, it must get upto *nrels elements from
   results and write them to the chip, if operations==SENSORS_PROC_REAL_WRITE.
   Note that on SENSORS_PROC_REAL_READ, I do not check whether results is
   large enough (by checking the incoming value of *nrels). This is not very
   good practice, but as long as you put less than about 5 values in results,
   you can assume it is large enough. */
void lm80_in(struct i2c_client *client, int operation, int ctl_name, 
             int *nrels_mag, long *results)
{
  struct lm80_data *data = client->data;
  int nr = ctl_name - LM80_SYSCTL_IN0;

  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 2;
  else if (operation == SENSORS_PROC_REAL_READ) {
    lm80_update_client(client);
    results[0] = IN_FROM_REG(data->in_min[nr],nr);
    results[1] = IN_FROM_REG(data->in_max[nr],nr);
    results[2] = IN_FROM_REG(data->in[nr],nr);
    *nrels_mag = 3;
  } else if (operation == SENSORS_PROC_REAL_WRITE) {
      if (*nrels_mag >= 1) {
        data->in_min[nr] = IN_TO_REG(results[0],nr);
        lm80_write_value(client,LM80_REG_IN_MIN(nr),data->in_min[nr]);
      }
      if (*nrels_mag >= 2) {
        data->in_max[nr] = IN_TO_REG(results[1],nr);
        lm80_write_value(client,LM80_REG_IN_MAX(nr),data->in_max[nr]);
      }
  }
}

void lm80_fan(struct i2c_client *client, int operation, int ctl_name,
              int *nrels_mag, long *results)
{
  struct lm80_data *data = client->data;
  int nr = ctl_name - LM80_SYSCTL_FAN1 + 1;

  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 0;
  else if (operation == SENSORS_PROC_REAL_READ) {
    lm80_update_client(client);
    results[0] = FAN_FROM_REG(data->fan_min[nr-1],
                              DIV_FROM_REG(data->fan_div[nr-1]));
    results[1] = FAN_FROM_REG(data->fan[nr-1],
                              DIV_FROM_REG(data->fan_div[nr-1]));
    *nrels_mag = 2;
  } else if (operation == SENSORS_PROC_REAL_WRITE) {
    if (*nrels_mag >= 1) {
      data->fan_min[nr-1] = FAN_TO_REG(results[0],
                            DIV_FROM_REG(data->fan_div[nr-1]));
      lm80_write_value(client,nr==1?LM80_REG_FAN1_MIN:LM80_REG_FAN2_MIN,
                       data->fan_min[nr-1]);
    }
  }
}


void lm80_temp(struct i2c_client *client, int operation, int ctl_name,
               int *nrels_mag, long *results)
{
  struct lm80_data *data = client->data;
  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 1;
  else if (operation == SENSORS_PROC_REAL_READ) {
    lm80_update_client(client);
    results[0] = TEMP_LIMIT_FROM_REG(data->temp_hot_max);
    results[1] = TEMP_LIMIT_FROM_REG(data->temp_hot_hyst);
    results[2] = TEMP_LIMIT_FROM_REG(data->temp_os_max);
    results[3] = TEMP_LIMIT_FROM_REG(data->temp_os_hyst);
    results[4] = TEMP_FROM_REG(data->temp);
    *nrels_mag = 5;
  } else if (operation == SENSORS_PROC_REAL_WRITE) {
    if (*nrels_mag >= 1) {
      data->temp_hot_max = TEMP_LIMIT_TO_REG(results[0]);
      lm80_write_value(client,LM80_REG_TEMP_HOT_MAX,data->temp_hot_max);
    }
    if (*nrels_mag >= 2) {
      data->temp_hot_hyst = TEMP_LIMIT_TO_REG(results[1]);
      lm80_write_value(client,LM80_REG_TEMP_HOT_HYST,data->temp_hot_hyst);
    }
    if (*nrels_mag >= 3) {
      data->temp_os_max = TEMP_LIMIT_TO_REG(results[2]);
      lm80_write_value(client,LM80_REG_TEMP_OS_MAX,data->temp_os_max);
    }
    if (*nrels_mag >= 4) {
      data->temp_os_hyst = TEMP_LIMIT_TO_REG(results[3]);
      lm80_write_value(client,LM80_REG_TEMP_OS_HYST,data->temp_os_hyst);
    }
  }
}

void lm80_alarms(struct i2c_client *client, int operation, int ctl_name,
                 int *nrels_mag, long *results)
{
  struct lm80_data *data = client->data;
  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 0;
  else if (operation == SENSORS_PROC_REAL_READ) {
    lm80_update_client(client);
    results[0] = ALARMS_FROM_REG(data->alarms);
    *nrels_mag = 1;
  }
}

void lm80_fan_div(struct i2c_client *client, int operation, int ctl_name,
                  int *nrels_mag, long *results)
{
  struct lm80_data *data = client->data;
  int old;

  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 0;
  else if (operation == SENSORS_PROC_REAL_READ) {
    lm80_update_client(client);
    results[0] = DIV_FROM_REG(data->fan_div[0]);
    results[1] = DIV_FROM_REG(data->fan_div[1]);
    results[2] = 2;
    *nrels_mag = 3;
  } else if (operation == SENSORS_PROC_REAL_WRITE) {
    old = lm80_read_value(client,LM80_REG_FANDIV);
    if (*nrels_mag >= 2) {
      data->fan_div[1] = DIV_TO_REG(results[1]);
      old = (old & 0xcf) | (data->fan_div[1] << 4);
    }
    if (*nrels_mag >= 1) {
      data->fan_div[0] = DIV_TO_REG(results[0]);
      old = (old & 0xf3) | (data->fan_div[0] << 2);
      lm80_write_value(client,LM80_REG_FANDIV,old);
    }
  }
}

int lm80_init(void)
{
  int res;

  printk("lm80.o version %s (%s)\n",LM_VERSION,LM_DATE);
  lm80_initialized = 0;

  if ((res =i2c_add_driver(&lm80_driver))) {
    printk("lm80.o: Driver registration failed, module not inserted.\n");
    lm80_cleanup();
    return res;
  }
  lm80_initialized ++;
  return 0;
}

int lm80_cleanup(void)
{
  int res;

  if (lm80_initialized >= 1) {
    if ((res = i2c_del_driver(&lm80_driver))) {
      printk("lm80.o: Driver deregistration failed, module not removed.\n");
      return res;
    }
    lm80_initialized --;
  }
  return 0;
}


#ifdef MODULE

MODULE_AUTHOR("Frodo Looijaard <frodol@dds.nl> and Philip Edelbrock <phil@netroedge.com>");
MODULE_DESCRIPTION("LM80 driver");

int init_module(void)
{
  return lm80_init();
}

int cleanup_module(void)
{
  return lm80_cleanup();
}

#endif /* MODULE */

