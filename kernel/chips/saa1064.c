/*
    saa1064.c - Part of lm_sensors, Linux kernel modules for hardware
             monitoring
    Copyright (c) 2003  Sascha Volkenandt <sascha@akv-soft.de>

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

/* A few notes about the SAA1064:

* The SAA1064 is a driver for 4-digit led displays produced by Philips
  Semiconductors. It can be found in HiFi equipment with such displays.
  I've found it inside a Kathrein analogue sat-receiver :-).

* The SAA1064 is quite simple to handle, it receives an address byte,
  telling which register is addressed, followed by up to five data
  bytes (according to the data sheet), while with each byte the register 
  address is increased by one, wrapping from 7 to 0.

* NOTE: This device doesn NOT have eight read-write registers, but eight
  write-only registers, and one read-only status byte (with no register 
  address).

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
static unsigned short normal_i2c_range[] = { 0x38, 0x3b, SENSORS_I2C_END };
static unsigned int normal_isa[] = { SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/* Insmod parameters */
SENSORS_INSMOD_1(saa1064);

/* The SAA1064 registers
  Registers:
  0 Control register
  1 Digit 1
  2 Digit 2
  3 Digit 3
  4 Digit 4
*/

#define SAA1064_REG_CONTROL 0x00
#define SAA1064_REG_DIGIT0 0x01
#define SAA1064_REG_DIGIT1 0x02
#define SAA1064_REG_DIGIT2 0x03
#define SAA1064_REG_DIGIT3 0x04

/* Control byte:
   Bit  0  Dynamic mode 
        1  Blank digits 1+3 (reversed)
        2  Blank digits 2+4 (reversed)
        3  Testmode (all digits on)
        4  Add 3mA output current to led's
        5  Add 6mA output current to led's
        6  Add 12mA output current to led's
        7  Unused, set to 0
*/

#define SAA1064_CTRL_DYNAMIC 0x01
#define SAA1064_CTRL_BLANK13 0x02
#define SAA1064_CTRL_BLANK24 0x04
#define SAA1064_CTRL_TEST    0x08
#define SAA1064_CTRL_BRIGHT  0x70

/* Status byte:
   The MSB set to 1 indicates powerloss since last read-out */
#define SAA1064_STAT_PWRLOSS 0x80

/* Get a bit from the control-byte. Except for BRIGHT, these return 
   zero/non-zero in a boolean fashion. BRIGHT returns a 3-bit value
   specifying the brightness */
#define SAA1064_CTRL_GET_DYNAMIC(c) (((c) & SAA1064_CTRL_DYNAMIC)>0)
#define SAA1064_CTRL_GET_BLANK13(c) (((c) & SAA1064_CTRL_BLANK13)>0)
#define SAA1064_CTRL_GET_BLANK24(c) (((c) & SAA1064_CTRL_BLANK24)>0)
#define SAA1064_CTRL_GET_TEST(c)    (((c) & SAA1064_CTRL_TEST)>0)
#define SAA1064_CTRL_GET_BRIGHT(c)  (((c) & SAA1064_CTRL_BRIGHT)>>4)

/* Get THE bit from the status byte. The LSB set to 1 indicates powerloss
   since last read-out */
#define SAA1064_STAT_GET_PWRLOSS(s)  (((s) & SAA1064_STAT_PWRLOSS)>0)

/* Set a bit in the control-byte to val and return the modified byte. 
   Except for BRIGHT, val is evaluated in a boolean fashion. */
#define SAA1064_CTRL_SET_DYNAMIC(c,val) ((val)>0 \
	? (c) | SAA1064_CTRL_DYNAMIC : (c) & ~SAA1064_CTRL_DYNAMIC)
#define SAA1064_CTRL_SET_BLANK13(c,val) ((val)>0 \
	? (c) | SAA1064_CTRL_BLANK13 : (c) & ~SAA1064_CTRL_BLANK13)
#define SAA1064_CTRL_SET_BLANK24(c,val) ((val)>0 \
	? (c) | SAA1064_CTRL_BLANK24 : (c) & ~SAA1064_CTRL_BLANK24)
#define SAA1064_CTRL_SET_TEST(c,val) ((val)>0 \
	? (c) | SAA1064_CTRL_TEST : (c) & ~SAA1064_CTRL_TEST)
#define SAA1064_CTRL_SET_BRIGHT(c,val) (((c) & ~SAA1064_CTRL_BRIGHT) \
	| (((val) << 4) & SAA1064_CTRL_BRIGHT))

/* Initial values */
#define SAA1064_INIT 0x7f	/* All digits on (testmode), full brightness */

