/*
    smartbatt.c - Part of lm_sensors, Linux kernel modules for hardware
             monitoring
    Copyright (c) 2002  M. D. Studebaker <mdsxyz123@yahoo.com>

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

    With GPL code from:
	   battery.c
	   Copyright (C) 2000 Linuxcare, Inc. 
	   battery.h
	   Copyright (C) 2000 Hypercore Software Design, Ltd.
*/

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/i2c-proc.h>
#include <linux/init.h>
#include "version.h"


/* Addresses to scan */
static unsigned short normal_i2c[] = { 0x0b, SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { SENSORS_I2C_END };
static unsigned int normal_isa[] = { SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/* Insmod parameters */
SENSORS_INSMOD_1(smartbatt);

/* Conversions */
#define TEMP_FROM_REG(r) (r - 2732)  /* tenths of degree kelvin to celsius */

/* The SMARTBATT registers */
#define SMARTBATT_REG_MODE 0x03
#define SMARTBATT_REG_TEMP 0x08
#define SMARTBATT_REG_V 0x09
#define SMARTBATT_REG_I 0x0a
#define SMARTBATT_REG_AVGI 0x0b
#define SMARTBATT_REG_RELCHG 0x0d
#define SMARTBATT_REG_ABSCHG 0x0e
#define SMARTBATT_REG_RUNTIME_E 0x11
#define SMARTBATT_REG_AVGTIME_E 0x12
#define SMARTBATT_REG_AVGTIME_F 0x13
#define SMARTBATT_REG_STATUS 0x16
#define SMARTBATT_REG_DESV 0x19
#define SMARTBATT_REG_DATE 0x1b
#define SMARTBATT_REG_SERIAL 0x1c
#define SMARTBATT_REG_MANUF 0x20
#define SMARTBATT_REG_NAME 0x21
#define SMARTBATT_REG_CHEM 0x22

#define COMM_TIMEOUT 16
#define BATTERY_STRING_MAX	33

/* Each client has this additional data */
struct smartbatt_data {
	int sysctl_id;

	struct semaphore update_lock;
	char valid;		/* !=0 if following fields are valid */
	unsigned long last_updated;	/* In jiffies */
#if 0
	char manufacturer[BATTERY_STRING_MAX];
	char device[BATTERY_STRING_MAX];
	char chemistry[BATTERY_STRING_MAX];
	int  serial;
	struct {
		unsigned int day:5;	/* Day (1-31) */
		unsigned int month:4;	/* Month (1-12) */
		unsigned int year:7;	/* Year (1980 + 0-127) */
	} manufacture_date;
#endif
	u16 temp, v, desv, i, avgi;	/* Register values */
	u16 rte, ate, atf, alarms;	/* Register values */
	u16 relchg, abschg;		/* Register values */
};

static int smartbatt_attach_adapter(struct i2c_adapter *adapter);
static int smartbatt_detect(struct i2c_adapter *adapter, int address,
		       unsigned short flags, int kind);
static void smartbatt_init_client(struct i2c_client *client);
static int smartbatt_detach_client(struct i2c_client *client);

static u16 swap_bytes(u16 val);
static int sb_read(struct i2c_client *client, u8 reg);
#if 0
static int smartbatt_write_value(struct i2c_client *client, u8 reg, u16 value);
#endif
static void smartbatt_temp(struct i2c_client *client, int operation,
		      int ctl_name, int *nrels_mag, long *results);
static void smartbatt_i(struct i2c_client *client, int operation,
		      int ctl_name, int *nrels_mag, long *results);
static void smartbatt_v(struct i2c_client *client, int operation,
		      int ctl_name, int *nrels_mag, long *results);
static void smartbatt_time(struct i2c_client *client, int operation,
		      int ctl_name, int *nrels_mag, long *results);
static void smartbatt_alarms(struct i2c_client *client, int operation,
		      int ctl_name, int *nrels_mag, long *results);
static void smartbatt_charge(struct i2c_client *client, int operation,
		      int ctl_name, int *nrels_mag, long *results);
static void smartbatt_update_client(struct i2c_client *client);


/* This is the driver that will be inserted */
static struct i2c_driver smartbatt_driver = {
	.owner		= THIS_MODULE,
	.name		= "Smart Battery chip driver",
	.id		= I2C_DRIVERID_SMARTBATT,
	.flags		= I2C_DF_NOTIFY,
	.attach_adapter	= smartbatt_attach_adapter,
	.detach_client	= smartbatt_detach_client,
};


/* -- SENSORS SYSCTL START -- */
#define SMARTBATT_SYSCTL_I 1001
#define SMARTBATT_SYSCTL_V 1002
#define SMARTBATT_SYSCTL_TEMP 1003
#define SMARTBATT_SYSCTL_TIME 1004
#define SMARTBATT_SYSCTL_ALARMS 1005
#define SMARTBATT_SYSCTL_CHARGE 1006

/* -- SENSORS SYSCTL END -- */

/* These files are created for each detected SMARTBATT. This is just a template;
   though at first sight, you might think we could use a statically
   allocated list, we need some way to get back to the parent - which
   is done through one of the 'extra' fields which are initialized
   when a new copy is allocated. */
static ctl_table smartbatt_dir_table_template[] = {
	{SMARTBATT_SYSCTL_I, "i", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &smartbatt_i},
	{SMARTBATT_SYSCTL_V, "v", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &smartbatt_v},
	{SMARTBATT_SYSCTL_TEMP, "temp", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &smartbatt_temp},
	{SMARTBATT_SYSCTL_TIME, "time", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &smartbatt_time},
	{SMARTBATT_SYSCTL_ALARMS, "alarms", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &smartbatt_alarms},
	{SMARTBATT_SYSCTL_CHARGE, "charge", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &smartbatt_charge},
	{0}
};

static int smartbatt_id = 0;

static int smartbatt_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_detect(adapter, &addr_data, smartbatt_detect);
}

/* This function is called by i2c_detect */
int smartbatt_detect(struct i2c_adapter *adapter, int address,
		unsigned short flags, int kind)
{
	int i;
	struct i2c_client *new_client;
	struct smartbatt_data *data;
	int err = 0;
	const char *type_name, *client_name;

	/* Make sure we aren't probing the ISA bus!! This is just a safety check
	   at this moment; i2c_detect really won't call us. */
#ifdef DEBUG
	if (i2c_is_isa_adapter(adapter)) {
		printk
		    ("smartbatt.o: smartbatt_detect called for an ISA bus adapter?!?\n");
		return 0;
	}
#endif

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_WORD_DATA))
		    goto ERROR0;

	/* OK. For now, we presume we have a valid client. We now create the
	   client structure, even though we cannot fill it completely yet.
	   But it allows us to access smartbatt_{read,write}_value. */
	if (!(new_client = kmalloc(sizeof(struct i2c_client) +
				   sizeof(struct smartbatt_data),
				   GFP_KERNEL))) {
		err = -ENOMEM;
		goto ERROR0;
	}

	data = (struct smartbatt_data *) (new_client + 1);
	new_client->addr = address;
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &smartbatt_driver;
	new_client->flags = 0;

	/* Now, we do the remaining detection. It is lousy. */
	if (kind < 0) {
		for (i = 0x08; i <= 0x0a; i++)
			if (i2c_smbus_read_word_data(new_client, i) != 0xff)
				goto ERROR1;
	}

	kind = smartbatt;
	type_name = "smartbatt";
	client_name = "Smart Battery";

	/* Fill in the remaining client fields and put it into the global list */
	strcpy(new_client->name, client_name);

	new_client->id = smartbatt_id++;
	data->valid = 0;
	init_MUTEX(&data->update_lock);

	/* Tell the I2C layer a new client has arrived */
	if ((err = i2c_attach_client(new_client)))
		goto ERROR3;

	/* Register a new directory entry with module sensors */
	if ((i = i2c_register_entry(new_client, type_name,
					smartbatt_dir_table_template)) < 0) {
		err = i;
		goto ERROR4;
	}
	data->sysctl_id = i;

	smartbatt_init_client(new_client);
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

