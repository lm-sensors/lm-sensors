/*
    ddcmon.c - Part of lm_sensors, Linux kernel modules for hardware
               monitoring
    Copyright (c) 1998, 1999, 2000  Frodo Looijaard <frodol@dds.nl>,
    Philip Edelbrock <phil@netroedge.com>,
    and Mark Studebaker <mdsxyz123@yahoo.com>
    Copyright (c) 2003  Jean Delvare <khali@linux-fr.org>

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

MODULE_LICENSE("GPL");

/* Addresses to scan */
static unsigned short normal_i2c[] = { 0x50, SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { SENSORS_I2C_END };
static unsigned int normal_isa[] = { SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/* Insmod parameters */
SENSORS_INSMOD_1(ddcmon);

static int checksum = 0;
MODULE_PARM(checksum, "i");
MODULE_PARM_DESC(checksum, "Only accept eeproms whose checksum is correct");

/* Many constants specified below */

/* DDCMON registers */
/* vendor section */
#define DDCMON_REG_MAN_ID 0x08
#define DDCMON_REG_PROD_ID 0x0A
#define DDCMON_REG_SERIAL 0x0C
#define DDCMON_REG_WEEK 0x10
#define DDCMON_REG_YEAR 0x11
/* EDID version */
#define DDCMON_REG_EDID_VER 0x12
#define DDCMON_REG_EDID_REV 0x13
/* display information */
#define DDCMON_REG_HORSIZE 0x15
#define DDCMON_REG_VERSIZE 0x16
#define DDCMON_REG_GAMMA 0x17
#define DDCMON_REG_DPMS_FLAGS 0x18
/* supported timings */
#define DDCMON_REG_ESTABLISHED_TIMINGS 0x23
#define DDCMON_REG_STANDARD_TIMINGS 0x26
#define DDCMON_REG_TIMBASE 0x36
#define DDCMON_REG_TIMINCR 18
#define DDCMON_REG_TIMNUM   4

#define DDCMON_REG_CHECKSUM 0x7f

/* Size of DDCMON in bytes */
#define DDCMON_SIZE 128

/* Each client has this additional data */
struct ddcmon_data {
	struct i2c_client client;
	int sysctl_id;

	struct semaphore update_lock;
	char valid;		/* !=0 if following fields are valid */
	unsigned long last_updated;	/* In jiffies */

	u8 data[DDCMON_SIZE];	/* Register values */
};


static int ddcmon_attach_adapter(struct i2c_adapter *adapter);
static int ddcmon_detect(struct i2c_adapter *adapter, int address,
			 unsigned short flags, int kind);
static int ddcmon_detach_client(struct i2c_client *client);

static void ddcmon_idcall(struct i2c_client *client, int operation,
			    int ctl_name, int *nrels_mag, long *results);
static void ddcmon_size(struct i2c_client *client, int operation,
			    int ctl_name, int *nrels_mag, long *results);
static void ddcmon_sync(struct i2c_client *client, int operation,
			    int ctl_name, int *nrels_mag, long *results);
static void ddcmon_maxclock(struct i2c_client *client, int operation,
			    int ctl_name, int *nrels_mag, long *results);
static void ddcmon_timings(struct i2c_client *client, int operation,
			    int ctl_name, int *nrels_mag, long *results);
static void ddcmon_serial(struct i2c_client *client, int operation,
			    int ctl_name, int *nrels_mag, long *results);
static void ddcmon_time(struct i2c_client *client, int operation,
			    int ctl_name, int *nrels_mag, long *results);
static void ddcmon_edid(struct i2c_client *client, int operation,
			    int ctl_name, int *nrels_mag, long *results);
static void ddcmon_gamma(struct i2c_client *client, int operation,
			    int ctl_name, int *nrels_mag, long *results);
static void ddcmon_dpms(struct i2c_client *client, int operation,
			    int ctl_name, int *nrels_mag, long *results);
static void ddcmon_standard_timing(struct i2c_client *client, int operation,
			    int ctl_name, int *nrels_mag, long *results);
static void ddcmon_update_client(struct i2c_client *client);


/* This is the driver that will be inserted */
static struct i2c_driver ddcmon_driver = {
	.name		= "DDCMON READER",
	.id		= I2C_DRIVERID_DDCMON,
	.flags		= I2C_DF_NOTIFY,
	.attach_adapter	= ddcmon_attach_adapter,
	.detach_client	= ddcmon_detach_client,
};

/* -- SENSORS SYSCTL START -- */

#define DDCMON_SYSCTL_ID 1010
#define DDCMON_SYSCTL_SIZE 1011
#define DDCMON_SYSCTL_SYNC 1012
#define DDCMON_SYSCTL_TIMINGS 1013
#define DDCMON_SYSCTL_SERIAL 1014
#define DDCMON_SYSCTL_TIME 1015
#define DDCMON_SYSCTL_EDID 1016
#define DDCMON_SYSCTL_GAMMA 1017
#define DDCMON_SYSCTL_DPMS 1018
#define DDCMON_SYSCTL_TIMING1 1021
#define DDCMON_SYSCTL_TIMING2 1022
#define DDCMON_SYSCTL_TIMING3 1023
#define DDCMON_SYSCTL_TIMING4 1024
#define DDCMON_SYSCTL_TIMING5 1025
#define DDCMON_SYSCTL_TIMING6 1026
#define DDCMON_SYSCTL_TIMING7 1027
#define DDCMON_SYSCTL_TIMING8 1028
#define DDCMON_SYSCTL_MAXCLOCK 1029

/* -- SENSORS SYSCTL END -- */

/* These files are created for each detected DDCMON. This is just a template;
   though at first sight, you might think we could use a statically
   allocated list, we need some way to get back to the parent - which
   is done through one of the 'extra' fields which are initialized
   when a new copy is allocated. */
static ctl_table ddcmon_dir_table_template[] = {
	{DDCMON_SYSCTL_ID, "id", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &ddcmon_idcall},
	{DDCMON_SYSCTL_SIZE, "size", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &ddcmon_size},
	{DDCMON_SYSCTL_SYNC, "sync", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &ddcmon_sync},
	{DDCMON_SYSCTL_TIMINGS, "timings", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &ddcmon_timings},
	{DDCMON_SYSCTL_SERIAL, "serial", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &ddcmon_serial},
	{DDCMON_SYSCTL_TIME, "time", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &ddcmon_time},
	{DDCMON_SYSCTL_EDID, "edid", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &ddcmon_edid},
	{DDCMON_SYSCTL_GAMMA, "gamma", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &ddcmon_gamma},
	{DDCMON_SYSCTL_DPMS, "dpms", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &ddcmon_dpms},
	{DDCMON_SYSCTL_TIMING1, "timing1", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &ddcmon_standard_timing},
	{DDCMON_SYSCTL_TIMING2, "timing2", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &ddcmon_standard_timing},
	{DDCMON_SYSCTL_TIMING3, "timing3", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &ddcmon_standard_timing},
	{DDCMON_SYSCTL_TIMING4, "timing4", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &ddcmon_standard_timing},
	{DDCMON_SYSCTL_TIMING5, "timing5", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &ddcmon_standard_timing},
	{DDCMON_SYSCTL_TIMING6, "timing6", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &ddcmon_standard_timing},
	{DDCMON_SYSCTL_TIMING7, "timing7", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &ddcmon_standard_timing},
	{DDCMON_SYSCTL_TIMING8, "timing8", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &ddcmon_standard_timing},
	{DDCMON_SYSCTL_MAXCLOCK, "maxclock", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &ddcmon_maxclock},
	{0}
};

static int ddcmon_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_detect(adapter, &addr_data, ddcmon_detect);
}

/* This function is called by i2c_detect */
int ddcmon_detect(struct i2c_adapter *adapter, int address,
		  unsigned short flags, int kind)
{
	int i, cs;
	struct i2c_client *new_client;
	struct ddcmon_data *data;
	int err = 0;
	const char *type_name, *client_name;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		    goto ERROR0;

	/* OK. For now, we presume we have a valid client. We now create the
	   client structure, even though we cannot fill it completely yet.
	   But it allows us to access ddcmon_{read,write}_value. */
	if (!(data = kmalloc(sizeof(struct ddcmon_data), GFP_KERNEL))) {
		err = -ENOMEM;
		goto ERROR0;
	}

	new_client = &data->client;
	memset(data->data, 0xff, DDCMON_SIZE);
	new_client->addr = address;
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &ddcmon_driver;
	new_client->flags = 0;

	/* prevent 24RF08 corruption (just in case) */
	i2c_smbus_write_quick(new_client, 0);

	/* Now, we do the remaining detection. */
	if (checksum) {
		int cs = 0;
		for (i = 0; i < 0x80; i++)
			cs += i2c_smbus_read_byte_data(new_client, i);
		if ((cs & 0xff) != 0)
			goto ERROR1;
	}

	/* Verify the first 8 locations 0x00FFFFFFFFFFFF00 */
	/* Allow force and force_ddcmon arguments */
	if(kind < 0)
	{
		for(i = 0; i < 8; i++) {
			cs = i2c_smbus_read_byte_data(new_client, i);
			if(i == 0 || i == 7) {
				if(cs != 0)
					goto ERROR1;
			} else if(cs != 0xff)
				goto ERROR1;
		}
	}

	type_name = "ddcmon";
	client_name = "DDC Monitor";

	/* Fill in the remaining client fields and put it in the global list */
	strcpy(new_client->name, client_name);
	data->valid = 0;
	init_MUTEX(&data->update_lock);

	/* Tell the I2C layer a new client has arrived */
	if ((err = i2c_attach_client(new_client)))
		goto ERROR3;

	/* Register a new directory entry with module sensors */
	if ((i = i2c_register_entry(new_client, type_name,
					ddcmon_dir_table_template,
					THIS_MODULE)) < 0) {
		err = i;
		goto ERROR4;
	}
	data->sysctl_id = i;

	return 0;

      ERROR4:
	i2c_detach_client(new_client);
      ERROR3:
      ERROR1:
	kfree(data);
      ERROR0:
	return err;
}

static int ddcmon_detach_client(struct i2c_client *client)
{
	int err;

	i2c_deregister_entry(((struct ddcmon_data *) (client->data))->
				 sysctl_id);
	if ((err = i2c_detach_client(client))) {
		printk
		    ("ddcmon.o: Client deregistration failed, client not detached.\n");
		return err;
	}
	kfree(client->data);
	return 0;
}

static void ddcmon_update_client(struct i2c_client *client)
{
	struct ddcmon_data *data = client->data;
	int i, j;

	down(&data->update_lock);

	if ((jiffies - data->last_updated > 300 * HZ) ||
	    (jiffies < data->last_updated) || !data->valid) {
		if (i2c_check_functionality(client->adapter,
		                            I2C_FUNC_SMBUS_READ_I2C_BLOCK))
		{
			for (i=0; i<DDCMON_SIZE; i+=I2C_SMBUS_I2C_BLOCK_MAX)
				if (i2c_smbus_read_i2c_block_data(client,
				                           i, data->data + i)
				                    != I2C_SMBUS_I2C_BLOCK_MAX) {
					printk(KERN_WARNING "ddcmon.o: block read fail at 0x%.2x!\n", i);
					goto DONE;
				}
		} else {
			if (i2c_smbus_write_byte(client, 0)) {
				printk(KERN_WARNING "ddcmon.o: read start fail at 0!\n");
				goto DONE;
			}
			for (i = 0; i < DDCMON_SIZE; i++) {
				j = i2c_smbus_read_byte(client);
				if (j < 0) {
					printk(KERN_WARNING "eeprom.o: read fail at 0x%.2x!\n", i);
					goto DONE;
				}
				data->data[i] = (u8) j;
			}
		}
		data->last_updated = jiffies;
		data->valid = 1;
	}
DONE:
	up(&data->update_lock);
}


void ddcmon_idcall(struct i2c_client *client, int operation,
		   int ctl_name, int *nrels_mag, long *results)
{
	struct ddcmon_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		ddcmon_update_client(client);
		results[0] = data->data[DDCMON_REG_MAN_ID + 1] |
		             (data->data[DDCMON_REG_MAN_ID] << 8);
		results[1] = data->data[DDCMON_REG_PROD_ID + 1] |
		             (data->data[DDCMON_REG_PROD_ID] << 8);
		*nrels_mag = 2;
	}
}

