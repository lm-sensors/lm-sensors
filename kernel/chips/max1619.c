/*
 * max1619.c - Part of lm_sensors, Linux kernel modules for hardware
 *             monitoring
 * Copyright (C) 2004 Alexey Fisher <fishor@mail.ru>
 *                    Jean Delvare <khali@linux-fr.org>
 *
 * Copied from lm90.c:
 * Copyright (C) 2003-2004 Jean Delvare <khali@linux-fr.org>
 *
 * The MAX1619 is a sensor chip made by Maxim. It reports up to two
 * temperatures (its own plus up to one external one).
 * Complete datasheet can be obtained from Maxim's website at:
 *   http://pdfserv.maxim-ic.com/en/ds/MAX1619.pdf
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/i2c-proc.h>
#include <linux/init.h>
#include "version.h"

/*
 * Addresses to scan
 */

static unsigned short normal_i2c[] = { SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = {0x18, 0x1a, 0x29, 0x2b,
	0x4c, 0x4e, SENSORS_I2C_END };
static unsigned int normal_isa[] = { SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/*
 * Insmod parameters
 */

SENSORS_INSMOD_1(max1619);

/*
 * The max1619 registers
 */

#define MAX1619_REG_R_CONFIG		0x03
#define MAX1619_REG_W_CONFIG		0x09
#define MAX1619_REG_R_CONVRATE		0x04
#define MAX1619_REG_W_CONVRATE		0x0A
#define MAX1619_REG_R_STATUS		0x02
#define MAX1619_REG_R_LOCAL_TEMP	0x00
#define MAX1619_REG_R_REMOTE_TEMP	0x01
#define MAX1619_REG_R_REMOTE_THIGH	0x07
#define MAX1619_REG_W_REMOTE_THIGH	0x0d
#define MAX1619_REG_R_REMOTE_TLOW	0x08
#define MAX1619_REG_W_REMOTE_TLOW	0x0E
#define MAX1619_REG_R_REMOTE_TMAX	0x10
#define MAX1619_REG_W_REMOTE_TMAX	0x12
#define MAX1619_REG_R_REMOTE_THYST	0x11
#define MAX1619_REG_W_REMOTE_THYST	0x13
#define MAX1619_REG_R_MAN_ID		0xFE
#define MAX1619_REG_R_CHIP_ID		0xFF

/*
 * Conversions and various macros
 */

#define TEMP_FROM_REG(val)	((val) & 0x80 ? (val)-0x100 : (val))
#define TEMP_TO_REG(val)	((val) < 0 ? (val)+0x100 : (val))

/*
 * Functions declaration
 */

static int max1619_attach_adapter(struct i2c_adapter *adapter);
static int max1619_detect(struct i2c_adapter *adapter, int address,
	unsigned short flags, int kind);
static void max1619_init_client(struct i2c_client *client);
static int max1619_detach_client(struct i2c_client *client);
static void max1619_local_temp(struct i2c_client *client, int operation,
	int ctl_name, int *nrels_mag, long *results);
static void max1619_remote_temp(struct i2c_client *client, int operation,
	int ctl_name, int *nrels_mag, long *results);
static void max1619_remote_crit(struct i2c_client *client, int operation,
	int ctl_name, int *nrels_mag, long *results);
static void max1619_alarms(struct i2c_client *client, int operation,
	int ctl_name, int *nrels_mag, long *results);

/*
 * Driver data (common to all clients)
 */

static struct i2c_driver max1619_driver = {
	.name		= "MAX1619 sensor driver",
	.flags		= I2C_DF_NOTIFY,
	.attach_adapter	= max1619_attach_adapter,
	.detach_client	= max1619_detach_client
};

/*
 * Client data (each client gets its own)
 */

struct max1619_data {
	struct i2c_client client;
	int sysctl_id;

	struct semaphore update_lock;
	char valid; /* zero until following fields are valid */
	unsigned long last_updated; /* in jiffies */

	/* register values */
	u8 local_temp;
	u8 remote_temp, remote_high, remote_low;
	u8 remote_hyst, remote_max;
	u8 alarms;
};

/*
 * Proc entries
 * These files are created for each detected max1619.
 */

/* -- SENSORS SYSCTL START -- */

#define MAX1619_SYSCTL_LOCAL_TEMP	1200
#define MAX1619_SYSCTL_REMOTE_TEMP	1201
#define MAX1619_SYSCTL_REMOTE_CRIT	1202
#define MAX1619_SYSCTL_ALARMS		1203

#define MAX1619_ALARM_REMOTE_THIGH	0x10
#define MAX1619_ALARM_REMOTE_TLOW	0x08
#define MAX1619_ALARM_REMOTE_OPEN	0x04
#define MAX1619_ALARM_REMOTE_OVERT	0x02

/* -- SENSORS SYSCTL END -- */


static ctl_table max1619_dir_table_template[] =
{
	{MAX1619_SYSCTL_LOCAL_TEMP, "temp1", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &max1619_local_temp},
	{MAX1619_SYSCTL_REMOTE_TEMP, "temp2", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &max1619_remote_temp},
	{MAX1619_SYSCTL_REMOTE_CRIT,"temp2_crit", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &max1619_remote_crit},
	{MAX1619_SYSCTL_ALARMS, "alarms", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &max1619_alarms},
	{0}
};

/*
 * Real code
 */

static int max1619_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_detect(adapter, &addr_data, max1619_detect);
}

/*
 * The following function does more than just detection. If detection
 * succeeds, it also registers the new chip.
 */
static int max1619_detect(struct i2c_adapter *adapter, int address,
	unsigned short flags, int kind)
{
	struct i2c_client *new_client;
	struct max1619_data *data;
	int err = 0;
	const char *type_name = "";
	const char *client_name = "";

#ifdef DEBUG
	if (i2c_is_isa_adapter(adapter)) {
		printk(KERN_DEBUG "max1619.o: Called for an ISA bus "
		       "adapter, aborting.\n");
		return 0;
	}
#endif

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
#ifdef DEBUG
		printk(KERN_DEBUG "max1619.o: I2C bus doesn't support "
		       "byte read mode, skipping.\n");
#endif
		return 0;
	}

	if (!(data = kmalloc(sizeof(struct max1619_data), GFP_KERNEL))) {
		printk(KERN_ERR "max1619.o: Out of memory in "
		       "max1619_detect (new_client).\n");
		return -ENOMEM;
	}

	/*
	 * The common I2C client data is placed right before the
	 * MAX1619-specific data. The MAX1619-specific data is pointed to
	 * by the data field from the I2C client da1ta.
	 */

	new_client = &data->client;
	new_client->addr = address;
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &max1619_driver;
	new_client->flags = 0;

	/*
	 * Now we do the remaining detection. A negative kind means that
	 * the driver was loaded with no force parameter (default), so we
	 * must both detect and identify the chip. A zero kind means that
	 * the driver was loaded with the force parameter, the detection
	 * step shall be skipped. A positive kind means that the driver
	 * was loaded with the force parameter and a given kind of chip is
	 * requested, so both the detection and the identification steps
	 * are skipped.
	 */

	if (kind < 0) {
		u8 reg_config, reg_convrate, reg_status;

		reg_config = i2c_smbus_read_byte_data(new_client,
			MAX1619_REG_R_CONFIG);
		reg_convrate = i2c_smbus_read_byte_data(new_client,
			MAX1619_REG_R_CONVRATE);
		reg_status = i2c_smbus_read_byte_data(new_client,
			MAX1619_REG_R_STATUS);

		if ((reg_config & 0x03) != 0x00
		 || reg_convrate > 0x07
		 || (reg_status & 0x61) != 0x00) {
#ifdef DEBUG
			printk(KERN_DEBUG "max1619.o: Detection failed at "
			       "0x%02x.\n", address);
#endif
			goto ERROR1;
		}
	}

	if (kind <= 0) {
		u8 man_id, chip_id;

		man_id = i2c_smbus_read_byte_data(new_client,
			MAX1619_REG_R_MAN_ID);
		chip_id = i2c_smbus_read_byte_data(new_client,
			MAX1619_REG_R_CHIP_ID);

		if ((man_id == 0x4D) && (chip_id == 0x04)) {
			kind = max1619;
		}
	}

	if (kind <= 0) {
		printk(KERN_INFO "max1619.o: Unsupported chip.\n");
		goto ERROR1;
	}

	if (kind == max1619) {
		type_name = "max1619";
		client_name = "MAX1619 chip";
	} else {
		printk(KERN_ERR "max1619.o: Unknown kind %d.\n", kind);
		goto ERROR1;
	}

	/*
	 * OK, we got a valid chip so we can fill in the remaining client
	 * fields.
	 */

	strcpy(new_client->name, client_name);
	data->valid = 0;
	init_MUTEX(&data->update_lock);

	/*
	 * Tell the I2C layer a new client has arrived.
	 */

	if ((err = i2c_attach_client(new_client))) {
		printk(KERN_ERR "max1619.o: Failed attaching client.\n");
		goto ERROR1;
	}

	/*
	 * Register a new directory entry.
	 */

	if ((err = i2c_register_entry(new_client, type_name,
	     max1619_dir_table_template, THIS_MODULE)) < 0) {
		printk(KERN_ERR "max1619.o: Failed registering directory "
		       "entry.\n");
		goto ERROR2;
	}
	data->sysctl_id = err;

	/*
	 * Initialize the MAX1619 chip.
	 */

	max1619_init_client(new_client);
	return 0;

ERROR2:
	i2c_detach_client(new_client);
ERROR1:
	kfree(data);
	return err;
}

