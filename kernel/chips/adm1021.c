/*
    adm1021.c - Part of lm_sensors, Linux kernel modules for hardware
             monitoring
    Copyright (c) 1998, 1999  Frodo Looijaard <frodol@dds.nl> and
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

/* adm1021 constants specified below */

/* The adm1021 registers */
/* Read-only */
#define adm1021_REG_TEMP 0x00
#define adm1021_REG_REMOTE_TEMP 0x01
#define adm1021_REG_STATUS 0x02
#define adm1021_REG_DEVICE_ID 0x0FE  /* should always read 0x41 */
#define adm1021_REG_DIE_CODE 0x0FF
/* These use different addresses for reading/writing */
#define adm1021_REG_CONFIG_R 0x03
#define adm1021_REG_CONFIG_W 0x09
#define adm1021_REG_CONV_RATE_R 0x04
#define adm1021_REG_CONV_RATE_W 0x0A
#define adm1021_REG_TOS_R 0x05
#define adm1021_REG_TOS_W 0x0B
#define adm1021_REG_REMOTE_TOS_R 0x07
#define adm1021_REG_REMOTE_TOS_W 0x0D
#define adm1021_REG_THYST_R 0x06
#define adm1021_REG_THYST_W 0x0C
#define adm1021_REG_REMOTE_THYST_R 0x08
#define adm1021_REG_REMOTE_THYST_W 0x0E
/* write-only */
#define adm1021_REG_ONESHOT 0x0F


/* Conversions  note: 1021 uses normal integer signed-byte format*/
#define TEMP_FROM_REG(val) (val > 127 ? val-256 : val)
#define TEMP_TO_REG(val)   (val < 0 ? val+256 : val)

/* Initial values */

/* Note: Eventhough I left the low and high limits named os and hyst, 
they don't quite work like a thermostat the way the LM75 does.  I.e., 
a lower temp than THYST actuall triggers an alarm instead of 
clearing it.  Weird, ey?   --Phil  */
#define adm1021_INIT_TOS 60
#define adm1021_INIT_THYST 20
#define adm1021_INIT_REMOTE_TOS 60
#define adm1021_INIT_REMOTE_THYST 20

/* Types of chips supported */
enum adm1021_type { adm1021, max1617 };

/* Each client has this additional data */
struct adm1021_data {
         int sysctl_id;
	 enum adm1021_type type;

         struct semaphore update_lock;
         char valid;                 /* !=0 if following fields are valid */
         unsigned long last_updated; /* In jiffies */

         u8 temp,temp_os,temp_hyst; /* Register values */
         u8 remote_temp,remote_temp_os,remote_temp_hyst,status,die_code; 
};

#ifdef MODULE
extern int init_module(void);
extern int cleanup_module(void);
#endif /* MODULE */

static int adm1021_init(void);
static int adm1021_cleanup(void);
static int adm1021_attach_adapter(struct i2c_adapter *adapter);
static int adm1021_detach_client(struct i2c_client *client);
static int adm1021_command(struct i2c_client *client, unsigned int cmd,
                        void *arg);
static void adm1021_inc_use (struct i2c_client *client);
static void adm1021_dec_use (struct i2c_client *client);
static int adm1021_read_value(struct i2c_client *client, u8 reg);
static int adm1021_write_value(struct i2c_client *client, u8 reg, u16 value);
static void adm1021_temp(struct i2c_client *client, int operation, int ctl_name,
                      int *nrels_mag, long *results);
static void adm1021_remote_temp(struct i2c_client *client, int operation, int ctl_name,
                      int *nrels_mag, long *results);
static void adm1021_status(struct i2c_client *client, int operation, int ctl_name,
                      int *nrels_mag, long *results);
static void adm1021_die_code(struct i2c_client *client, int operation, int ctl_name,
                      int *nrels_mag, long *results);
static void adm1021_update_client(struct i2c_client *client);


