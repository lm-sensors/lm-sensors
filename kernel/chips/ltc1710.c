/*
    ltc1710.c - Part of lm_sensors, Linux kernel modules for hardware
             monitoring
    Copyright (c) 1999  Frodo Looijaard <frodol@dds.nl> and
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

/* A few notes about the LTC1710:

* The LTC1710 is a dual programmable switch.  It can be used to turn
  anything on or off anything which consumes less than 300mA of 
  current and up to 5.5V
  
* The LTC1710 is a very, very simple SMBus device with three possible 
   SMBus addresses (0x58,0x59, or 0x5A).  Only SMBus byte writes
   (command writes) are supported.

* Since only writes are supported, READS DON'T WORK!  The device 
  plays dead in the event of a read, so this makes detection a 
  bit tricky.
  
* BTW- I can safely say that this driver has been tested under
  every possible case, so there should be no bugs. :')
  
  --Phil

*/


#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/i2c-proc.h>
#include <linux/init.h>
#include "version.h"

MODULE_LICENSE("GPL");

/* Addresses to scan */
static unsigned short normal_i2c[] = { SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { 0x58, 0x5a, SENSORS_I2C_END };
static unsigned int normal_isa[] = { SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/* Insmod parameters */
SENSORS_INSMOD_1(ltc1710);

/* The LTC1710 registers */

/* (No registers.  [Wow! This thing is SIMPLE!] ) */

/* Initial values */
#define LTC1710_INIT 0		/* Both off */

/* Each client has this additional data */
struct ltc1710_data {
	struct i2c_client client;
	int sysctl_id;

	struct semaphore update_lock;
	char valid;		/* !=0 if following fields are valid */
	unsigned long last_updated;	/* In jiffies */

	u8 status;		/* Register values */
};

static int ltc1710_attach_adapter(struct i2c_adapter *adapter);
static int ltc1710_detect(struct i2c_adapter *adapter, int address,
			  unsigned short flags, int kind);
static int ltc1710_detach_client(struct i2c_client *client);

static void ltc1710_switch1(struct i2c_client *client, int operation,
			    int ctl_name, int *nrels_mag, long *results);
static void ltc1710_switch2(struct i2c_client *client, int operation,
			    int ctl_name, int *nrels_mag, long *results);
static void ltc1710_update_client(struct i2c_client *client);


/* This is the driver that will be inserted */
static struct i2c_driver ltc1710_driver = {
	.name		= "LTC1710 sensor chip driver",
	.id		= I2C_DRIVERID_LTC1710,
	.flags		= I2C_DF_NOTIFY,
	.attach_adapter	= ltc1710_attach_adapter,
	.detach_client	= ltc1710_detach_client,
};

/* -- SENSORS SYSCTL START -- */

#define LTC1710_SYSCTL_SWITCH_1 1000
#define LTC1710_SYSCTL_SWITCH_2 1001

/* -- SENSORS SYSCTL END -- */

/* These files are created for each detected LTC1710. This is just a template;
   though at first sight, you might think we could use a statically
   allocated list, we need some way to get back to the parent - which
   is done through one of the 'extra' fields which are initialized
   when a new copy is allocated. */
static ctl_table ltc1710_dir_table_template[] = {
	{LTC1710_SYSCTL_SWITCH_1, "switch1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &ltc1710_switch1},
	{LTC1710_SYSCTL_SWITCH_2, "switch2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &ltc1710_switch2},
	{0}
};

static int ltc1710_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_detect(adapter, &addr_data, ltc1710_detect);
}

/* This function is called by i2c_detect */
int ltc1710_detect(struct i2c_adapter *adapter, int address,
		   unsigned short flags, int kind)
{
	int i;
	struct i2c_client *new_client;
	struct ltc1710_data *data;
	int err = 0;
	const char *type_name, *client_name;

	/* Make sure we aren't probing the ISA bus!! This is just a safety check
	   at this moment; i2c_detect really won't call us. */
#ifdef DEBUG
	if (i2c_is_isa_adapter(adapter)) {
		printk
		    ("ltc1710.o: ltc1710_detect called for an ISA bus adapter?!?\n");
		return 0;
	}
#endif

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE))
		goto ERROR0;

	/* OK. For now, we presume we have a valid client. We now create the
	   client structure, even though we cannot fill it completely yet.
	   But it allows us to access ltc1710_{read,write}_value. */
	if (!(data = kmalloc(sizeof(struct ltc1710_data), GFP_KERNEL))) {
		err = -ENOMEM;
		goto ERROR0;
	}

	new_client = &data->client;
	new_client->addr = address;
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &ltc1710_driver;
	new_client->flags = 0;

	/* Now, we would do the remaining detection. But the LTC1710 is plainly
	   impossible to detect! Stupid chip. */

	/* Determine the chip type - only one kind supported! */
	if (kind <= 0)
		kind = ltc1710;

	if (kind == ltc1710) {
		type_name = "ltc1710";
		client_name = "LTC1710 chip";
	} else {
#ifdef DEBUG
		printk("ltc1710.o: Internal error: unknown kind (%d)?!?",
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
					ltc1710_dir_table_template,
					THIS_MODULE)) < 0) {
		err = i;
		goto ERROR4;
	}
	data->sysctl_id = i;

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


static int ltc1710_detach_client(struct i2c_client *client)
{
	int err;

	i2c_deregister_entry(((struct ltc1710_data *) (client->data))->
				 sysctl_id);

	if ((err = i2c_detach_client(client))) {
		printk
		    ("ltc1710.o: Client deregistration failed, client not detached.\n");
		return err;
	}

	kfree(client->data);

	return 0;

}

static void ltc1710_update_client(struct i2c_client *client)
{
	struct ltc1710_data *data = client->data;

	down(&data->update_lock);

	if ((jiffies - data->last_updated > HZ + HZ / 2) ||
	    (jiffies < data->last_updated) || !data->valid) {

#ifdef DEBUG
		printk("Starting ltc1710 update\n");
#endif

		/* data->status = i2c_smbus_read_byte(client); 
		   Unfortunately, reads always fail!  */
		data->last_updated = jiffies;
		data->valid = 1;
	}

	up(&data->update_lock);
}


void ltc1710_switch1(struct i2c_client *client, int operation,
		     int ctl_name, int *nrels_mag, long *results)
{
	struct ltc1710_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		ltc1710_update_client(client);
		results[0] = data->status & 1;
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->status = (data->status & 2) | results[0];
			i2c_smbus_write_byte(client, data->status);
		}
	}
}

void ltc1710_switch2(struct i2c_client *client, int operation,
		     int ctl_name, int *nrels_mag, long *results)
{
	struct ltc1710_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		ltc1710_update_client(client);
		results[0] = (data->status & 2) >> 1;
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->status =
			    (data->status & 1) | (results[0] << 1);
			i2c_smbus_write_byte(client, data->status);
		}
	}
}

static int __init sm_ltc1710_init(void)
{
	printk("ltc1710.o version %s (%s)\n", LM_VERSION, LM_DATE);
	return i2c_add_driver(&ltc1710_driver);
}

static void __exit sm_ltc1710_exit(void)
{
	i2c_del_driver(&ltc1710_driver);
}



MODULE_AUTHOR
    ("Frodo Looijaard <frodol@dds.nl> and Philip Edelbrock <phil@netroedge.com>");
MODULE_DESCRIPTION("LTC1710 driver");

module_init(sm_ltc1710_init);
module_exit(sm_ltc1710_exit);
