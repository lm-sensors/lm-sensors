/*
    gl518sm.c - A Linux module for reading sensor data.
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


/* Many GL518 constants specified below */

/* The GL518 registers */
#define GL518_REG_CHIP_ID 0x00
#define GL518_REG_REVISION 0x01
#define GL518_REG_VENDOR_ID 0x02
#define GL518_REG_CONF 0x03
#define GL518_REG_TEMP 0x04
#define GL518_REG_TEMP_OVER 0x05
#define GL518_REG_TEMP_HYST 0x06
#define GL518_REG_FAN_COUNT 0x07
#define GL518_REG_FAN_LIMIT 0x08
#define GL518_REG_VIN1_LIMIT 0x09
#define GL518_REG_VIN2_LIMIT 0x0a
#define GL518_REG_VIN3_LIMIT 0x0b
#define GL518_REG_VDD_LIMIT 0x0c
#define GL518_REG_VOLT 0x0d
#define GL518_REG_MISC 0x0f
#define GL518_REG_ALARM 0x10
#define GL518_REG_MASK 0x11
#define GL518_REG_INT 0x12


/* Conversions. Rounding is only done on the TO_REG variants. */
#define TEMP_TO_REG(val) (((((val)<0?(val)-5:(val)+5) / 10) + 119) & 0xff)
#define TEMP_FROM_REG(val) (((val) - 119) * 10)

#define FAN_TO_REG(val,div) (((val)==0?255:\
                             (960000+((val)*(div)))/(2*(val)*(div))) & 0xff)
#define FAN_FROM_REG(val,div) ((val)==0?-1:(val)==255?0:960000/(2*(val)*(div)))

#define IN_TO_REG(val) ((((val)*10+8)/19) & 0xff)
#define IN_FROM_REG(val) (((val)*19)/10)

#define DIV_TO_REG(val) ((val)==8?3:(val)==4?2:(val)==1?0:1)
#define DIV_FROM_REG(val) (1 << (val))

/* Initial values */
#define GL518_INIT_TEMP_OVER 600
#define GL518_INIT_TEMP_HYST 500
#define GL518_INIT_FAN_MIN_1 3000
#define GL518_INIT_FAN_MIN_2 3000

/* What are sane values for these?!? */
#define GL518_INIT_VIN_1 3000
#define GL518_INIT_VIN_2 3000
#define GL518_INIT_VIN_3 3000
#define GL518_INIT_VDD 3000

#define GL518_INIT_PERCENTAGE 10

#define GL518_INIT_VIN_MIN_1 \
        (GL518_INIT_VIN_1 - GL518_INIT_VIN_1 * GL518_INIT_PERCENTAGE / 100) 
#define GL518_INIT_VIN_MAX_1 \
        (GL518_INIT_VIN_1 + GL518_INIT_VIN_1 * GL518_INIT_PERCENTAGE / 100) 
#define GL518_INIT_VIN_MIN_2 \
        (GL518_INIT_VIN_2 - GL518_INIT_VIN_2 * GL518_INIT_PERCENTAGE / 100) 
#define GL518_INIT_VIN_MAX_2 \
        (GL518_INIT_VIN_2 + GL518_INIT_VIN_2 * GL518_INIT_PERCENTAGE / 100) 
#define GL518_INIT_VIN_MIN_3 \
        (GL518_INIT_VIN_3 - GL518_INIT_VIN_3 * GL518_INIT_PERCENTAGE / 100) 
#define GL518_INIT_VIN_MAX_3 \
        (GL518_INIT_VIN_3 + GL518_INIT_VIN_3 * GL518_INIT_PERCENTAGE / 100) 
#define GL518_INIT_VDD_MIN \
        (GL518_INIT_VDD - GL518_INIT_VDD * GL518_INIT_PERCENTAGE / 100) 
#define GL518_INIT_VDD_MAX \
        (GL518_INIT_VDD + GL518_INIT_VDD * GL518_INIT_PERCENTAGE / 100) 


/* Each client has this additional data */
struct gl518_data {
         int sysctl_id;

         struct semaphore update_lock;
         char valid;                 /* !=0 if following fields are valid */
         unsigned long last_updated; /* In jiffies */

         u8 vin3;                    /* Register value */
         u8 voltage_min[4];          /* Register values; [0] = VDD */
         u8 voltage_max[4];          /* Register values; [0] = VDD */
         u8 fan[2];
         u8 fan_min[2];
         u8 temp;                    /* Register values */
         u8 temp_over;               /* Register values */
         u8 temp_hyst;               /* Register values */
         u8 alarms;                  /* Register value */
         u8 fan_div[2];              /* Register encoding, shifted right */
};