/* This is the driver that will be inserted */
static struct i2c_driver adm1021_driver = {
  /* name */            "adm1021, MAX1617 sensor driver",
  /* id */              I2C_DRIVERID_ADM1021,
  /* flags */           DF_NOTIFY,
  /* attach_adapter */  &adm1021_attach_adapter,
  /* detach_client */   &adm1021_detach_client,
  /* command */         &adm1021_command,
  /* inc_use */         &adm1021_inc_use,
  /* dec_use */         &adm1021_dec_use
};

/* These files are created for each detected adm1021. This is just a template;
   though at first sight, you might think we could use a statically
   allocated list, we need some way to get back to the parent - which
   is done through one of the 'extra' fields which are initialized
   when a new copy is allocated. */
static ctl_table adm1021_dir_table_template[] = {
  { ADM1021_SYSCTL_TEMP, "temp", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &adm1021_temp },
    {ADM1021_SYSCTL_REMOTE_TEMP, "remote_temp", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &adm1021_remote_temp },
    {ADM1021_SYSCTL_DIE_CODE, "die_code", NULL, 0, 0444, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &adm1021_die_code },
    {ADM1021_SYSCTL_STATUS, "status", NULL, 0, 0444, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &adm1021_status },
  { 0 }
};

/* Used by init/cleanup */
static int adm1021_initialized = 0;

/* I choose here for semi-static allocation. Complete dynamic
   allocation could also be used; the code needed for this would probably
   take more memory than the datastructure takes now. */
#define MAX_adm1021_NR 9
static struct i2c_client *adm1021_list[MAX_adm1021_NR];


int adm1021_attach_adapter(struct i2c_adapter *adapter)
{
  int address,err,i;
  struct i2c_client *new_client;
  struct adm1021_data *data;
  enum adm1021_type type;
  const char *type_name;
  const char *client_name;

  err = 0;

  /* OK, this is no detection. I know. It will do for now, though.  */

  /* Set err only if a global error would make registering other clients
     impossible too (like out-of-memory). */
  for (address = 0x18; (! err) && (address <= 0x54); address ++) {
    /* Legal addresses: 0x18,19,20,  29,30,31,  52,53,54
	(Notice the two jumps) */
    if (address==0x21) { address=0x28; continue;}
    if (address==0x32) { address=0x51; continue;}
    /* Later on, we will keep a list of registered addresses for each
       adapter, and check whether they are used here */
    
    /* Verify device is 1021 by checked special DEVICE_ID 
       register (I wish all SMBus chips had this..)       */
    /* The MAX1617 does not have it, regrettably. */
    if (smbus_read_byte_data(adapter,address,adm1021_REG_DEVICE_ID) != 0x41){
	type = adm1021;
        type_name = "adm1021";
        client_name = "ADM1021 chip";
       } else { 
        type = max1617;
        type_name = "max1617";
        client_name = "MAX1617 chip";
    }
      /* continue; */

    /* Allocate space for a new client structure */
    if (! (new_client =  kmalloc(sizeof(struct i2c_client) +
                                sizeof(struct adm1021_data),
                               GFP_KERNEL))) {
      err = -ENOMEM;
      continue;
    }

    /* Find a place in our global list */
    for (i = 0; i < MAX_adm1021_NR; i++)
      if (! adm1021_list[i])
         break;
    if (i == MAX_adm1021_NR) {
      err = -ENOMEM;
      printk("adm1021.o: No empty slots left, recompile and heighten "
             "MAX_adm1021_NR!\n");
      goto ERROR1;
    }
    adm1021_list[i] = new_client;
    
    /* Fill the new client structure with data */
    data = (struct adm1021_data *) (new_client + 1);
    new_client->data = data;
    new_client->id = i;
    new_client->addr = address;
    new_client->adapter = adapter;
    new_client->driver = &adm1021_driver;
    strcpy(new_client->name,client_name);
    data->type = type;
    data->valid = 0;
    data->update_lock = MUTEX;
    
    /* Tell i2c-core a new client has arrived */
    if ((err = i2c_attach_client(new_client)))
      goto ERROR2;
    
    /* Register a new directory entry with module sensors */
    if ((err = sensors_register_entry(new_client,type_name,
                                      adm1021_dir_table_template)) < 0)
      goto ERROR3;
    data->sysctl_id = err;
    err = 0;

    /* Initialize the adm1021 chip */
    adm1021_write_value(new_client,adm1021_REG_TOS_W,
                     TEMP_TO_REG(adm1021_INIT_TOS));
    adm1021_write_value(new_client,adm1021_REG_THYST_W,
                     TEMP_TO_REG(adm1021_INIT_THYST));
    adm1021_write_value(new_client,adm1021_REG_REMOTE_TOS_W,
                     TEMP_TO_REG(adm1021_INIT_REMOTE_TOS));
    adm1021_write_value(new_client,adm1021_REG_REMOTE_THYST_W,
                     TEMP_TO_REG(adm1021_INIT_REMOTE_THYST));
    adm1021_write_value(new_client,adm1021_REG_CONFIG_W,0);  /* Enable ADC and disable suspend mode */
     /* Set Conversion rate to 1/sec (this can be tinkered with) */
    adm1021_write_value(new_client,adm1021_REG_CONV_RATE_W,0x04); 
    
    continue;
/* OK, this is not exactly good programming practice, usually. But it is
   very code-efficient in this case. */

ERROR3:
    i2c_detach_client(new_client);
ERROR2:
    adm1021_list[i] = NULL;
ERROR1:
    kfree(new_client);
  }
  return err;
}

