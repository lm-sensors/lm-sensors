/*
  fscher.c - Part of lm_sensors, Linux kernel modules for hardware
  monitoring
  Copyright (C) 2003, 2004 Reinhard Nissl <rnissl@gmx.de>
  
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
   fujitsu siemens hermes chip, 
   module based on fscpos.c 
   Copyright (C) 2000 Hermann Jung <hej@odn.de>
   Copyright (C) 1998, 1999 Frodo Looijaard <frodol@dds.nl>
   and Philip Edelbrock <phil@netroedge.com>
*/

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/i2c-proc.h>
#include <linux/init.h>
#include "version.h"

#ifndef I2C_DRIVERID_FSCHER
#define I2C_DRIVERID_FSCHER		1046
#endif

/* Addresses to scan */
static unsigned short normal_i2c[] = { 0x73, SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { SENSORS_I2C_END };
static unsigned int normal_isa[] = { SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/* Insmod parameters */
SENSORS_INSMOD_1(fscher);

/* The FSCHER registers */

/* chip identification */
#define FSCHER_REG_IDENT_0       0x00
#define FSCHER_REG_IDENT_1       0x01
#define FSCHER_REG_IDENT_2       0x02
#define FSCHER_REG_REVISION      0x03

/* global control and status */
#define FSCHER_REG_EVENT_STATE   0x04
#define FSCHER_REG_CONTROL       0x05

/* watchdog */
#define FSCHER_REG_WDOG_PRESET   0x28
#define FSCHER_REG_WDOG_STATE    0x23
#define FSCHER_REG_WDOG_CONTROL  0x21

/* fan 0  */
#define FSCHER_REG_FAN0_MIN      0x55
#define FSCHER_REG_FAN0_ACT      0x0e
#define FSCHER_REG_FAN0_STATE    0x0d
#define FSCHER_REG_FAN0_RIPPLE   0x0f

/* fan 1  */
#define FSCHER_REG_FAN1_MIN      0x65
#define FSCHER_REG_FAN1_ACT      0x6b
#define FSCHER_REG_FAN1_STATE    0x62
#define FSCHER_REG_FAN1_RIPPLE   0x6f

/* fan 2  */
#define FSCHER_REG_FAN2_MIN      0xb5
#define FSCHER_REG_FAN2_ACT      0xbb
#define FSCHER_REG_FAN2_STATE    0xb2
#define FSCHER_REG_FAN2_RIPPLE   0xbf

/* voltage supervision */
#define FSCHER_REG_VOLT_12       0x45
#define FSCHER_REG_VOLT_5        0x42
#define FSCHER_REG_VOLT_BATT     0x48

/* temperatures */
/* sensor 0 */
#define FSCHER_REG_TEMP0_ACT     0x64
#define FSCHER_REG_TEMP0_STATE   0x71

/* sensor 1 */
#define FSCHER_REG_TEMP1_ACT     0x32
#define FSCHER_REG_TEMP1_STATE   0x81

/* sensor 2 */
#define FSCHER_REG_TEMP2_ACT     0x35
#define FSCHER_REG_TEMP2_STATE   0x91



/* Initial limits */

/* For each registered FSCHER, we need to keep some data in memory. It is
   dynamically allocated, at the same time when a new fscher client is
   allocated. */
struct fscher_data {
  struct i2c_client client;
  int sysctl_id;

  struct semaphore update_lock;
  char valid;	     /* !=0 if following fields are valid */
  unsigned long last_updated;	/* In jiffies */

  u8  revision;        /* revision of chip */
  u8  global_event;    /* global event status */
  u8  global_control;  /* global control register */
  u8  watchdog[3];     /* watchdog */
  u8  volt[3];         /* 12, 5, battery current */ 
  u8  temp_act[3];     /* temperature */
  u8  temp_status[3];  /* status of sensor */
  u8  fan_act[3];      /* fans revolutions per second */
  u8  fan_status[3];   /* fan status */
  u8  fan_min[3];      /* fan min value for rps */
  u8  fan_ripple[3];   /* divider for rps */
};


static int fscher_attach_adapter(struct i2c_adapter *adapter);
static int fscher_detect(struct i2c_adapter *adapter, int address,
                         unsigned short flags, int kind);
static int fscher_detach_client(struct i2c_client *client);

static int fscher_read_value(struct i2c_client *client, u8 reg);
static int fscher_write_value(struct i2c_client *client, u8 reg,
                              u8 value);
static void fscher_update_client(struct i2c_client *client);
static void fscher_init_client(struct i2c_client *client);


static void fscher_in(struct i2c_client *client, int operation, int ctl_name,
                      int *nrels_mag, long *results);
static void fscher_pwm(struct i2c_client *client, int operation,
                       int ctl_name, int *nrels_mag, long *results);
static void fscher_pwm_internal(struct i2c_client *client, int operation,
                                int ctl_name, int *nrels_mag, long *results, 
                                int nr, int reg_min);
static void fscher_fan(struct i2c_client *client, int operation,
                       int ctl_name, int *nrels_mag, long *results);
static void fscher_fan_internal(struct i2c_client *client, int operation,
                                int ctl_name, int *nrels_mag, long *results, 
                                int nr, int reg_state, int res_ripple);
static void fscher_temp(struct i2c_client *client, int operation,
		      	int ctl_name, int *nrels_mag, long *results);
static void fscher_volt(struct i2c_client *client, int operation,
		      	int ctl_name, int *nrels_mag, long *results);
static void fscher_wdog(struct i2c_client *client, int operation,
		      	int ctl_name, int *nrels_mag, long *results);

static struct i2c_driver fscher_driver = {
  .name		        = "FSCHER sensor driver",
  .id		        = I2C_DRIVERID_FSCHER,
  .flags		= I2C_DF_NOTIFY,
  .attach_adapter	= fscher_attach_adapter,
  .detach_client	= fscher_detach_client,
};

/* -- SENSORS SYSCTL START -- */
#define FSCHER_SYSCTL_VOLT0    1000       /* 12 volt supply */
#define FSCHER_SYSCTL_VOLT1    1001       /* 5 volt supply */
#define FSCHER_SYSCTL_VOLT2    1002       /* batterie voltage */
#define FSCHER_SYSCTL_FAN0     1101       /* state, ripple, actual value
                                             fan 0 */
#define FSCHER_SYSCTL_FAN1     1102       /* state, ripple, actual value
                                             fan 1 */
#define FSCHER_SYSCTL_FAN2     1103       /* state, ripple, actual value
                                             fan 2 */
#define FSCHER_SYSCTL_TEMP0    1201       /* state and value of sensor 0,
                                             cpu die */
#define FSCHER_SYSCTL_TEMP1    1202       /* state and value of sensor 1,
                                             motherboard */
#define FSCHER_SYSCTL_TEMP2    1203       /* state and value of sensor 2,
                                             chassis */
#define FSCHER_SYSCTL_PWM0     1301       /* min fan 0 */
#define FSCHER_SYSCTL_PWM1     1302       /* min fan 1 */
#define FSCHER_SYSCTL_PWM2     1303       /* min fan 2 */
#define FSCHER_SYSCTL_REV      2000       /* revision */
#define FSCHER_SYSCTL_EVENT    2001       /* global event status */
#define FSCHER_SYSCTL_CONTROL  2002       /* global control byte */
#define FSCHER_SYSCTL_WDOG     2003       /* watch dog preset, state and
                                             control */
/* -- SENSORS SYSCTL END -- */

/* These files are created for each detected FSCHER. This is just a template;
   though at first sight, you might think we could use a statically
   allocated list, we need some way to get back to the parent - which
   is done through one of the 'extra' fields which are initialized 
   when a new copy is allocated. */
static ctl_table fscher_dir_table_template[] = {
  {FSCHER_SYSCTL_REV, "rev", NULL, 0, 0444, NULL, &i2c_proc_real,
   &i2c_sysctl_real, NULL, &fscher_in},
  {FSCHER_SYSCTL_EVENT, "alarms", NULL, 0, 0444, NULL, &i2c_proc_real,
   &i2c_sysctl_real, NULL, &fscher_in},
  {FSCHER_SYSCTL_CONTROL, "control", NULL, 0, 0644, NULL, &i2c_proc_real,
   &i2c_sysctl_real, NULL, &fscher_in},
  {FSCHER_SYSCTL_TEMP0, "temp1", NULL, 0, 0644, NULL, &i2c_proc_real,
   &i2c_sysctl_real, NULL, &fscher_temp},
  {FSCHER_SYSCTL_TEMP1, "temp2", NULL, 0, 0644, NULL, &i2c_proc_real,
   &i2c_sysctl_real, NULL, &fscher_temp},
  {FSCHER_SYSCTL_TEMP2, "temp3", NULL, 0, 0644, NULL, &i2c_proc_real,
   &i2c_sysctl_real, NULL, &fscher_temp},
  {FSCHER_SYSCTL_VOLT0, "in0", NULL, 0, 0444, NULL, &i2c_proc_real,
   &i2c_sysctl_real, NULL, &fscher_volt},
  {FSCHER_SYSCTL_VOLT1, "in1", NULL, 0, 0444, NULL, &i2c_proc_real,
   &i2c_sysctl_real, NULL, &fscher_volt},
  {FSCHER_SYSCTL_VOLT2, "in2", NULL, 0, 0444, NULL, &i2c_proc_real,
   &i2c_sysctl_real, NULL, &fscher_volt},
  {FSCHER_SYSCTL_FAN0, "fan1", NULL, 0, 0644, NULL, &i2c_proc_real,
   &i2c_sysctl_real, NULL, &fscher_fan},
  {FSCHER_SYSCTL_FAN1, "fan2", NULL, 0, 0644, NULL, &i2c_proc_real,
   &i2c_sysctl_real, NULL, &fscher_fan},
  {FSCHER_SYSCTL_FAN2, "fan3", NULL, 0, 0644, NULL, &i2c_proc_real,
   &i2c_sysctl_real, NULL, &fscher_fan},
  {FSCHER_SYSCTL_PWM0, "pwm1", NULL, 0, 0644, NULL, &i2c_proc_real,
   &i2c_sysctl_real, NULL, &fscher_pwm},
  {FSCHER_SYSCTL_PWM1, "pwm2", NULL, 0, 0644, NULL, &i2c_proc_real,
   &i2c_sysctl_real, NULL, &fscher_pwm},
  {FSCHER_SYSCTL_PWM2, "pwm3", NULL, 0, 0644, NULL, &i2c_proc_real,
   &i2c_sysctl_real, NULL, &fscher_pwm},
  {FSCHER_SYSCTL_WDOG, "wdog", NULL, 0, 0644, NULL, &i2c_proc_real,
   &i2c_sysctl_real, NULL, &fscher_wdog},
  {0}
};

static int fscher_attach_adapter(struct i2c_adapter *adapter)
{
  return i2c_detect(adapter, &addr_data, fscher_detect);
}

int fscher_detect(struct i2c_adapter *adapter, int address,
                  unsigned short flags, int kind)
{
  int i;
  struct i2c_client *new_client;
  struct fscher_data *data;
  int err = 0;
  const char *type_name, *client_name;

  /* Make sure we aren't probing the ISA bus!! This is just a safety
     check at this moment; i2c_detect really won't call us. */
#ifdef DEBUG
  if (i2c_is_isa_adapter(adapter)) {
    printk("fscher.o: fscher_detect called for an ISA bus adapter?!?\n");
    return 0;
  }
#endif

  if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
    goto ERROR0;

  /* OK. For now, we presume we have a valid client. We now create the
     client structure, even though we cannot fill it completely yet.
     But it allows us to access fscher_{read,write}_value. */
  if (!(data = kmalloc(sizeof(struct fscher_data), GFP_KERNEL))) {
    err = -ENOMEM;
    goto ERROR0;
  }

  new_client = &data->client;
  new_client->addr = address;
  new_client->data = data;
  new_client->adapter = adapter;
  new_client->driver = &fscher_driver;
  new_client->flags = 0;

  /* Do the remaining detection unless force or force_fscher parameter */
  if (kind < 0) {
    if (fscher_read_value(new_client, FSCHER_REG_IDENT_0) != 0x48) /* 'H' */
      goto ERROR1;
    if (fscher_read_value(new_client, FSCHER_REG_IDENT_1) != 0x45) /* 'E' */
      goto ERROR1;
    if (fscher_read_value(new_client, FSCHER_REG_IDENT_2) != 0x52) /* 'R' */
      goto ERROR1;
  }

  kind = fscher;

  type_name = "fscher";
  client_name = "fsc hermes chip";

  /* Fill in the remaining client fields and put it into the
     global list */
  strcpy(new_client->name, client_name);
  data->valid = 0;
  init_MUTEX(&data->update_lock);

  /* Tell the I2C layer a new client has arrived */
  if ((err = i2c_attach_client(new_client)))
    goto ERROR3;

  /* Register a new directory entry with module sensors */
  if ((i = i2c_register_entry(new_client, type_name,
                              fscher_dir_table_template,
			      THIS_MODULE)) < 0) {
    err = i;
    goto ERROR4;
  }
  data->sysctl_id = i;

  fscher_init_client(new_client);
  return 0;

/* OK, this is not exactly good programming practice, usually. But it is
   very code-efficient in this case. */
 ERROR4:
  i2c_detach_client(new_client);
 ERROR3:
 ERROR1:
  kfree(data);
 ERROR0:
  return err;
}

static int fscher_detach_client(struct i2c_client *client)
{
  int err;

  i2c_deregister_entry(((struct fscher_data *) (client->data))->sysctl_id);

  if ((err = i2c_detach_client(client))) {
    printk("fscher.o: Client deregistration failed, client not detached.\n");
    return err;
  }

  kfree(client->data);

  return 0;
}

static int fscher_read_value(struct i2c_client *client, u8 reg)
{
#ifdef DEBUG
  printk("fscher: read reg 0x%02x\n",reg);
#endif
  return i2c_smbus_read_byte_data(client, reg);
}

static int fscher_write_value(struct i2c_client *client, u8 reg, u8 value)
{
#ifdef DEBUG
  printk("fscher: write reg 0x%02x, val 0x%02x\n",reg, value);
#endif
  return i2c_smbus_write_byte_data(client, reg, value);
}

/* Called when we have found a new FSCHER. */
static void fscher_init_client(struct i2c_client *client)
{
  struct fscher_data *data = client->data;

  /* read revision from chip */
  data->revision =  fscher_read_value(client,FSCHER_REG_REVISION);
}

static void fscher_update_client(struct i2c_client *client)
{
  struct fscher_data *data = client->data;

  down(&data->update_lock);

  if ((jiffies - data->last_updated > 2 * HZ) ||
      (jiffies < data->last_updated) || !data->valid) {

#ifdef DEBUG
    printk("Starting fscher update\n");
#endif
    data->temp_act[0] = fscher_read_value(client, FSCHER_REG_TEMP0_ACT);
    data->temp_act[1] = fscher_read_value(client, FSCHER_REG_TEMP1_ACT);
    data->temp_act[2] = fscher_read_value(client, FSCHER_REG_TEMP2_ACT);
    data->temp_status[0] = fscher_read_value(client, FSCHER_REG_TEMP0_STATE);
    data->temp_status[1] = fscher_read_value(client, FSCHER_REG_TEMP1_STATE);
    data->temp_status[2] = fscher_read_value(client, FSCHER_REG_TEMP2_STATE);

    data->volt[0] = fscher_read_value(client, FSCHER_REG_VOLT_12);
    data->volt[1] = fscher_read_value(client, FSCHER_REG_VOLT_5);
    data->volt[2] = fscher_read_value(client, FSCHER_REG_VOLT_BATT);

    data->fan_act[0] = fscher_read_value(client, FSCHER_REG_FAN0_ACT);
    data->fan_act[1] = fscher_read_value(client, FSCHER_REG_FAN1_ACT);
    data->fan_act[2] = fscher_read_value(client, FSCHER_REG_FAN2_ACT);
    data->fan_status[0] = fscher_read_value(client, FSCHER_REG_FAN0_STATE);
    data->fan_status[1] = fscher_read_value(client, FSCHER_REG_FAN1_STATE);
    data->fan_status[2] = fscher_read_value(client, FSCHER_REG_FAN2_STATE);
    data->fan_min[0] = fscher_read_value(client, FSCHER_REG_FAN0_MIN);
    data->fan_min[1] = fscher_read_value(client, FSCHER_REG_FAN1_MIN);
    data->fan_min[2] = fscher_read_value(client, FSCHER_REG_FAN2_MIN);
    data->fan_ripple[0] = fscher_read_value(client, FSCHER_REG_FAN0_RIPPLE);
    data->fan_ripple[1] = fscher_read_value(client, FSCHER_REG_FAN1_RIPPLE);
    data->fan_ripple[2] = fscher_read_value(client, FSCHER_REG_FAN2_RIPPLE);

    data->watchdog[0] = fscher_read_value(client, FSCHER_REG_WDOG_PRESET);
    data->watchdog[1] = fscher_read_value(client, FSCHER_REG_WDOG_STATE);
    data->watchdog[2] = fscher_read_value(client, FSCHER_REG_WDOG_CONTROL);

    data->global_event = fscher_read_value(client, FSCHER_REG_EVENT_STATE);
    data->global_control = fscher_read_value(client, FSCHER_REG_CONTROL);

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
void fscher_in(struct i2c_client *client, int operation, int ctl_name,
               int *nrels_mag, long *results)
{
  struct fscher_data *data = client->data;

  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 0;
  else if (operation == SENSORS_PROC_REAL_READ) {
    fscher_update_client(client);
    switch(ctl_name) {
    case FSCHER_SYSCTL_REV:
      results[0] = data->revision ;
      break;
    case FSCHER_SYSCTL_EVENT:
      /* bits 6, 5 and 2 are reserved => mask with 0x9b */
      results[0] = data->global_event & 0x9b;
      break;
    case FSCHER_SYSCTL_CONTROL:
      results[0] = data->global_control & 0x01;
      break;
    default:
      printk("fscher: ctl_name %d not supported\n", ctl_name);
      *nrels_mag = 0;
      return;
    }
    *nrels_mag = 1;
  } else if (operation == SENSORS_PROC_REAL_WRITE) {
    if((ctl_name == FSCHER_SYSCTL_CONTROL) && (*nrels_mag >= 1)) {
      data->global_control = (results[0] & 0x01);
      printk("fscher: writing 0x%02x to global_control\n",
             data->global_control);
      fscher_write_value(client,FSCHER_REG_CONTROL,
                         data->global_control);
    }
    else
      printk("fscher: writing to chip not supported\n");
  }
}


#define TEMP_FROM_REG(val)    (val-128)

void fscher_temp(struct i2c_client *client, int operation, int ctl_name,
                 int *nrels_mag, long *results)
{
  struct fscher_data *data = client->data;

  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 0;
  else if (operation == SENSORS_PROC_REAL_READ) {
    fscher_update_client(client);
    switch(ctl_name) {
    case FSCHER_SYSCTL_TEMP0:
      results[0] = data->temp_status[0] & 0x03;
      results[1] = TEMP_FROM_REG(data->temp_act[0]);
      break;
    case FSCHER_SYSCTL_TEMP1:
      results[0] = data->temp_status[1] & 0x03;
      results[1] = TEMP_FROM_REG(data->temp_act[1]);
      break;
    case FSCHER_SYSCTL_TEMP2:
      results[0] = data->temp_status[2] & 0x03;
      results[1] = TEMP_FROM_REG(data->temp_act[2]);
      break;
    default:
      printk("fscher: ctl_name %d not supported\n", ctl_name);
      *nrels_mag = 0;
      return;
    }
    *nrels_mag = 2;
  } else if (operation == SENSORS_PROC_REAL_WRITE) {
    if(*nrels_mag >= 1) {
      switch(ctl_name) {
      case FSCHER_SYSCTL_TEMP0:
        data->temp_status[0] = (data->temp_status[0] & ~0x02)
          | (results[0] & 0x02);
        printk("fscher: writing value 0x%02x to temp0_status\n",
               data->temp_status[0]);
        fscher_write_value(client, FSCHER_REG_TEMP0_STATE,
                           data->temp_status[0] & 0x02);
        break;
      case FSCHER_SYSCTL_TEMP1:
        data->temp_status[1] = (data->temp_status[1] & ~0x02)
          | (results[0] & 0x02);
        printk("fscher: writing value 0x%02x to temp1_status\n",
               data->temp_status[1]);
        fscher_write_value(client, FSCHER_REG_TEMP1_STATE,
                           data->temp_status[1] & 0x02);
        break;
      case FSCHER_SYSCTL_TEMP2:
        data->temp_status[2] = (data->temp_status[2] & ~0x02)
          | (results[0] & 0x02);
        printk("fscher: writing value 0x%02x to temp2_status\n",
               data->temp_status[2]);
        fscher_write_value(client, FSCHER_REG_TEMP2_STATE,
                           data->temp_status[2] & 0x02);
        break;
      default:
        printk("fscher: ctl_name %d not supported\n",ctl_name);
      }
    }
    else
      printk("fscher: writing to chip not supported\n");
  }
}

/*
 * The final conversion is specified in sensors.conf, as it depends on
 * mainboard specific values. We export the registers contents as
 * pseudo-hundredths-of-Volts (range 0V - 2.55V). Not that it makes much
 * sense per se, but it minimizes the conversions count and keeps the
 * values within a usual range.
 */
void fscher_volt(struct i2c_client *client, int operation, int ctl_name,
                 int *nrels_mag, long *results)
{
  struct fscher_data *data = client->data;

  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 2;
  else if (operation == SENSORS_PROC_REAL_READ) {
    fscher_update_client(client);
    switch(ctl_name) {
    case FSCHER_SYSCTL_VOLT0:
      results[0] = data->volt[0];
      break;
    case FSCHER_SYSCTL_VOLT1:
      results[0] = data->volt[1];
      break;
    case FSCHER_SYSCTL_VOLT2:
      results[0] = data->volt[2];
      break;
    default:
      printk("fscher: ctl_name %d not supported\n", ctl_name);
      *nrels_mag = 0;
      return;
    }
    *nrels_mag = 1;
  } else if (operation == SENSORS_PROC_REAL_WRITE) {
    printk("fscher: writing to chip not supported\n");
  }
}

void fscher_pwm(struct i2c_client *client, int operation, int ctl_name,
                int *nrels_mag, long *results)
{

  switch(ctl_name) {
  case FSCHER_SYSCTL_PWM0:
    fscher_pwm_internal(client,operation,ctl_name,nrels_mag,results,
                        0,FSCHER_REG_FAN0_MIN);
    break;
  case FSCHER_SYSCTL_PWM1:
    fscher_pwm_internal(client,operation,ctl_name,nrels_mag,results,
                        1,FSCHER_REG_FAN1_MIN);
    break;
  case FSCHER_SYSCTL_PWM2:
    fscher_pwm_internal(client,operation,ctl_name,nrels_mag,results,
                        2,FSCHER_REG_FAN2_MIN);
    break;
  default:
    printk("fscher: illegal pwm nr %d\n",ctl_name);
  }
}
			
void fscher_pwm_internal(struct i2c_client *client, int operation,
                         int ctl_name, int *nrels_mag, long *results,
                         int nr, int reg_min)
{
  struct fscher_data *data = client->data;

  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 0;
  else if (operation == SENSORS_PROC_REAL_READ) {
    fscher_update_client(client);
    results[0] = data->fan_min[nr];
    *nrels_mag = 1;
  } else if (operation == SENSORS_PROC_REAL_WRITE) {
    if(*nrels_mag >= 1) {  
      data->fan_min[nr] = results[0];
      printk("fscher: writing value 0x%02x to fan%d_min\n",
             data->fan_min[nr],nr);
      fscher_write_value(client,reg_min,data->fan_min[nr]);
    }
  }
}

void fscher_fan(struct i2c_client *client, int operation, int ctl_name,
                int *nrels_mag, long *results)
{
  switch(ctl_name) {
  case FSCHER_SYSCTL_FAN0:
    fscher_fan_internal(client,operation,ctl_name,nrels_mag,results,
                        0,FSCHER_REG_FAN0_STATE, FSCHER_REG_FAN0_RIPPLE);
    break;
  case FSCHER_SYSCTL_FAN1:
    fscher_fan_internal(client,operation,ctl_name,nrels_mag,results,
                        1,FSCHER_REG_FAN1_STATE, FSCHER_REG_FAN1_RIPPLE);
    break;
  case FSCHER_SYSCTL_FAN2:
    fscher_fan_internal(client,operation,ctl_name,nrels_mag,results,
                        2,FSCHER_REG_FAN2_STATE, FSCHER_REG_FAN2_RIPPLE);
    break;
  default:
    printk("fscher: illegal fan nr %d\n",ctl_name);
  }
}
			
#define RPM_FROM_REG(val)   (val*60)

void fscher_fan_internal(struct i2c_client *client, int operation,
                         int ctl_name, int *nrels_mag, long *results,
                         int nr, int reg_state, int reg_ripple)
{
  struct fscher_data *data = client->data;

  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 0;
  else if (operation == SENSORS_PROC_REAL_READ) {
    fscher_update_client(client);
    results[0] = data->fan_status[nr] & 0x04;
    results[1] = data->fan_ripple[nr] & 0x03;
    results[2] = RPM_FROM_REG(data->fan_act[nr]);
    *nrels_mag = 3;
  } else if (operation == SENSORS_PROC_REAL_WRITE) {
    if(*nrels_mag >= 1) {
      data->fan_status[nr] = results[0] & 0x04;
      printk("fscher: writing value 0x%02x to fan%d_status\n",
             data->fan_status[nr],nr);
      fscher_write_value(client,reg_state,data->fan_status[nr]);
    }
    if(*nrels_mag >= 2) {
      if((results[1] & 0x03) == 0) {
        printk("fscher: fan%d ripple 0 not allowed\n",nr);
        return;
      }
      data->fan_ripple[nr] = results[1] & 0x03;
      printk("fscher: writing value 0x%02x to fan%d_ripple\n",
             data->fan_ripple[nr],nr);
      fscher_write_value(client,reg_ripple,data->fan_ripple[nr]);
    }	
  }
}

void fscher_wdog(struct i2c_client *client, int operation, int ctl_name,
                 int *nrels_mag, long *results)
{
  struct fscher_data *data = client->data;

  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 0;
  else if (operation == SENSORS_PROC_REAL_READ) {
    fscher_update_client(client);
    results[0] = data->watchdog[0] ;
    results[1] = data->watchdog[1] & 0x02;
    results[2] = data->watchdog[2] & 0xd0;
    *nrels_mag = 3;
  } else if (operation == SENSORS_PROC_REAL_WRITE) {
    if (*nrels_mag >= 1) {
      data->watchdog[0] = results[0] & 0xff;
      printk("fscher: writing value 0x%02x to wdog_preset\n",
             data->watchdog[0]); 
      fscher_write_value(client,FSCHER_REG_WDOG_PRESET,data->watchdog[0]);
    } 
    if (*nrels_mag >= 2) {
      data->watchdog[1] = results[1] & 0x02;
      printk("fscher: writing value 0x%02x to wdog_state\n",
             data->watchdog[1]); 
      fscher_write_value(client,FSCHER_REG_WDOG_STATE,data->watchdog[1]);
    }
    if (*nrels_mag >= 3) {
      data->watchdog[2] = results[2] & 0xf0;
      printk("fscher: writing value 0x%02x to wdog_control\n",
             data->watchdog[2]); 
      fscher_write_value(client,FSCHER_REG_WDOG_CONTROL,data->watchdog[2]);
    }
  }
}

static int __init sm_fscher_init(void)
{
  printk("fscher.o version %s (%s)\n", LM_VERSION, LM_DATE);
  return i2c_add_driver(&fscher_driver);
}

static void __exit sm_fscher_exit(void)
{
  i2c_del_driver(&fscher_driver);
}



MODULE_AUTHOR("Reinhard Nissl <rnissl@gmx.de> based on work from Hermann"
              " Jung <hej@odn.de>, Frodo Looijaard <frodol@dds.nl> and"
              " Philip Edelbrock <phil@netroedge.com>");
MODULE_DESCRIPTION("fujitsu siemens hermes chip driver");
MODULE_LICENSE("GPL");

module_init(sm_fscher_init);
module_exit(sm_fscher_exit);