#ifdef MODULE
extern int init_module(void);
extern int cleanup_module(void);
#endif /* MODULE */

static int gl518_init(void);
static int gl518_cleanup(void);
static int gl518_attach_adapter(struct i2c_adapter *adapter);
static int gl518_detach_client(struct i2c_client *client);
static int gl518_command(struct i2c_client *client, unsigned int cmd,
                        void *arg);
static void gl518_inc_use (struct i2c_client *client);
static void gl518_dec_use (struct i2c_client *client);
static int gl518_read_value(struct i2c_client *client, u8 reg);
static int gl518_write_value(struct i2c_client *client, u8 reg, u16 value);
static void gl518_update_client(struct i2c_client *client);

static void gl518_vin(struct i2c_client *client, int operation, 
                      int ctl_name, int *nrels_mag, long *results);
static void gl518_fan(struct i2c_client *client, int operation, 
                      int ctl_name, int *nrels_mag, long *results);
static void gl518_temp(struct i2c_client *client, int operation, 
                       int ctl_name, int *nrels_mag, long *results);
static void gl518_fan_div(struct i2c_client *client, int operation, 
                          int ctl_name, int *nrels_mag, long *results);
static void gl518_alarms(struct i2c_client *client, int operation, 
                         int ctl_name, int *nrels_mag, long *results);

/* This is the driver that will be inserted */
static struct i2c_driver gl518_driver = {
  /* name */            "GL518SM sensor chip driver",
  /* id */              I2C_DRIVERID_GL518,
  /* flags */           DF_NOTIFY,
  /* attach_adapter */  &gl518_attach_adapter,
  /* detach_client */   &gl518_detach_client,
  /* command */         &gl518_command,
  /* inc_use */         &gl518_inc_use,
  /* dec_use */         &gl518_dec_use
};

/* These files are created for each detected GL518. This is just a template;
   though at first sight, you might think we could use a statically
   allocated list, we need some way to get back to the parent - which
   is done through one of the 'extra' fields which are initialized
   when a new copy is allocated. */
static ctl_table gl518_dir_table_template[] = {
  { GL518_SYSCTL_VIN1, "vin1", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &gl518_vin },
  { GL518_SYSCTL_VIN2, "vin2", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &gl518_vin },
  { GL518_SYSCTL_VIN3, "vin3", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &gl518_vin },
  { GL518_SYSCTL_VDD, "vdd", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &gl518_vin },
  { GL518_SYSCTL_FAN1, "fan1", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &gl518_fan },
  { GL518_SYSCTL_FAN2, "fan2", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &gl518_fan },
  { GL518_SYSCTL_TEMP, "temp", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &gl518_temp },
  { GL518_SYSCTL_FAN_DIV, "fan_div", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &gl518_fan_div },
  { GL518_SYSCTL_ALARMS, "alarms", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &gl518_alarms },
  { 0 }
};

/* Used by init/cleanup */
static int gl518_initialized = 0;

/* I choose here for semi-static LM78 allocation. Complete dynamic
   allocation could also be used; the code needed for this would probably
   take more memory than the datastructure takes now. */
#define MAX_GL518_NR 4
static struct i2c_client *gl518_list[MAX_GL518_NR];