/* Each client has this additional data */
struct saa1064_data {
	struct i2c_client client;
	int sysctl_id;

	struct semaphore update_lock;
	char valid;   /* !=0 if following fields are valid */
	unsigned long last_updated; /* In jiffies */

	u8 control;		/* Control reg */
	u8 digits[4];	/* Digits regs */
	u8 status;    /* Status byte (read) */
};

static int saa1064_attach_adapter(struct i2c_adapter *adapter);
static int saa1064_detect(struct i2c_adapter *adapter, int address,
			  unsigned short flags, int kind);
static int saa1064_detach_client(struct i2c_client *client);

static void saa1064_bright(struct i2c_client *client, int operation,
			               int ctl_name, int *nrels_mag, long *results);
static void saa1064_test(struct i2c_client *client, int operation,
			               int ctl_name, int *nrels_mag, long *results);
static void saa1064_disp(struct i2c_client *client, int operation,
			               int ctl_name, int *nrels_mag, long *results);
static void saa1064_refresh(struct i2c_client *client, int operation,
			               int ctl_name, int *nrels_mag, long *results);
static void saa1064_update_client(struct i2c_client *client);
static void saa1064_init_client(struct i2c_client *client);

/* This is the driver that will be inserted */
static struct i2c_driver saa1064_driver = {
	.name           = "SAA1064 sensor chip driver",
	.id             = I2C_DRIVERID_SAA1064,
	.flags          = I2C_DF_NOTIFY,
	.attach_adapter = saa1064_attach_adapter,
	.detach_client  = saa1064_detach_client,
};

/* -- SENSORS SYSCTL START -- */

#define SAA1064_SYSCTL_BRIGHT     1000 /* Brightness, 0-7 */
#define SAA1064_SYSCTL_TEST       1001 /* Testmode (on = all digits lit) */
#define SAA1064_SYSCTL_DISP       1005 /* four eight bit values */
#define SAA1064_SYSCTL_REFRESH    1006 /* refresh digits in case of powerloss */

/* -- SENSORS SYSCTL END -- */

/* These files are created for each detected SAA1064. This is just a template;
   though at first sight, you might think we could use a statically
   allocated list, we need some way to get back to the parent - which
   is done through one of the 'extra' fields which are initialized
   when a new copy is allocated. */
static ctl_table saa1064_dir_table_template[] = {
	{SAA1064_SYSCTL_BRIGHT, "bright", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &saa1064_bright},
	{SAA1064_SYSCTL_TEST, "test", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &saa1064_test},
	{SAA1064_SYSCTL_DISP, "disp", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &saa1064_disp},
	{SAA1064_SYSCTL_REFRESH, "refresh", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &saa1064_refresh},
	{0}
};

static int saa1064_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_detect(adapter, &addr_data, saa1064_detect);
}

/* This function is called by i2c_detect */
static int saa1064_detect(struct i2c_adapter *adapter, int address,
		   unsigned short flags, int kind)
{
	int i;
	struct i2c_client *new_client;
	struct saa1064_data *data;
	int err = 0;
	const char *type_name, *client_name;

	/* Make sure we aren't probing the ISA bus!! This is just a safety check
	   at this moment; i2c_detect really won't call us. */
#ifdef DEBUG
	if (i2c_is_isa_adapter(adapter)) {
		printk
		    ("saa1064.o: saa1064_detect called for an ISA bus adapter?!?\n");
		return 0;
	}
#endif

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_READ_BYTE 
			| I2C_FUNC_SMBUS_WRITE_BYTE_DATA | I2C_FUNC_SMBUS_WRITE_I2C_BLOCK))
		goto ERROR0;

	/* OK. For now, we presume we have a valid client. We now create the
	   client structure, even though we cannot fill it completely yet.
	   But it allows us to access i2c_smbus_read_byte */
	if (!(data = kmalloc(sizeof(struct saa1064_data), GFP_KERNEL))) {
		err = -ENOMEM;
		goto ERROR0;
	}

	new_client = &data->client;
	new_client->addr = address;
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &saa1064_driver;
	new_client->flags = 0;

	if (kind < 0) {
		if ((i2c_smbus_read_byte(new_client) & ~SAA1064_STAT_PWRLOSS) != 0x00
				|| i2c_smbus_read_byte(new_client) != 0x00
				|| i2c_smbus_read_byte(new_client) != 0x00
				|| i2c_smbus_read_byte(new_client) != 0x00)
			goto ERROR1;
	}

	/* If detection was requested, it has also passed til now. Only one chip, 
	   so no further evaluation of kind. */
	kind = saa1064;
	type_name = "saa1064";
	client_name = "SAA1064 chip";

	/* Fill in the remaining client fields and put it into the global list */
	strcpy(new_client->name, client_name);
	data->valid = 0;
	init_MUTEX(&data->update_lock);

	/* Tell the I2C layer a new client has arrived */
	if ((err = i2c_attach_client(new_client)))
		goto ERROR3;

	/* Register a new directory entry with module sensors */
	if ((i = i2c_register_entry(new_client, type_name,
					saa1064_dir_table_template,
					THIS_MODULE)) < 0) {
		err = i;
		goto ERROR4;
	}
	data->sysctl_id = i;

