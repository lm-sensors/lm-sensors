/*
    eeprom.c - Part of lm_sensors, Linux kernel modules for hardware
               monitoring
    Copyright (c) 1998, 1999  Frodo Looijaard <frodol@dds.nl> and
    Philip Edelbrock <phil@netroedge.com>

    2003-08-18  Jean Delvare <khali@linux-fr.org>
    Divide the eeprom in 2-row (arbitrary) slices. This significantly
    speeds sensors up, as well as various scripts using the eeprom
    module.

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
#include <linux/sched.h> /* for capable() */
#include "version.h"

MODULE_LICENSE("GPL");

/* Addresses to scan */
static unsigned short normal_i2c[] = { SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { 0x50, 0x57, SENSORS_I2C_END };
static unsigned int normal_isa[] = { SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/* Insmod parameters */
SENSORS_INSMOD_1(eeprom);


/* possible natures */
#define NATURE_UNKNOWN 0
#define NATURE_VAIO 1

/* Size of EEPROM in bytes */
#define EEPROM_SIZE 256

/* Each client has this additional data */
struct eeprom_data {
	struct i2c_client client;
	int sysctl_id;

	struct semaphore update_lock;
	u8 valid;		/* bitfield, bit!=0 if slice is valid */
	unsigned long last_updated[8];	/* In jiffies, 8 slices */

	u8 data[EEPROM_SIZE];	/* Register values */
	u8 nature;
};


static int eeprom_attach_adapter(struct i2c_adapter *adapter);
static int eeprom_detect(struct i2c_adapter *adapter, int address,
			 unsigned short flags, int kind);
static int eeprom_detach_client(struct i2c_client *client);

#if 0
static int eeprom_write_value(struct i2c_client *client, u8 reg,
			      u8 value);
#endif

static void eeprom_contents(struct i2c_client *client, int operation,
			    int ctl_name, int *nrels_mag, long *results);
static void eeprom_update_client(struct i2c_client *client, u8 slice);


/* This is the driver that will be inserted */
static struct i2c_driver eeprom_driver = {
	.name		= "EEPROM READER",
	.id		= I2C_DRIVERID_EEPROM,
	.flags		= I2C_DF_NOTIFY,
	.attach_adapter	= eeprom_attach_adapter,
	.detach_client	= eeprom_detach_client,
};

/* -- SENSORS SYSCTL START -- */

#define EEPROM_SYSCTL1 1000
#define EEPROM_SYSCTL2 1001
#define EEPROM_SYSCTL3 1002
#define EEPROM_SYSCTL4 1003
#define EEPROM_SYSCTL5 1004
#define EEPROM_SYSCTL6 1005
#define EEPROM_SYSCTL7 1006
#define EEPROM_SYSCTL8 1007
#define EEPROM_SYSCTL9 1008
#define EEPROM_SYSCTL10 1009
#define EEPROM_SYSCTL11 1010
#define EEPROM_SYSCTL12 1011
#define EEPROM_SYSCTL13 1012
#define EEPROM_SYSCTL14 1013
#define EEPROM_SYSCTL15 1014
#define EEPROM_SYSCTL16 1015

/* -- SENSORS SYSCTL END -- */

/* These files are created for each detected EEPROM. This is just a template;
   though at first sight, you might think we could use a statically
   allocated list, we need some way to get back to the parent - which
   is done through one of the 'extra' fields which are initialized
   when a new copy is allocated. */
static ctl_table eeprom_dir_table_template[] = {
	{EEPROM_SYSCTL1, "00", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &eeprom_contents},
	{EEPROM_SYSCTL2, "10", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &eeprom_contents},
	{EEPROM_SYSCTL3, "20", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &eeprom_contents},
	{EEPROM_SYSCTL4, "30", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &eeprom_contents},
	{EEPROM_SYSCTL5, "40", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &eeprom_contents},
	{EEPROM_SYSCTL6, "50", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &eeprom_contents},
	{EEPROM_SYSCTL7, "60", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &eeprom_contents},
	{EEPROM_SYSCTL8, "70", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &eeprom_contents},
	{EEPROM_SYSCTL9, "80", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &eeprom_contents},
	{EEPROM_SYSCTL10, "90", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &eeprom_contents},
	{EEPROM_SYSCTL11, "a0", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &eeprom_contents},
	{EEPROM_SYSCTL12, "b0", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &eeprom_contents},
	{EEPROM_SYSCTL13, "c0", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &eeprom_contents},
	{EEPROM_SYSCTL14, "d0", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &eeprom_contents},
	{EEPROM_SYSCTL15, "e0", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &eeprom_contents},
	{EEPROM_SYSCTL16, "f0", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &eeprom_contents},
	{0}
};

static int eeprom_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_detect(adapter, &addr_data, eeprom_detect);
}

/* This function is called by i2c_detect */
int eeprom_detect(struct i2c_adapter *adapter, int address,
		  unsigned short flags, int kind)
{
	int i;
	struct i2c_client *new_client;
	struct eeprom_data *data;
	int err = 0;
	const char *type_name;

	/* There are three ways we can read the EEPROM data:
	   (1) I2C block reads (faster, but unsupported by most adapters)
	   (2) Consecutive byte reads (100% overhead)
	   (3) Regular byte data reads (200% overhead)
	   The third method is not implemented by this driver because all
	   known adapters support at least the second. */
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_READ_BYTE_DATA
					    | I2C_FUNC_SMBUS_BYTE))
		goto ERROR0;

	/* OK. For now, we presume we have a valid client. We now create the
	   client structure, even though we cannot fill it completely yet.
	   But it allows us to access eeprom_{read,write}_value. */
	if (!(data = kmalloc(sizeof(struct eeprom_data), GFP_KERNEL))) {
		err = -ENOMEM;
		goto ERROR0;
	}

	new_client = &data->client;
	memset(data->data, 0xff, EEPROM_SIZE);
	new_client->addr = address;
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &eeprom_driver;
	new_client->flags = 0;

	/* prevent 24RF08 corruption */
	i2c_smbus_write_quick(new_client, 0);

	type_name = "eeprom";

	/* Fill in the remaining client fields and put it into the global list */
	strcpy(new_client->name, "EEPROM chip");
	data->valid = 0;
	init_MUTEX(&data->update_lock);
	data->nature = NATURE_UNKNOWN;

	/* Tell the I2C layer a new client has arrived */
	if ((err = i2c_attach_client(new_client)))
		goto ERROR3;

	/* Detect the Vaio nature of EEPROMs.
	   We use the "PCG-" prefix as the signature. */
	if (address == 0x57) {
		if (i2c_smbus_read_byte_data(new_client, 0x80) == 'P'
		 && i2c_smbus_read_byte(new_client) == 'C'
		 && i2c_smbus_read_byte(new_client) == 'G'
		 && i2c_smbus_read_byte(new_client) == '-') {
			printk(KERN_INFO "Vaio EEPROM detected, "
			       "enabling password protection\n");
			data->nature = NATURE_VAIO;
		}
	}

	/* Register a new directory entry with module sensors */
	if ((i = i2c_register_entry(new_client, type_name,
					eeprom_dir_table_template,
					THIS_MODULE)) < 0) {
		err = i;
		goto ERROR4;
	}
	data->sysctl_id = i;

	return 0;

      ERROR4:
	i2c_detach_client(new_client);
      ERROR3:
	kfree(data);
      ERROR0:
	return err;
}

