/*
    lm75.c - Part of lm_sensors, Linux kernel modules for hardware
             monitoring
    Copyright (c) 1998, 1999  Frodo Looijaard <frodol@dds.nl>

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
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/i2c-proc.h>
#include <linux/init.h>
#include "version.h"
#include "lm75.h"

/* Addresses to scan */
static unsigned short normal_i2c[] = { SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { 0x48, 0x4f, SENSORS_I2C_END };
static unsigned int normal_isa[] = { SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/* Insmod parameters */
SENSORS_INSMOD_1(lm75);

/* Many LM75 constants specified below */

/* The LM75 registers */
#define LM75_REG_TEMP 0x00
#define LM75_REG_CONF 0x01
#define LM75_REG_TEMP_HYST 0x02
#define LM75_REG_TEMP_OS 0x03

/* Each client has this additional data */
struct lm75_data {
	struct i2c_client client;
	int sysctl_id;

	struct semaphore update_lock;
	char valid;		/* !=0 if following fields are valid */
	unsigned long last_updated;	/* In jiffies */

	u16 temp, temp_os, temp_hyst;	/* Register values */
};

static int lm75_attach_adapter(struct i2c_adapter *adapter);
static int lm75_detect(struct i2c_adapter *adapter, int address,
		       unsigned short flags, int kind);
static void lm75_init_client(struct i2c_client *client);
static int lm75_detach_client(struct i2c_client *client);

static int lm75_read_value(struct i2c_client *client, u8 reg);
static int lm75_write_value(struct i2c_client *client, u8 reg, u16 value);
static void lm75_temp(struct i2c_client *client, int operation,
		      int ctl_name, int *nrels_mag, long *results);
static void lm75_update_client(struct i2c_client *client);


/* This is the driver that will be inserted */
static struct i2c_driver lm75_driver = {
	.name		= "LM75 sensor chip driver",
	.id		= I2C_DRIVERID_LM75,
	.flags		= I2C_DF_NOTIFY,
	.attach_adapter	= lm75_attach_adapter,
	.detach_client	= lm75_detach_client,
};

/* -- SENSORS SYSCTL START -- */

#define LM75_SYSCTL_TEMP 1200	/* Degrees Celcius * 10 */

/* -- SENSORS SYSCTL END -- */

/* These files are created for each detected LM75. This is just a template;
   though at first sight, you might think we could use a statically
   allocated list, we need some way to get back to the parent - which
   is done through one of the 'extra' fields which are initialized
   when a new copy is allocated. */
static ctl_table lm75_dir_table_template[] = {
	{LM75_SYSCTL_TEMP, "temp", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &lm75_temp},
	{0}
};

static int lm75_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_detect(adapter, &addr_data, lm75_detect);
}

/* This function is called by i2c_detect */
int lm75_detect(struct i2c_adapter *adapter, int address,
		unsigned short flags, int kind)
{
	int i;
	struct i2c_client *new_client;
	struct lm75_data *data;
	int err = 0;
	const char *type_name, *client_name;

	/* Make sure we aren't probing the ISA bus!! This is just a safety check
	   at this moment; i2c_detect really won't call us. */
#ifdef DEBUG
	if (i2c_is_isa_adapter(adapter)) {
		printk
		    ("lm75.o: lm75_detect called for an ISA bus adapter?!?\n");
		return 0;
	}
#endif

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA |
				     I2C_FUNC_SMBUS_WORD_DATA))
		    goto error0;

	/* OK. For now, we presume we have a valid client. We now create the
	   client structure, even though we cannot fill it completely yet.
	   But it allows us to access lm75_{read,write}_value. */
	if (!(data = kmalloc(sizeof(struct lm75_data), GFP_KERNEL))) {
		err = -ENOMEM;
		goto error0;
	}

	new_client = &data->client;
	new_client->addr = address;
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &lm75_driver;
	new_client->flags = 0;

	/* Now, we do the remaining detection. There is no identification-
	   dedicated register so we have to rely on several tricks:
	   unused bits, registers cycling over 8-address boundaries,
	   addresses 0x04-0x07 returning the last read value.
	   The cycling+unused addresses combination is not tested,
	   since it would significantly slow the detection down and would
	   hardly add any value. */
	if (kind < 0) {
		int cur, conf, hyst, os;

		/* Unused addresses */
		cur = i2c_smbus_read_word_data(new_client, 0);
		conf = i2c_smbus_read_byte_data(new_client, 1);
		hyst = i2c_smbus_read_word_data(new_client, 2);
		if (i2c_smbus_read_word_data(new_client, 4) != hyst
		 || i2c_smbus_read_word_data(new_client, 5) != hyst
		 || i2c_smbus_read_word_data(new_client, 6) != hyst
		 || i2c_smbus_read_word_data(new_client, 7) != hyst)
		 	goto error1;
		os = i2c_smbus_read_word_data(new_client, 3);
		if (i2c_smbus_read_word_data(new_client, 4) != os
		 || i2c_smbus_read_word_data(new_client, 5) != os
		 || i2c_smbus_read_word_data(new_client, 6) != os
		 || i2c_smbus_read_word_data(new_client, 7) != os)
		 	goto error1;

		/* Unused bits */
		if (conf & 0xe0)
		 	goto error1;

		/* Addresses cycling */
		for (i = 8; i < 0xff; i += 8)
			if (i2c_smbus_read_byte_data(new_client, i + 1) != conf
			 || i2c_smbus_read_word_data(new_client, i + 2) != hyst
			 || i2c_smbus_read_word_data(new_client, i + 3) != os)
				goto error1;
	}

	/* Determine the chip type - only one kind supported! */
	if (kind <= 0)
		kind = lm75;

	if (kind == lm75) {
		type_name = "lm75";
		client_name = "LM75 chip";
	} else {
		pr_debug("lm75.o: Internal error: unknown kind (%d)?!?", kind);
		goto error1;
	}

	/* Fill in the remaining client fields and put it into the global list */
	strcpy(new_client->name, client_name);
	data->valid = 0;
	init_MUTEX(&data->update_lock);

	/* Tell the I2C layer a new client has arrived */
	if ((err = i2c_attach_client(new_client)))
		goto error3;

	/* Register a new directory entry with module sensors */
	if ((i = i2c_register_entry(new_client, type_name,
					lm75_dir_table_template,
					THIS_MODULE)) < 0) {
		err = i;
		goto error4;
	}
	data->sysctl_id = i;

	lm75_init_client(new_client);
	return 0;

