/*
    xeontemp.c - Part of lm_sensors, Linux kernel modules for hardware
             monitoring
    Copyright (c) 1998, 1999,2003  Frodo Looijaard <frodol@dds.nl>,
    Philip Edelbrock <phil@netroedge.com>, and
    Mark D. Studebaker <mdsxyz123@yahoo.com>

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

/* The Xeon temperature sensor looks just like an ADM1021 with the remote
   sensor only. There is are no ID registers so detection is difficult. */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/i2c-proc.h>
#include <linux/init.h>
#include "version.h"

#ifndef I2C_DRIVERID_XEONTEMP
#define I2C_DRIVERID_XEONTEMP	1045
#endif

/* Addresses to scan */
static unsigned short normal_i2c[] = { 0x18, 0x1a, 0x29, 0x2b,
	0x4c, 0x4e, SENSORS_I2C_END
};
static unsigned short normal_i2c_range[] = { SENSORS_I2C_END };
static unsigned int normal_isa[] = { SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/* Insmod parameters */
SENSORS_INSMOD_1(xeontemp);

/* xeontemp constants specified below */

/* The registers */
/* Read-only */
#define XEONTEMP_REG_REMOTE_TEMP 0x01
#define XEONTEMP_REG_STATUS 0x02
/* These use different addresses for reading/writing */
#define XEONTEMP_REG_CONFIG_R 0x03
#define XEONTEMP_REG_CONFIG_W 0x09
#define XEONTEMP_REG_CONV_RATE_R 0x04
#define XEONTEMP_REG_CONV_RATE_W 0x0A
/* limits */
#define XEONTEMP_REG_REMOTE_TOS_R 0x07
#define XEONTEMP_REG_REMOTE_TOS_W 0x0D
#define XEONTEMP_REG_REMOTE_THYST_R 0x08
#define XEONTEMP_REG_REMOTE_THYST_W 0x0E
/* write-only */
#define XEONTEMP_REG_ONESHOT 0x0F

#define XEONTEMP_ALARM_RTEMP (XEONTEMP_ALARM_RTEMP_HIGH | XEONTEMP_ALARM_RTEMP_LOW\
                             | XEONTEMP_ALARM_RTEMP_NA)
#define XEONTEMP_ALARM_ALL  XEONTEMP_ALARM_RTEMP

/* Conversions. Rounding and limit checking is only done on the TO_REG
   variants. Note that you should be a bit careful with which arguments
   these macros are called: arguments may be evaluated more than once.
   Fixing this is just not worth it. */
/* Conversions  note: 1021 uses normal integer signed-byte format*/
#define TEMP_FROM_REG(val) (val > 127 ? val-256 : val)
#define TEMP_TO_REG(val)   (SENSORS_LIMIT((val < 0 ? val+256 : val),0,255))

/* Each client has this additional data */
struct xeontemp_data {
	struct i2c_client client;
	int sysctl_id;
	enum chips type;

	struct semaphore update_lock;
	char valid;		/* !=0 if following fields are valid */
	unsigned long last_updated;	/* In jiffies */

	u8 remote_temp, remote_temp_os, remote_temp_hyst, alarms;
	u8 fail;
};

static int xeontemp_attach_adapter(struct i2c_adapter *adapter);
static int xeontemp_detect(struct i2c_adapter *adapter, int address,
			  unsigned short flags, int kind);
static void xeontemp_init_client(struct i2c_client *client);
static int xeontemp_detach_client(struct i2c_client *client);
static int xeontemp_read_value(struct i2c_client *client, u8 reg);
static int xeontemp_rd_good(u8 *val, struct i2c_client *client, u8 reg, u8 mask);
static int xeontemp_write_value(struct i2c_client *client, u8 reg,
			       u16 value);
static void xeontemp_remote_temp(struct i2c_client *client, int operation,
				int ctl_name, int *nrels_mag,
				long *results);
static void xeontemp_alarms(struct i2c_client *client, int operation,
			   int ctl_name, int *nrels_mag, long *results);
static void xeontemp_update_client(struct i2c_client *client);