void ddcmon_size(struct i2c_client *client, int operation,
		 int ctl_name, int *nrels_mag, long *results)
{
	struct ddcmon_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		ddcmon_update_client(client);
		results[0] = data->data[DDCMON_REG_VERSIZE];
		results[1] = data->data[DDCMON_REG_HORSIZE];
		*nrels_mag = 2;
	}
}

void ddcmon_sync(struct i2c_client *client, int operation,
		    int ctl_name, int *nrels_mag, long *results)
{
	int i, j;
	struct ddcmon_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		ddcmon_update_client(client);
		*nrels_mag = 4;
		/* look for monitor limits entry */
		for(i = DDCMON_REG_TIMBASE;
		    i < DDCMON_REG_TIMBASE +
		        (DDCMON_REG_TIMNUM * DDCMON_REG_TIMINCR);
		    i += DDCMON_REG_TIMINCR) {
			if (data->data[i] == 0x00
			 && data->data[i + 1] == 0x00
			 && data->data[i + 2] == 0x00
			 && data->data[i + 3] == 0xfd) {
				for(j = 0; j < 4; j++)
					results[j] = data->data[i + j + 5];
				return;
			}
		}
		for(j = 0; j < 4; j++)
			results[j] = 0;
	}
}

void ddcmon_maxclock(struct i2c_client *client, int operation,
		    int ctl_name, int *nrels_mag, long *results)
{
	int i;
	struct ddcmon_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		ddcmon_update_client(client);
		*nrels_mag = 1;
		/* look for monitor limits entry */
		for(i = DDCMON_REG_TIMBASE;
		    i < DDCMON_REG_TIMBASE +
		        (DDCMON_REG_TIMNUM * DDCMON_REG_TIMINCR);
		    i += DDCMON_REG_TIMINCR) {
			if (data->data[i] == 0x00
			 && data->data[i + 1] == 0x00
			 && data->data[i + 2] == 0x00
			 && data->data[i + 3] == 0xfd) {
				results[0] = (data->data[i + 9] == 0xff ?
				             0 : data->data[i + 9] * 10);
				return;
			}
		}
		results[0] = 0;
	}
}

