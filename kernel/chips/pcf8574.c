/*
    pcf8574.c - Part of lm_sensors, Linux kernel modules for hardware
             monitoring
    Copyright (c) 2000  Frodo Looijaard <frodol@dds.nl>, 
                        Philip Edelbrock <phil@netroedge.com>,
                        Dan Eaton <dan.eaton@rocketlogix.com>

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

/* A few notes about the PCF8574:

* The PCF8574 is an 8-bit I/O expander for the I2C bus produced by
  Philips Semiconductors.  It is designed to provide a byte I2C
  interface to up to 8 separate devices.
  
* The PCF8574 appears as a very simple SMBus device which can be
  read from or written to with SMBUS byte read/write accesses.

* Because of the general purpose nature of this device, it will most
  likely be necessary to customize the /proc interface to suit the
  specific application.

  --Dan

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
static unsigned short normal_i2c_range[] = { 0x20, 0x27, 0x38, 0x3f, SENSORS_I2C_END };
static unsigned int normal_isa[] = { SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/* Insmod parameters */
SENSORS_INSMOD_2(pcf8574, pcf8574a);

/* The PCF8574 registers */

/* (No registers.  [Wow! This thing is SIMPLE!] ) */

/* Initial values */
#define PCF8574_INIT 255	/* All outputs on (input mode) */

/* Each client has this additional data */
struct pcf8574_data {
	struct i2c_client client;
	int sysctl_id;

	struct semaphore update_lock;

	u8 read, write;		/* Register values */
};

static int pcf8574_attach_adapter(struct i2c_adapter *adapter);
static int pcf8574_detect(struct i2c_adapter *adapter, int address,
			  unsigned short flags, int kind);
static int pcf8574_detach_client(struct i2c_client *client);

static void pcf8574_read(struct i2c_client *client, int operation,
			               int ctl_name, int *nrels_mag, long *results);
static void pcf8574_write(struct i2c_client *client, int operation,
			               int ctl_name, int *nrels_mag, long *results);
static void pcf8574_update_client(struct i2c_client *client);
static void pcf8574_init_client(struct i2c_client *client);

/* This is the driver that will be inserted */
static struct i2c_driver pcf8574_driver = {
	.name		= "PCF8574 sensor chip driver",
	.id		= I2C_DRIVERID_PCF8574,
	.flags		= I2C_DF_NOTIFY,
	.attach_adapter	= pcf8574_attach_adapter,
	.detach_client	= pcf8574_detach_client,
};


/* -- SENSORS SYSCTL START -- */
#define PCF8574_SYSCTL_READ     1000
#define PCF8574_SYSCTL_WRITE    1001

/* -- SENSORS SYSCTL END -- */

/* These files are created for each detected PCF8574. This is just a template;
   though at first sight, you might think we could use a statically
   allocated list, we need some way to get back to the parent - which
   is done through one of the 'extra' fields which are initialized
   when a new copy is allocated. */
static ctl_table pcf8574_dir_table_template[] = {
	{PCF8574_SYSCTL_READ, "read", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &pcf8574_read},
	{PCF8574_SYSCTL_WRITE, "write", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &pcf8574_write},
	{0}
};

static int pcf8574_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_detect(adapter, &addr_data, pcf8574_detect);
}

/* This function is called by i2c_detect */
int pcf8574_detect(struct i2c_adapter *adapter, int address,
		   unsigned short flags, int kind)
{
	int i;
	struct i2c_client *new_client;
	struct pcf8574_data *data;
	int err = 0;
	const char *type_name, *client_name;

	/* Make sure we aren't probing the ISA bus!! This is just a safety check
	   at this moment; i2c_detect really won't call us. */
#ifdef DEBUG
	if (i2c_is_isa_adapter(adapter)) {
		printk
		    ("pcf8574.o: pcf8574_detect called for an ISA bus adapter?!?\n");
		return 0;
	}
#endif

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE))
		goto ERROR0;

	/* OK. For now, we presume we have a valid client. We now create the
	   client structure, even though we cannot fill it completely yet. */
	if (!(data = kmalloc(sizeof(struct pcf8574_data), GFP_KERNEL))) {
		err = -ENOMEM;
		goto ERROR0;
	}

	new_client = &data->client;
	new_client->addr = address;
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &pcf8574_driver;
	new_client->flags = 0;

	/* Now, we would do the remaining detection. But the PCF8574 is plainly
	   impossible to detect! Stupid chip. */

	/* Determine the chip type */
	if (kind <= 0) {
		if (address >= 0x38 && address <= 0x3f)
			kind = pcf8574a;
		else
			kind = pcf8574;
	}

	if (kind == pcf8574a) {
		type_name = "pcf8574a";
		client_name = "PCF8574A chip";
	} else {
		type_name = "pcf8574";
		client_name = "PCF8574 chip";
	}

	/* Fill in the remaining client fields and put it into the global list */
	strcpy(new_client->name, client_name);
	init_MUTEX(&data->update_lock);

	/* Tell the I2C layer a new client has arrived */
	if ((err = i2c_attach_client(new_client)))
		goto ERROR1;

	/* Register a new directory entry with module sensors */
	if ((i = i2c_register_entry(new_client, type_name,
				    pcf8574_dir_table_template,
				    THIS_MODULE)) < 0) {
		err = i;
		goto ERROR2;
	}
	data->sysctl_id = i;

	/* Initialize the PCF8574 chip */
	pcf8574_init_client(new_client);
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

static int pcf8574_detach_client(struct i2c_client *client)
{
	int err;

	i2c_deregister_entry(((struct pcf8574_data *) (client->data))->
				 sysctl_id);

	if ((err = i2c_detach_client(client))) {
		printk("pcf8574.o: Client deregistration failed, "
		       "client not detached.\n");
		return err;
	}

	kfree(client->data);

	return 0;
}

/* Called when we have found a new PCF8574. */
static void pcf8574_init_client(struct i2c_client *client)
{
	struct pcf8574_data *data = client->data;
	data->write = PCF8574_INIT;
	i2c_smbus_write_byte(client, data->write);
}


static void pcf8574_update_client(struct i2c_client *client)
{
	struct pcf8574_data *data = client->data;

	down(&data->update_lock);

#ifdef DEBUG
	printk("Starting pcf8574 update\n");
#endif

	data->read = i2c_smbus_read_byte(client); 

	up(&data->update_lock);
}


void pcf8574_read(struct i2c_client *client, int operation,
		  int ctl_name, int *nrels_mag, long *results)
{
	struct pcf8574_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		pcf8574_update_client(client);
		results[0] = data->read;
		*nrels_mag = 1;
	}  
}
void pcf8574_write(struct i2c_client *client, int operation,
		   int ctl_name, int *nrels_mag, long *results)
{
	struct pcf8574_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		results[0] = data->write; 
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->write = results[0];
			i2c_smbus_write_byte(client, data->write);
		}
	}
}


static int __init sm_pcf8574_init(void)
{
	printk("pcf8574.o version %s (%s)\n", LM_VERSION, LM_DATE);
	return i2c_add_driver(&pcf8574_driver);
}

static void __exit sm_pcf8574_exit(void)
{
	i2c_del_driver(&pcf8574_driver);
}


MODULE_AUTHOR("Frodo Looijaard <frodol@dds.nl>, "
	      "Philip Edelbrock <phil@netroedge.com>, "
	      "Dan Eaton <dan.eaton@rocketlogix.com> and "
	      "Aurelien Jarno <aurelien@aurel32.net>");
MODULE_DESCRIPTION("PCF8574 driver");

module_init(sm_pcf8574_init);
module_exit(sm_pcf8574_exit);