int adm1021_detach_client(struct i2c_client *client)
{
  int err,i;
  for (i = 0; i < MAX_adm1021_NR; i++)
    if (client == adm1021_list[i])
      break;
  if ((i == MAX_adm1021_NR)) {
    printk("adm1021.o: Client to detach not found.\n");
    return -ENOENT;
  }

  sensors_deregister_entry(((struct adm1021_data *)(client->data))->sysctl_id);

  if ((err = i2c_detach_client(client))) {
    printk("adm1021.o: Client deregistration failed, client not detached.\n");
    return err;
  }

  adm1021_list[i] = NULL;
  kfree(client);
  return 0;
}


/* No commands defined yet */
int adm1021_command(struct i2c_client *client, unsigned int cmd, void *arg)
{
  return 0;
}

void adm1021_inc_use (struct i2c_client *client)
{
#ifdef MODULE
  MOD_INC_USE_COUNT;
#endif
}

void adm1021_dec_use (struct i2c_client *client)
{
#ifdef MODULE
  MOD_DEC_USE_COUNT;
#endif
}

/* All registers are byte-sized */
int adm1021_read_value(struct i2c_client *client, u8 reg)
{
    return smbus_read_byte_data(client->adapter,client->addr,reg);
}

int adm1021_write_value(struct i2c_client *client, u8 reg, u16 value)
{
    return smbus_write_byte_data(client->adapter,client->addr,reg,value);
}

void adm1021_update_client(struct i2c_client *client)
{
  struct adm1021_data *data = client->data;

  down(&data->update_lock);

  if ((jiffies - data->last_updated > HZ+HZ/2 ) ||
      (jiffies < data->last_updated) || ! data->valid) {

#ifdef DEBUG
    printk("Starting adm1021 update\n");
#endif

    data->temp = adm1021_read_value(client,adm1021_REG_TEMP);
    data->temp_os = adm1021_read_value(client,adm1021_REG_TOS_R);
    data->temp_hyst = adm1021_read_value(client,adm1021_REG_THYST_R);
    data->remote_temp = adm1021_read_value(client,adm1021_REG_REMOTE_TEMP);
    data->remote_temp_os = adm1021_read_value(client,adm1021_REG_REMOTE_TOS_R);
    data->remote_temp_hyst = adm1021_read_value(client,adm1021_REG_REMOTE_THYST_R);
    data->die_code = adm1021_read_value(client,adm1021_REG_DIE_CODE);
    data->status = adm1021_read_value(client,adm1021_REG_STATUS);
    data->last_updated = jiffies;
    data->valid = 1;
  }

  up(&data->update_lock);
}