/* OK, this is not exactly good programming practice, usually. But it is
   very code-efficient in this case. */

      error4:
	i2c_detach_client(new_client);
      error3:
      error1:
	kfree(data);
      error0:
	return err;
}

static int lm75_detach_client(struct i2c_client *client)
{
	struct lm75_data *data = client->data;

	i2c_deregister_entry(data->sysctl_id);
	i2c_detach_client(client);
	kfree(client->data);
	return 0;
}

/* All registers are word-sized, except for the configuration register.
   LM75 uses a high-byte first convention, which is exactly opposite to
   the usual practice. */
static int lm75_read_value(struct i2c_client *client, u8 reg)
{
	if (reg == LM75_REG_CONF)
		return i2c_smbus_read_byte_data(client, reg);
	else
		return swab16(i2c_smbus_read_word_data(client, reg));
}

/* All registers are word-sized, except for the configuration register.
   LM75 uses a high-byte first convention, which is exactly opposite to
   the usual practice. */
static int lm75_write_value(struct i2c_client *client, u8 reg, u16 value)
{
	if (reg == LM75_REG_CONF)
		return i2c_smbus_write_byte_data(client, reg, value);
	else
		return i2c_smbus_write_word_data(client, reg, swab16(value));
}

static void lm75_init_client(struct i2c_client *client)
{
	int i;

	/* Enable if in shutdown */
	i = lm75_read_value(client, LM75_REG_CONF);
	if(i >= 0 && ((u8) i) & 0x01)
		lm75_write_value(client, LM75_REG_CONF, ((u8) i) & 0xfe);
}

static void lm75_update_client(struct i2c_client *client)
{
	struct lm75_data *data = client->data;

	down(&data->update_lock);

	if ((jiffies - data->last_updated > HZ + HZ / 2) ||
	    (jiffies < data->last_updated) || !data->valid) {
		pr_debug("Starting lm75 update\n");

		data->temp = lm75_read_value(client, LM75_REG_TEMP);
		data->temp_os = lm75_read_value(client, LM75_REG_TEMP_OS);
		data->temp_hyst =
		    lm75_read_value(client, LM75_REG_TEMP_HYST);
		data->last_updated = jiffies;
		data->valid = 1;
	}

	up(&data->update_lock);
}


void lm75_temp(struct i2c_client *client, int operation, int ctl_name,
	       int *nrels_mag, long *results)
{
	struct lm75_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 1;
	else if (operation == SENSORS_PROC_REAL_READ) {
		lm75_update_client(client);
		results[0] = LM75_TEMP_FROM_REG(data->temp_os);
		results[1] = LM75_TEMP_FROM_REG(data->temp_hyst);
		results[2] = LM75_TEMP_FROM_REG(data->temp);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->temp_os = LM75_TEMP_TO_REG(results[0]);
			lm75_write_value(client, LM75_REG_TEMP_OS,
					 data->temp_os);
		}
		if (*nrels_mag >= 2) {
			data->temp_hyst = LM75_TEMP_TO_REG(results[1]);
			lm75_write_value(client, LM75_REG_TEMP_HYST,
					 data->temp_hyst);
		}
	}
}

static int __init sm_lm75_init(void)
{
	printk(KERN_INFO "lm75.o version %s (%s)\n", LM_VERSION, LM_DATE);
	return i2c_add_driver(&lm75_driver);
}

static void __exit sm_lm75_exit(void)
{
	i2c_del_driver(&lm75_driver);
}

MODULE_AUTHOR("Frodo Looijaard <frodol@dds.nl>");
MODULE_DESCRIPTION("LM75 driver");
MODULE_LICENSE("GPL");

module_init(sm_lm75_init);
module_exit(sm_lm75_exit);
