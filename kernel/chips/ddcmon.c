/*
    ddcmon.c - Part of lm_sensors, Linux kernel modules for hardware
               monitoring
    Copyright (c) 1998, 1999, 2000  Frodo Looijaard <frodol@dds.nl>,
    Philip Edelbrock <phil@netroedge.com>,
    and Mark Studebaker <mdsxyz123@yahoo.com>

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
static unsigned short normal_i2c[] = { 0x50, SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { SENSORS_I2C_END };
static unsigned int normal_isa[] = { SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/* Insmod parameters */
SENSORS_INSMOD_1(ddcmon);

/* Many constants specified below */

/* DDCMON registers */
#define DDCMON_REG_ID 0x08
#define DDCMON_REG_SERIAL 0x0C
#define DDCMON_REG_HORSIZE 0x15
#define DDCMON_REG_VERSIZE 0x16
#define DDCMON_REG_TIMINGS 0x23
#define DDCMON_REG_TIMBASE 0x36
#define DDCMON_REG_TIMINCR 18
#define DDCMON_REG_TIMNUM   4
#define DDCMON_REG_TIMOFFSET 5
#define DDCMON_REG_CHECKSUM 0x7f

/* Size of DDCMON in bytes */
#define DDCMON_SIZE 128

/* Each client has this additional data */
struct ddcmon_data {
	int sysctl_id;

	struct semaphore update_lock;
	char valid;		/* !=0 if following fields are valid */
	unsigned long last_updated;	/* In jiffies */

	u8 data[DDCMON_SIZE];	/* Register values */
	int memtype;
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
int __init sensors_ddcmon_init(void);
static int __init ddcmon_cleanup(void);

static int ddcmon_attach_adapter(struct i2c_adapter *adapter);
static int ddcmon_detect(struct i2c_adapter *adapter, int address,
			 unsigned short flags, int kind);
static int ddcmon_detach_client(struct i2c_client *client);
static int ddcmon_command(struct i2c_client *client, unsigned int cmd,
			  void *arg);

static void ddcmon_inc_use(struct i2c_client *client);
static void ddcmon_dec_use(struct i2c_client *client);

static void ddcmon_idcall(struct i2c_client *client, int operation,
			    int ctl_name, int *nrels_mag, long *results);
static void ddcmon_size(struct i2c_client *client, int operation,
			    int ctl_name, int *nrels_mag, long *results);
static void ddcmon_sync(struct i2c_client *client, int operation,
			    int ctl_name, int *nrels_mag, long *results);
static void ddcmon_timings(struct i2c_client *client, int operation,
			    int ctl_name, int *nrels_mag, long *results);
static void ddcmon_serial(struct i2c_client *client, int operation,
			    int ctl_name, int *nrels_mag, long *results);
static void ddcmon_update_client(struct i2c_client *client);


/* This is the driver that will be inserted */
static struct i2c_driver ddcmon_driver = {
	/* name */ "DDCMON READER",
	/* id */ I2C_DRIVERID_DDCMON,
	/* flags */ I2C_DF_NOTIFY,
	/* attach_adapter */ &ddcmon_attach_adapter,
	/* detach_client */ &ddcmon_detach_client,
	/* command */ &ddcmon_command,
	/* inc_use */ &ddcmon_inc_use,
	/* dec_use */ &ddcmon_dec_use
};

/* These files are created for each detected DDCMON. This is just a template;
   though at first sight, you might think we could use a statically
   allocated list, we need some way to get back to the parent - which
   is done through one of the 'extra' fields which are initialized
   when a new copy is allocated. */
static ctl_table ddcmon_dir_table_template[] = {
	{DDCMON_SYSCTL_ID, "ID", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &ddcmon_idcall},
	{DDCMON_SYSCTL_SIZE, "size", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &ddcmon_size},
	{DDCMON_SYSCTL_SYNC, "sync", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &ddcmon_sync},
	{DDCMON_SYSCTL_TIMINGS, "timings", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &ddcmon_timings},
	{DDCMON_SYSCTL_SERIAL, "serial", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &ddcmon_serial},
	{0}
};

/* Used by init/cleanup */
static int __initdata ddcmon_initialized = 0;

static int ddcmon_id = 0;

int ddcmon_attach_adapter(struct i2c_adapter *adapter)
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
	if (!(new_client = kmalloc(sizeof(struct i2c_client) +
				   sizeof(struct ddcmon_data),
				   GFP_KERNEL))) {
		err = -ENOMEM;
		goto ERROR0;
	}