void ddcmon_timings(struct i2c_client *client, int operation,
		    int ctl_name, int *nrels_mag, long *results)
{
	struct ddcmon_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		ddcmon_update_client(client);
		results[0] = data->data[DDCMON_REG_ESTABLISHED_TIMINGS] |
		             (data->data[DDCMON_REG_ESTABLISHED_TIMINGS + 1] << 8) |
		             (data->data[DDCMON_REG_ESTABLISHED_TIMINGS + 2] << 16);
		*nrels_mag = 1;
	}
}

void ddcmon_serial(struct i2c_client *client, int operation,
		    int ctl_name, int *nrels_mag, long *results)
{
	struct ddcmon_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		ddcmon_update_client(client);
		results[0] = data->data[DDCMON_REG_SERIAL] |
		             (data->data[DDCMON_REG_SERIAL + 1] << 8) |
		             (data->data[DDCMON_REG_SERIAL + 2] << 16) |
		             (data->data[DDCMON_REG_SERIAL + 3] << 24);
		*nrels_mag = 1;
	}
}

void ddcmon_time(struct i2c_client *client, int operation,
		 int ctl_name, int *nrels_mag, long *results)
{
	struct ddcmon_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		ddcmon_update_client(client);
		results[0] = data->data[DDCMON_REG_YEAR] + 1990;
		results[1] = data->data[DDCMON_REG_WEEK];
		*nrels_mag = 2;
	}
}