int gl518_attach_adapter(struct i2c_adapter *adapter)
{
  int address,err,i;
  struct i2c_client *new_client;
  struct gl518_data *data;

  err = 0;

  /* OK, this is no detection. I know. It will do for now, though.  */

  /* Set err only if a global error would make registering other clients
     impossible too (like out-of-memory). */
  for (address = 0x2c; (! err) && (address <= 0x2d); address ++) {

    /* Later on, we will keep a list of registered addresses for each
       adapter, and check whether they are used here */
    
    if ((i = smbus_read_byte_data(adapter,address,GL518_REG_CHIP_ID)) < 0)
      continue;

    if (i != 0x80)
      continue;

    /* Real detection code goes here */

    /* Allocate space for a new client structure */
    if (! (new_client =  kmalloc(sizeof(struct i2c_client) +
                                sizeof(struct gl518_data),
                               GFP_KERNEL))) {
      err = -ENOMEM;
      continue;
    }

    /* Find a place in our global list */
    for (i = 0; i < MAX_GL518_NR; i++)
      if (! gl518_list[i])
         break;
    if (i == MAX_GL518_NR) {
      err = -ENOMEM;
      printk("gl518sm.o: No empty slots left, recompile and heighten "
             "MAX_GL518_NR!\n");
      goto ERROR1;
    }
    gl518_list[i] = new_client;
    
    /* Fill the new client structure with data */
    data = (struct gl518_data *) (new_client + 1);
    new_client->data = data;
    new_client->id = i;
    new_client->addr = address;
    new_client->adapter = adapter;
    new_client->driver = &gl518_driver;
    strcpy(new_client->name,"GL518SM chip");
    data->valid = 0;
    data->update_lock = MUTEX;
    
    /* Tell i2c-core a new client has arrived */
    if ((err = i2c_attach_client(new_client)))
      goto ERROR2;
    
    /* Register a new directory entry with module sensors */
    if ((err = sensors_register_entry(new_client,"gl518",
                                      gl518_dir_table_template)) < 0)
      goto ERROR3;
    data->sysctl_id = err;
    err = 0;

    /* Initialize the GL518SM chip */
    /* Power-on defaults (bit 7=1) */
    gl518_write_value(new_client,GL518_REG_CONF,0x80); 
    /* No noisy output (bit 2=1), Comparator mode (bit 3=0), two fans (bit4=0),
       standby mode (bit6=0) */
    gl518_write_value(new_client,GL518_REG_CONF,0x04); 
    gl518_write_value(new_client,GL518_REG_TEMP_HYST,
                      TEMP_TO_REG(GL518_INIT_TEMP_HYST));
    gl518_write_value(new_client,GL518_REG_TEMP_OVER,
                      TEMP_TO_REG(GL518_INIT_TEMP_OVER));
    gl518_write_value(new_client,GL518_REG_MISC,(DIV_TO_REG(2) << 6) | 
                                                (DIV_TO_REG(2) << 4) | 0x08);
    gl518_write_value(new_client,GL518_REG_FAN_LIMIT,
                      (FAN_TO_REG(GL518_INIT_FAN_MIN_1,2) << 8) |
                      FAN_TO_REG(GL518_INIT_FAN_MIN_2,2));
    gl518_write_value(new_client,GL518_REG_VIN1_LIMIT,
                      (IN_TO_REG(GL518_INIT_VIN_MIN_1) << 8) |
                      IN_TO_REG(GL518_INIT_VIN_MAX_1));
    gl518_write_value(new_client,GL518_REG_VIN2_LIMIT,
                      (IN_TO_REG(GL518_INIT_VIN_MIN_2) << 8) |
                      IN_TO_REG(GL518_INIT_VIN_MAX_2));
    gl518_write_value(new_client,GL518_REG_VIN3_LIMIT,
                      (IN_TO_REG(GL518_INIT_VIN_MIN_3) << 8) |
                      IN_TO_REG(GL518_INIT_VIN_MAX_3));
    gl518_write_value(new_client,GL518_REG_VDD_LIMIT,
                      (IN_TO_REG(GL518_INIT_VDD_MIN) << 8) |
                      IN_TO_REG(GL518_INIT_VDD_MAX));
    /* Clear status register (bit 5=1), start (bit6=1) */
    gl518_write_value(new_client,GL518_REG_CONF,0x64);

    continue;
/* OK, this is not exactly good programming practice, usually. But it is
   very code-efficient in this case. */

ERROR3:
    i2c_detach_client(new_client);
ERROR2:
    gl518_list[i] = NULL;
ERROR1:
    kfree(new_client);
  }
  return err;
}

int gl518_detach_client(struct i2c_client *client)
{
  int err,i;
  for (i = 0; i < MAX_GL518_NR; i++)
    if (client == gl518_list[i])
      break;
  if ((i == MAX_GL518_NR)) {
    printk("gl518sm.o: Client to detach not found.\n");
    return -ENOENT;
  }

  sensors_deregister_entry(((struct gl518_data *)(client->data))->sysctl_id);

  if ((err = i2c_detach_client(client))) {
    printk("gl518sm.o: Client deregistration failed, client not detached.\n");
    return err;
  }

  gl518_list[i] = NULL;
  kfree(client);
  return 0;
}


/* No commands defined yet */
int gl518_command(struct i2c_client *client, unsigned int cmd, void *arg)
{
  return 0;
}

/* Nothing here yet */
void gl518_inc_use (struct i2c_client *client)
{
#ifdef MODULE
  MOD_INC_USE_COUNT;
#endif
}

