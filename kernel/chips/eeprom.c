/*
    eeprom.c - Part of lm_sensors, Linux kernel modules for hardware
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

#include <linux/module.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include "sensors.h"
#include "version.h"
#include <linux/init.h>

#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,2,18)) || \
    (LINUX_VERSION_CODE == KERNEL_VERSION(2,3,0))
#define init_MUTEX(s) do { *(s) = MUTEX; } while(0)
#endif

#ifndef THIS_MODULE
#define THIS_MODULE NULL
#endif

/* Addresses to scan */
static unsigned short normal_i2c[] = { SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { 0x50, 0x57, SENSORS_I2C_END };
static unsigned int normal_isa[] = { SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/* Insmod parameters */
SENSORS_INSMOD_1(eeprom);

static int checksum = 0;
MODULE_PARM(checksum, "i");
MODULE_PARM_DESC(checksum,
		 "Only accept eeproms whose checksum is correct");


/* Many constants specified below */

/* EEPROM registers */
#define EEPROM_REG_CHECKSUM 0x3f

/* EEPROM memory types: */
#define ONE_K		1
#define TWO_K		2
#define FOUR_K		3
#define EIGHT_K		4
#define SIXTEEN_K	5

/* Conversions */
/* Size of EEPROM in bytes */
#define EEPROM_SIZE 256

/* Each client has this additional data */
struct eeprom_data {
	int sysctl_id;

	struct semaphore update_lock;
	char valid;		/* !=0 if following fields are valid */
	unsigned long last_updated;	/* In jiffies */

	u8 data[EEPROM_SIZE];	/* Register values */
#if 0
	int memtype;
#endif
};

#ifdef MODULE
extern int init_module(void);
extern int cleanup_module(void);
#endif				/* MODULE */

#ifdef MODULE
static
#else
extern
#endif
int __init sensors_eeprom_init(void);
static int __init eeprom_cleanup(void);

static int eeprom_attach_adapter(struct i2c_adapter *adapter);
static int eeprom_detect(struct i2c_adapter *adapter, int address,
			 unsigned short flags, int kind);
static int eeprom_detach_client(struct i2c_client *client);
static int eeprom_command(struct i2c_client *client, unsigned int cmd,
			  void *arg);

static void eeprom_inc_use(struct i2c_client *client);
static void eeprom_dec_use(struct i2c_client *client);

#if 0
static int eeprom_write_value(struct i2c_client *client, u8 reg,
			      u8 value);
#endif

static void eeprom_contents(struct i2c_client *client, int operation,
			    int ctl_name, int *nrels_mag, long *results);
static void eeprom_update_client(struct i2c_client *client);


/* This is the driver that will be inserted */
static struct i2c_driver eeprom_driver = {
	/* name */ "EEPROM READER",
	/* id */ I2C_DRIVERID_EEPROM,
	/* flags */ I2C_DF_NOTIFY,
	/* attach_adapter */ &eeprom_attach_adapter,
	/* detach_client */ &eeprom_detach_client,
	/* command */ &eeprom_command,
	/* inc_use */ &eeprom_inc_use,
	/* dec_use */ &eeprom_dec_use
};

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

/* Used by init/cleanup */
static int __initdata eeprom_initialized = 0;

static int eeprom_id = 0;

int eeprom_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_detect(adapter, &addr_data, eeprom_detect);
}

/* This function is called by i2c_detect */
int eeprom_detect(struct i2c_adapter *adapter, int address,
		  unsigned short flags, int kind)
{
	int i, cs;
	struct i2c_client *new_client;
	struct eeprom_data *data;
	int err = 0;
	const char *type_name, *client_name;

	/* Make sure we aren't probing the ISA bus!! This is just a safety check
	   at this moment; i2c_detect really won't call us. */
#ifdef DEBUG
	if (i2c_is_isa_adapter(adapter)) {
		printk
		    ("eeprom.o: eeprom_detect called for an ISA bus adapter?!?\n");
		return 0;
	}
#endif

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		    goto ERROR0;

	/* OK. For now, we presume we have a valid client. We now create the
	   client structure, even though we cannot fill it completely yet.
	   But it allows us to access eeprom_{read,write}_value. */
	if (!(new_client = kmalloc(sizeof(struct i2c_client) +
				   sizeof(struct eeprom_data),
				   GFP_KERNEL))) {
		err = -ENOMEM;
		goto ERROR0;
	}

	data = (struct eeprom_data *) (new_client + 1);
	new_client->addr = address;
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &eeprom_driver;
	new_client->flags = 0;

	/* Now, we do the remaining detection. It is not there, unless you force
	   the checksum to work out. */
	if (checksum) {
		cs = 0;
		for (i = 0; i <= 0x3e; i++)
			cs += i2c_smbus_read_byte_data(new_client, i);
		cs &= 0xff;
		if (i2c_smbus_read_byte_data
		    (new_client, EEPROM_REG_CHECKSUM) != cs)
			goto ERROR1;
	}

	/* Determine the chip type - only one kind supported! */
	if (kind <= 0)
		kind = eeprom;

	if (kind == eeprom) {
		type_name = "eeprom";
		client_name = "EEPROM chip";
	} else {
#ifdef DEBUG
		printk("eeprom.o: Internal error: unknown kind (%d)?!?",
		       kind);
#endif
		goto ERROR1;
	}

	/* Fill in the remaining client fields and put it into the global list */
	strcpy(new_client->name, client_name);

	new_client->id = eeprom_id++;
	data->valid = 0;
	init_MUTEX(&data->update_lock);

	/* Tell the I2C layer a new client has arrived */
	if ((err = i2c_attach_client(new_client)))
		goto ERROR3;

	/* Register a new directory entry with module sensors */
	if ((i = i2c_register_entry(new_client, type_name,
					eeprom_dir_table_template,
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
	kfree(new_client);
      ERROR0:
	return err;
}

int eeprom_detach_client(struct i2c_client *client)
{
	int err;

	i2c_deregister_entry(((struct eeprom_data *) (client->data))->
				 sysctl_id);

	if ((err = i2c_detach_client(client))) {
		printk
		    ("eeprom.o: Client deregistration failed, client not detached.\n");
		return err;
	}

	kfree(client);

	return 0;
}


/* No commands defined yet */
int eeprom_command(struct i2c_client *client, unsigned int cmd, void *arg)
{
	return 0;
}

void eeprom_inc_use(struct i2c_client *client)
{
#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif
}

void eeprom_dec_use(struct i2c_client *client)
{
#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
}

#if 0
/* No writes yet (PAE) */
int eeprom_write_value(struct i2c_client *client, u8 reg, u8 value)
{
	return i2c_smbus_write_byte_data(client, reg, value);
}
#endif

void eeprom_update_client(struct i2c_client *client)
{
	struct eeprom_data *data = client->data;
	int i;

	down(&data->update_lock);

	if ((jiffies - data->last_updated > 300 * HZ) |
	    (jiffies < data->last_updated) || !data->valid) {

#ifdef DEBUG
		printk("Starting eeprom update\n");
#endif

		if (i2c_smbus_write_byte(client, 0)) {
#ifdef DEBUG
			printk("eeprom read start has failed!\n");
#endif
		}
		for (i = 0; i < EEPROM_SIZE; i++) {
			data->data[i] = (u8) i2c_smbus_read_byte(client);
		}

		data->last_updated = jiffies;
		data->valid = 1;
	}

	up(&data->update_lock);
}


void eeprom_contents(struct i2c_client *client, int operation,
		     int ctl_name, int *nrels_mag, long *results)
{
	int i;
	int base = 0;
	struct eeprom_data *data = client->data;

	switch (ctl_name) {
		case EEPROM_SYSCTL2:
			base = 16;
			break;
		case EEPROM_SYSCTL3:
			base = 32;
			break;
		case EEPROM_SYSCTL4:
			base = 48;
			break;
		case EEPROM_SYSCTL5:
			base = 64;
			break;
		case EEPROM_SYSCTL6:
			base = 80;
			break;
		case EEPROM_SYSCTL7:
			base = 96;
			break;
		case EEPROM_SYSCTL8:
			base = 112;
			break;
		case EEPROM_SYSCTL9:
			base = 128;
			break;
		case EEPROM_SYSCTL10:
			base = 144;
			break;
		case EEPROM_SYSCTL11:
			base = 160;
			break;
		case EEPROM_SYSCTL12:
			base = 176;
			break;
		case EEPROM_SYSCTL13:
			base = 192;
			break;
		case EEPROM_SYSCTL14:
			base = 208;
			break;
		case EEPROM_SYSCTL15:
			base = 224;
			break;
		case EEPROM_SYSCTL16:
			base = 240;
			break;
	}

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		eeprom_update_client(client);
		for (i = 0; i < 16; i++) {
			results[i] = data->data[i + base];
		}
#ifdef DEBUG
		printk("eeprom.o: 0x%X EEPROM Contents (base %d): ",
		       client->addr, base);
		for (i = 0; i < 16; i++) {
			printk(" 0x%X", data->data[i + base]);
		}
		printk(" .\n");
#endif
		*nrels_mag = 16;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {

/* No writes to the EEPROM (yet, anyway) (PAE) */
		printk("eeprom.o: No writes to EEPROMs supported!\n");
	}
}

int __init sensors_eeprom_init(void)
{
	int res;

	printk("eeprom.o version %s (%s)\n", LM_VERSION, LM_DATE);
	eeprom_initialized = 0;
	if ((res = i2c_add_driver(&eeprom_driver))) {
		printk
		    ("eeprom.o: Driver registration failed, module not inserted.\n");
		eeprom_cleanup();
		return res;
	}
	eeprom_initialized++;
	return 0;
}

int __init eeprom_cleanup(void)
{
	int res;

	if (eeprom_initialized >= 1) {
		if ((res = i2c_del_driver(&eeprom_driver))) {
			printk
			    ("eeprom.o: Driver deregistration failed, module not removed.\n");
			return res;
		}
	} else
		eeprom_initialized--;

	return 0;
}

EXPORT_NO_SYMBOLS;

#ifdef MODULE

MODULE_AUTHOR
    ("Frodo Looijaard <frodol@dds.nl> and Philip Edelbrock <phil@netroedge.com>");
MODULE_DESCRIPTION("EEPROM driver");

int init_module(void)
{
	return sensors_eeprom_init();
}

int cleanup_module(void)
{
	return eeprom_cleanup();
}

#endif				/* MODULE */
