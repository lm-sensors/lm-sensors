/*
    adm9240.c - Part of lm_sensors, Linux kernel modules for hardware
             monitoring
    Copyright (c) 1999  Frodo Looijaard <frodol@dds.nl>
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

/* 
	A couple notes about the ADM2940:

* It claims to be 'LM7x' register compatible.  This must be in reference
  to only the LM78, because it is missing stuff to emulate LM75's as well. 
  (like the Winbond W83781 does)
 
* This driver was written from rev. 0 of the PDF, but it seems well 
  written and complete (unlike the W83781 which is horrible and has
  supposidly gone through a few revisions.. rev 0 of that one must
  have been in crayon on construction paper...)
  
* All analog inputs can range from 0 to 2.5, eventhough some inputs are
  marked as being 5V, 12V, etc.  I don't have any real voltages going 
  into my prototype, so I'm not sure that things are computed right, 
  but at least the limits seem to be working OK.
  
* Another curiousity is that the fan_div seems to be read-only.  I.e.,
  any written value to it doesn't seem to make any difference.  The
  fan_div seems to be 'stuck' at 2 (which isn't a bad value in most cases).
  
  
  --Phil

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

/* Many ADM9240 constants specified below */

#define ADM9240_REG_IN_MAX(nr) (0x2b + (nr) * 2)
#define ADM9240_REG_IN_MIN(nr) (0x2c + (nr) * 2)
#define ADM9240_REG_IN(nr) (0x20 + (nr))

/* The ADM9240 registers */
#define ADM9240_REG_TEST 0x15
#define ADM9240_REG_ANALOG_OUT 0x19
/* These are all read-only */
#define ADM9240_REG_2_5V 0x20
#define ADM9240_REG_VCCP1 0x21
#define ADM9240_REG_3_3V 0x22
#define ADM9240_REG_5V 0x23
#define ADM9240_REG_12V 0x24
#define ADM9240_REG_VCCP2 0x25
#define ADM9240_REG_TEMP 0x27
#define ADM9240_REG_FAN1 0x28
#define ADM9240_REG_FAN2 0x29
#define ADM9240_REG_COMPANY_ID 0x3E  /* Should always read 0x23 */
#define ADM9240_REG_DIE_REV 0x3F
/* These are read/write */
#define ADM9240_REG_2_5V_HIGH 0x2B
#define ADM9240_REG_2_5V_LOW 0x2C
#define ADM9240_REG_VCCP1_HIGH 0x2D
#define ADM9240_REG_VCCP1_LOW 0x2E
#define ADM9240_REG_3_3V_HIGH 0x2F
#define ADM9240_REG_3_3V_LOW 0x30
#define ADM9240_REG_5V_HIGH 0x31
#define ADM9240_REG_5V_LOW 0x32
#define ADM9240_REG_12V_HIGH 0x33
#define ADM9240_REG_12V_LOW 0x34
#define ADM9240_REG_VCCP2_HIGH 0x35
#define ADM9240_REG_VCCP2_LOW 0x36
#define ADM9240_REG_TOS 0x39
#define ADM9240_REG_THYST 0x3A
#define ADM9240_REG_FAN1_MIN 0x3B
#define ADM9240_REG_FAN2_MIN 0x3C

#define ADM9240_REG_CONFIG 0x40
#define ADM9240_REG_INT1_STAT 0x41
#define ADM9240_REG_INT2_STAT 0x42
#define ADM9240_REG_INT1_MASK 0x43
#define ADM9240_REG_INT2_MASK 0x44

#define ADM9240_REG_COMPAT 0x45 /* dummy compat. register for other drivers? */
#define ADM9240_REG_CHASSIS_CLEAR 0x46
#define ADM9240_REG_VID_FAN_DIV 0x47
#define ADM9240_REG_I2C_ADDR 0x48
#define ADM9240_REG_VID4 0x49
#define ADM9240_REG_TEMP_CONFIG 0x4B

