/*
    fscpos.c - Part of lm_sensors, Linux kernel modules for hardware
             monitoring
    Copyright (c) 2001 Hermann Jung <hej@odn.de>

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

/* 
    fujitsu siemens poseidon chip, 
    module based on lm80.c 
    Copyright (c) 1998, 1999  Frodo Looijaard <frodol@dds.nl>
    and Philip Edelbrock <phil@netroedge.com>
*/

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/i2c-proc.h>
#include <linux/init.h>
#include "version.h"

/* Addresses to scan */
static unsigned short normal_i2c[] = { 0x73, SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { SENSORS_I2C_END };
static unsigned int normal_isa[] = { SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/* Insmod parameters */
SENSORS_INSMOD_1(fscpos);

/* The FSCPOS registers */

/* chip identification */
#define FSCPOS_REG_IDENT_0    0x00
#define FSCPOS_REG_IDENT_1    0x01
#define FSCPOS_REG_IDENT_2    0x02
#define FSCPOS_REG_REVISION   0x03

/* global control and status */
#define FSCPOS_REG_EVENT_STATE  0x04
#define FSCPOS_REG_CONTROL       0x05

/* watchdog */
#define FSCPOS_REG_WDOG_PRESET      0x28
#define FSCPOS_REG_WDOG_STATE       0x23
#define FSCPOS_REG_WDOG_CONTROL     0x21

/* fan 0  */
#define FSCPOS_REG_FAN0_MIN      0x55
#define FSCPOS_REG_FAN0_ACT      0x0e
#define FSCPOS_REG_FAN0_STATE   0x0d
#define FSCPOS_REG_FAN0_RIPPLE   0x0f

/* fan 1  */
#define FSCPOS_REG_FAN1_MIN      0x65
#define FSCPOS_REG_FAN1_ACT      0x6b
#define FSCPOS_REG_FAN1_STATE   0x62
#define FSCPOS_REG_FAN1_RIPPLE   0x6f

/* fan 2  */
/* min speed fan2 not supported */
#define FSCPOS_REG_FAN2_ACT      0xab
#define FSCPOS_REG_FAN2_STATE   0xa2
#define FSCPOS_REG_FAN2_RIPPLE   0x0af

/* voltage supervision */
#define FSCPOS_REG_VOLT_12       0x45
#define FSCPOS_REG_VOLT_5        0x42
#define FSCPOS_REG_VOLT_BATT     0x48

/* temperatures */
/* sensor 0 */
#define FSCPOS_REG_TEMP0_ACT       0x64
#define FSCPOS_REG_TEMP0_STATE    0x71

/* sensor 1 */
#define FSCPOS_REG_TEMP1_ACT       0x32
#define FSCPOS_REG_TEMP1_STATE    0x81

/* sensor 2 */
#define FSCPOS_REG_TEMP2_ACT       0x35
#define FSCPOS_REG_TEMP2_STATE    0x91




/* Conversions. Rounding and limit checking is only done on the TO_REG
   variants. Note that you should be a bit careful with which arguments
   these macros are called: arguments may be evaluated more than once.
   Fixing this is just not worth it. */

#define IN_TO_REG(val,nr) (SENSORS_LIMIT((val),0,255))
#define IN_FROM_REG(val,nr) (val)

/* Initial limits */

/* For each registered FSCPOS, we need to keep some data in memory. It is
   dynamically allocated, at the same time when a new fscpos client is
   allocated. */
struct fscpos_data {
	struct i2c_client client;
	int sysctl_id;

	struct semaphore update_lock;
	char valid;		/* !=0 if following fields are valid */
	unsigned long last_updated;	/* In jiffies */

	u8  revision;        /* revision of chip */
	u8  global_event;    /* global event status */
	u8  global_control;  /* global control register */
	u8  watchdog[3];     /* watchdog */
	u8  volt[3];         /* 12, 5, battery current */ 
	u8  temp_act[3];     /* temperature */
	u8  temp_status[3];  /* status of sensor */
	u8  fan_act[3];      /* fans revolutions per second */
	u8  fan_status[3];   /* fan status */
	u8  fan_min[3];      /* fan min value for rps */
	u8  fan_ripple[3];   /* divider for rps */
};


static int fscpos_attach_adapter(struct i2c_adapter *adapter);
static int fscpos_detect(struct i2c_adapter *adapter, int address,
		       unsigned short flags, int kind);
static int fscpos_detach_client(struct i2c_client *client);

static int fscpos_read_value(struct i2c_client *client, u8 reg);
static int fscpos_write_value(struct i2c_client *client, u8 reg,
			    u8 value);
static void fscpos_update_client(struct i2c_client *client);
static void fscpos_init_client(struct i2c_client *client);


static void fscpos_in(struct i2c_client *client, int operation, int ctl_name,
		    	int *nrels_mag, long *results);
static void fscpos_fan(struct i2c_client *client, int operation,
		     	int ctl_name, int *nrels_mag, long *results);
static void fscpos_fan_internal(struct i2c_client *client, int operation,
		     	int ctl_name, int *nrels_mag, long *results, 
		     	int nr, int reg_state, int reg_min, int res_ripple);
static void fscpos_temp(struct i2c_client *client, int operation,
		      	int ctl_name, int *nrels_mag, long *results);
static void fscpos_volt(struct i2c_client *client, int operation,
		      	int ctl_name, int *nrels_mag, long *results);
static void fscpos_wdog(struct i2c_client *client, int operation,
		      	int ctl_name, int *nrels_mag, long *results);

static struct i2c_driver fscpos_driver = {
	.name		= "FSCPOS sensor driver",
	.id		= I2C_DRIVERID_FSCPOS,
	.flags		= I2C_DF_NOTIFY,
	.attach_adapter	= fscpos_attach_adapter,
	.detach_client	= fscpos_detach_client,
};

/* -- SENSORS SYSCTL START -- */
#define FSCPOS_SYSCTL_VOLT0    1000       /* 12 volt supply */
#define FSCPOS_SYSCTL_VOLT1    1001       /* 5 volt supply */
#define FSCPOS_SYSCTL_VOLT2    1002       /* batterie voltage*/
#define FSCPOS_SYSCTL_FAN0     1101       /* state, min, ripple, actual value fan 0 */
#define FSCPOS_SYSCTL_FAN1     1102       /* state, min, ripple, actual value fan 1 */
#define FSCPOS_SYSCTL_FAN2     1103       /* state, min, ripple, actual value fan 2 */
#define FSCPOS_SYSCTL_TEMP0    1201       /* state and value of sensor 0, cpu die */
#define FSCPOS_SYSCTL_TEMP1    1202       /* state and value of sensor 1, motherboard */
#define FSCPOS_SYSCTL_TEMP2    1203       /* state and value of sensor 2, chassis */
#define FSCPOS_SYSCTL_REV     2000        /* Revision */
#define FSCPOS_SYSCTL_EVENT   2001        /* global event status */
#define FSCPOS_SYSCTL_CONTROL 2002        /* global control byte */
#define FSCPOS_SYSCTL_WDOG     2003       /* state, min, ripple, actual value fan 2 */
/* -- SENSORS SYSCTL END -- */

/* These files are created for each detected FSCPOS. This is just a template;
   though at first sight, you might think we could use a statically
   allocated list, we need some way to get back to the parent - which
   is done through one of the 'extra' fields which are initialized 
   when a new copy is allocated. */
static ctl_table fscpos_dir_table_template[] = {
	{FSCPOS_SYSCTL_REV, "rev", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &fscpos_in},
	{FSCPOS_SYSCTL_EVENT, "alarms", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &fscpos_in},
	{FSCPOS_SYSCTL_CONTROL, "control", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &fscpos_in},
	{FSCPOS_SYSCTL_TEMP0, "temp1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &fscpos_temp},
	{FSCPOS_SYSCTL_TEMP1, "temp2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &fscpos_temp},
	{FSCPOS_SYSCTL_TEMP2, "temp3", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &fscpos_temp},
	{FSCPOS_SYSCTL_VOLT0, "in0", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &fscpos_volt},
	{FSCPOS_SYSCTL_VOLT1, "in1", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &fscpos_volt},
	{FSCPOS_SYSCTL_VOLT2, "in2", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &fscpos_volt},
	{FSCPOS_SYSCTL_FAN0, "fan1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &fscpos_fan},
	{FSCPOS_SYSCTL_FAN1, "fan2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &fscpos_fan},
	{FSCPOS_SYSCTL_FAN2, "fan3", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &fscpos_fan},
	{FSCPOS_SYSCTL_WDOG, "wdog", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &fscpos_wdog},
	{0}
};

static int fscpos_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_detect(adapter, &addr_data, fscpos_detect);
}

static int fscpos_detect(struct i2c_adapter *adapter, int address,
			 unsigned short flags, int kind)
{
	int i;
	struct i2c_client *new_client;
	struct fscpos_data *data;
	int err = 0;
	const char *type_name, *client_name;

	/* Make sure we aren't probing the ISA bus!! This is just a safety check
	   at this moment; i2c_detect really won't call us. */
#ifdef DEBUG
	if (i2c_is_isa_adapter(adapter)) {
		printk
		    ("fscpos.o: fscpos_detect called for an ISA bus adapter?!?\n");
		return 0;
	}
#endif

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		goto ERROR0;

	/* OK. For now, we presume we have a valid client. We now create the
	   client structure, even though we cannot fill it completely yet.
	   But it allows us to access fscpos_{read,write}_value. */
	if (!(data = kmalloc(sizeof(struct fscpos_data), GFP_KERNEL))) {
		err = -ENOMEM;
		goto ERROR0;
	}

	new_client = &data->client;
	new_client->addr = address;
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &fscpos_driver;
	new_client->flags = 0;

	/* Do the remaining detection unless force or force_fscpos parameter */
	if (kind < 0) {
		if (fscpos_read_value(new_client, FSCPOS_REG_IDENT_0) != 0x50)
			goto ERROR1;
		if (fscpos_read_value(new_client, FSCPOS_REG_IDENT_1) != 0x45)
			goto ERROR1;
		if (fscpos_read_value(new_client, FSCPOS_REG_IDENT_2) != 0x47)
			goto ERROR1;
	}

	kind = fscpos;

	type_name = "fscpos";
	client_name = "fsc poseidon chip";

	/* Fill in the remaining client fields and put it into the global list */
	strcpy(new_client->name, client_name);
	data->valid = 0;
	init_MUTEX(&data->update_lock);

	/* Tell the I2C layer a new client has arrived */
	if ((err = i2c_attach_client(new_client)))
		goto ERROR3;

	/* Register a new directory entry with module sensors */
	if ((i = i2c_register_entry(new_client, type_name,
					fscpos_dir_table_template,
					THIS_MODULE)) < 0) {
		err = i;
		goto ERROR4;
	}
	data->sysctl_id = i;

	fscpos_init_client(new_client);
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

static int fscpos_detach_client(struct i2c_client *client)
{
	int err;

	i2c_deregister_entry(((struct fscpos_data *) (client->data))->
				 sysctl_id);

	if ((err = i2c_detach_client(client))) {
		printk
		    ("fscpos.o: Client deregistration failed, client not detached.\n");
		return err;
	}

	kfree(client->data);

	return 0;
}

static int fscpos_read_value(struct i2c_client *client, u8 reg)
{
#ifdef DEBUG
	printk("fscpos: read reg 0x%02x\n",reg);
#endif
	return i2c_smbus_read_byte_data(client, reg);
}

static int fscpos_write_value(struct i2c_client *client, u8 reg, u8 value)
{
#ifdef DEBUG
	printk("fscpos: write reg 0x%02x, val 0x%02x\n",reg, value);
#endif
	return i2c_smbus_write_byte_data(client, reg, value);
}

/* Called when we have found a new FSCPOS. */
static void fscpos_init_client(struct i2c_client *client)
{
	struct fscpos_data *data = client->data;

	/* read revision from chip */
	data->revision =  fscpos_read_value(client,FSCPOS_REG_REVISION);
	/* setup missing fan2_min value */
	data->fan_min[2] = 0xff;
}

static void fscpos_update_client(struct i2c_client *client)
{
	struct fscpos_data *data = client->data;

	down(&data->update_lock);

	if ((jiffies - data->last_updated > 2 * HZ) ||
	    (jiffies < data->last_updated) || !data->valid) {

#ifdef DEBUG
		printk("Starting fscpos update\n");
#endif
		data->temp_act[0] = fscpos_read_value(client, FSCPOS_REG_TEMP0_ACT);
		data->temp_act[1] = fscpos_read_value(client, FSCPOS_REG_TEMP1_ACT);
		data->temp_act[2] = fscpos_read_value(client, FSCPOS_REG_TEMP2_ACT);
		data->temp_status[0] = fscpos_read_value(client, FSCPOS_REG_TEMP0_STATE);
		data->temp_status[1] = fscpos_read_value(client, FSCPOS_REG_TEMP1_STATE);
		data->temp_status[2] = fscpos_read_value(client, FSCPOS_REG_TEMP2_STATE);

		data->volt[0] = fscpos_read_value(client, FSCPOS_REG_VOLT_12);
		data->volt[1] = fscpos_read_value(client, FSCPOS_REG_VOLT_5);
		data->volt[2] = fscpos_read_value(client, FSCPOS_REG_VOLT_BATT);

		data->fan_act[0] = fscpos_read_value(client, FSCPOS_REG_FAN0_ACT);
		data->fan_act[1] = fscpos_read_value(client, FSCPOS_REG_FAN1_ACT);
		data->fan_act[2] = fscpos_read_value(client, FSCPOS_REG_FAN2_ACT);
		data->fan_status[0] = fscpos_read_value(client, FSCPOS_REG_FAN0_STATE);
		data->fan_status[1] = fscpos_read_value(client, FSCPOS_REG_FAN1_STATE);
		data->fan_status[2] = fscpos_read_value(client, FSCPOS_REG_FAN2_STATE);
		data->fan_min[0] = fscpos_read_value(client, FSCPOS_REG_FAN0_MIN);
		data->fan_min[1] = fscpos_read_value(client, FSCPOS_REG_FAN1_MIN);
		/* fan2_min is not supported */
		data->fan_ripple[0] = fscpos_read_value(client, FSCPOS_REG_FAN0_RIPPLE);
		data->fan_ripple[1] = fscpos_read_value(client, FSCPOS_REG_FAN1_RIPPLE);
		data->fan_ripple[2] = fscpos_read_value(client, FSCPOS_REG_FAN2_RIPPLE);

		data->watchdog[0] = fscpos_read_value(client, FSCPOS_REG_WDOG_PRESET);
		data->watchdog[1] = fscpos_read_value(client, FSCPOS_REG_WDOG_STATE);
		data->watchdog[2] = fscpos_read_value(client, FSCPOS_REG_WDOG_CONTROL);

		data->global_event = fscpos_read_value(client, FSCPOS_REG_EVENT_STATE);

                data->last_updated = jiffies;
                data->valid = 1;                 
	}

	up(&data->update_lock);
}


/* The next few functions are the call-back functions of the /proc/sys and
   sysctl files. Which function is used is defined in the ctl_table in
   the extra1 field.
   Each function must return the magnitude (power of 10 to divide the date
   with) if it is called with operation==SENSORS_PROC_REAL_INFO. It must
   put a maximum of *nrels elements in results reflecting the data of this
   file, and set *nrels to the number it actually put in it, if operation==
   SENSORS_PROC_REAL_READ. Finally, it must get upto *nrels elements from
   results and write them to the chip, if operations==SENSORS_PROC_REAL_WRITE.
   Note that on SENSORS_PROC_REAL_READ, I do not check whether results is
   large enough (by checking the incoming value of *nrels). This is not very
   good practice, but as long as you put less than about 5 values in results,
   you can assume it is large enough. */
void fscpos_in(struct i2c_client *client, int operation, int ctl_name,
	     int *nrels_mag, long *results)
{
	struct fscpos_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		fscpos_update_client(client);
		switch(ctl_name) {
			case FSCPOS_SYSCTL_REV:
				results[0] = data->revision ;
				break;
			case FSCPOS_SYSCTL_EVENT:
				results[0] = data->global_event & 0x1f;
				break;
			case FSCPOS_SYSCTL_CONTROL:
				results[0] = data->global_control & 0x01;
				break;
			default:
				printk("fscpos: ctl_name %d not supported\n",
					ctl_name);
				*nrels_mag = 0;
				return;
		}
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if((ctl_name == FSCPOS_SYSCTL_CONTROL) && (*nrels_mag >= 1)) {
			data->global_control = (results[0] & 0x01);
			printk("fscpos: writing 0x%02x to global_control\n",
				data->global_control);
			fscpos_write_value(client,FSCPOS_REG_CONTROL,
				data->global_control);
		}
		else
			printk("fscpos: writing to chip not supported\n");
	}
}

#define TEMP_FROM_REG(val)    (val-128)


void fscpos_temp(struct i2c_client *client, int operation, int ctl_name,
	       int *nrels_mag, long *results)
{
	struct fscpos_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		fscpos_update_client(client);
		switch(ctl_name) {
			case FSCPOS_SYSCTL_TEMP0:
				results[0] = data->temp_status[0] & 0x03;
				results[1] = TEMP_FROM_REG(data->temp_act[0]);
				break;
			case FSCPOS_SYSCTL_TEMP1:
				results[0] = data->temp_status[1] & 0x03;
				results[1] = TEMP_FROM_REG(data->temp_act[1]);
				break;
			case FSCPOS_SYSCTL_TEMP2:
				results[0] = data->temp_status[2] & 0x03;
				results[1] = TEMP_FROM_REG(data->temp_act[2]);
				break;
			default:
				printk("fscpos: ctl_name %d not supported\n",
					ctl_name);
				*nrels_mag = 0;
				return;
		}
		*nrels_mag = 2;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if(*nrels_mag >= 1) {
			switch(ctl_name) {
				case FSCPOS_SYSCTL_TEMP0:
					data->temp_status[0] = 
						(data->temp_status[0] & ~0x02) 
						| (results[0] & 0x02);
					printk("fscpos: writing value 0x%02x "
						"to temp0_status\n",
						data->temp_status[0]);
					fscpos_write_value(client,
						FSCPOS_REG_TEMP0_STATE,
						data->temp_status[0] & 0x02);
					break;
				case FSCPOS_SYSCTL_TEMP1:
					data->temp_status[1] = (data->temp_status[1] & ~0x02) | (results[0] & 0x02);
					printk("fscpos: writing value 0x%02x to temp1_status\n", data->temp_status[1]);
					fscpos_write_value(client,FSCPOS_REG_TEMP1_STATE,
						data->temp_status[1] & 0x02);
					break;
				case FSCPOS_SYSCTL_TEMP2:
					data->temp_status[2] = (data->temp_status[2] & ~0x02) | (results[0] & 0x02);
					printk("fscpos: writing value 0x%02x to temp2_status\n", data->temp_status[2]);
					fscpos_write_value(client,FSCPOS_REG_TEMP2_STATE,
						data->temp_status[2] & 0x02);
					break;
				default:
					printk("fscpos: ctl_name %d not supported\n",ctl_name);
			}
		}
		else
			printk("fscpos: writing to chip not supported\n");
	}
}

#define VOLT_FROM_REG(val,mult)    (val*mult/255)

void fscpos_volt(struct i2c_client *client, int operation, int ctl_name,
	       int *nrels_mag, long *results)
{
	struct fscpos_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 2;
	else if (operation == SENSORS_PROC_REAL_READ) {
		fscpos_update_client(client);
		switch(ctl_name) {
			case FSCPOS_SYSCTL_VOLT0:
				results[0] = VOLT_FROM_REG(data->volt[0],1420);
				break;
			case FSCPOS_SYSCTL_VOLT1:
				results[0] = VOLT_FROM_REG(data->volt[1],660);
				break;
			case FSCPOS_SYSCTL_VOLT2:
				results[0] = VOLT_FROM_REG(data->volt[2],330);
				break;
			default:
				printk("fscpos: ctl_name %d not supported\n",
					ctl_name);
				*nrels_mag = 0;
				return;
		}
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
			printk("fscpos: writing to chip not supported\n");
	}
}

void fscpos_fan(struct i2c_client *client, int operation, int ctl_name,
	       int *nrels_mag, long *results)
{

	switch(ctl_name) {
		case FSCPOS_SYSCTL_FAN0:
			fscpos_fan_internal(client,operation,ctl_name,nrels_mag,results,
				0,FSCPOS_REG_FAN0_STATE,FSCPOS_REG_FAN0_MIN,
				FSCPOS_REG_FAN0_RIPPLE);
			break;
		case FSCPOS_SYSCTL_FAN1:
			fscpos_fan_internal(client,operation,ctl_name,nrels_mag,results,
				1,FSCPOS_REG_FAN1_STATE,FSCPOS_REG_FAN1_MIN,
				FSCPOS_REG_FAN1_RIPPLE);
			break;
		case FSCPOS_SYSCTL_FAN2:
			fscpos_fan_internal(client,operation,ctl_name,nrels_mag,results,
				2,FSCPOS_REG_FAN2_STATE,0xff,
				FSCPOS_REG_FAN2_RIPPLE);
			break;
		default:
			printk("fscpos: illegal fan nr %d\n",ctl_name);
	}
}
			
#define RPM_FROM_REG(val)   (val*60)

void fscpos_fan_internal(struct i2c_client *client, int operation, int ctl_name,
	       int *nrels_mag, long *results, int nr,
	       int reg_state, int reg_min, int reg_ripple )
{
	struct fscpos_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		fscpos_update_client(client);
		results[0] = data->fan_status[nr] & 0x04;
		results[1] = data->fan_min[nr];
		results[2] = data->fan_ripple[nr] & 0x03;
		results[3] = RPM_FROM_REG(data->fan_act[nr]);
		*nrels_mag = 4;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if(*nrels_mag >= 1) {
			data->fan_status[nr] = results[0] & 0x04;
			printk("fscpos: writing value 0x%02x to fan%d_status\n",
				data->fan_status[nr],nr);
			fscpos_write_value(client,reg_state,
				data->fan_status[nr]);
		}
		if((*nrels_mag >= 2) && (nr < 2)) {  
			/* minimal speed for fan2 not supported */
			data->fan_min[nr] = results[1];
			printk("fscpos: writing value 0x%02x to fan%d_min\n",
				data->fan_min[nr],nr);
			fscpos_write_value(client,reg_min,
				data->fan_min[nr]);
		}
		if(*nrels_mag >= 3) {
			if((results[2] & 0x03) == 0) {
				printk("fscpos: fan%d ripple 0 not allowed\n",nr);
				return;
			}
			data->fan_ripple[nr] = results[2] & 0x03;
			printk("fscpos: writing value 0x%02x to fan%d_ripple\n",
				data->fan_ripple[nr],nr);
			fscpos_write_value(client,reg_ripple,
				data->fan_ripple[nr]);
		}	
	}
}

void fscpos_wdog(struct i2c_client *client, int operation, int ctl_name,
	     int *nrels_mag, long *results)
{
	struct fscpos_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		fscpos_update_client(client);
		results[0] = data->watchdog[0] ;
		results[1] = data->watchdog[1] & 0x02;
		results[2] = data->watchdog[2] & 0xb0;
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->watchdog[0] = results[0] & 0xff;
			printk("fscpos: writing value 0x%02x to wdog_preset\n",
				data->watchdog[0]); 
			fscpos_write_value(client,FSCPOS_REG_WDOG_PRESET,
				data->watchdog[0]);
		} 
		if (*nrels_mag >= 2) {
			data->watchdog[1] = results[1] & 0x02;
			printk("fscpos: writing value 0x%02x to wdog_state\n",
				data->watchdog[1]); 
			fscpos_write_value(client,FSCPOS_REG_WDOG_STATE,
				data->watchdog[1]);
		}
		if (*nrels_mag >= 3) {
			data->watchdog[2] = results[2] & 0xb0;
			printk("fscpos: writing value 0x%02x to wdog_control\n",
				data->watchdog[2]); 
			fscpos_write_value(client,FSCPOS_REG_WDOG_CONTROL,
				data->watchdog[2]);
		}
	}
}

static int __init sm_fscpos_init(void)
{
	printk("fscpos.o version %s (%s)\n", LM_VERSION, LM_DATE);
	return i2c_add_driver(&fscpos_driver);
}

static void __exit sm_fscpos_exit(void)
{
	i2c_del_driver(&fscpos_driver);
}



MODULE_AUTHOR
    ("Hermann Jung <hej@odn.de> based on work from Frodo Looijaard <frodol@dds.nl> and Philip Edelbrock <phil@netroedge.com>");
MODULE_DESCRIPTION("fujitsu siemens poseidon chip driver");
MODULE_LICENSE("GPL");

module_init(sm_fscpos_init);
module_exit(sm_fscpos_exit);
