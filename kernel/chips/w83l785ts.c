/*
 * w83l785ts.c - Part of lm_sensors, Linux kernel modules for hardware
 *               monitoring
 * Copyright (C) 2003-2004  Jean Delvare <khali@linux-fr.org>
 *
 * Inspired from the lm83 driver. The W83L785TS-S is a sensor chip made
 * by Winbond. It reports a single external temperature with a 1 deg
 * resolution and a 3 deg accuracy. Data sheet can be obtained from
 * Winbond's website at:
 *   http://www.winbond-usa.com/products/winbond_products/pdfs/PCIC/W83L785TS-S.pdf
 *
 * Thanks to James Bolt <james@evilpenguin.com> for benchmarking the read
 * error handling mechanism.
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
#include <linux/delay.h>
#include "version.h"

#ifndef I2C_DRIVERID_W83L785TS
#define I2C_DRIVERID_W83L785TS	1047
#endif

/* How many retries on register read error */
#define MAX_RETRIES	5

/*
 * Address to scan
 * Address is fully defined internally and cannot be changed.
 */

static unsigned short normal_i2c[] = { 0x2e, SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { SENSORS_I2C_END };
static unsigned int normal_isa[] = { SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/*
 * Insmod parameters
 */

SENSORS_INSMOD_1(w83l785ts);

/*
 * The W83L785TS-S registers
 * Manufacturer ID is 0x5CA3 for Winbond.
 */

#define W83L785TS_REG_MAN_ID1		0x4D
#define W83L785TS_REG_MAN_ID2		0x4C
#define W83L785TS_REG_CHIP_ID		0x4E
#define W83L785TS_REG_CONFIG		0x40
#define W83L785TS_REG_TYPE		0x52
#define W83L785TS_REG_TEMP		0x27
#define W83L785TS_REG_TEMP_OVER		0x53 /* not sure about this one */

/*
 * Conversions
 * The W83L785TS-S uses signed 8-bit values.
 */

#define TEMP_FROM_REG(val)	(val & 0x80 ? val-0x100 : val)

/*
 * Functions declaration
 */

static int w83l785ts_attach_adapter(struct i2c_adapter *adapter);
static int w83l785ts_detect(struct i2c_adapter *adapter, int address, unsigned
	short flags, int kind);
static int w83l785ts_detach_client(struct i2c_client *client);
static u8 w83l785ts_read_value(struct i2c_client *client, u8 reg, u8 defval);
static void w83l785ts_update_client(struct i2c_client *client);
static void w83l785ts_temp(struct i2c_client *client, int operation, int
	ctl_name, int *nrels_mag, long *results);

/*
 * Driver data (common to all clients)
 */
 
static struct i2c_driver w83l785ts_driver = {
	.name		= "W83L785S-S sensor driver",
	.id		= I2C_DRIVERID_W83L785TS,
	.flags		= I2C_DF_NOTIFY,
	.attach_adapter	= w83l785ts_attach_adapter,
	.detach_client	= w83l785ts_detach_client,
};

/*
 * Client data (each client gets its own)
 */

struct w83l785ts_data {
	struct i2c_client client;
	int sysctl_id;

	struct semaphore update_lock;
	char valid; /* zero until following fields are valid */
	unsigned long last_updated; /* in jiffies */

	/* registers values */
	u8 temp, temp_over;
};

/*
 * Proc entries
 * These files are created for each detected W83L785TS-S.
 */

/* -- SENSORS SYSCTL START -- */

#define W83L785TS_SYSCTL_TEMP	1200

/* -- SENSORS SYSCTL END -- */


static ctl_table w83l785ts_dir_table_template[] =
{
	{W83L785TS_SYSCTL_TEMP, "temp", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &w83l785ts_temp},
	{0}
};

/*
 * Real code
 */

static int w83l785ts_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_detect(adapter, &addr_data, w83l785ts_detect);
}

/*
 * The following function does more than just detection. If detection
 * succeeds, it also registers the new chip.
 */
static int w83l785ts_detect(struct i2c_adapter *adapter, int address,
	unsigned short flags, int kind)
{
	struct i2c_client *new_client;
	struct w83l785ts_data *data;
	int err = 0;
	const char *type_name = "";
	const char *client_name = "";

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
#ifdef DEBUG
		printk(KERN_DEBUG "w83l785ts.o: I2C bus doesn't support "
			"byte read mode, skipping.\n");
#endif
		return 0;
	}

	if (!(data = kmalloc(sizeof(struct w83l785ts_data), GFP_KERNEL))) {
		printk(KERN_ERR "w83l785ts.o: Out of memory in w83l785ts_detect "
			"(new_client).\n");
		return -ENOMEM;
	}

	/*
	 * The common I2C client data is placed right after the
	 * W83L785TS-specific. The W83L785TS-specific data is pointed to by the
	 * data field from the I2C client data.
	 */

	new_client = &data->client;
	new_client->addr = address;
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &w83l785ts_driver;
	new_client->flags = 0;

	/*
	 * Now we do the remaining detection. A negative kind means that
	 * the driver was loaded with no force parameter (default), so we
	 * must both detect and identify the chip (actually there is only
	 * one possible kind of chip for now, W83L785TS-S). A zero kind means
	 * that the driver was loaded with the force parameter, the detection
	 * step shall be skipped. A positive kind means that the driver
	 * was loaded with the force parameter and a given kind of chip is
	 * requested, so both the detection and the identification steps
	 * are skipped.
	 */

	if (kind < 0) { /* detection */
		if (((w83l785ts_read_value(new_client, W83L785TS_REG_CONFIG, 0)
			& 0x80) != 0x00)
		 || ((w83l785ts_read_value(new_client, W83L785TS_REG_TYPE, 0)
		 	& 0xFC) != 0x00)) {
#ifdef DEBUG
			printk(KERN_DEBUG "w83l785ts.o: Detection failed at "
				"0x%02x.\n", address);
#endif
			goto ERROR1;
		}
	}

	if (kind <= 0) { /* identification */
		u16 man_id;
		u8 chip_id;

		man_id = (w83l785ts_read_value(new_client, W83L785TS_REG_MAN_ID1, 0) << 8)
		       +  w83l785ts_read_value(new_client, W83L785TS_REG_MAN_ID2, 0);
		chip_id = w83l785ts_read_value(new_client, W83L785TS_REG_CHIP_ID, 0);
		if (man_id == 0x5CA3) { /* Winbond */
			if (chip_id == 0x70)
				kind = w83l785ts;
		}
	}

	if (kind <= 0) { /* identification failed */
		printk(KERN_INFO "w83l785ts.o: Unsupported chip.\n");
		goto ERROR1;
	}

	if (kind == w83l785ts) {
		type_name = "w83l785ts";
		client_name = "W83L785TS-S chip";
	} else {
		printk(KERN_ERR "w83l785ts.o: Unknown kind %d.\n", kind);
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
#ifdef DEBUG
		printk(KERN_ERR "w83l785ts.o: Failed attaching client.\n");
#endif
		goto ERROR1;
	}

	/*
	 * Register a new directory entry.
	 */

	if ((err = i2c_register_entry(new_client, type_name,
	    w83l785ts_dir_table_template, THIS_MODULE)) < 0) {
#ifdef DEBUG
		printk(KERN_ERR "w83l785ts.o: Failed registering directory "
			"entry.\n");
#endif
		goto ERROR2;
	}
	data->sysctl_id = err;

	/*
	 * Initialize the W83L785TS chip
	 * Nothing yet, assume it is already started.
	 */

	return 0;

	ERROR2:
	i2c_detach_client(new_client);
	ERROR1:
	kfree(data);
	return err;
}