void adm1021_temp(struct i2c_client *client, int operation, int ctl_name,
               int *nrels_mag, long *results)
{
  struct adm1021_data *data = client->data;
  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 0;
  else if (operation == SENSORS_PROC_REAL_READ) {
    adm1021_update_client(client);
    results[0] = TEMP_FROM_REG(data->temp_os);
    results[1] = TEMP_FROM_REG(data->temp_hyst);
    results[2] = TEMP_FROM_REG(data->temp);
    *nrels_mag = 3;
  } else if (operation == SENSORS_PROC_REAL_WRITE) {
    if (*nrels_mag >= 1) {
      data->temp_os = TEMP_TO_REG(results[0]);
      adm1021_write_value(client,adm1021_REG_TOS_W,data->temp_os);
    }
    if (*nrels_mag >= 2) {
      data->temp_hyst = TEMP_TO_REG(results[1]);
      adm1021_write_value(client,adm1021_REG_THYST_W,data->temp_hyst);
    }
  }
}

void adm1021_remote_temp(struct i2c_client *client, int operation, int ctl_name,
               int *nrels_mag, long *results)
{
  struct adm1021_data *data = client->data;
  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 0;
  else if (operation == SENSORS_PROC_REAL_READ) {
    adm1021_update_client(client);
    results[0] = TEMP_FROM_REG(data->remote_temp_os);
    results[1] = TEMP_FROM_REG(data->remote_temp_hyst);
    results[2] = TEMP_FROM_REG(data->remote_temp);
    *nrels_mag = 3;
  } else if (operation == SENSORS_PROC_REAL_WRITE) {
    if (*nrels_mag >= 1) {
      data->remote_temp_os = TEMP_TO_REG(results[0]);
      adm1021_write_value(client,adm1021_REG_REMOTE_TOS_W,data->remote_temp_os);
    }
    if (*nrels_mag >= 2) {
      data->remote_temp_hyst = TEMP_TO_REG(results[1]);
      adm1021_write_value(client,adm1021_REG_REMOTE_THYST_W,data->remote_temp_hyst);
    }
  }
}

void adm1021_die_code(struct i2c_client *client, int operation, int ctl_name,
               int *nrels_mag, long *results)
{
  struct adm1021_data *data = client->data;
  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 0;
  else if (operation == SENSORS_PROC_REAL_READ) {
    adm1021_update_client(client);
    results[0] = data->die_code;
    *nrels_mag = 1;
  } else if (operation == SENSORS_PROC_REAL_WRITE) {
    /* Can't write to it */
  }
}

void adm1021_status(struct i2c_client *client, int operation, int ctl_name,
               int *nrels_mag, long *results)
{
  struct adm1021_data *data = client->data;
  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 0;
  else if (operation == SENSORS_PROC_REAL_READ) {
    adm1021_update_client(client);
    results[0] = data->status;
    *nrels_mag = 1;
  } else if (operation == SENSORS_PROC_REAL_WRITE) {
    /* Can't write to it */
  }
}

int adm1021_init(void)
{
  int res;

  printk("adm1021.o version %s (%s)\n",LM_VERSION,LM_DATE);
  adm1021_initialized = 0;
  if ((res = i2c_add_driver(&adm1021_driver))) {
    printk("adm1021.o: Driver registration failed, module not inserted.\n");
    adm1021_cleanup();
    return res;
  }
  adm1021_initialized ++;
  return 0;
}

int adm1021_cleanup(void)
{
  int res;

  if (adm1021_initialized >= 1) {
    if ((res = i2c_del_driver(&adm1021_driver))) {
      printk("adm1021.o: Driver deregistration failed, module not removed.\n");
      return res;
    }
    adm1021_initialized --;
  }

  return 0;
}


#ifdef MODULE

MODULE_AUTHOR("Frodo Looijaard <frodol@dds.nl> and Philip Edelbrock <phil@netroedge.com>");
MODULE_DESCRIPTION("adm1021 driver");

int init_module(void)
{
  return adm1021_init();
}

int cleanup_module(void)
{
  return adm1021_cleanup();
}

#endif /* MODULE */