static int eeprom_detach_client(struct i2c_client *client)
{
	int err;

	i2c_deregister_entry(((struct eeprom_data *) (client->data))->
				 sysctl_id);

	if ((err = i2c_detach_client(client))) {
		printk
		    ("eeprom.o: Client deregistration failed, client not detached.\n");
		return err;
	}

	kfree(client->data);

	return 0;
}


#if 0
/* No writes yet (PAE) */
static int eeprom_write_value(struct i2c_client *client, u8 reg, u8 value)
{
	return i2c_smbus_write_byte_data(client, reg, value);
}
#endif

static void eeprom_update_client(struct i2c_client *client, u8 slice)
{
	struct eeprom_data *data = client->data;
	int i, j;

	down(&data->update_lock);

	if (!(data->valid & (1 << slice))
	 || (jiffies - data->last_updated[slice] > 300 * HZ)
	 || (jiffies < data->last_updated[slice])) {

#ifdef DEBUG
		printk(KERN_DEBUG "eeprom.o: Starting update, slice %u\n", slice);
#endif

		if (i2c_check_functionality(client->adapter,
		                            I2C_FUNC_SMBUS_READ_I2C_BLOCK))
		{
			for (i = slice << 5; i < (slice + 1) << 5;
			                            i += I2C_SMBUS_I2C_BLOCK_MAX)
				if (i2c_smbus_read_i2c_block_data(client,
				                           i, data->data + i)
				                    != I2C_SMBUS_I2C_BLOCK_MAX) {
					printk(KERN_WARNING "eeprom.o: block read fail at 0x%.2x!\n", i);
					goto DONE;
				}
		} else {
			if (i2c_smbus_write_byte(client, slice << 5)) {
				printk(KERN_WARNING "eeprom.o: read start fail at 0x%.2x!\n", slice << 5);
				goto DONE;
			}
			for (i = slice << 5; i < (slice + 1) << 5; i++) {
				j = i2c_smbus_read_byte(client);
				if (j < 0) {
					printk(KERN_WARNING "eeprom.o: read fail at 0x%.2x!\n", i);
					goto DONE;
				}
				data->data[i] = (u8) j;
			}
		}
		data->last_updated[slice] = jiffies;
		data->valid |= (1 << slice);
	}
DONE:
	up(&data->update_lock);
}


