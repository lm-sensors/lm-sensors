/*
    ds1621.c - Part of lm_sensors, Linux kernel modules for hardware
             monitoring
    Christian W. Zuckschwerdt  <zany@triq.net>  2000-11-23
    based on lm75.c by Frodo Looijaard <frodol@dds.nl>

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

/* Supports DS1621. See doc/chips/ds1621 for details */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/i2c-proc.h>
#include <linux/init.h>
#include "version.h"

MODULE_LICENSE("GPL");

/* Addresses to scan */
static unsigned short normal_i2c[] = { SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { 0x48, 0x4f, SENSORS_I2C_END };
static unsigned int normal_isa[] = { SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/* Insmod parameters */
SENSORS_INSMOD_1(ds1621);

/* Many DS1621 constants specified below */

/* Config register used for detection         */
/*  7    6    5    4    3    2    1    0      */
/* |Done|THF |TLF |NVB | X  | X  |POL |1SHOT| */
#define DS1621_REG_CONFIG_NVB 0x10
#define DS1621_REG_CONFIG_POLARITY 0x02
#define DS1621_REG_CONFIG_1SHOT 0x01
#define DS1621_REG_CONFIG_DONE 0x80

/* Note: the done bit is always unset if continuous conversion is in progress.
         We need to stop the continuous conversion or switch to single shot
         before this bit becomes available!
 */

/* The DS1621 registers */
#define DS1621_REG_TEMP 0xAA /* word, RO */
#define DS1621_REG_TEMP_OVER 0xA1 /* word, RW */
#define DS1621_REG_TEMP_HYST 0xA2 /* word, RW -- it's a low temp trigger */
#define DS1621_REG_CONF 0xAC /* byte, RW */
#define DS1621_REG_TEMP_COUNTER 0xA8 /* byte, RO */
#define DS1621_REG_TEMP_SLOPE 0xA9 /* byte, RO */
#define DS1621_COM_START 0xEE /* no data */
#define DS1621_COM_STOP 0x22 /* no data */

/* Conversions. Rounding and limit checking is only done on the TO_REG
   variants. Note that you should be a bit careful with which arguments
   these macros are called: arguments may be evaluated more than once.
   Fixing this is just not worth it. */
#define TEMP_FROM_REG(val) ((((val & 0x7fff) >> 7) * 5) | \
                            ((val & 0x8000)?-256:0))
#define TEMP_TO_REG(val)   (SENSORS_LIMIT((val<0 ? (0x200+((val)/5))<<7 : \
                                          (((val) + 2) / 5) << 7),0,0xffff))
#define ALARMS_FROM_REG(val) ((val) & \
                              (DS1621_ALARM_TEMP_HIGH | DS1621_ALARM_TEMP_LOW))
#define ITEMP_FROM_REG(val) ((((val & 0x7fff) >> 8)) | \
                            ((val & 0x8000)?-256:0))

/* Each client has this additional data */
struct ds1621_data {
	struct i2c_client client;
	int sysctl_id;

	struct semaphore update_lock;
	char valid;		/* !=0 if following fields are valid */
	unsigned long last_updated;	/* In jiffies */

	u16 temp, temp_over, temp_hyst;	/* Register values, word */
	u8 conf;			/* Register encoding, combined */

	char enable;	/* !=0 if we're expected to restart the conversion */
	u8 temp_int, temp_counter, temp_slope;	/* Register values, byte */
};

static int ds1621_attach_adapter(struct i2c_adapter *adapter);
static int ds1621_detect(struct i2c_adapter *adapter, int address,
			 unsigned short flags, int kind);
static void ds1621_init_client(struct i2c_client *client);
static int ds1621_detach_client(struct i2c_client *client);