/* Nothing here yet */
void gl518_dec_use (struct i2c_client *client)
{
#ifdef MODULE
  MOD_DEC_USE_COUNT;
#endif
}

/* Registers 0x07 to 0x0c are word-sized, others are byte-sized */
int gl518_read_value(struct i2c_client *client, u8 reg)
{
  if ((reg >= 0x07) && (reg <= 0x0c)) 
    return smbus_read_word_data(client->adapter,client->addr,reg);
  else
    return smbus_read_byte_data(client->adapter,client->addr,reg);
}

int gl518_write_value(struct i2c_client *client, u8 reg, u16 value)
{
  if ((reg >= 0x07) && (reg <= 0x0c)) 
    return smbus_write_word_data(client->adapter,client->addr,reg,value);
  else
    return smbus_write_byte_data(client->adapter,client->addr,reg,value);
}

void gl518_update_client(struct i2c_client *client)
{
  struct gl518_data *data = client->data;
  int val;

  down(&data->update_lock);

  if ((jiffies - data->last_updated > HZ+HZ/2 ) ||
      (jiffies < data->last_updated) || ! data->valid) {

#ifdef DEBUG
    printk("Starting gl518 update\n");
#endif

    data->vin3 = gl518_read_value(client,GL518_REG_VOLT);

    val = gl518_read_value(client,GL518_REG_VDD_LIMIT);
    data->voltage_min[0] = val & 0xff;
    data->voltage_max[0] = (val >> 8) & 0xff;
    val = gl518_read_value(client,GL518_REG_VIN1_LIMIT);
    data->voltage_min[1] = val & 0xff;
    data->voltage_max[1] = (val >> 8) & 0xff;
    val = gl518_read_value(client,GL518_REG_VIN2_LIMIT);
    data->voltage_min[2] = val & 0xff;
    data->voltage_max[2] = (val >> 8) & 0xff;
    val = gl518_read_value(client,GL518_REG_VIN3_LIMIT);
    data->voltage_min[3] = val & 0xff;
    data->voltage_max[3] = (val >> 8) & 0xff;

    val = gl518_read_value(client,GL518_REG_FAN_COUNT);
    data->fan[0] = (val >> 8) & 0xff;
    data->fan[1] = val & 0xff;

    val = gl518_read_value(client,GL518_REG_FAN_LIMIT);
    data->fan_min[0] = (val >> 8) & 0xff;
    data->fan_min[1] = val & 0xff;

    data->temp = gl518_read_value(client,GL518_REG_TEMP);
    data->temp_over = gl518_read_value(client,GL518_REG_TEMP_OVER);
    data->temp_hyst = gl518_read_value(client,GL518_REG_TEMP_HYST);

    data->alarms = gl518_read_value(client,GL518_REG_ALARM);

    val = gl518_read_value(client,GL518_REG_MISC);
    data->fan_div[0] = (val >> 4) & 0x03;
    data->fan_div[1] = (val >> 6) & 0x03;

    data->last_updated = jiffies;
    data->valid = 1;
  }

  up(&data->update_lock);
}


void gl518_temp(struct i2c_client *client, int operation, int ctl_name,
                int *nrels_mag, long *results)
{
  struct gl518_data *data = client->data;
  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 1;
  else if (operation == SENSORS_PROC_REAL_READ) {
    gl518_update_client(client);
    results[0] = TEMP_FROM_REG(data->temp_over);
    results[1] = TEMP_FROM_REG(data->temp_hyst);
    results[2] = TEMP_FROM_REG(data->temp);
    *nrels_mag = 3;
  } else if (operation == SENSORS_PROC_REAL_WRITE) {
    if (*nrels_mag >= 1) {
      data->temp_over = TEMP_TO_REG(results[0]);
      gl518_write_value(client,GL518_REG_TEMP_OVER,data->temp_over);
    }
    if (*nrels_mag >= 2) {
      data->temp_over = TEMP_TO_REG(results[1]);
      gl518_write_value(client,GL518_REG_TEMP_HYST,data->temp_over);
    }
  }
}