static int w83l785ts_detach_client(struct i2c_client *client)
{
	int err;

	i2c_deregister_entry(((struct w83l785ts_data *) (client->data))->sysctl_id);
	if ((err = i2c_detach_client(client))) {
		printk(KERN_ERR "w83l785ts.o: Client deregistration failed, "
			"client not detached.\n");
		return err;
	}

	kfree(client->data);
	return 0;
}

static u8 w83l785ts_read_value(struct i2c_client *client, u8 reg, u8 defval)
{
	int value, i;

	/* Frequent read errors have been reported on Asus boards, so we
	 * retry on read errors. If it still fails (unlikely), return the
	 * default value requested by the caller. */
	for (i = 1; i <= MAX_RETRIES; i++) {
		value = i2c_smbus_read_byte_data(client, reg);
		if (value >= 0)
			return value;
		printk(KERN_WARNING "w83l785ts.o: Read failed, will retry "
			"in %d.\n", i);
		mdelay(i);
	}

	printk(KERN_ERR "w83l785ts.o: Couldn't read value from register. "
		"Please report.\n");
	return defval;
}

static void w83l785ts_update_client(struct i2c_client *client)
{
	struct w83l785ts_data *data = client->data;

	down(&data->update_lock);

	if ((jiffies - data->last_updated > HZ * 2)
	 || (jiffies < data->last_updated)
	 || !data->valid) {
#ifdef DEBUG
		printk(KERN_DEBUG "w83l785ts.o: Updating data.\n");
#endif
		data->temp = w83l785ts_read_value(client, W83L785TS_REG_TEMP,
			data->temp);
		data->temp_over = w83l785ts_read_value(client,
			W83L785TS_REG_TEMP_OVER, data->temp_over);
		data->last_updated = jiffies;
		data->valid = 1;
	}

	up(&data->update_lock);
}

static void w83l785ts_temp(struct i2c_client *client, int operation,
	int ctl_name, int *nrels_mag, long *results)
{
	struct w83l785ts_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO) {
		*nrels_mag = 0; /* magnitude */
	} else if (operation == SENSORS_PROC_REAL_READ) {
		w83l785ts_update_client(client);
		results[0] = TEMP_FROM_REG(data->temp_over);
		results[1] = TEMP_FROM_REG(data->temp);
		*nrels_mag = 2;
	}
}

static int __init sm_w83l785ts_init(void)
{
	printk(KERN_INFO "w83l785ts.o version %s (%s)\n", LM_VERSION, LM_DATE);
	return i2c_add_driver(&w83l785ts_driver);
}

static void __exit sm_w83l785ts_exit(void)
{
	i2c_del_driver(&w83l785ts_driver);
}

MODULE_AUTHOR("Jean Delvare <khali@linux-fr.org>");
MODULE_DESCRIPTION("W83L785TS-S sensor driver");
MODULE_LICENSE("GPL");

module_init(sm_w83l785ts_init);
module_exit(sm_w83l785ts_exit);
