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
static unsigned short normal_i2c[] = { SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { 0x0b, SENSORS_I2C_END };
static unsigned int normal_isa[] = { SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/* Insmod parameters */
SENSORS_INSMOD_1(smartbatt);

/* The SMARTBATT registers */
#define SMARTBATT_REG_MODE 0x03
#define SMARTBATT_REG_TEMP 0x08
#define SMARTBATT_REG_V 0x09
#define SMARTBATT_REG_I 0x0a
#define SMARTBATT_REG_AVGI 0x0b
#define SMARTBATT_REG_RELCHG 0x0d
#define SMARTBATT_REG_ABSCHG 0x0e
#define SMARTBATT_REG_REMCAP 0x0f
#define SMARTBATT_REG_CHGCAP 0x10
#define SMARTBATT_REG_RUNTIME_E 0x11
#define SMARTBATT_REG_AVGTIME_E 0x12
#define SMARTBATT_REG_AVGTIME_F 0x13
#define SMARTBATT_REG_CHGI 0x14
#define SMARTBATT_REG_CHGV 0x15
#define SMARTBATT_REG_STATUS 0x16
#define SMARTBATT_REG_CYCLECT 0x17
#define SMARTBATT_REG_DESCAP 0x18
#define SMARTBATT_REG_DESV 0x19
#define SMARTBATT_REG_DATE 0x1b
#define SMARTBATT_REG_SERIAL 0x1c
#define SMARTBATT_REG_MANUF 0x20
#define SMARTBATT_REG_NAME 0x21
#define SMARTBATT_REG_CHEM 0x22

#define BATTERY_STRING_MAX	64
#define COMM_TIMEOUT 16


/* Each client has this additional data */
struct smartbatt_data {
	struct i2c_client client;
	int sysctl_id;

	struct semaphore update_lock;
	char valid;		/* !=0 if following fields are valid */
	unsigned long last_updated;	/* In jiffies */
#if 0
	char manufacturer[BATTERY_STRING_MAX];
	char device[BATTERY_STRING_MAX];
	char chemistry[BATTERY_STRING_MAX];
#endif
	int  serial;
	struct {
		unsigned int day:5;	/* Day (1-31) */
		unsigned int month:4;	/* Month (1-12) */
		unsigned int year:7;	/* Year (1980 + 0-127) */
	} manufacture_date;
	u16 mode, temp, v, i, avgi;	/* Register values */
	u16 relchg, abschg, remcap, chgcap;	/* Register values */
	u16 rte, ate, atf, chgi, chgv;	/* Register values */
	u16 status, cyclect, descap, desv;	/* Register values */
};

static int smartbatt_attach_adapter(struct i2c_adapter *adapter);
static int smartbatt_detect(struct i2c_adapter *adapter, int address,
		       unsigned short flags, int kind);
static void smartbatt_init_client(struct i2c_client *client);
static int smartbatt_detach_client(struct i2c_client *client);

static int smartbatt_read(struct i2c_client *client, u8 reg);
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
static void smartbatt_status(struct i2c_client *client, int operation,
		      int ctl_name, int *nrels_mag, long *results);
static void smartbatt_error(struct i2c_client *client, int operation,
		      int ctl_name, int *nrels_mag, long *results);
static void smartbatt_update_client(struct i2c_client *client);


/* This is the driver that will be inserted */
static struct i2c_driver smartbatt_driver = {
	.name		= "Smart Battery chip driver",
	.id		= I2C_DRIVERID_SMARTBATT,
	.flags		= I2C_DF_NOTIFY,
	.attach_adapter	= smartbatt_attach_adapter,
	.detach_client	= smartbatt_detach_client,
};


/* -- SENSORS SYSCTL START -- */

/* Status Register Bits */
/* * * * * * Alarm Bits * * * * */ 
#define SMARTBATT_OVER_CHARGED_ALARM 0x8000 
#define SMARTBATT_TERMINATE_CHARGE_ALARM 0x4000 
#define SMARTBATT_OVER_TEMP_ALARM 0x1000 
#define SMARTBATT_TERMINATE_DISCHARGE_ALARM 0x0800 
#define SMARTBATT_REMAINING_CAPACITY_ALARM  0x0200 
#define SMARTBATT_REMAINING_TIME_ALARM 0x0100 
/* * * * * * Status Bits * * * * */
#define SMARTBATT_INITIALIZED 0x0080 
#define SMARTBATT_DISCHARGING 0x0040 
#define SMARTBATT_FULLY_CHARGED 0x0020 
#define SMARTBATT_FULLY_DISCHARGED 0x0010 
/* * * * * * Error Bits * * * * */ 
#define SMARTBATT_OK 0x0000 
#define SMARTBATT_BUSY 0x0001 
#define SMARTBATT_RESERVED_COMMAND 0x0002 
#define SMARTBATT_UNSUPPORTED_COMMAND 0x0003 
#define SMARTBATT_ACCESS_DENIED 0x0004 
#define SMARTBATT_OVER_UNDERFLOW 0x0005 
#define SMARTBATT_BAD_SIZE 0x0006 
#define SMARTBATT_UNKNOWN_ERROR 0x0007

#define SMARTBATT_ALARM (SMARTBATT_OVER_CHARGED_ALARM \
		| SMARTBATT_TERMINATE_CHARGE_ALARM | SMARTBATT_OVER_TEMP_ALARM \
		| SMARTBATT_TERMINATE_DISCHARGE_ALARM \
		| SMARTBATT_REMAINING_CAPACITY_ALARM \
		| SMARTBATT_REMAINING_TIME_ALARM)

#define SMARTBATT_STATUS (SMARTBATT_INITIALIZED | SMARTBATT_DISCHARGING \
		| SMARTBATT_FULLY_CHARGED | SMARTBATT_FULLY_DISCHARGED )