/* Conversions. Rounding is only done on the TO_REG variants. */
#define IN_TO_REG(val,nr) ((val) & 0xff)
#define IN_FROM_REG(val,nr) (val)

static inline unsigned char
FAN_TO_REG (unsigned rpm, unsigned divisor)
{
  unsigned val;
  
  if (rpm == 0)
      return 255;

  val = (1350000 + rpm * divisor / 2) / (rpm * divisor);
  if (val > 255)
      val = 255;
  return val;
}
#define FAN_FROM_REG(val,div) ((val)==0?-1:\
                               (val)==255?0:1350000/((div)*(val)))

#define TEMP_FROM_REG(val) adm9240_temp_from_reg(val)

#define TEMP_LIMIT_TO_REG(val) (((val)<0?(((val)-50)/100)&0xff:\
                                           ((val)+50)/100) & 0xff)
#define TEMP_LIMIT_FROM_REG(val) (((val)>0x80?(val)-0x100:(val))*100)

#define ALARMS_FROM_REG(val) (val) 

#define DIV_FROM_REG(val) (1 << val)
#define DIV_TO_REG(val) (val==1?0:(val==2?1:(val==4?2:3)))

#define VID_FROM_REG(val) ((val)==0x1f?0:(val)>=0x10?510-(val)*10:\
                           (val)>=0x06?0:205-(val)*5)

/* Initial limits */
#define ADM9240_INIT_IN_0 190
#define ADM9240_INIT_IN_1 190
#define ADM9240_INIT_IN_2 190
#define ADM9240_INIT_IN_3 190
#define ADM9240_INIT_IN_4 190
#define ADM9240_INIT_IN_5 190

#define ADM9240_INIT_IN_PERCENTAGE 10

#define ADM9240_INIT_IN_MIN_0 \
        (ADM9240_INIT_IN_0 - ADM9240_INIT_IN_0 * ADM9240_INIT_IN_PERCENTAGE / 100) 
#define ADM9240_INIT_IN_MAX_0 \
        (ADM9240_INIT_IN_0 + ADM9240_INIT_IN_0 * ADM9240_INIT_IN_PERCENTAGE / 100) 
#define ADM9240_INIT_IN_MIN_1 \
        (ADM9240_INIT_IN_1 - ADM9240_INIT_IN_1 * ADM9240_INIT_IN_PERCENTAGE / 100) 
#define ADM9240_INIT_IN_MAX_1 \
        (ADM9240_INIT_IN_1 + ADM9240_INIT_IN_1 * ADM9240_INIT_IN_PERCENTAGE / 100) 
#define ADM9240_INIT_IN_MIN_2 \
        (ADM9240_INIT_IN_2 - ADM9240_INIT_IN_2 * ADM9240_INIT_IN_PERCENTAGE / 100) 
#define ADM9240_INIT_IN_MAX_2 \
        (ADM9240_INIT_IN_2 + ADM9240_INIT_IN_2 * ADM9240_INIT_IN_PERCENTAGE / 100) 
#define ADM9240_INIT_IN_MIN_3 \
        (ADM9240_INIT_IN_3 - ADM9240_INIT_IN_3 * ADM9240_INIT_IN_PERCENTAGE / 100) 
#define ADM9240_INIT_IN_MAX_3 \
        (ADM9240_INIT_IN_3 + ADM9240_INIT_IN_3 * ADM9240_INIT_IN_PERCENTAGE / 100) 
#define ADM9240_INIT_IN_MIN_4 \
        (ADM9240_INIT_IN_4 - ADM9240_INIT_IN_4 * ADM9240_INIT_IN_PERCENTAGE / 100) 
#define ADM9240_INIT_IN_MAX_4 \
        (ADM9240_INIT_IN_4 + ADM9240_INIT_IN_4 * ADM9240_INIT_IN_PERCENTAGE / 100) 