static int ds1621_read_value(struct i2c_client *client, u8 reg);
static int ds1621_write_value(struct i2c_client *client, u8 reg, u16 value);
static void ds1621_temp(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void ds1621_alarms(struct i2c_client *client, int operation,
			  int ctl_name, int *nrels_mag, long *results);
static void ds1621_enable(struct i2c_client *client, int operation,
			  int ctl_name, int *nrels_mag, long *results);
static void ds1621_continuous(struct i2c_client *client, int operation,
			      int ctl_name, int *nrels_mag, long *results);
static void ds1621_polarity(struct i2c_client *client, int operation,
			    int ctl_name, int *nrels_mag, long *results);
static void ds1621_update_client(struct i2c_client *client);


/* This is the driver that will be inserted */
static struct i2c_driver ds1621_driver = {
	.name		= "DS1621 sensor driver",
	.id		= I2C_DRIVERID_DS1621,
	.flags		= I2C_DF_NOTIFY,
	.attach_adapter	= ds1621_attach_adapter,
	.detach_client	= ds1621_detach_client,
};

/* -- SENSORS SYSCTL START -- */
#define DS1621_SYSCTL_TEMP 1200	/* Degrees Celcius * 10 */
#define DS1621_SYSCTL_ALARMS 2001	/* bitvector */
#define DS1621_ALARM_TEMP_HIGH 0x40
#define DS1621_ALARM_TEMP_LOW 0x20
#define DS1621_SYSCTL_ENABLE 2002
#define DS1621_SYSCTL_CONTINUOUS 2003
#define DS1621_SYSCTL_POLARITY 2004

/* -- SENSORS SYSCTL END -- */

/* These files are created for each detected DS1621. This is just a template;
   though at first sight, you might think we could use a statically
   allocated list, we need some way to get back to the parent - which
   is done through one of the 'extra' fields which are initialized
   when a new copy is allocated. */
static ctl_table ds1621_dir_table_template[] = {
	{DS1621_SYSCTL_TEMP, "temp", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &ds1621_temp},
	{DS1621_SYSCTL_ALARMS, "alarms", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &ds1621_alarms},
	{DS1621_SYSCTL_ENABLE, "enable", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &ds1621_enable},
	{DS1621_SYSCTL_CONTINUOUS, "continuous", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &ds1621_continuous},
	{DS1621_SYSCTL_POLARITY, "polarity", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &ds1621_polarity},
	{0}
};

static int ds1621_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_detect(adapter, &addr_data, ds1621_detect);
}

/* This function is called by i2c_detect */
int ds1621_detect(struct i2c_adapter *adapter, int address,
		unsigned short flags, int kind)
{
	int i, conf, temp;
	struct i2c_client *new_client;
	struct ds1621_data *data;
	int err = 0;
	const char *type_name, *client_name;

	/* Make sure we aren't probing the ISA bus!! This is just a safety check
	   at this moment; i2c_detect really won't call us. */
#ifdef DEBUG
	if (i2c_is_isa_adapter(adapter)) {
		printk
		 ("ds1621.o: ds1621_detect called for an ISA bus adapter?!?\n");
		return 0;
	}
#endif

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA |
				     I2C_FUNC_SMBUS_WORD_DATA |
				     I2C_FUNC_SMBUS_WRITE_BYTE))
		    goto ERROR0;

	/* OK. For now, we presume we have a valid client. We now create the
	   client structure, even though we cannot fill it completely yet.
	   But it allows us to access ds1621_{read,write}_value. */
	if (!(data = kmalloc(sizeof(struct ds1621_data), GFP_KERNEL))) {
		err = -ENOMEM;
		goto ERROR0;
	}

	new_client = &data->client;
	new_client->addr = address;
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &ds1621_driver;
	new_client->flags = 0;

	/* Now, we do the remaining detection. It is lousy. */
	if (kind < 0) {
		/* The NVB bit should be low if no EEPROM write has been
		   requested during the latest 10ms, which is highly
		   improbable in our case. */
		conf = i2c_smbus_read_byte_data(new_client,
						DS1621_REG_CONF);
		if (conf & DS1621_REG_CONFIG_NVB)
			goto ERROR1;
		/* The 7 lowest bits of a temperature should always be 0. */
		temp = ds1621_read_value(new_client, 
					 DS1621_REG_TEMP);
		if (temp & 0x007f)
			goto ERROR1;
		temp = ds1621_read_value(new_client, 
					 DS1621_REG_TEMP_HYST);
		if (temp & 0x007f)
			goto ERROR1;
		temp = ds1621_read_value(new_client, 
					 DS1621_REG_TEMP_OVER);
		if (temp & 0x007f)
			goto ERROR1;
	}

	/* Determine the chip type - only one kind supported! */
	if (kind <= 0)
		kind = ds1621;

	if (kind == ds1621) {
		type_name = "ds1621";
		client_name = "DS1621 chip";
	} else {
#ifdef DEBUG
		printk("ds1621.o: Internal error: unknown kind (%d)?!?",
		       kind);
#endif
		goto ERROR1;
	}

	/* Fill in remaining client fields and put it into the global list */
	strcpy(new_client->name, client_name);
	data->valid = 0;
	init_MUTEX(&data->update_lock);

	/* Tell the I2C layer a new client has arrived */
	if ((err = i2c_attach_client(new_client)))
		goto ERROR3;

	/* Register a new directory entry with module sensors */
	if ((i = i2c_register_entry(new_client, type_name,
					ds1621_dir_table_template,
					THIS_MODULE)) < 0) {
		err = i;
		goto ERROR4;
	}
	data->sysctl_id = i;

	ds1621_init_client(new_client);
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