void ddcmon_edid(struct i2c_client *client, int operation,
		 int ctl_name, int *nrels_mag, long *results)
{
	struct ddcmon_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		ddcmon_update_client(client);
		results[0] = data->data[DDCMON_REG_EDID_VER];
		results[1] = data->data[DDCMON_REG_EDID_REV];
		*nrels_mag = 2;
	}
}

void ddcmon_gamma(struct i2c_client *client, int operation,
		 int ctl_name, int *nrels_mag, long *results)
{
	struct ddcmon_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 2;
	else if (operation == SENSORS_PROC_REAL_READ) {
		ddcmon_update_client(client);
		results[0] = 100 + data->data[DDCMON_REG_GAMMA];
		*nrels_mag = 1;
	}
}

void ddcmon_dpms(struct i2c_client *client, int operation,
		 int ctl_name, int *nrels_mag, long *results)
{
	struct ddcmon_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		ddcmon_update_client(client);
		results[0] = data->data[DDCMON_REG_DPMS_FLAGS];
		*nrels_mag = 1;
	}
}

void ddcmon_standard_timing(struct i2c_client *client, int operation,
		 int ctl_name, int *nrels_mag, long *results)
{
	struct ddcmon_data *data = client->data;
	int nr = ctl_name - DDCMON_SYSCTL_TIMING1;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		ddcmon_update_client(client);
		/* If both bytes of the timing are 0x00 or 0x01, then the timing
		   slot is unused. */
		if ((data->data[DDCMON_REG_STANDARD_TIMINGS + nr * 2]
		    | data->data[DDCMON_REG_STANDARD_TIMINGS + nr * 2 + 1]) & 0xfe) {
			results[0] = (data->data[DDCMON_REG_STANDARD_TIMINGS + nr * 2] + 31) * 8;
			switch (data->data[DDCMON_REG_STANDARD_TIMINGS + nr * 2 + 1] >> 6) {
				/* We don't care about rounding issues there, it really
				   should be OK without it. */
				case 0x00:
					results[1] = results[0]; /* unconfirmed */
					break;
				case 0x01:
					results[1] = results[0] * 3 / 4;
					break;
				case 0x02:
					results[1] = results[0] * 4 / 5;
					break;
				case 0x03:
					results[1] = results[0] * 9 / 16;
					break;
			}
			results[2] = (data->data[DDCMON_REG_STANDARD_TIMINGS + nr * 2 + 1] & 0x3f) + 60;
		} else {
			results[0] = 0;
			results[1] = 0;
			results[2] = 0;
		}
		*nrels_mag = 3;
	}
}

static int __init sm_ddcmon_init(void)
{
	printk("ddcmon.o version %s (%s)\n", LM_VERSION, LM_DATE);
	return i2c_add_driver(&ddcmon_driver);
}

static void __exit sm_ddcmon_exit(void)
{
	i2c_del_driver(&ddcmon_driver);
}



MODULE_AUTHOR("Frodo Looijaard <frodol@dds.nl>, "
	      "Philip Edelbrock <phil@netroedge.com>, "
	      "Mark Studebaker <mdsxyz123@yahoo.com> "
		  "and Jean Delvare <khali@linux-fr.org>");
MODULE_DESCRIPTION("DDCMON driver");

module_init(sm_ddcmon_init);
module_exit(sm_ddcmon_exit);
