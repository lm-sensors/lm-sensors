/*
    mic74.c - Intended to become part of lm_sensors, Linux 
    kernel modules for hardware monitoring

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

/* Supports Micrel MIC74 */

/* 
	A couple notes about the MIC74:
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
static unsigned short normal_i2c_range[] = { 0x20, 0x27, SENSORS_I2C_END };
static unsigned int normal_isa[] = { SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/* Insmod parameters */
SENSORS_INSMOD_1(mic74);

/* The mic74 registers */

#define MIC74_REG_CONFIG	0x00
#define MIC74_REG_DATA_DIR	0x01
#define MIC74_REG_OUT_CFG	0x02
#define MIC74_REG_STATUS	0x03
#define MIC74_REG_INT_MASK	0x04
#define MIC74_REG_DATA		0x05
#define MIC74_REG_FAN_SPEED	0x06

/* Initial values */
/* All registers are 0, except for MIC74_REG_DATA which is 0xFF */

/* For each registered MIC74, we need to keep some data in memory. It is
   dynamically allocated, at the same time when a new mic74 client is
   allocated. */
struct mic74_data {
	int sysctl_id;
	enum chips type;

	struct semaphore update_lock;
	char valid;		/* !=0 if following fields are valid */
	unsigned long last_updated;	/* In jiffies */

	u8 config;		/* Register value */
	u8 dataDir;		/* Register value */
	u8 outCfg;		/* Register value */
	u8 status;		/* Register value */
	u8 intMask;		/* Register value */
	u8 data;		/* Register value */
	u8 fanSpeed;		/* Register value */
};

static int mic74_attach_adapter(struct i2c_adapter *adapter);
static int mic74_detect(struct i2c_adapter *adapter, int address,
			  unsigned short flags, int kind);
static int mic74_detach_client(struct i2c_client *client);

static int mic74_read_value(struct i2c_client *client, u8 reg);
static int mic74_write_value(struct i2c_client *client, u8 reg,
			       u8 value);
static void mic74_update_client(struct i2c_client *client);
static void mic74_init_client(struct i2c_client *client);

static void mic74_reg_rw(struct i2c_client *client, int operation, int ctl_name,
			 int *nrels_mag, long *results);
static void mic74_reg_ro(struct i2c_client *client, int operation, int ctl_name,
			 int *nrels_mag, long *results);

/* I choose here for semi-static MIC74 allocation. Complete dynamic
   allocation could also be used; the code needed for this would probably
   take more memory than the datastructure takes now. */
static int mic74_id;

static struct i2c_driver mic74_driver = {
	.name	= "MIC74 sensor driver",
	.flags	= I2C_DF_NOTIFY,
	.attach_adapter	= mic74_attach_adapter,
	.detach_client	= mic74_detach_client
};

/* -- SENSORS SYSCTL START -- */
#define MIC74_SYSCTL_REG0     1000
#define MIC74_SYSCTL_REG1     1001
#define MIC74_SYSCTL_REG2     1002
#define MIC74_SYSCTL_REG3     1003
#define MIC74_SYSCTL_REG4     1004
#define MIC74_SYSCTL_REG5     1005
#define MIC74_SYSCTL_REG6     1006
/* -- SENSORS SYSCTL END -- */

/* The /proc/sys entries */
/* These files are created for each detected MIC74. This is just a template;
   though at first sight, you might think we could use a statically
   allocated list, we need some way to get back to the parent - which
   is done through one of the 'extra' fields which are initialized 
   when a new copy is allocated. */
static ctl_table mic74_dir_table_template[] = {
	{MIC74_SYSCTL_REG0,		/* ctl_name */
	 "dev_cfg", 			/* procname */
	 NULL, 				/* data */
	 0, 				/* maxlen */
	 0644, 				/* mode */
	 NULL, 				/* child */
	 &i2c_proc_real,		/* proc_handler */
	 &i2c_sysctl_real, 		/* strategy */
	 NULL, 				/* de */
	 &mic74_reg_rw },		/* extra1, extra2 is by default NULL? */
	{MIC74_SYSCTL_REG1, "dir", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &mic74_reg_rw },
	{MIC74_SYSCTL_REG2, "out_cfg", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &mic74_reg_rw },
	{MIC74_SYSCTL_REG3, "status", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &mic74_reg_ro },
	{MIC74_SYSCTL_REG4, "int_mask", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &mic74_reg_rw },
	{MIC74_SYSCTL_REG5, "data", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &mic74_reg_rw },
	{MIC74_SYSCTL_REG6, "fan_speed", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &mic74_reg_rw },
	{0}
};

int mic74_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_detect(adapter, &addr_data, mic74_detect);
}