static int ds1621_detach_client(struct i2c_client *client)
{
	int err;

	i2c_deregister_entry(((struct ds1621_data *) (client->data))->
				 sysctl_id);

	if ((err = i2c_detach_client(client))) {
		printk
	   ("ds1621.o: Client deregistration failed, client not detached.\n");
		return err;
	}

	kfree(client->data);

	return 0;
}


/* All registers are word-sized, except for the configuration register.
   DS1621 uses a high-byte first convention, which is exactly opposite to
   the usual practice. */
static int ds1621_read_value(struct i2c_client *client, u8 reg)
{
	if ((reg == DS1621_REG_CONF) || (reg == DS1621_REG_TEMP_COUNTER)
	    || (reg == DS1621_REG_TEMP_SLOPE))
		return i2c_smbus_read_byte_data(client, reg);
	else
		return swab16(i2c_smbus_read_word_data(client, reg));
}

/* All registers are word-sized, except for the configuration register.
   DS1621 uses a high-byte first convention, which is exactly opposite to
   the usual practice. */
static int ds1621_write_value(struct i2c_client *client, u8 reg, u16 value)
{
	if ( (reg == DS1621_COM_START) || (reg == DS1621_COM_STOP) )
		return i2c_smbus_write_byte(client, reg);
	else
	if ((reg == DS1621_REG_CONF) || (reg == DS1621_REG_TEMP_COUNTER)
	    || (reg == DS1621_REG_TEMP_SLOPE))
		return i2c_smbus_write_byte_data(client, reg, value);
	else
		return i2c_smbus_write_word_data(client, reg, swab16(value));
}

static void ds1621_init_client(struct i2c_client *client)
{
	int reg;

	reg = ds1621_read_value(client, DS1621_REG_CONF);
	/* start the continous conversion */
	if(reg & 0x01)
		ds1621_write_value(client, DS1621_REG_CONF, reg & 0xfe);
}

static void ds1621_update_client(struct i2c_client *client)
{
	struct ds1621_data *data = client->data;
	u8 new_conf;

	down(&data->update_lock);

	if ((jiffies - data->last_updated > HZ + HZ / 2) ||
	    (jiffies < data->last_updated) || !data->valid) {

#ifdef DEBUG
		printk("Starting ds1621 update\n");
#endif

		data->conf = ds1621_read_value(client, DS1621_REG_CONF);

		data->temp = ds1621_read_value(client,
					       DS1621_REG_TEMP);
		data->temp_over = ds1621_read_value(client,
		                                    DS1621_REG_TEMP_OVER);
		data->temp_hyst = ds1621_read_value(client,
						    DS1621_REG_TEMP_HYST);

		/* wait for the DONE bit before reading extended values */

		if (data->conf & DS1621_REG_CONFIG_DONE) {
			data->temp_counter = ds1621_read_value(client,
						     DS1621_REG_TEMP_COUNTER);
			data->temp_slope = ds1621_read_value(client,
						     DS1621_REG_TEMP_SLOPE);
			data->temp_int = ITEMP_FROM_REG(data->temp);
			/* restart the conversion */
			if (data->enable)
				ds1621_write_value(client, DS1621_COM_START, 0);
		}

		/* reset alarms if neccessary */
		new_conf = data->conf;
		if (data->temp < data->temp_over)
			new_conf &= ~DS1621_ALARM_TEMP_HIGH;
		if (data->temp > data->temp_hyst)
			new_conf &= ~DS1621_ALARM_TEMP_LOW;
		if (data->conf != new_conf)
			ds1621_write_value(client, DS1621_REG_CONF,
					   new_conf);

		data->last_updated = jiffies;
		data->valid = 1;
	}

	up(&data->update_lock);
}