#define ADM9240_INIT_IN_MIN_5 \
        (ADM9240_INIT_IN_5 - ADM9240_INIT_IN_5 * ADM9240_INIT_IN_PERCENTAGE / 100) 
#define ADM9240_INIT_IN_MAX_5 \
        (ADM9240_INIT_IN_5 + ADM9240_INIT_IN_5 * ADM9240_INIT_IN_PERCENTAGE / 100) 

#define ADM9240_INIT_FAN_MIN_1 3000
#define ADM9240_INIT_FAN_MIN_2 3000

#define ADM9240_INIT_TEMP_OS_MAX 600
#define ADM9240_INIT_TEMP_OS_HYST 500
#define ADM9240_INIT_TEMP_HOT_MAX 700
#define ADM9240_INIT_TEMP_HOT_HYST 600

#ifdef MODULE
extern int init_module(void);
extern int cleanup_module(void);
#endif /* MODULE */

/* For each registered ADM9240, we need to keep some data in memory. That
   data is pointed to by adm9240_list[NR]->data. The structure itself is
   dynamically allocated, at the same time when a new adm9240 client is
   allocated. */
struct adm9240_data {
         int sysctl_id;

         struct semaphore update_lock;
         char valid;                 /* !=0 if following fields are valid */
         unsigned long last_updated; /* In jiffies */

         u8 in[6];                   /* Register value */
         u8 in_max[6];               /* Register value */
         u8 in_min[6];               /* Register value */
         u8 fan[2];                  /* Register value */
         u8 fan_min[2];              /* Register value */
         u8 fan_div[2];              /* Register encoding, shifted right */
         int temp;                   /* Temp, shifted right */
         u8 temp_os_max;             /* Register value */
         u8 temp_os_hyst;            /* Register value */
         u16 alarms;                 /* Register encoding, combined */
         u8 analog_out;              /* Register value */
         u8 vid;                     /* Register value combined */
};


static int adm9240_init(void);
static int adm9240_cleanup(void);

static int adm9240_attach_adapter(struct i2c_adapter *adapter);
static int adm9240_detach_client(struct i2c_client *client);
static int adm9240_new_client(struct i2c_adapter *adapter,
                           struct i2c_client *new_client);
static void adm9240_remove_client(struct i2c_client *client);
static int adm9240_command(struct i2c_client *client, unsigned int cmd, 
                        void *arg);
static void adm9240_inc_use (struct i2c_client *client);
static void adm9240_dec_use (struct i2c_client *client);

static long adm9240_temp_from_reg(u16 regs);

static int adm9240_read_value(struct i2c_client *client, u8 register);
static int adm9240_write_value(struct i2c_client *client, u8 register, u8 value);
static void adm9240_update_client(struct i2c_client *client);
static void adm9240_init_client(struct i2c_client *client);


static void adm9240_in(struct i2c_client *client, int operation, int ctl_name,
                    int *nrels_mag, long *results);
static void adm9240_fan(struct i2c_client *client, int operation, int ctl_name,
                     int *nrels_mag, long *results);
static void adm9240_temp(struct i2c_client *client, int operation, int ctl_name,
                      int *nrels_mag, long *results);
static void adm9240_alarms(struct i2c_client *client, int operation, int ctl_name,
                        int *nrels_mag, long *results);
static void adm9240_fan_div(struct i2c_client *client, int operation, int ctl_name,
                         int *nrels_mag, long *results);
static void adm9240_analog_out(struct i2c_client *client, int operation, int ctl_name,
                         int *nrels_mag, long *results);
static void adm9240_vid(struct i2c_client *client, int operation, int ctl_name,
                         int *nrels_mag, long *results);

/* I choose here for semi-static ADM9240 allocation. Complete dynamic
   allocation could also be used; the code needed for this would probably
   take more memory than the datastructure takes now. */
#define MAX_ADM9240_NR 4
static struct i2c_client *adm9240_list[MAX_ADM9240_NR];

/* The driver. I choose to use type i2c_driver, as at is identical to both
   smbus_driver and isa_driver, and clients could be of either kind */