static void max1619_init_client(struct i2c_client *client)
{
	u8 config;

	/*
	 * Start the conversions.
	 */

	/* Set conversion rate to 2 Hz */
	i2c_smbus_write_byte_data(client, MAX1619_REG_W_CONVRATE, 5);

	/* Start monitoring */
	config = i2c_smbus_read_byte_data(client, MAX1619_REG_R_CONFIG);
	if (config & 0x40)
		i2c_smbus_write_byte_data(client, MAX1619_REG_W_CONFIG,
			config & 0xBF);
}


static int max1619_detach_client(struct i2c_client *client)
{
	int err;

	i2c_deregister_entry(((struct max1619_data *)
		(client->data))->sysctl_id);
	if ((err = i2c_detach_client(client))) {
		printk(KERN_ERR "max1619.o: Client deregistration failed, "
		       "client not detached.\n");
		return err;
	}

	kfree(client->data);
	return 0;
}

static void max1619_update_client(struct i2c_client *client)
{
	struct max1619_data *data = client->data;

	down(&data->update_lock);

	if ((jiffies - data->last_updated > HZ * 2) ||
	    (jiffies < data->last_updated) || !data->valid) {
#ifdef DEBUG
		printk(KERN_DEBUG "max1619.o: Updating data.\n");
#endif

		data->local_temp = i2c_smbus_read_byte_data(client,
			MAX1619_REG_R_LOCAL_TEMP);
		data->remote_temp = i2c_smbus_read_byte_data(client,
			MAX1619_REG_R_REMOTE_TEMP);
		data->remote_high = i2c_smbus_read_byte_data(client,
			MAX1619_REG_R_REMOTE_THIGH);
		data->remote_low = i2c_smbus_read_byte_data(client,
			MAX1619_REG_R_REMOTE_TLOW);
		data->remote_max = i2c_smbus_read_byte_data(client,
			MAX1619_REG_R_REMOTE_TMAX);
		data->remote_hyst = i2c_smbus_read_byte_data(client,
			MAX1619_REG_R_REMOTE_THYST);
		data->alarms = i2c_smbus_read_byte_data(client,
			MAX1619_REG_R_STATUS);

		data->last_updated = jiffies;
		data->valid = 1;
	}

	up(&data->update_lock);
}