void ds1621_temp(struct i2c_client *client, int operation, int ctl_name,
		 int *nrels_mag, long *results)
{
	struct ds1621_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		if (!(data->conf & DS1621_REG_CONFIG_DONE) ||
		    (data->temp_counter > data->temp_slope) ||
		    (data->temp_slope == 0)) {
			*nrels_mag = 1;
		} else {
			*nrels_mag = 2;
		}
	else if (operation == SENSORS_PROC_REAL_READ) {
		ds1621_update_client(client);
		/* decide wether to calculate more precise temp */
		if (!(data->conf & DS1621_REG_CONFIG_DONE) ||
		    (data->temp_counter > data->temp_slope) ||
		    (data->temp_slope == 0)) {
			results[0] = TEMP_FROM_REG(data->temp_over);
			results[1] = TEMP_FROM_REG(data->temp_hyst);
			results[2] = TEMP_FROM_REG(data->temp);
		} else {
			results[0] = TEMP_FROM_REG(data->temp_over)*10;
			results[1] = TEMP_FROM_REG(data->temp_hyst)*10;
			results[2] = data->temp_int * 100 - 25 +
				((data->temp_slope - data->temp_counter) *
				 100 / data->temp_slope);
		}
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->temp_over = TEMP_TO_REG(results[0]);
			ds1621_write_value(client, DS1621_REG_TEMP_OVER,
					 data->temp_over);
		}
		if (*nrels_mag >= 2) {
			data->temp_hyst = TEMP_TO_REG(results[1]);
			ds1621_write_value(client, DS1621_REG_TEMP_HYST,
					 data->temp_hyst);
		}
	}
}

void ds1621_alarms(struct i2c_client *client, int operation, int ctl_name,
		   int *nrels_mag, long *results)
{
	struct ds1621_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		ds1621_update_client(client);
		results[0] = ALARMS_FROM_REG(data->conf);
		*nrels_mag = 1;
	}
}

void ds1621_enable(struct i2c_client *client, int operation, int ctl_name,
		   int *nrels_mag, long *results)
{
	/* If you really screw up your chip (like I did) this is */
	/* sometimes needed to (re)start the continous conversion */
	/* there is no data to read so this might hang your SMBus! */

	struct ds1621_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		ds1621_update_client(client);
		results[0] = !(data->conf & DS1621_REG_CONFIG_DONE);
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			if (results[0]) {
				ds1621_write_value(client, DS1621_COM_START, 0);
				data->enable=1;
			} else {
				ds1621_write_value(client, DS1621_COM_STOP, 0);
				data->enable=0;
			}
		} else {
			ds1621_write_value(client, DS1621_COM_START, 0);
			data->enable=1;
		}
	}
}

void ds1621_continuous(struct i2c_client *client, int operation, int ctl_name,
		       int *nrels_mag, long *results)
{
	struct ds1621_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		ds1621_update_client(client);
		results[0] = !(data->conf & DS1621_REG_CONFIG_1SHOT);
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		ds1621_update_client(client);
		if (*nrels_mag >= 1) {
			if (results[0]) {
				ds1621_write_value(client, DS1621_REG_CONF,
						   data->conf & ~DS1621_REG_CONFIG_1SHOT);
			} else {
				ds1621_write_value(client, DS1621_REG_CONF,
						   data->conf | DS1621_REG_CONFIG_1SHOT);
			}
		} else {
			ds1621_write_value(client, DS1621_REG_CONF,
					   data->conf & ~DS1621_REG_CONFIG_1SHOT);
		}
	}
}

void ds1621_polarity(struct i2c_client *client, int operation, int ctl_name,
		     int *nrels_mag, long *results)
{
	struct ds1621_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		ds1621_update_client(client);
		results[0] = !(!(data->conf & DS1621_REG_CONFIG_POLARITY));
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		ds1621_update_client(client);
		if (*nrels_mag >= 1) {
			if (results[0]) {
				ds1621_write_value(client, DS1621_REG_CONF,
						   data->conf | DS1621_REG_CONFIG_POLARITY);
			} else {
				ds1621_write_value(client, DS1621_REG_CONF,
						   data->conf & ~DS1621_REG_CONFIG_POLARITY);
			}
		}
	}
}

static int __init sm_ds1621_init(void)
{
	printk("ds1621.o version %s (%s)\n", LM_VERSION, LM_DATE);
	return i2c_add_driver(&ds1621_driver);
}

static void __exit sm_ds1621_exit(void)
{
	i2c_del_driver(&ds1621_driver);
}



MODULE_AUTHOR("Christian W. Zuckschwerdt <zany@triq.net>");
MODULE_DESCRIPTION("DS1621 driver");

module_init(sm_ds1621_init);
module_exit(sm_ds1621_exit);