/* (amalysh) read only mode, otherwise any limit's writing confuse BIOS */
static int read_only = 0;


/* This is the driver that will be inserted */
static struct i2c_driver xeontemp_driver = {
	.name		= "Xeon temp sensor driver",
	.id		= I2C_DRIVERID_XEONTEMP,
	.flags		= I2C_DF_NOTIFY,
	.attach_adapter	= xeontemp_attach_adapter,
	.detach_client	= xeontemp_detach_client,
};

/* -- SENSORS SYSCTL START -- */

#define XEONTEMP_SYSCTL_REMOTE_TEMP 1201
#define XEONTEMP_SYSCTL_ALARMS 1203

#define XEONTEMP_ALARM_RTEMP_HIGH 0x10
#define XEONTEMP_ALARM_RTEMP_LOW 0x08
#define XEONTEMP_ALARM_RTEMP_NA 0x04

/* -- SENSORS SYSCTL END -- */

/* These files are created for each detected xeontemp. This is just a template;
   though at first sight, you might think we could use a statically
   allocated list, we need some way to get back to the parent - which
   is done through one of the 'extra' fields which are initialized
   when a new copy is allocated. */
static ctl_table xeontemp_dir_table_template[] = {
	{XEONTEMP_SYSCTL_REMOTE_TEMP, "temp", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &xeontemp_remote_temp},
	{XEONTEMP_SYSCTL_ALARMS, "alarms", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &xeontemp_alarms},
	{0}
};

static int xeontemp_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_detect(adapter, &addr_data, xeontemp_detect);
}

static int xeontemp_detect(struct i2c_adapter *adapter, int address,
			  unsigned short flags, int kind)
{
	int i;
	struct i2c_client *new_client;
	struct xeontemp_data *data;
	int err = 0;
	const char *type_name = "";
	const char *client_name = "";

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		goto error0;

	/* OK. For now, we presume we have a valid client. We now create the
	   client structure, even though we cannot fill it completely yet.
	   But it allows us to access xeontemp_{read,write}_value. */

	if (!(data = kmalloc(sizeof(struct xeontemp_data), GFP_KERNEL))) {
		err = -ENOMEM;
		goto error0;
	}

	new_client = &data->client;
	new_client->addr = address;
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &xeontemp_driver;
	new_client->flags = 0;

	/* Now, we do the remaining detection. */

	if (kind < 0) {
		if (
		    (xeontemp_read_value(new_client, XEONTEMP_REG_STATUS) &
		     0x03) != 0x00)
			goto error1;
	}

	/* Determine the chip type. */

	if (kind <= 0) {
		kind = xeontemp;
	}

	type_name = "xeontemp";
	client_name = "xeon sensors";

	/* Fill in the remaining client fields and put it into the global list */
	strcpy(new_client->name, client_name);
	data->type = kind;
	data->valid = 0;
	init_MUTEX(&data->update_lock);

	/* Tell the I2C layer a new client has arrived */
	if ((err = i2c_attach_client(new_client)))
		goto error3;

	/* Register a new directory entry with module sensors */
	if ((i = i2c_register_entry(new_client,	type_name,
				    xeontemp_dir_table_template,
				    THIS_MODULE)) < 0) {
		err = i;
		goto error4;
	}
	data->sysctl_id = i;

	xeontemp_init_client(new_client);
	return 0;

      error4:
	i2c_detach_client(new_client);
      error3:
      error1:
	kfree(data);
      error0:
	return err;
}

static void xeontemp_init_client(struct i2c_client *client)
{
	/* Enable ADC and disable suspend mode */
	xeontemp_write_value(client, XEONTEMP_REG_CONFIG_W, 0);
	/* Set Conversion rate to 1/sec (this can be tinkered with) */
	xeontemp_write_value(client, XEONTEMP_REG_CONV_RATE_W, 0x04);
}

static int xeontemp_detach_client(struct i2c_client *client)
{

	int err;

	i2c_deregister_entry(((struct xeontemp_data *) (client->data))->
				 sysctl_id);

	if ((err = i2c_detach_client(client))) {
		printk
		    ("xeontemp.o: Client deregistration failed, client not detached.\n");
		return err;
	}

	kfree(client->data);

	return 0;

}