static struct i2c_driver adm9240_driver = {
  /* name */		"ADM9240 sensor driver",
  /* id */		I2C_DRIVERID_ADM9240,
  /* flags */		DF_NOTIFY,
  /* attach_adapter */  &adm9240_attach_adapter,
  /* detach_client */	&adm9240_detach_client,
  /* command */		&adm9240_command,
  /* inc_use */		&adm9240_inc_use,
  /* dec_use */		&adm9240_dec_use
};

/* Used by adm9240_init/cleanup */
static int adm9240_initialized = 0;

/* The /proc/sys entries */
/* These files are created for each detected ADM9240. This is just a template;
   though at first sight, you might think we could use a statically
   allocated list, we need some way to get back to the parent - which
   is done through one of the 'extra' fields which are initialized 
   when a new copy is allocated. */
static ctl_table adm9240_dir_table_template[] = {
  { ADM9240_SYSCTL_IN0, "2.5V", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &adm9240_in },
  { ADM9240_SYSCTL_IN1, "Vccp1", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &adm9240_in },
  { ADM9240_SYSCTL_IN2, "3.3V", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &adm9240_in },
  { ADM9240_SYSCTL_IN3, "5V", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &adm9240_in },
  { ADM9240_SYSCTL_IN4, "12V", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &adm9240_in },
  { ADM9240_SYSCTL_IN5, "Vccp2", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &adm9240_in },
  { ADM9240_SYSCTL_FAN1, "fan1", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &adm9240_fan },
  { ADM9240_SYSCTL_FAN2, "fan2", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &adm9240_fan },
  { ADM9240_SYSCTL_TEMP, "temp", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &adm9240_temp },
  { ADM9240_SYSCTL_FAN_DIV, "fan_div", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &adm9240_fan_div },
  { ADM9240_SYSCTL_ALARMS, "alarms", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &adm9240_alarms },
  { ADM9240_SYSCTL_ANALOG_OUT, "analog_out", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &adm9240_analog_out },
  { ADM9240_SYSCTL_VID, "vid", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &adm9240_vid },
  { 0 }
};


int adm9240_attach_adapter(struct i2c_adapter *adapter)
{
  int address,err,temp;
  struct i2c_client *new_client;
  const char *type_name,*client_name;

  err = 0;
  /* Make sure we aren't probing the ISA bus!! */
  if (i2c_is_isa_adapter(adapter)) return 0;
  
  /* The address of the ADM2940 must at least start somewhere in
     0x2C to 0x2F, but can be changed to be anyelse after that. 
     (But, why??) */
  for (address = 0x2C; (! err) && (address <= 0x2f); address ++) {

    /* Later on, we will keep a list of registered addresses for each
       adapter, and check whether they are used here */

    if (smbus_read_byte_data(adapter,address,ADM9240_REG_COMPANY_ID) != 0x23) 
      continue;

    temp=smbus_read_byte_data(adapter,address,ADM9240_REG_DIE_REV);
    printk("adm9240.o: ADM9240 detected with die rev.: 0x%X\n",temp);
    type_name = "adm9240";
    client_name = "ADM9240 chip";


    /* Allocate space for a new client structure. To counter memory
       fragmentation somewhat, we only do one kmalloc. */
    if (! (new_client = kmalloc(sizeof(struct i2c_client) + 
                                sizeof(struct adm9240_data),
                               GFP_KERNEL))) {
      err = -ENOMEM;
      continue;
    }

    /* Fill the new client structure with data */
    new_client->data = (struct adm9240_data *) (new_client + 1);
    new_client->addr = address;
    strcpy(new_client->name,client_name);
    if ((err = adm9240_new_client(adapter,new_client)))
      goto ERROR2;

    /* Tell i2c-core a new client has arrived */
    if ((err = i2c_attach_client(new_client))) 
      goto ERROR3;

    /* Register a new directory entry with module sensors */
    if ((err = sensors_register_entry(new_client,type_name,
                                      adm9240_dir_table_template)) < 0)
      goto ERROR4;
    ((struct adm9240_data *) (new_client->data))->sysctl_id = err;
    err = 0;

    /* Initialize the ADM9240 chip */
    adm9240_init_client(new_client);
    continue;

/* OK, this is not exactly good programming practice, usually. But it is
   very code-efficient in this case. */
ERROR4:
    i2c_detach_client(new_client);
ERROR3:
    adm9240_remove_client((struct i2c_client *) new_client);
ERROR2:
    kfree(new_client);
  }
  return err;
}