static void max1619_local_temp(struct i2c_client *client, int operation,
	int ctl_name, int *nrels_mag, long *results)
{
	struct max1619_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0; /* magnitude */
	else if (operation == SENSORS_PROC_REAL_READ) {
		max1619_update_client(client);
		results[0] = TEMP_FROM_REG(data->local_temp);
		*nrels_mag = 1;
	}

}

static void max1619_remote_temp(struct i2c_client *client, int operation,
	int ctl_name, int *nrels_mag, long *results)
{
	struct max1619_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0; /* magnitude */
	else if (operation == SENSORS_PROC_REAL_READ) {
		max1619_update_client(client);
		results[0] = TEMP_FROM_REG(data->remote_high);
		results[1] = TEMP_FROM_REG(data->remote_low);
		results[2] = TEMP_FROM_REG(data->remote_temp);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->remote_high = TEMP_TO_REG(results[0]);
			i2c_smbus_write_byte_data(client,
				MAX1619_REG_W_REMOTE_THIGH, data->remote_high);
		}
		if (*nrels_mag >= 2) {
			data->remote_low = TEMP_TO_REG(results[1]);
			i2c_smbus_write_byte_data(client,
				MAX1619_REG_W_REMOTE_TLOW, data->remote_low);

		}
	}
}

static void max1619_remote_crit(struct i2c_client *client, int operation,
	int ctl_name, int *nrels_mag, long *results)
{
	struct max1619_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0; /* magnitude */
	else if (operation == SENSORS_PROC_REAL_READ) {
		max1619_update_client(client);
		results[0] = TEMP_FROM_REG(data->remote_max);
		results[1] = TEMP_FROM_REG(data->remote_hyst);
		*nrels_mag = 2;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->remote_max = TEMP_TO_REG(results[0]);
			i2c_smbus_write_byte_data(client,
				MAX1619_REG_W_REMOTE_TMAX, data->remote_max);
		}
		if (*nrels_mag >= 2) {
			data->remote_hyst = TEMP_TO_REG(results[1]);
			i2c_smbus_write_byte_data(client,
				MAX1619_REG_W_REMOTE_THYST, data->remote_hyst);
		}
	}
}

static void max1619_alarms(struct i2c_client *client, int operation,
	int ctl_name, int *nrels_mag, long *results)
{
	struct max1619_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0; /* magnitude */
	else if (operation == SENSORS_PROC_REAL_READ) {
		max1619_update_client(client);
		results[0] = data->alarms;
		*nrels_mag = 1;
	}
}


static int __init sm_max1619_init(void)
{
	printk(KERN_INFO "max1619.o version %s (%s)\n", LM_VERSION, LM_DATE);
	return i2c_add_driver(&max1619_driver);
}

static void __exit sm_max1619_exit(void)
{
	i2c_del_driver(&max1619_driver);
}

MODULE_AUTHOR("Alexey Fisher <fishor@mail.ru>");
MODULE_DESCRIPTION("MAX1619 sensor driver");
MODULE_LICENSE("GPL");

module_init(sm_max1619_init);
module_exit(sm_max1619_exit);