#define SMARTBATT_ERROR (SMARTBATT_BUSY | SMARTBATT_RESERVED_COMMAND \
		| SMARTBATT_UNSUPPORTED_COMMAND | SMARTBATT_ACCESS_DENIED \
		| SMARTBATT_OVER_UNDERFLOW | SMARTBATT_BAD_SIZE\
		| SMARTBATT_UNKNOWN_ERROR)


#define SMARTBATT_SYSCTL_I 1001
#define SMARTBATT_SYSCTL_V 1002
#define SMARTBATT_SYSCTL_TEMP 1003
#define SMARTBATT_SYSCTL_TIME 1004
#define SMARTBATT_SYSCTL_ALARMS 1005
#define SMARTBATT_SYSCTL_STATUS 1006
#define SMARTBATT_SYSCTL_ERROR 1007
#define SMARTBATT_SYSCTL_CHARGE 1008

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
	{SMARTBATT_SYSCTL_STATUS, "status", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &smartbatt_status},
	{SMARTBATT_SYSCTL_ERROR, "error", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &smartbatt_error},
	{SMARTBATT_SYSCTL_CHARGE, "charge", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &smartbatt_charge},
	{0}
};

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
	if (!(data = kmalloc(sizeof(struct smartbatt_data), GFP_KERNEL))) {
		err = -ENOMEM;
		goto ERROR0;
	}

	new_client = &data->client;
	new_client->addr = address;
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &smartbatt_driver;
	new_client->flags = 0;

	/* Lousy detection. Check the temp, voltage, and current registers */
	if (kind < 0) {
		for (i = 0x08; i <= 0x0a; i++)
			if (i2c_smbus_read_word_data(new_client, i) == 0xffff)
				goto ERROR1;
	}

	kind = smartbatt;
	type_name = "smartbatt";
	client_name = "Smart Battery";

	/* Fill in the remaining client fields and put it into the global list */
	strcpy(new_client->name, client_name);
	data->valid = 0;
	init_MUTEX(&data->update_lock);

	/* Tell the I2C layer a new client has arrived */
	if ((err = i2c_attach_client(new_client)))
		goto ERROR3;

	/* Register a new directory entry with module sensors */
	if ((i = i2c_register_entry(new_client, type_name,
					smartbatt_dir_table_template,
					THIS_MODULE)) < 0) {
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
	kfree(data);
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

	kfree(client->data);

	return 0;
}

static int smartbatt_read(struct i2c_client *client, u8 reg)
{ 
	int n = COMM_TIMEOUT;
	int val;
	do { 
		val = i2c_smbus_read_word_data(client, reg);
	} while ((val == -1) && (n-- > 0));
	return val;
}

#if 0
static int smartbatt_write_value(struct i2c_client *client, u8 reg, u16 value)
{
	/* Why swap bytes? */
	return i2c_smbus_write_word_data(client, reg, swab16(value));
}
#endif