int adm9240_detach_client(struct i2c_client *client)
{
  int err,i;
  for (i = 0; i < MAX_ADM9240_NR; i++)
    if (client == adm9240_list[i])
      break;
  if ((i == MAX_ADM9240_NR)) {
    printk("adm9240.o: Client to detach not found.\n");
    return -ENOENT;
  }

  sensors_deregister_entry(((struct adm9240_data *)(client->data))->sysctl_id);

  if ((err = i2c_detach_client(client))) {
    printk("adm9240.o: Client deregistration failed, client not detached.\n");
    return err;
  }
  adm9240_remove_client(client);
  kfree(client);
  return 0;
}


/* Find a free slot, and initialize most of the fields */
int adm9240_new_client(struct i2c_adapter *adapter,
                    struct i2c_client *new_client)
{
  int i;
  struct adm9240_data *data;

  /* First, seek out an empty slot */
  for(i = 0; i < MAX_ADM9240_NR; i++)
    if (! adm9240_list[i])
      break;
  if (i == MAX_ADM9240_NR) {
    printk("adm9240.o: No empty slots left, recompile and heighten "
           "MAX_ADM9240_NR!\n");
    return -ENOMEM;
  }
  
  adm9240_list[i] = new_client;
  new_client->id = i;
  new_client->adapter = adapter;
  new_client->driver = &adm9240_driver;
  data = new_client->data;
  data->valid = 0;
  data->update_lock = MUTEX;
  return 0;
}

/* Inverse of adm9240_new_client */
void adm9240_remove_client(struct i2c_client *client)
{
  int i;
  for (i = 0; i < MAX_ADM9240_NR; i++)
    if (client == adm9240_list[i]) 
      adm9240_list[i] = NULL;
}

/* No commands defined yet */
int adm9240_command(struct i2c_client *client, unsigned int cmd, void *arg)
{
  return 0;
}

void adm9240_inc_use (struct i2c_client *client)
{
#ifdef MODULE
  MOD_INC_USE_COUNT;
#endif
}

void adm9240_dec_use (struct i2c_client *client)
{
#ifdef MODULE
  MOD_DEC_USE_COUNT;
#endif
}
 

long adm9240_temp_from_reg(u16 temp)
{
  if (temp < 256)
   return (((temp & 0x1fe) >> 1) * 10) + ((temp & 1) * 5);
  else
   return ((((temp & 0x01fe) >> 1) - 255) * 10) - ((temp & 1) * 5);
}

int adm9240_read_value(struct i2c_client *client, u8 reg)
{
  return 0xFF & smbus_read_byte_data(client->adapter,client->addr, reg);
}

int adm9240_write_value(struct i2c_client *client, u8 reg, u8 value)
{
  return smbus_write_byte_data(client->adapter, client->addr, reg,value);
}