static int mic74_detect(struct i2c_adapter *adapter, int address,
			unsigned short flags, int kind)
{
	int i;
	struct i2c_client *new_client;
	struct mic74_data *data;
	int err = 0;
	const char *type_name = "";
	const char *client_name = "";

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		goto ERROR0;

	/* OK. For now, we presume we have a valid client. We now create the
	   client structure, even though we cannot fill it completely yet.
	   But it allows us to access mic74_{read,write}_value. */

	if (!(new_client = kmalloc(sizeof(struct i2c_client) +
				   sizeof(struct mic74_data),
				   GFP_KERNEL))) {
		err = -ENOMEM;
		goto ERROR0;
	}

	data = (struct mic74_data *) (new_client + 1);
	new_client->addr = address;
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &mic74_driver;
	new_client->flags = 0;
#ifdef CONFIG_AMAZON
	/* make sure we can search for the client later */
	new_client->flags |= I2C_CLIENT_ALLOW_USE;
#endif

	/* Determine the chip type - only one kind */
	if (kind <= 0)
		kind = mic74;

	if (kind == mic74) {
		type_name = "mic74";
		client_name = "MIC74 chip";
	} 
	else {
		goto ERROR1;
	}

	/* Fill in the remaining client fields and put it into the global list */
	strcpy(new_client->name, client_name);
	data->type = kind;

	new_client->id = mic74_id++;
	data->valid = 0;
	init_MUTEX(&data->update_lock);

	/* Tell the I2C layer a new client has arrived */
	if ((err = i2c_attach_client(new_client)))
		goto ERROR3;

	/* Register a new directory entry with module sensors */
	if ((i = i2c_register_entry(new_client,
					type_name,
					mic74_dir_table_template,
					THIS_MODULE)) < 0) {
		err = i;
		goto ERROR4;
	}
	data->sysctl_id = i;

	/* Initialize the MIC74 chip */
	mic74_init_client(new_client);
	return 0;

/* OK, this is not exactly good programming practice, usually. But it is
   very code-efficient in this case. */

      ERROR4:
	i2c_detach_client(new_client);
      ERROR3:
      ERROR1:
	kfree(new_client);
      ERROR0:
	printk("mic74.o: mic74_detect failed\n");
	return err;
}

int mic74_detach_client(struct i2c_client *client)
{
	int err;

	i2c_deregister_entry(((struct mic74_data *) (client->data))->
				 sysctl_id);

	if ((err = i2c_detach_client(client))) {
		printk
		    ("mic74.o: Client deregistration failed, client not detached.\n");
		return err;
	}

	kfree(client);
	return 0;
}

int mic74_read_value(struct i2c_client *client, u8 reg)
{
	return 0xFF & i2c_smbus_read_byte_data(client, reg);
}

int mic74_write_value(struct i2c_client *client, u8 reg, u8 value)
{
	return i2c_smbus_write_byte_data(client, reg, value);
}

/* Called when we have found a new MIC74. */
void mic74_init_client(struct i2c_client *client)
{
	/* don't init, defaults are fine */
}

void mic74_update_client(struct i2c_client *client)
{
	struct mic74_data *data = client->data;

	down(&data->update_lock);

	if ((jiffies - data->last_updated > HZ) || 
	    (jiffies < data->last_updated) || 
	    !data->valid) {

#ifdef DEBUG
		printk("Starting mic74 update\n");
#endif
		data->config = mic74_read_value(client, MIC74_REG_CONFIG);
		data->dataDir = mic74_read_value(client, MIC74_REG_DATA_DIR);
		data->outCfg = mic74_read_value(client, MIC74_REG_OUT_CFG);
		data->status = mic74_read_value(client, MIC74_REG_STATUS);
		data->intMask = mic74_read_value(client, MIC74_REG_INT_MASK);
		data->data = mic74_read_value(client, MIC74_REG_DATA);
		data->fanSpeed = mic74_read_value(client, MIC74_REG_FAN_SPEED);

		data->last_updated = jiffies;
		data->valid = 1;
	}

	up(&data->update_lock);
}


void mic74_reg_rw(struct i2c_client *client, int operation, int ctl_name,
		  int *nrels_mag, long *results)
{
	struct mic74_data *data = client->data;

	int nr = ctl_name - MIC74_SYSCTL_REG0;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {

		mic74_update_client(client);

		switch (nr) {
		case 0:
			results[0] = data->config;
			break;
		case 1:
			results[0] = data->dataDir;
			break;
		case 2:
			results[0] = data->outCfg;
			break;
		case 3:
			results[0] = data->status;
			break;
		case 4:
			results[0] = data->intMask;
			break;
		case 5:
			results[0] = data->data;
			break;
		case 6:
			results[0] = data->fanSpeed;
			break;
		}

		*nrels_mag = 1;
	} 
	else if (operation == SENSORS_PROC_REAL_WRITE) {
		
		if (*nrels_mag >= 1)
			mic74_write_value(client, 
					  (u8)nr, 
					  (u8)(results[0] & 0xFF));
	}
}

/* This is identical to the above except that it has now
   WRITE code and it calls the _rw routine above for a READ */
void mic74_reg_ro(struct i2c_client *client, int operation, int ctl_name,
		  int *nrels_mag, long *results)
{
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {

		mic74_reg_rw(client, operation, ctl_name,
			     nrels_mag, results);
		*nrels_mag = 1;
	} 
}

static int __init sensors_mic74_init(void)
{
	printk("mic74.o version %s (%s)\n", LM_VERSION, LM_DATE);
	return(i2c_add_driver(&mic74_driver));
}

static void __init mic74_cleanup(void)
{
	i2c_del_driver(&mic74_driver);
}

MODULE_AUTHOR("zebo25/MDS");
MODULE_DESCRIPTION("MIC74 driver");

module_init(sensors_mic74_init);
module_exit(mic74_cleanup);