#define COMM_TIMEOUT 16
static void get_battery_info(struct i2c_client *client)
{
  struct smartbatt_data *data = client->data;
  int val;

  down(&data->update_lock);
  data->chgcap = smartbatt_read(client, SMARTBATT_REG_CHGCAP);
  data->descap = smartbatt_read(client, SMARTBATT_REG_DESCAP);
  data->desv = smartbatt_read(client, SMARTBATT_REG_DESV);
  /* ManufactureDate */
  val = smartbatt_read(client, SMARTBATT_REG_DATE);
  data->manufacture_date.day=val & 0x1F;
  data->manufacture_date.month=(val >> 5) & 0x0F;
  data->manufacture_date.year=(val >> 9) & 0x7F;

  /* SerialNumber */
  data->serial = smartbatt_read(client, SMARTBATT_REG_SERIAL);
#if 0
  /* ManufacturerName */
  n=COMM_TIMEOUT;
  do {
    val = i2c_smbus_read_block_data(client, 0x20, data->manufacturer);
  } while ((val == -1) && (n-- > 0));
  data->manufacturer[val]=0;	

  /* DeviceName */
  n=COMM_TIMEOUT;
  do {
    val = i2c_smbus_read_block_data(client, 0x21, data->device);
  } while ((val == -1) && (n-- > 0));
  data->device[val]=0;	

  /* DeviceChemistry */
  n=COMM_TIMEOUT;
  do {
    val = i2c_smbus_read_block_data(client, 0x22, data->chemistry);
  } while ((val == -1) && (n-- > 0));
  data->chemistry[val]=0;	
#endif
  up(&data->update_lock);

}

static void smartbatt_init_client(struct i2c_client *client)
{
	get_battery_info( client );
}

static void smartbatt_update_client(struct i2c_client *client)
{
	struct smartbatt_data *data = client->data;

	down(&data->update_lock);

	if ((jiffies - data->last_updated > HZ + HZ / 2) ||
	    (jiffies < data->last_updated) || !data->valid) {
		data->mode = smartbatt_read(client, SMARTBATT_REG_MODE);
		data->temp = smartbatt_read(client, SMARTBATT_REG_TEMP);
		data->i = smartbatt_read(client, SMARTBATT_REG_I);
		data->avgi = smartbatt_read(client, SMARTBATT_REG_AVGI);
		data->v = smartbatt_read(client, SMARTBATT_REG_V);
		data->chgi = smartbatt_read(client, SMARTBATT_REG_CHGI);
		data->chgv = smartbatt_read(client, SMARTBATT_REG_CHGV);
		data->ate = smartbatt_read(client, SMARTBATT_REG_AVGTIME_E);
		data->atf = smartbatt_read(client, SMARTBATT_REG_AVGTIME_F);
		data->rte = smartbatt_read(client, SMARTBATT_REG_RUNTIME_E);
		data->status = smartbatt_read(client, SMARTBATT_REG_STATUS);
		data->cyclect = smartbatt_read(client, SMARTBATT_REG_CYCLECT);
		data->relchg = smartbatt_read(client, SMARTBATT_REG_RELCHG);
		data->abschg = smartbatt_read(client, SMARTBATT_REG_ABSCHG);
		data->remcap = smartbatt_read(client, SMARTBATT_REG_REMCAP);
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
		results[0] = data->temp - 2731; /* convert from Kelvin */
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
		results[0] = data->chgi;
		results[1] = data->avgi;
		results[2] = data->i;
		*nrels_mag = 3;
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
		results[0] = data->chgv;
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
		results[0] = data->status & SMARTBATT_ALARM;
		*nrels_mag = 1;
	}
}

void smartbatt_status(struct i2c_client *client, int operation, int ctl_name,
		 int *nrels_mag, long *results)
{
	struct smartbatt_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		smartbatt_update_client(client);
		results[0] = data->status & SMARTBATT_STATUS;
		*nrels_mag = 1;
	}
}

void smartbatt_error(struct i2c_client *client, int operation, int ctl_name,
		 int *nrels_mag, long *results)
{
	struct smartbatt_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		smartbatt_update_client(client);
		results[0] = data->status & SMARTBATT_ERROR;
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
		results[2] = data->chgi;
		results[3] = data->chgv;
		*nrels_mag = 4;
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
