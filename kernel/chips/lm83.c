/*
 * lm83.c - Part of lm_sensors, Linux kernel modules for hardware
 *          monitoring
 * Copyright (C) 2003  Jean Delvare <khali@linux-fr.org>
 *
 * Heavily inspired from the lm78, lm75 and adm1021 drivers. The LM83 is
 * a sensor chip made by National Semiconductor. It reports up to four
 * temperatures (its own plus up to three external ones) with a 1 deg
 * resolution and a 3-4 deg accuracy. Complete datasheet can be obtained
 * from National's website at:
 *   http://www.national.com/pf/LM/LM83.html
 * Since the datasheet omits to give the chip stepping code, I give it
 * here: 0x03 (at register 0xff).
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
 * Address is selected using 2 three-level pins, resulting in 9 possible
 * addresses.
 */

static unsigned short normal_i2c[] = { SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { 0x18, 0x1a, 0x29, 0x2b,
	0x4c, 0x4e, SENSORS_I2C_END };
static unsigned int normal_isa[] = { SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/*
 * Insmod parameters
 */

SENSORS_INSMOD_1(lm83);

/*
 * The LM83 registers
 * Manufacturer ID is 0x01 for National Semiconductor.
 */

#define LM83_REG_R_MAN_ID        0xFE
#define LM83_REG_R_CHIP_ID       0xFF
#define LM83_REG_R_CONFIG        0x03
#define LM83_REG_W_CONFIG        0x09
#define LM83_REG_R_STATUS1       0x02
#define LM83_REG_R_STATUS2       0x35
#define LM83_REG_R_LOCAL_TEMP    0x00
#define LM83_REG_R_LOCAL_HIGH    0x05
#define LM83_REG_W_LOCAL_HIGH    0x0B
#define LM83_REG_R_REMOTE1_TEMP  0x30
#define LM83_REG_R_REMOTE1_HIGH  0x38
#define LM83_REG_W_REMOTE1_HIGH  0x50
#define LM83_REG_R_REMOTE2_TEMP  0x01
#define LM83_REG_R_REMOTE2_HIGH  0x07
#define LM83_REG_W_REMOTE2_HIGH  0x0D
#define LM83_REG_R_REMOTE3_TEMP  0x31
#define LM83_REG_R_REMOTE3_HIGH  0x3A
#define LM83_REG_W_REMOTE3_HIGH  0x52
#define LM83_REG_R_TCRIT         0x42
#define LM83_REG_W_TCRIT         0x5A

/*
 * Conversions and various macros
 * The LM83 uses signed 8-bit values with LSB = 1 degree Celcius.
 */

#define TEMP_FROM_REG(val)	(val)
#define TEMP_TO_REG(val)	((val) <= -50 ? -50 : \
				 (val) >= 127 ? 127 : (val))

static const u8 LM83_REG_R_TEMP[] = {
	LM83_REG_R_LOCAL_TEMP,
	LM83_REG_R_REMOTE1_TEMP,
	LM83_REG_R_REMOTE2_TEMP,
	LM83_REG_R_REMOTE3_TEMP
};

static const u8 LM83_REG_R_HIGH[] = {
	LM83_REG_R_LOCAL_HIGH,
	LM83_REG_R_REMOTE1_HIGH,
	LM83_REG_R_REMOTE2_HIGH,
	LM83_REG_R_REMOTE3_HIGH
};

static const u8 LM83_REG_W_HIGH[] = {
	LM83_REG_W_LOCAL_HIGH,
	LM83_REG_W_REMOTE1_HIGH,
	LM83_REG_W_REMOTE2_HIGH,
	LM83_REG_W_REMOTE3_HIGH
};

/*
 * Functions declaration
 */

static int lm83_attach_adapter(struct i2c_adapter *adapter);
static int lm83_detect(struct i2c_adapter *adapter, int address, unsigned
	short flags, int kind);
static int lm83_detach_client(struct i2c_client *client);
static void lm83_update_client(struct i2c_client *client);
static void lm83_temp(struct i2c_client *client, int operation, int
	ctl_name, int *nrels_mag, long *results);
static void lm83_tcrit(struct i2c_client *client, int operation, int
	ctl_name, int *nrels_mag, long *results);
static void lm83_alarms(struct i2c_client *client, int operation, int
	ctl_name, int *nrels_mag, long *results);

/*
 * Driver data (common to all clients)
 */
 
static struct i2c_driver lm83_driver = {
	.name           = "LM83 sensor driver",
	.id             = I2C_DRIVERID_LM83,
	.flags          = I2C_DF_NOTIFY,
	.attach_adapter = lm83_attach_adapter,
	.detach_client  = lm83_detach_client,
};

/*
 * Client data (each client gets its own)
 */

struct lm83_data
{
	struct i2c_client client;
	int sysctl_id;

	struct semaphore update_lock;
	char valid; /* zero until following fields are valid */
	unsigned long last_updated; /* in jiffies */

	/* registers values */
	s8 temp[4], temp_high[4], tcrit;
	u16 alarms; /* bitvector, combined */
};

/*
 * Proc entries
 * These files are created for each detected LM83.
 */

/* -- SENSORS SYSCTL START -- */

#define LM83_SYSCTL_LOCAL_TEMP    1200
#define LM83_SYSCTL_REMOTE1_TEMP  1201
#define LM83_SYSCTL_REMOTE2_TEMP  1202
#define LM83_SYSCTL_REMOTE3_TEMP  1203
#define LM83_SYSCTL_TCRIT         1208
#define LM83_SYSCTL_ALARMS        1210

#define LM83_ALARM_LOCAL_HIGH     0x0040
#define LM83_ALARM_LOCAL_CRIT     0x0001
#define LM83_ALARM_REMOTE1_HIGH   0x8000
#define LM83_ALARM_REMOTE1_CRIT   0x0100
#define LM83_ALARM_REMOTE1_OPEN   0x2000
#define LM83_ALARM_REMOTE2_HIGH   0x0010
#define LM83_ALARM_REMOTE2_CRIT   0x0002
#define LM83_ALARM_REMOTE2_OPEN   0x0004
#define LM83_ALARM_REMOTE3_HIGH   0x1000
#define LM83_ALARM_REMOTE3_CRIT   0x0200
#define LM83_ALARM_REMOTE3_OPEN   0x0400

/* -- SENSORS SYSCTL END -- */


static ctl_table lm83_dir_table_template[] =
{
	{LM83_SYSCTL_LOCAL_TEMP, "temp1", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &lm83_temp},
	{LM83_SYSCTL_REMOTE1_TEMP, "temp2", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &lm83_temp},
	{LM83_SYSCTL_REMOTE2_TEMP, "temp3", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &lm83_temp},
	{LM83_SYSCTL_REMOTE3_TEMP, "temp4", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &lm83_temp},
	{LM83_SYSCTL_TCRIT, "tcrit", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &lm83_tcrit},
	{LM83_SYSCTL_ALARMS, "alarms", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &lm83_alarms},
	{0}
};

/*
 * Real code
 */

static int lm83_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_detect(adapter, &addr_data, lm83_detect);
}

