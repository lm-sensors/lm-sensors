/*
    thmc50.c - Part of lm_sensors, Linux kernel modules for hardware
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

#define DEBUG 1

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/i2c-proc.h>
#include <linux/init.h>
#include "version.h"

MODULE_LICENSE("GPL");

/* Addresses to scan */
static unsigned short normal_i2c[] = { SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { 0x2D, 0x2E, SENSORS_I2C_END };
static unsigned int normal_isa[] = { SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/* Insmod parameters */
SENSORS_INSMOD_1(thmc50);

/* Many THMC50 constants specified below */

/* The THMC50 registers */
#define THMC50_REG_TEMP 0x27
#define THMC50_REG_CONF 0x40
#define THMC50_REG_TEMP_HYST 0x3A
#define THMC50_REG_TEMP_OS 0x39

#define THMC50_REG_TEMP_TRIP 0x13
#define THMC50_REG_TEMP_REMOTE_TRIP 0x14
#define THMC50_REG_TEMP_DEFAULT_TRIP 0x17
#define THMC50_REG_TEMP_REMOTE_DEFAULT_TRIP 0x18
#define THMC50_REG_ANALOG_OUT 0x19
#define THMC50_REG_REMOTE_TEMP 0x26
#define THMC50_REG_REMOTE_TEMP_HYST 0x38
#define THMC50_REG_REMOTE_TEMP_OS 0x37

#define THMC50_REG_INTER 0x41
#define THMC50_REG_INTER_MIRROR 0x4C
#define THMC50_REG_INTER_MASK 0x43

#define THMC50_REG_COMPANY_ID 0x3E
#define THMC50_REG_DIE_CODE 0x3F


/* Conversions. Rounding and limit checking is only done on the TO_REG
   variants. Note that you should be a bit careful with which arguments
   these macros are called: arguments may be evaluated more than once.
   Fixing this is just not worth it. */
#define TEMP_FROM_REG(val) ((val>127)?val - 0x0100:val)
#define TEMP_TO_REG(val)   ((val<0)?0x0100+val:val)

/* Each client has this additional data */
struct thmc50_data {
	struct i2c_client client;
	int sysctl_id;

	struct semaphore update_lock;
	char valid;		/* !=0 if following fields are valid */
	unsigned long last_updated;	/* In jiffies */

	u16 temp, temp_os, temp_hyst,
	    remote_temp, remote_temp_os, remote_temp_hyst,
	    inter, inter_mask, die_code, analog_out;	/* Register values */
};

static int thmc50_attach_adapter(struct i2c_adapter *adapter);
static int thmc50_detect(struct i2c_adapter *adapter, int address,
			 unsigned short flags, int kind);
static void thmc50_init_client(struct i2c_client *client);
static int thmc50_detach_client(struct i2c_client *client);

static int thmc50_read_value(struct i2c_client *client, u8 reg);
static int thmc50_write_value(struct i2c_client *client, u8 reg,
			      u16 value);
static void thmc50_temp(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void thmc50_remote_temp(struct i2c_client *client, int operation,
			       int ctl_name, int *nrels_mag,
			       long *results);
static void thmc50_inter(struct i2c_client *client, int operation,
			 int ctl_name, int *nrels_mag, long *results);
static void thmc50_inter_mask(struct i2c_client *client, int operation,
			      int ctl_name, int *nrels_mag, long *results);
static void thmc50_die_code(struct i2c_client *client, int operation,
			    int ctl_name, int *nrels_mag, long *results);
static void thmc50_analog_out(struct i2c_client *client, int operation,
			      int ctl_name, int *nrels_mag, long *results);
static void thmc50_update_client(struct i2c_client *client);


/* This is the driver that will be inserted */
static struct i2c_driver thmc50_driver = {
	.name		= "THMC50 sensor chip driver",
	.id		= I2C_DRIVERID_THMC50,
	.flags		= I2C_DF_NOTIFY,
	.attach_adapter	= thmc50_attach_adapter,
	.detach_client	= thmc50_detach_client,
};

/* -- SENSORS SYSCTL START -- */

#define THMC50_SYSCTL_TEMP 1200	/* Degrees Celcius */
#define THMC50_SYSCTL_REMOTE_TEMP 1201	/* Degrees Celcius */
#define THMC50_SYSCTL_INTER 1202
#define THMC50_SYSCTL_INTER_MASK 1203
#define THMC50_SYSCTL_DIE_CODE 1204
#define THMC50_SYSCTL_ANALOG_OUT 1205

/* -- SENSORS SYSCTL END -- */

/* These files are created for each detected THMC50. This is just a template;
   though at first sight, you might think we could use a statically
   allocated list, we need some way to get back to the parent - which
   is done through one of the 'extra' fields which are initialized
   when a new copy is allocated. */
static ctl_table thmc50_dir_table_template[] = {
	{THMC50_SYSCTL_TEMP, "temp1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &thmc50_temp},
	{THMC50_SYSCTL_REMOTE_TEMP, "temp2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &thmc50_remote_temp},
	{THMC50_SYSCTL_INTER, "inter", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &thmc50_inter},
	{THMC50_SYSCTL_INTER_MASK, "inter_mask", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &thmc50_inter_mask},
	{THMC50_SYSCTL_DIE_CODE, "die_code", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &thmc50_die_code},
	{THMC50_SYSCTL_ANALOG_OUT, "analog_out", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &thmc50_analog_out},
	{0}
};


static int thmc50_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_detect(adapter, &addr_data, thmc50_detect);
}

/* This function is called by i2c_detect */
int thmc50_detect(struct i2c_adapter *adapter, int address,
		  unsigned short flags, int kind)
{
	int company, i;
	struct i2c_client *new_client;
	struct thmc50_data *data;
	int err = 0;
	const char *type_name, *client_name;

#ifdef DEBUG
	printk("thmc50.o: Probing for THMC50 at 0x%2X on bus %d\n",
	       address, adapter->id);
#endif

	/* Make sure we aren't probing the ISA bus!! This is just a safety check
	   at this moment; i2c_detect really won't call us. */
#ifdef DEBUG
	if (i2c_is_isa_adapter(adapter)) {
		printk
		    ("thmc50.o: thmc50_detect called for an ISA bus adapter?!?\n");
		return 0;
	}
#endif

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		goto ERROR0;

	/* OK. For now, we presume we have a valid client. We now create the
	   client structure, even though we cannot fill it completely yet.
	   But it allows us to access thmc50_{read,write}_value. */
	if (!(data = kmalloc(sizeof(struct thmc50_data), GFP_KERNEL))) {
		err = -ENOMEM;
		goto ERROR0;
	}

	new_client = &data->client;
	new_client->addr = address;
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &thmc50_driver;
	new_client->flags = 0;

	/* Now, we do the remaining detection. */
	company =
	    i2c_smbus_read_byte_data(new_client, THMC50_REG_COMPANY_ID);

	if (company != 0x49) {
#ifdef DEBUG
		printk
		    ("thmc50.o: Detect of THMC50 failed (reg 3E: 0x%X)\n",
		     company);
#endif
		goto ERROR1;
	}

	/* Determine the chip type - only one kind supported! */
	kind = thmc50;

	if (kind == thmc50) {
		type_name = "thmc50";
		client_name = "THMC50 chip";
	} else {
#ifdef DEBUG
		printk("thmc50.o: Internal error: unknown kind (%d)?!?",
		       kind);
#endif
		goto ERROR1;
	}

	/* Fill in the remaining client fields and put it into the global list */
	strcpy(new_client->name, client_name);
	data->valid = 0;
	init_MUTEX(&data->update_lock);

	/* Tell the I2C layer a new client has arrived */
	if ((err = i2c_attach_client(new_client)))
		goto ERROR3;

	/* Register a new directory entry with module sensors */
	if ((i = i2c_register_entry(new_client, type_name,
					thmc50_dir_table_template,
					THIS_MODULE)) < 0) {
		err = i;
		goto ERROR4;
	}
	data->sysctl_id = i;

	thmc50_init_client(new_client);
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

static int thmc50_detach_client(struct i2c_client *client)
{
	int err;

	i2c_deregister_entry(((struct thmc50_data *) (client->data))->
				 sysctl_id);

	if ((err = i2c_detach_client(client))) {
		printk
		    ("thmc50.o: Client deregistration failed, client not detached.\n");
		return err;
	}

	kfree(client->data);

	return 0;
}


/* All registers are word-sized, except for the configuration register.
   THMC50 uses a high-byte first convention, which is exactly opposite to
   the usual practice. */
static int thmc50_read_value(struct i2c_client *client, u8 reg)
{
	return i2c_smbus_read_byte_data(client, reg);
}

/* All registers are word-sized, except for the configuration register.
   THMC50 uses a high-byte first convention, which is exactly opposite to
   the usual practice. */
static int thmc50_write_value(struct i2c_client *client, u8 reg, u16 value)
{
	return i2c_smbus_write_byte_data(client, reg, value);
}

static void thmc50_init_client(struct i2c_client *client)
{
	thmc50_write_value(client, THMC50_REG_CONF, 1);
}

static void thmc50_update_client(struct i2c_client *client)
{
	struct thmc50_data *data = client->data;

	down(&data->update_lock);

	if ((jiffies - data->last_updated > HZ + HZ / 2) ||
	    (jiffies < data->last_updated) || !data->valid) {

#ifdef DEBUG
		printk("Starting thmc50 update\n");
#endif

		data->temp = thmc50_read_value(client, THMC50_REG_TEMP);
		data->temp_os =
		    thmc50_read_value(client, THMC50_REG_TEMP_OS);
		data->temp_hyst =
		    thmc50_read_value(client, THMC50_REG_TEMP_HYST);
		data->remote_temp =
		    thmc50_read_value(client, THMC50_REG_REMOTE_TEMP);
		data->remote_temp_os =
		    thmc50_read_value(client, THMC50_REG_REMOTE_TEMP_OS);
		data->remote_temp_hyst =
		    thmc50_read_value(client, THMC50_REG_REMOTE_TEMP_HYST);
		data->inter = thmc50_read_value(client, THMC50_REG_INTER);
		data->inter_mask =
		    thmc50_read_value(client, THMC50_REG_INTER_MASK);
		data->die_code =
		    thmc50_read_value(client, THMC50_REG_DIE_CODE);
		data->analog_out =
		    thmc50_read_value(client, THMC50_REG_ANALOG_OUT);
		data->last_updated = jiffies;
		data->valid = 1;
	}

	up(&data->update_lock);
}


void thmc50_temp(struct i2c_client *client, int operation, int ctl_name,
		 int *nrels_mag, long *results)
{
	struct thmc50_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		thmc50_update_client(client);
		results[0] = TEMP_FROM_REG(data->temp_os);
		results[1] = TEMP_FROM_REG(data->temp_hyst);
		results[2] = TEMP_FROM_REG(data->temp);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->temp_os = TEMP_TO_REG(results[0]);
			thmc50_write_value(client, THMC50_REG_TEMP_OS,
					   data->temp_os);
		}
		if (*nrels_mag >= 2) {
			data->temp_hyst = TEMP_TO_REG(results[1]);
			thmc50_write_value(client, THMC50_REG_TEMP_HYST,
					   data->temp_hyst);
		}
	}
}


void thmc50_remote_temp(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results)
{
	struct thmc50_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		thmc50_update_client(client);
		results[0] = TEMP_FROM_REG(data->remote_temp_os);
		results[1] = TEMP_FROM_REG(data->remote_temp_hyst);
		results[2] = TEMP_FROM_REG(data->remote_temp);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->remote_temp_os = TEMP_TO_REG(results[0]);
			thmc50_write_value(client,
					   THMC50_REG_REMOTE_TEMP_OS,
					   data->remote_temp_os);
		}
		if (*nrels_mag >= 2) {
			data->remote_temp_hyst = TEMP_TO_REG(results[1]);
			thmc50_write_value(client,
					   THMC50_REG_REMOTE_TEMP_HYST,
					   data->remote_temp_hyst);
		}
	}
}


void thmc50_inter(struct i2c_client *client, int operation, int ctl_name,
		  int *nrels_mag, long *results)
{
	struct thmc50_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		thmc50_update_client(client);
		results[0] = data->inter;
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		printk("thmc50.o: No writes to Interrupt register!\n");
	}
}