	data = (struct ddcmon_data *) (new_client + 1);
	new_client->addr = address;
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &ddcmon_driver;
	new_client->flags = 0;

	/* Now, we do the remaining detection. */
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

	new_client->id = ddcmon_id++;
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
	kfree(new_client);
      ERROR0:
	return err;
}

int ddcmon_detach_client(struct i2c_client *client)
{
	int err;

	i2c_deregister_entry(((struct ddcmon_data *) (client->data))->
				 sysctl_id);
	if ((err = i2c_detach_client(client))) {
		printk
		    ("ddcmon.o: Client deregistration failed, client not detached.\n");
		return err;
	}
	kfree(client);
	return 0;
}

/* No commands defined yet */
int ddcmon_command(struct i2c_client *client, unsigned int cmd, void *arg)
{
	return 0;
}

void ddcmon_inc_use(struct i2c_client *client)
{
#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif
}

void ddcmon_dec_use(struct i2c_client *client)
{
#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
}

void ddcmon_update_client(struct i2c_client *client)
{
	struct ddcmon_data *data = client->data;
	int i;

	down(&data->update_lock);

	if ((jiffies - data->last_updated > 300 * HZ) ||
	    (jiffies < data->last_updated) || !data->valid) {
		if (i2c_smbus_write_byte(client, 0)) {
#ifdef DEBUG
			printk("ddcmon read start has failed!\n");
#endif
		}
		for (i = 0; i < DDCMON_SIZE; i++) {
			data->data[i] = (u8) i2c_smbus_read_byte(client);
		}

		data->last_updated = jiffies;
		data->valid = 1;
	}

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
		results[0] = data->data[DDCMON_REG_ID + 1] |
		             (data->data[DDCMON_REG_ID] << 8) |
		             (data->data[DDCMON_REG_ID + 3] << 16) |
		             (data->data[DDCMON_REG_ID + 2] << 24);
		*nrels_mag = 1;
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
		/* look for sync entry */
		for(i = DDCMON_REG_TIMBASE;
		    i < DDCMON_REG_TIMBASE +
		        (DDCMON_REG_TIMNUM * DDCMON_REG_TIMINCR);
		    i += DDCMON_REG_TIMINCR) {
			if(data->data[i] == 0) {
				for(j = 0; j < 4; j++)
					results[j] = data->data[i + j +
					                DDCMON_REG_TIMOFFSET];
				return;
			}
		}
		for(j = 0; j < 4; j++)
			results[j] = 0;
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
		results[0] = data->data[DDCMON_REG_TIMINGS] |
		             (data->data[DDCMON_REG_TIMINGS + 1] << 8) |
		             (data->data[DDCMON_REG_TIMINGS + 2] << 16);
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

int __init sensors_ddcmon_init(void)
{
	int res;

	printk("ddcmon.o version %s (%s)\n", LM_VERSION, LM_DATE);
	ddcmon_initialized = 0;
	if ((res = i2c_add_driver(&ddcmon_driver))) {
		printk
		    ("ddcmon.o: Driver registration failed, module not inserted.\n");
		ddcmon_cleanup();
		return res;
	}
	ddcmon_initialized++;
	return 0;
}

int __init ddcmon_cleanup(void)
{
	int res;

	if (ddcmon_initialized >= 1) {
		if ((res = i2c_del_driver(&ddcmon_driver))) {
			printk
			    ("ddcmon.o: Driver deregistration failed, module not removed.\n");
			return res;
		}
	} else
		ddcmon_initialized--;
	return 0;
}

EXPORT_NO_SYMBOLS;

#ifdef MODULE

MODULE_AUTHOR("Frodo Looijaard <frodol@dds.nl>, "
	      "Philip Edelbrock <phil@netroedge .com>, "
	      "and Mark Studebaker <mdsxyz123@yahoo.com>");
MODULE_DESCRIPTION("DDCMON driver");

int init_module(void)
{
	return sensors_ddcmon_init();
}

int cleanup_module(void)
{
	return ddcmon_cleanup();
}

#endif				/* MODULE */