/*
 * The following function does more than just detection. If detection
 * succeeds, it also registers the new chip.
 */
static int lm83_detect(struct i2c_adapter *adapter, int address, unsigned
	short flags, int kind)
{
	struct i2c_client *new_client;
	struct lm83_data *data;
	int err = 0;
	const char *type_name = "";
	const char *client_name = "";

#ifdef DEBUG
	if (i2c_is_isa_adapter(adapter))
	{
		printk("lm83.o: Called for an ISA bus adapter, aborting.\n");
		return 0;
	}
#endif

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
	{
#ifdef DEBUG
		printk("lm83.o: I2C bus doesn't support byte read mode, "
		       "skipping.\n");
#endif
		return 0;
	}

	if (!(data = kmalloc(sizeof(struct lm83_data), GFP_KERNEL)))
	{
		printk("lm83.o: Out of memory in lm83_detect (new_client).\n");
		return -ENOMEM;
	}

	/*
	 * The common I2C client data is placed right before the
	 * LM83-specific data. The LM83-specific data is pointed to by the
	 * data field from the I2C client data.
	 */

	new_client = &data->client;
	new_client->addr = address;
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &lm83_driver;
	new_client->flags = 0;

	/*
	 * Now we do the remaining detection. A negative kind means that
	 * the driver was loaded with no force parameter (default), so we
	 * must both detect and identify the chip (actually there is only
	 * one possible kind of chip for now, LM83). A zero kind means that
	 * the driver was loaded with the force parameter, the detection
	 * step shall be skipped. A positive kind means that the driver
	 * was loaded with the force parameter and a given kind of chip is
	 * requested, so both the detection and the identification steps
	 * are skipped.
	 */

	/* Default to an LM83 if forced */
	if (kind == 0)
		kind = lm83;

	if (kind < 0) /* detection */
	{
		if (((i2c_smbus_read_byte_data(new_client, LM83_REG_R_STATUS1)
		      & 0xA8) != 0x00)
		||  ((i2c_smbus_read_byte_data(new_client, LM83_REG_R_STATUS2)
		      & 0x48) != 0x00)
		||  ((i2c_smbus_read_byte_data(new_client, LM83_REG_R_CONFIG)
		      & 0x41) != 0x00))
		{
#ifdef DEBUG
			printk(KERN_DEBUG "lm83.o: Detection failed at 0x%02x.\n",
				address);
#endif
			goto ERROR1;
		}
	}

	if (kind <= 0) /* identification */
	{
		u8 man_id, chip_id;

		man_id = i2c_smbus_read_byte_data(new_client, LM83_REG_R_MAN_ID);
		chip_id = i2c_smbus_read_byte_data(new_client, LM83_REG_R_CHIP_ID);
		if (man_id == 0x01) /* National Semiconductor */
		{
			if (chip_id == 0x03)
				kind = lm83;
		}
	}

	if (kind <= 0) /* identification failed */
	{
		printk("lm83.o: Unsupported chip.\n");
		goto ERROR1;
	}

	if (kind == lm83)
	{
		type_name = "lm83";
		client_name = "LM83 chip";
	}
	else
	{
		printk("lm83.o: Unknown kind %d.\n", kind);
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

	if ((err = i2c_attach_client(new_client)))
	{
#ifdef DEBUG
		printk("lm83.o: Failed attaching client.\n");
#endif
		goto ERROR1;
	}

	/*
	 * Register a new directory entry.
	 */

	if ((err = i2c_register_entry(new_client, type_name,
	     lm83_dir_table_template, THIS_MODULE)) < 0)
	{
#ifdef DEBUG
		printk("lm83.o: Failed registering directory entry.\n");
#endif
		goto ERROR2;
	}
	data->sysctl_id = err;

	/*
	 * Initialize the LM83 chip
	 * (Nothing to do for this one.)
	 */

	return 0;

	ERROR2:
	i2c_detach_client(new_client);
	ERROR1:
	kfree(data);
	return err;
}

static int lm83_detach_client(struct i2c_client *client)
{
	int err;

	i2c_deregister_entry(((struct lm83_data *) (client->data))->sysctl_id);
	if ((err = i2c_detach_client(client)))
	{
		printk("lm83.o: Client deregistration failed, client not "
		       "detached.\n");
		return err;
	}

	kfree(client->data);
	return 0;
}

static void lm83_update_client(struct i2c_client *client)
{
	struct lm83_data *data = client->data;

	down(&data->update_lock);

	if ((jiffies - data->last_updated > HZ * 2) ||
	    (jiffies < data->last_updated) || !data->valid)
	{
		int nr;
#ifdef DEBUG
		printk("lm83.o: Updating LM83 data.\n");
#endif
		for (nr = 0; nr < 4 ; nr++)
		{
			data->temp[nr] =
				i2c_smbus_read_byte_data(client, LM83_REG_R_TEMP[nr]);
			data->temp_high[nr] =
				i2c_smbus_read_byte_data(client, LM83_REG_R_HIGH[nr]);
		}
		data->tcrit = i2c_smbus_read_byte_data(client, LM83_REG_R_TCRIT);
		data->alarms =
			i2c_smbus_read_byte_data(client, LM83_REG_R_STATUS1) +
			(i2c_smbus_read_byte_data(client, LM83_REG_R_STATUS2) << 8);

		data->last_updated = jiffies;
		data->valid = 1;
	}

	up(&data->update_lock);
}

static void lm83_temp(struct i2c_client *client, int operation, int
	ctl_name, int *nrels_mag, long *results)
{
	struct lm83_data *data = client->data;
	int nr = ctl_name - LM83_SYSCTL_LOCAL_TEMP;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0; /* magnitude */
	else if (operation == SENSORS_PROC_REAL_READ)
	{
		lm83_update_client(client);
		results[0] = TEMP_FROM_REG(data->temp_high[nr]);
		results[1] = TEMP_FROM_REG(data->temp[nr]);
		*nrels_mag = 2;
	}
	else if (operation == SENSORS_PROC_REAL_WRITE)
	{
		if (*nrels_mag >= 1)
		{
			data->temp_high[nr] = TEMP_TO_REG(results[0]);
			i2c_smbus_write_byte_data(client, LM83_REG_W_HIGH[nr],
					    data->temp_high[nr]);
		}
	}
}

static void lm83_tcrit(struct i2c_client *client, int operation, int
	ctl_name, int *nrels_mag, long *results)
{
	struct lm83_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0; /* magnitude */
	else if (operation == SENSORS_PROC_REAL_READ)
	{
		lm83_update_client(client);
		results[0] = TEMP_FROM_REG(data->tcrit);
		*nrels_mag = 1;
	}
	else if (operation == SENSORS_PROC_REAL_WRITE)
	{
		if (*nrels_mag >= 1)
		{
			data->tcrit = TEMP_TO_REG(results[0]);
			i2c_smbus_write_byte_data(client, LM83_REG_W_TCRIT,
				data->tcrit);
		}
	}
}

static void lm83_alarms(struct i2c_client *client, int operation, int
	ctl_name, int *nrels_mag, long *results)
{
	struct lm83_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0; /* magnitude */
	else if (operation == SENSORS_PROC_REAL_READ)
	{
		lm83_update_client(client);
		results[0] = data->alarms;
		*nrels_mag = 1;
	}
}

static int __init sm_lm83_init(void)
{
	printk(KERN_INFO "lm83.o version %s (%s)\n", LM_VERSION, LM_DATE);
	return i2c_add_driver(&lm83_driver);
}

static void __exit sm_lm83_exit(void)
{
	i2c_del_driver(&lm83_driver);
}

MODULE_AUTHOR("Jean Delvare <khali@linux-fr.org>");
MODULE_DESCRIPTION("LM83 sensor driver");
MODULE_LICENSE("GPL");

module_init(sm_lm83_init);
module_exit(sm_lm83_exit);