void thmc50_inter_mask(struct i2c_client *client, int operation,
		       int ctl_name, int *nrels_mag, long *results)
{
	struct thmc50_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		thmc50_update_client(client);
		results[0] = data->inter_mask;
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->inter_mask = results[0];
			thmc50_write_value(client, THMC50_REG_INTER_MASK,
					   data->inter_mask);
		}
	}
}


void thmc50_die_code(struct i2c_client *client, int operation,
		     int ctl_name, int *nrels_mag, long *results)
{
	struct thmc50_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		thmc50_update_client(client);
		results[0] = data->die_code;
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		printk("thmc50.o: No writes to Die-Code register!\n");
	}
}


void thmc50_analog_out(struct i2c_client *client, int operation,
		       int ctl_name, int *nrels_mag, long *results)
{
	struct thmc50_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		thmc50_update_client(client);
		results[0] = data->analog_out;
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->analog_out = results[0];
			thmc50_write_value(client, THMC50_REG_ANALOG_OUT,
					   data->analog_out);
		}
	}
}




static int __init sm_thmc50_init(void)
{
	printk("thmc50.o version %s (%s)\n", LM_VERSION, LM_DATE);

	return i2c_add_driver(&thmc50_driver);
}

static void __exit sm_thmc50_exit(void)
{
	i2c_del_driver(&thmc50_driver);
}



MODULE_AUTHOR
    ("Frodo Looijaard <frodol@dds.nl> and Philip Edelbrock <phil@netroedge.com>");
MODULE_DESCRIPTION("THMC50 driver");

module_init(sm_thmc50_init);
module_exit(sm_thmc50_exit);