void gl518_vin(struct i2c_client *client, int operation, int ctl_name, 
               int *nrels_mag, long *results)
{
  struct gl518_data *data = client->data;
  int nr = ctl_name - GL518_SYSCTL_VDD;
  int regnr,old=0;

  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 2;
  else if (operation == SENSORS_PROC_REAL_READ) {
    gl518_update_client(client);
    results[0] = IN_FROM_REG(data->voltage_min[nr]);
    results[1] = IN_FROM_REG(data->voltage_max[nr]);
    if (nr == 3)
      results[2] = IN_FROM_REG(data->vin3);
    else
      results[2] = 0;
    *nrels_mag = 3;
  } else if (operation == SENSORS_PROC_REAL_WRITE) {
    regnr=nr==0?GL518_REG_VDD_LIMIT:nr==1?GL518_REG_VIN1_LIMIT:nr==2?
                GL518_REG_VIN2_LIMIT:GL518_REG_VIN3_LIMIT;
    if (*nrels_mag == 1)
      old = gl518_read_value(client,regnr) & 0xff00;
    if (*nrels_mag >= 2) {
      data->voltage_max[nr] = IN_TO_REG(results[1]);
      old = data->voltage_max[nr] << 8;
    }
    if (*nrels_mag >= 1) {
      data->voltage_min[nr] = IN_TO_REG(results[0]);
      old |= data->voltage_min[nr];
      gl518_write_value(client,regnr,old);
    }
  }
} 


void gl518_fan(struct i2c_client *client, int operation, int ctl_name, 
               int *nrels_mag, long *results)
{
  struct gl518_data *data = client->data;
  int nr = ctl_name - GL518_SYSCTL_FAN1;
  int old;

  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 0;
  else if (operation == SENSORS_PROC_REAL_READ) {
    gl518_update_client(client);
    results[0] = FAN_FROM_REG(data->fan_min[nr],data->fan_div[nr]);
    results[1] = FAN_FROM_REG(data->fan[nr],data->fan_div[nr]);
    *nrels_mag = 2;
  } else if (operation == SENSORS_PROC_REAL_WRITE) {
    if (*nrels_mag >= 1) {
      data->fan_min[nr] = FAN_TO_REG(results[0],data->fan_div[nr]);
      old = gl518_read_value(client,GL518_REG_FAN_LIMIT);
      if (nr == 0)
        old = (old & 0x00ff) | (data->fan_min[nr] << 8);
      else
        old = (old & 0xff00) | data->fan_min[nr];
      gl518_write_value(client,GL518_REG_FAN_LIMIT,old);
    }
  }
}


void gl518_alarms(struct i2c_client *client, int operation, int ctl_name, 
                  int *nrels_mag, long *results)
{
  struct gl518_data *data = client->data;
  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 0;
  else if (operation == SENSORS_PROC_REAL_READ) {
    gl518_update_client(client);
    results[0] = data->alarms;
    *nrels_mag = 1;
  }
}

void gl518_fan_div(struct i2c_client *client, int operation, int ctl_name,
                   int *nrels_mag, long *results)
{
  struct gl518_data *data = client->data;
  int old;
  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 0;
  else if (operation == SENSORS_PROC_REAL_READ) {
    gl518_update_client(client);
    results[0] = DIV_FROM_REG(data->fan_div[0]);
    results[1] = DIV_FROM_REG(data->fan_div[1]);
    *nrels_mag = 2;
  } else if (operation == SENSORS_PROC_REAL_WRITE) {
    old = gl518_read_value(client,GL518_REG_MISC);
    if (*nrels_mag >= 2) {
      data->fan_div[1] = DIV_TO_REG(results[1]);
      old = (old & 0x3f) | (data->fan_div[1] << 6);
    }
    if (*nrels_mag >= 1) {
      data->fan_div[1] = DIV_TO_REG(results[0]);
      old = (old & 0xcf) | (data->fan_div[0] << 4);
      gl518_write_value(client,GL518_REG_MISC,old);
    }
  }
}


int gl518_init(void)
{
  int res;

  printk("gl518sm.o version %s (%s)\n",LM_VERSION,LM_DATE);
  gl518_initialized = 0;
  if ((res = i2c_add_driver(&gl518_driver))) {
    printk("gl518sm.o: Driver registration failed, module not inserted.\n");
    gl518_cleanup();
    return res;
  }
  gl518_initialized ++;
  return 0;
}

int gl518_cleanup(void)
{
  int res;

  if (gl518_initialized >= 1) {
    if ((res = i2c_del_driver(&gl518_driver))) {
      printk("gl518.o: Driver deregistration failed, module not removed.\n");
      return res;
    }
    gl518_initialized --;
  }

  return 0;
}


#ifdef MODULE

MODULE_AUTHOR("Frodo Looijaard <frodol@dds.nl>");
MODULE_DESCRIPTION("GL518SM driver");

int init_module(void)
{
  return gl518_init();
}

int cleanup_module(void)
{
  return gl518_cleanup();
}

#endif /* MODULE */