#ifdef DEBUG
	printk("saa1064.o: Module initialization complete.\n");
#endif

	/* Initialize the SAA1064 chip */
	saa1064_init_client(new_client);
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


int saa1064_detach_client(struct i2c_client *client)
{
	int err;

	i2c_deregister_entry(((struct saa1064_data *) (client->data))->
				 sysctl_id);

	if ((err = i2c_detach_client(client))) {
		printk
		    ("saa1064.o: Client deregistration failed, client not detached.\n");
		return err;
	}

	kfree(client->data);

	return 0;
}

/* Called when we have found a new SAA1064. */
void saa1064_init_client(struct i2c_client *client)
{
	struct saa1064_data *data = client->data;
	data->control = SAA1064_INIT;
	memset(data->digits, 0x00, 4);

	i2c_smbus_write_byte_data(client, SAA1064_REG_CONTROL, data->control);
	i2c_smbus_write_i2c_block_data(client, SAA1064_REG_DIGIT0, 4, data->digits);
}

/* Update status byte, and write registers in case a powerloss occurred */
static void saa1064_update_client(struct i2c_client *client)
{
	struct saa1064_data *data = client->data;

	down(&data->update_lock);

	if ((jiffies - data->last_updated > 5*HZ) ||
	    (jiffies < data->last_updated) || !data->valid) {

#ifdef DEBUG
		printk("saa1064.o: starting saa1064 update\n");
#endif

		data->status = i2c_smbus_read_byte(client);
		if (SAA1064_STAT_GET_PWRLOSS(data->status)) {
			i2c_smbus_write_byte_data(client, SAA1064_REG_CONTROL, data->control);
			i2c_smbus_write_i2c_block_data(client, SAA1064_REG_DIGIT0, 4, data->digits);
		}
		
		data->last_updated = jiffies;
		data->valid = 1;
	}

	up(&data->update_lock);
}


static void saa1064_bright(struct i2c_client *client, int operation,
		    int ctl_name, int *nrels_mag, long *results)
{
	struct saa1064_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		results[0] = SAA1064_CTRL_GET_BRIGHT(data->control);
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->control = SAA1064_CTRL_SET_BRIGHT(data->control, results[0]);
			i2c_smbus_write_byte_data(client, SAA1064_REG_CONTROL, data->control);
		}
	}
}


static void saa1064_test(struct i2c_client *client, int operation,
		    int ctl_name, int *nrels_mag, long *results)
{
	struct saa1064_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		results[0] = SAA1064_CTRL_GET_TEST(data->control);
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->control = SAA1064_CTRL_SET_TEST(data->control, results[0]);
			i2c_smbus_write_byte_data(client, SAA1064_REG_CONTROL, data->control);
		}
	}
}


static void saa1064_disp(struct i2c_client *client, int operation,
		    int ctl_name, int *nrels_mag, long *results)
{
	struct saa1064_data *data = client->data;
	int i;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		for (i = 0; i < 4; ++i)
			results[i] = (long)data->digits[i];
		*nrels_mag = 4;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		for (i = 0; i < max(*nrels_mag, 4); ++i)
			data->digits[i] = (u8)results[i];
		i2c_smbus_write_i2c_block_data(client, SAA1064_REG_DIGIT0, 4, data->digits);
	}
}


static void saa1064_refresh(struct i2c_client *client, int operation,
		    int ctl_name, int *nrels_mag, long *results)
{
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		saa1064_update_client(client);
		*nrels_mag = 0;
	}  
}


static int __init sensors_saa1064_init(void)
{
	printk("saa1064.o version %s (%s)\n", LM_VERSION, LM_DATE);
	return i2c_add_driver(&saa1064_driver);
}


static void __exit sensors_saa1064_exit(void)
{
	i2c_del_driver(&saa1064_driver);
}



MODULE_AUTHOR("Sascha Volkenandt <sascha@akv-soft.de>");
MODULE_DESCRIPTION("SAA1064 driver");

module_init(sensors_saa1064_init);
module_exit(sensors_saa1064_exit);