/* Called when we have found a new ADM9240. It should set limits, etc. */
void adm9240_init_client(struct i2c_client *client)
{
  /* Reset all except Watchdog values and last conversion values
     This sets fan-divs to 2, among others. This makes most other
     initializations unnecessary */
  adm9240_write_value(client,ADM9240_REG_CONFIG,0x80);

  adm9240_write_value(client,ADM9240_REG_IN_MIN(0),IN_TO_REG(ADM9240_INIT_IN_MIN_0,0));
  adm9240_write_value(client,ADM9240_REG_IN_MAX(0),IN_TO_REG(ADM9240_INIT_IN_MAX_0,0));
  adm9240_write_value(client,ADM9240_REG_IN_MIN(1),IN_TO_REG(ADM9240_INIT_IN_MIN_1,1));
  adm9240_write_value(client,ADM9240_REG_IN_MAX(1),IN_TO_REG(ADM9240_INIT_IN_MAX_1,1));
  adm9240_write_value(client,ADM9240_REG_IN_MIN(2),IN_TO_REG(ADM9240_INIT_IN_MIN_2,2));
  adm9240_write_value(client,ADM9240_REG_IN_MAX(2),IN_TO_REG(ADM9240_INIT_IN_MAX_2,2));
  adm9240_write_value(client,ADM9240_REG_IN_MIN(3),IN_TO_REG(ADM9240_INIT_IN_MIN_3,3));
  adm9240_write_value(client,ADM9240_REG_IN_MAX(3),IN_TO_REG(ADM9240_INIT_IN_MAX_3,3));
  adm9240_write_value(client,ADM9240_REG_IN_MIN(4),IN_TO_REG(ADM9240_INIT_IN_MIN_4,4));
  adm9240_write_value(client,ADM9240_REG_IN_MAX(4),IN_TO_REG(ADM9240_INIT_IN_MAX_4,4));
  adm9240_write_value(client,ADM9240_REG_IN_MIN(5),IN_TO_REG(ADM9240_INIT_IN_MIN_5,5));
  adm9240_write_value(client,ADM9240_REG_IN_MAX(5),IN_TO_REG(ADM9240_INIT_IN_MAX_5,5));
  adm9240_write_value(client,ADM9240_REG_FAN1_MIN,FAN_TO_REG(ADM9240_INIT_FAN_MIN_1,2));
  adm9240_write_value(client,ADM9240_REG_FAN2_MIN,FAN_TO_REG(ADM9240_INIT_FAN_MIN_2,2));
  adm9240_write_value(client,ADM9240_REG_TOS,
                   TEMP_LIMIT_TO_REG(ADM9240_INIT_TEMP_OS_MAX));
  adm9240_write_value(client,ADM9240_REG_THYST,
                   TEMP_LIMIT_TO_REG(ADM9240_INIT_TEMP_OS_HYST));
  adm9240_write_value(client,ADM9240_REG_TEMP_CONFIG,0x00);

  /* Start monitoring */
  adm9240_write_value(client,ADM9240_REG_CONFIG,0x01);
}