/* All registers are byte-sized */
static int xeontemp_read_value(struct i2c_client *client, u8 reg)
{
	return i2c_smbus_read_byte_data(client, reg);
}

/* only update value if read succeeded; set fail bit if failed */
static int xeontemp_rd_good(u8 *val, struct i2c_client *client, u8 reg, u8 mask)
{
	int i;
	struct xeontemp_data *data = client->data;

	i = i2c_smbus_read_byte_data(client, reg);
	if (i < 0) {
		data->fail |= mask;
		return i;
	}
	*val = i;
	return 0;
}

static int xeontemp_write_value(struct i2c_client *client, u8 reg, u16 value)
{
	if (read_only > 0)
		return 0;

	return i2c_smbus_write_byte_data(client, reg, value);
}

static void xeontemp_update_client(struct i2c_client *client)
{
	struct xeontemp_data *data = client->data;

	down(&data->update_lock);

	if ((jiffies - data->last_updated > HZ + HZ / 2) ||
	    (jiffies < data->last_updated) || !data->valid) {

#ifdef DEBUG
		printk("Starting xeontemp update\n");
#endif

		data->fail = 0;
		xeontemp_rd_good(&(data->remote_temp), client,
		                XEONTEMP_REG_REMOTE_TEMP, XEONTEMP_ALARM_RTEMP);
		xeontemp_rd_good(&(data->remote_temp_os), client,
		                XEONTEMP_REG_REMOTE_TOS_R, XEONTEMP_ALARM_RTEMP);
		xeontemp_rd_good(&(data->remote_temp_hyst), client,
		                XEONTEMP_REG_REMOTE_THYST_R,
		                XEONTEMP_ALARM_RTEMP);
		data->alarms = XEONTEMP_ALARM_ALL;
		if (!xeontemp_rd_good(&(data->alarms), client,
		                     XEONTEMP_REG_STATUS, 0))
			data->alarms &= XEONTEMP_ALARM_ALL;
		data->last_updated = jiffies;
		data->valid = 1;
	}

	up(&data->update_lock);
}

void xeontemp_remote_temp(struct i2c_client *client, int operation,
			 int ctl_name, int *nrels_mag, long *results)
{
	struct xeontemp_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		xeontemp_update_client(client);
		results[0] = TEMP_FROM_REG(data->remote_temp_os);
		results[1] = TEMP_FROM_REG(data->remote_temp_hyst);
		results[2] = TEMP_FROM_REG(data->remote_temp);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->remote_temp_os = TEMP_TO_REG(results[0]);
			xeontemp_write_value(client,
					    XEONTEMP_REG_REMOTE_TOS_W,
					    data->remote_temp_os);
		}
		if (*nrels_mag >= 2) {
			data->remote_temp_hyst = TEMP_TO_REG(results[1]);
			xeontemp_write_value(client,
					    XEONTEMP_REG_REMOTE_THYST_W,
					    data->remote_temp_hyst);
		}
	}
}

void xeontemp_alarms(struct i2c_client *client, int operation, int ctl_name,
		    int *nrels_mag, long *results)
{
	struct xeontemp_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		xeontemp_update_client(client);
		results[0] = data->alarms | data->fail;
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		/* Can't write to it */
	}
}

static int __init sm_xeontemp_init(void)
{
	printk(KERN_INFO "xeontemp.o version %s (%s)\n", LM_VERSION, LM_DATE);
	return i2c_add_driver(&xeontemp_driver);
}

static void __exit sm_xeontemp_exit(void)
{
	i2c_del_driver(&xeontemp_driver);
}

MODULE_AUTHOR
    ("Frodo Looijaard <frodol@dds.nl> and Philip Edelbrock <phil@netroedge.com>");
MODULE_DESCRIPTION("xeontemp driver");
MODULE_LICENSE("GPL");

MODULE_PARM(read_only, "i");
MODULE_PARM_DESC(read_only, "Don't set any values, read only mode");

module_init(sm_xeontemp_init)
module_exit(sm_xeontemp_exit)
