/*
 * pca9540.c - Part of lm_sensors, Linux kernel modules for hardware
 *             monitoring
 * Copyright (c) 2004  Jean Delvare <khali@linux-fr.org>
 *
 * Based on pcf8574.c from the same project by Frodo Looijaard,
 * Philip Edelbrock, Dan Eaton and Aurelien Jarno.
 *
 * The PCA9540 is a 2-channel I2C multiplexer made by Philips
 * Semiconductors. It is controlled via the I2C bus itself.
 * The SCL/SDA upstream pair fans out to two SCL/SDA downstream
 * pairs, or channels. Only one SCL/SDA channel is selected at a
 * time, determined by the contents of the programmable control
 * register.
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

MODULE_LICENSE("GPL");

/* Addresses to scan */
static unsigned short normal_i2c[] = { 0x70, SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { SENSORS_I2C_END };
static unsigned int normal_isa[] = { SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/* Insmod parameters */
SENSORS_INSMOD_1(pca9540);

/* Each client has this additional data */
struct pca9540_data {
	struct i2c_client client;
	int sysctl_id;

	struct semaphore update_lock;

	u8 control;	/* Register value */
};

static int pca9540_attach_adapter(struct i2c_adapter *adapter);
static int pca9540_detect(struct i2c_adapter *adapter, int address,
			  unsigned short flags, int kind);
static int pca9540_detach_client(struct i2c_client *client);
static void pca9540_update_client(struct i2c_client *client);

static void pca9540_channel(struct i2c_client *client, int operation,
			    int ctl_name, int *nrels_mag, long *results);

/* This is the driver that will be inserted */
static struct i2c_driver pca9540_driver = {
	.name		= "PCA9540 chip driver",
	.flags		= I2C_DF_NOTIFY,
	.attach_adapter	= pca9540_attach_adapter,
	.detach_client	= pca9540_detach_client,
};


/* -- SENSORS SYSCTL START -- */

#define PCA9540_SYSCTL_CHANNEL		1000

/* -- SENSORS SYSCTL END -- */

static ctl_table pca9540_dir_table_template[] = {
	{PCA9540_SYSCTL_CHANNEL, "channel", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &pca9540_channel},
	{0}
};

static int pca9540_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_detect(adapter, &addr_data, pca9540_detect);
}

/* This function is called by i2c_detect. */
int pca9540_detect(struct i2c_adapter *adapter, int address,
		   unsigned short flags, int kind)
{
	int i;
	struct i2c_client *new_client;
	struct pca9540_data *data;
	int err = 0;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE))
		goto ERROR0;

	/* OK. For now, we presume we have a valid client. We now create the
	   client structure, even though we cannot fill it completely yet. */
	if (!(data = kmalloc(sizeof(struct pca9540_data), GFP_KERNEL))) {
		err = -ENOMEM;
		goto ERROR0;
	}

	new_client = &data->client;
	new_client->addr = address;
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &pca9540_driver;
	new_client->flags = 0;

	/* The detection is very weak. */
	if (kind < 0) {
		u8 reg = i2c_smbus_read_byte(new_client);
		if ((reg & 0xfa) != 0x00
		 || reg != i2c_smbus_read_byte(new_client)
		 || reg != i2c_smbus_read_byte(new_client)
		 || reg != i2c_smbus_read_byte(new_client))
			goto ERROR1;
	}

	kind = pca9540;

	/* Fill in the remaining client fields and put it into the global list */
	strcpy(new_client->name, "PCA9540 chip");
	init_MUTEX(&data->update_lock);

	/* Tell the I2C layer a new client has arrived */
	if ((err = i2c_attach_client(new_client)))
		goto ERROR1;

	/* Register a new directory entry with module sensors */
	if ((i = i2c_register_entry(new_client, "pca9540",
				    pca9540_dir_table_template,
				    THIS_MODULE)) < 0) {
		err = i;
		goto ERROR2;
	}
	data->sysctl_id = i;

	/* Initialize the PCA9540 chip: nothing needed */

	return 0;

/* OK, this is not exactly good programming practice, usually. But it is
   very code-efficient in this case. */

      ERROR2:
	i2c_detach_client(new_client);
      ERROR1:
	kfree(data);
      ERROR0:
	return err;
}

static int pca9540_detach_client(struct i2c_client *client)
{
	int err;

	i2c_deregister_entry(((struct pca9540_data *) (client->data))->sysctl_id);

	if ((err = i2c_detach_client(client))) {
		printk(KERN_ERR "pca9540.o: Client deregistration failed, "
		       "client not detached.\n");
		return err;
	}

	kfree(client->data);

	return 0;
}

static void pca9540_update_client(struct i2c_client *client)
{
	struct pca9540_data *data = client->data;

	down(&data->update_lock);

#ifdef DEBUG
	printk(KERN_DEBUG "pca9540.o: Starting update.\n");
#endif
	data->control = i2c_smbus_read_byte(client) & 0x07; 

	up(&data->update_lock);
}


void pca9540_channel(struct i2c_client *client, int operation,
		     int ctl_name, int *nrels_mag, long *results)
{
	struct pca9540_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		pca9540_update_client(client);
		switch(data->control) {
			case 4: results[0] = 1; break;
			case 5: results[0] = 2; break;
			default: results[0] = 0;
		}
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			switch(results[0]) {
				case 1: data->control = 4; break;
				case 2: data->control = 5; break;
				case 0: data->control = 0; break;
				default: return; /* invalid value */
			}
			i2c_smbus_write_byte(client, data->control);
		}
	}
}


static int __init pca9540_init(void)
{
	printk("pca9540.o version %s (%s)\n", LM_VERSION, LM_DATE);
	return i2c_add_driver(&pca9540_driver);
}

static void __exit pca9540_exit(void)
{
	i2c_del_driver(&pca9540_driver);
}


MODULE_AUTHOR("Jean Delvare <khali@linux-fr.org>");
MODULE_DESCRIPTION("PCA9540 driver");

module_init(pca9540_init);
module_exit(pca9540_exit);