static int smartbatt_detach_client(struct i2c_client *client)
{
	int err;

	i2c_deregister_entry(((struct smartbatt_data *) (client->data))->
				 sysctl_id);

	if ((err = i2c_detach_client(client))) {
		printk
		    ("smartbatt.o: Client deregistration failed, client not detached.\n");
		return err;
	}

	kfree(client);

	return 0;
}


static u16 swap_bytes(u16 val)
{
	return (val >> 8) | (val << 8);
}

static int sb_read(struct i2c_client *client, u8 reg)
{
	return swap_bytes(i2c_smbus_read_word_data(client, reg));
}

#if 0
static int smartbatt_write_value(struct i2c_client *client, u8 reg, u16 value)
{
	return i2c_smbus_write_word_data(client, reg, swap_bytes(value));
}
#endif

#if 0
/* this is code from battery.c. No strings support yet in i2c-proc.c so
   all we could do is print this out at startup if we wanted.
*/
int
static battery_info(int fd, struct battery_info *info)
{
  int n;
  int val;

  /* ManufactureDate */
  val = sb_read(SMARTBATT_REG_DATE);
  info->manufacture_date.day=val & 0x1F;
  info->manufacture_date.month=(val >> 5) & 0x0F;
  info->manufacture_date.year=(val >> 9) & 0x7F;

  /* SerialNumber */
  info->serial = sb_read(SMARTBATT_REG_SERIAL

  /* ManufacturerName */
  n = COMM_TIMEOUT;
  do {
    val = i2c_smbus_read_block_data(fd, 0x20, info->manufacturer);
  } while ((val == -1) && (n-- > 0));
  info->manufacturer[val]=0;	

  /* DeviceName */
  n = COMM_TIMEOUT;
  do {
    val = i2c_smbus_read_block_data(fd, 0x21, info->device);
  } while ((val == -1) && (n-- > 0));
  info->device[val]=0;	

  /* DeviceChemistry */
  n = COMM_TIMEOUT;
  do {
    val = i2c_smbus_read_block_data(fd, 0x22, info->chemistry);
  } while ((val == -1) && (n-- > 0));
  info->chemistry[val]=0;	

  return 0;
}
#endif

static void smartbatt_init_client(struct i2c_client *client)
{

}

static void smartbatt_update_client(struct i2c_client *client)
{
	struct smartbatt_data *data = client->data;

	down(&data->update_lock);

	if ((jiffies - data->last_updated > HZ + HZ / 2) ||
	    (jiffies < data->last_updated) || !data->valid) {
		data->temp = sb_read(client, SMARTBATT_REG_TEMP);
		data->i = sb_read(client, SMARTBATT_REG_I);
		data->avgi = sb_read(client, SMARTBATT_REG_AVGI);
		data->v = sb_read(client, SMARTBATT_REG_V);
		data->desv = sb_read(client, SMARTBATT_REG_DESV);
		data->ate = sb_read(client, SMARTBATT_REG_AVGTIME_E);
		data->atf = sb_read(client, SMARTBATT_REG_AVGTIME_F);
		data->rte = sb_read(client, SMARTBATT_REG_RUNTIME_E);
		data->alarms = sb_read(client, SMARTBATT_REG_STATUS);
		data->relchg = sb_read(client, SMARTBATT_REG_RELCHG);
		data->abschg = sb_read(client, SMARTBATT_REG_ABSCHG);
		data->last_updated = jiffies;
		data->valid = 1;
	}

	up(&data->update_lock);
}


void smartbatt_temp(struct i2c_client *client, int operation, int ctl_name,
	       int *nrels_mag, long *results)
{
	struct smartbatt_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 1;
	else if (operation == SENSORS_PROC_REAL_READ) {
		smartbatt_update_client(client);
		results[0] = TEMP_FROM_REG(data->temp);
		*nrels_mag = 1;
	}
}

void smartbatt_i(struct i2c_client *client, int operation, int ctl_name,
	       int *nrels_mag, long *results)
{
	struct smartbatt_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 3;
	else if (operation == SENSORS_PROC_REAL_READ) {
		smartbatt_update_client(client);
		results[0] = data->avgi;
		results[1] = data->i;
		*nrels_mag = 2;
	}
}

void smartbatt_v(struct i2c_client *client, int operation, int ctl_name,
	       int *nrels_mag, long *results)
{
	struct smartbatt_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 3;
	else if (operation == SENSORS_PROC_REAL_READ) {
		smartbatt_update_client(client);
		results[0] = data->desv;
		results[1] = data->v;
		*nrels_mag = 2;
	}
}

void smartbatt_time(struct i2c_client *client, int operation, int ctl_name,
	       int *nrels_mag, long *results)
{
	struct smartbatt_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		smartbatt_update_client(client);
		results[0] = data->ate;
		results[1] = data->atf;
		results[2] = data->rte;
		*nrels_mag = 3;
	}
}

void smartbatt_alarms(struct i2c_client *client, int operation, int ctl_name,
		 int *nrels_mag, long *results)
{
	struct smartbatt_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		smartbatt_update_client(client);
		results[0] = data->alarms;
		*nrels_mag = 1;
	}
}

void smartbatt_charge(struct i2c_client *client, int operation, int ctl_name,
		 int *nrels_mag, long *results)
{
	struct smartbatt_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		smartbatt_update_client(client);
		results[0] = data->relchg;
		results[1] = data->abschg;
		*nrels_mag = 2;
	}
}

static int __init sm_smartbatt_init(void)
{
	printk("smartbatt.o version %s (%s)\n", LM_VERSION, LM_DATE);
	return i2c_add_driver(&smartbatt_driver);
}

static void __exit sm_smartbatt_exit(void)
{
	i2c_del_driver(&smartbatt_driver);
}



MODULE_AUTHOR("M. Studebaker <mdsxyz123@yahoo.com>");
MODULE_DESCRIPTION("Smart Battery driver");
MODULE_LICENSE("GPL");

module_init(sm_smartbatt_init);
module_exit(sm_smartbatt_exit);