void eeprom_contents(struct i2c_client *client, int operation,
		     int ctl_name, int *nrels_mag, long *results)
{
	int i;
	int nr = ctl_name - EEPROM_SYSCTL1;
	struct eeprom_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		eeprom_update_client(client, nr >> 1);
		/* Hide Vaio security settings to regular users */
		if (nr == 0 && data->nature == NATURE_VAIO
		 && !capable(CAP_SYS_ADMIN))
			for (i = 0; i < 16; i++)
				results[i] = 0;
		else
			for (i = 0; i < 16; i++)
				results[i] = data->data[i + nr * 16];
#ifdef DEBUG
		printk(KERN_DEBUG "eeprom.o: 0x%X EEPROM contents (row %d):",
		       client->addr, nr + 1);
		if (nr == 0 && data->nature == NATURE_VAIO)
		 	printk(" <hidden for security reasons>\n");
		else {
			for (i = 0; i < 16; i++)
				printk(" 0x%02X", data->data[i + nr * 16]);
			printk("\n");
		}
#endif
		*nrels_mag = 16;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {

/* No writes to the EEPROM (yet, anyway) (PAE) */
		printk("eeprom.o: No writes to EEPROMs supported!\n");
	}
}

static int __init sm_eeprom_init(void)
{
	printk("eeprom.o version %s (%s)\n", LM_VERSION, LM_DATE);
	return i2c_add_driver(&eeprom_driver);
}

static void __exit sm_eeprom_exit(void)
{
	i2c_del_driver(&eeprom_driver);
}



MODULE_AUTHOR
    ("Frodo Looijaard <frodol@dds.nl> and Philip Edelbrock <phil@netroedge.com>");
MODULE_DESCRIPTION("EEPROM driver");

module_init(sm_eeprom_init);
module_exit(sm_eeprom_exit);