void adm9240_update_client(struct i2c_client *client)
{
  struct adm9240_data *data = client->data;
  u8 i;

  down(&data->update_lock);

  if ((jiffies - data->last_updated > 2*HZ ) ||
      (jiffies < data->last_updated) || ! data->valid) {

#ifdef DEBUG
    printk("Starting adm9240 update\n");
#endif
    for (i = 0; i <= 5; i++) {
      data->in[i]     = adm9240_read_value(client,ADM9240_REG_IN(i));
      data->in_min[i] = adm9240_read_value(client,ADM9240_REG_IN_MIN(i));
      data->in_max[i] = adm9240_read_value(client,ADM9240_REG_IN_MAX(i));
    }
    data->fan[0] = adm9240_read_value(client,ADM9240_REG_FAN1);
    data->fan_min[0] = adm9240_read_value(client,ADM9240_REG_FAN1_MIN);
    data->fan[1] = adm9240_read_value(client,ADM9240_REG_FAN2);
    data->fan_min[1] = adm9240_read_value(client,ADM9240_REG_FAN2_MIN);
    data->temp = (adm9240_read_value(client,ADM9240_REG_TEMP) << 1) +
                 ((adm9240_read_value(client,ADM9240_REG_TEMP_CONFIG) & 0x80) >> 7);
    data->temp_os_max = adm9240_read_value(client,ADM9240_REG_TOS);
    data->temp_os_hyst = adm9240_read_value(client,ADM9240_REG_THYST);

    i = adm9240_read_value(client,ADM9240_REG_VID_FAN_DIV);
    data->fan_div[0] = (i >> 4) & 0x03;
    data->fan_div[1] = (i >> 6) & 0x03;
    data->vid = i & 0x0f;
    data->vid |= (adm9240_read_value(client,ADM9240_REG_VID4) & 0x01) << 4;

    data->alarms = adm9240_read_value(client,ADM9240_REG_INT1_STAT) +
                   (adm9240_read_value(client,ADM9240_REG_INT2_STAT) << 8);
    data->analog_out = adm9240_read_value(client,ADM9240_REG_ANALOG_OUT);
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
void adm9240_in(struct i2c_client *client, int operation, int ctl_name, 
             int *nrels_mag, long *results)
{

 int scales[6]={250, 270, 330, 500, 1200, 270};

  struct adm9240_data *data = client->data;
  int nr = ctl_name - ADM9240_SYSCTL_IN0;

  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 2;
  else if (operation == SENSORS_PROC_REAL_READ) {
    adm9240_update_client(client);
    results[0] = IN_FROM_REG(data->in_min[nr],nr) * scales[nr] / 192;
    results[1] = IN_FROM_REG(data->in_max[nr],nr) * scales[nr] / 192;
    results[2] = IN_FROM_REG(data->in[nr],nr) * scales[nr] / 192;
    *nrels_mag = 3;
  } else if (operation == SENSORS_PROC_REAL_WRITE) {
      if (*nrels_mag >= 1) {
        data->in_min[nr] = IN_TO_REG((results[0]*192)/scales[nr],nr);
        adm9240_write_value(client,ADM9240_REG_IN_MIN(nr),data->in_min[nr]);
      }
      if (*nrels_mag >= 2) {
        data->in_max[nr] = IN_TO_REG((results[1]*192)/scales[nr],nr);
        adm9240_write_value(client,ADM9240_REG_IN_MAX(nr),data->in_max[nr]);
      }
  }
}

void adm9240_fan(struct i2c_client *client, int operation, int ctl_name,
              int *nrels_mag, long *results)
{
  struct adm9240_data *data = client->data;
  int nr = ctl_name - ADM9240_SYSCTL_FAN1 + 1;

  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 0;
  else if (operation == SENSORS_PROC_REAL_READ) {
    adm9240_update_client(client);
    results[0] = FAN_FROM_REG(data->fan_min[nr-1],
                              DIV_FROM_REG(data->fan_div[nr-1]));
    results[1] = FAN_FROM_REG(data->fan[nr-1],
                              DIV_FROM_REG(data->fan_div[nr-1]));
    *nrels_mag = 2;
  } else if (operation == SENSORS_PROC_REAL_WRITE) {
    if (*nrels_mag >= 1) {
      data->fan_min[nr-1] = FAN_TO_REG(results[0],
                            DIV_FROM_REG(data->fan_div[nr-1]));
      adm9240_write_value(client,nr==1?ADM9240_REG_FAN1_MIN:ADM9240_REG_FAN2_MIN,
                       data->fan_min[nr-1]);
    }
  }
}


void adm9240_temp(struct i2c_client *client, int operation, int ctl_name,
               int *nrels_mag, long *results)
{
  struct adm9240_data *data = client->data;
  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 1;
  else if (operation == SENSORS_PROC_REAL_READ) {
    adm9240_update_client(client);
    results[0] = TEMP_LIMIT_FROM_REG(data->temp_os_max);
    results[1] = TEMP_LIMIT_FROM_REG(data->temp_os_hyst);
    results[2] = TEMP_FROM_REG(data->temp);
    *nrels_mag = 3;
  } else if (operation == SENSORS_PROC_REAL_WRITE) {
    if (*nrels_mag >= 1) {
      data->temp_os_max = TEMP_LIMIT_TO_REG(results[0]);
      adm9240_write_value(client,ADM9240_REG_TOS,data->temp_os_max);
    }
    if (*nrels_mag >= 2) {
      data->temp_os_hyst = TEMP_LIMIT_TO_REG(results[1]);
      adm9240_write_value(client,ADM9240_REG_THYST,data->temp_os_hyst);
    }
  }
}

void adm9240_alarms(struct i2c_client *client, int operation, int ctl_name,
                 int *nrels_mag, long *results)
{
  struct adm9240_data *data = client->data;
  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 0;
  else if (operation == SENSORS_PROC_REAL_READ) {
    adm9240_update_client(client);
    results[0] = ALARMS_FROM_REG(data->alarms);
    *nrels_mag = 1;
  }
}

void adm9240_fan_div(struct i2c_client *client, int operation, int ctl_name,
                  int *nrels_mag, long *results)
{
  struct adm9240_data *data = client->data;
  int old;

  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 0;
  else if (operation == SENSORS_PROC_REAL_READ) {
    adm9240_update_client(client);
    results[0] = DIV_FROM_REG(data->fan_div[0]);
    results[1] = DIV_FROM_REG(data->fan_div[1]);
    *nrels_mag = 2;
  } else if (operation == SENSORS_PROC_REAL_WRITE) {
    old = adm9240_read_value(client,ADM9240_REG_VID_FAN_DIV);
    if (*nrels_mag >= 2) {
      data->fan_div[1] = DIV_TO_REG(results[1]);
      old = (old & 0xcf) | (data->fan_div[1] << 6);
    }
    if (*nrels_mag >= 1) {
      data->fan_div[0] = DIV_TO_REG(results[0]);
      old = (old & 0x3f) | (data->fan_div[0] << 4);
      adm9240_write_value(client,ADM9240_REG_VID_FAN_DIV,old);
    }
  }
}

void adm9240_analog_out(struct i2c_client *client, int operation, int ctl_name,
                  int *nrels_mag, long *results)
{
  struct adm9240_data *data = client->data;

  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 0;
  else if (operation == SENSORS_PROC_REAL_READ) {
    adm9240_update_client(client);
    results[0] = data->analog_out;
    *nrels_mag = 1;
  } else if (operation == SENSORS_PROC_REAL_WRITE) {
    if (*nrels_mag >= 1) {
      data->analog_out = results[0];
      adm9240_write_value(client,ADM9240_REG_ANALOG_OUT,data->analog_out);
    }
  }
}

void adm9240_vid(struct i2c_client *client, int operation, int ctl_name,
              int *nrels_mag, long *results)
{
  struct adm9240_data *data = client->data;
  
  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 2;
  else if (operation == SENSORS_PROC_REAL_READ) {
    adm9240_update_client(client);
    results[0] = VID_FROM_REG(data->vid);
    *nrels_mag = 1;
  }
}

int adm9240_init(void)
{
  int res;

  printk("adm9240.o version %s (%s)\n",LM_VERSION,LM_DATE);
  adm9240_initialized = 0;

  if ((res =i2c_add_driver(&adm9240_driver))) {
    printk("adm9240.o: Driver registration failed, module not inserted.\n");
    adm9240_cleanup();
    return res;
  }
  adm9240_initialized ++;
  return 0;
}

int adm9240_cleanup(void)
{
  int res;

  if (adm9240_initialized >= 1) {
    if ((res = i2c_del_driver(&adm9240_driver))) {
      printk("adm9240.o: Driver deregistration failed, module not removed.\n");
      return res;
    }
    adm9240_initialized --;
  }
  return 0;
}


#ifdef MODULE

MODULE_AUTHOR("Frodo Looijaard <frodol@dds.nl> and Philip Edelbrock <phil@netroedge.com>");
MODULE_DESCRIPTION("ADM9240 driver");

int init_module(void)
{
  return adm9240_init();
}

int cleanup_module(void)
{
  return adm9240_cleanup();
}

#endif /* MODULE */

