/*
    fscscy.c - Part of lm_sensors, Linux kernel modules for hardware
             monitoring
    Copyright (c) 2001 Martin Knoblauch <mkn@teraport.de, knobi@knobisoft.de>

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
    fujitsu siemens scylla chip, 
    module based on lm80.c, fscpos.c
    Copyright (c) 1998, 1999  Frodo Looijaard <frodol@dds.nl>
    and Philip Edelbrock <phil@netroedge.com>
*/

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/i2c-proc.h>
#include <linux/init.h>
#include "version.h"

MODULE_LICENSE("GPL");

/* Addresses to scan */
static unsigned short normal_i2c[] = { 0x73, SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { SENSORS_I2C_END };
static unsigned int normal_isa[] = { SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/* Insmod parameters */
SENSORS_INSMOD_1(fscscy);

/* The FSCSCY registers */

/* chip identification */
#define FSCSCY_REG_IDENT_0    0x00
#define FSCSCY_REG_IDENT_1    0x01
#define FSCSCY_REG_IDENT_2    0x02
#define FSCSCY_REG_REVISION   0x03

/* global control and status */
#define FSCSCY_REG_EVENT_STATE  0x04
#define FSCSCY_REG_CONTROL       0x05

/* watchdog */
#define FSCSCY_REG_WDOG_PRESET      0x28
#define FSCSCY_REG_WDOG_STATE       0x23
#define FSCSCY_REG_WDOG_CONTROL     0x21

/*
** Fan definitions
**
** _RPMMIN: Minimum speed. Can be set via interface, but only for three of the fans
**          FAN1_RPMMIN is wired to Fan 0 (CPU Fans)
**          FAN4_RPMMIN is wired to Fan 2 (PS Fans ??)
**          FAN5_RPMMIN is wired to Fan 3 (AUX Fans ??)
** _ACT:    Actual Fan Speed
** _STATE:  Fan status register
** _RIPPLE: Fan speed multiplier
*/

/* fan 0  */
#define FSCSCY_REG_FAN0_RPMMIN	0x65
#define FSCSCY_REG_FAN0_ACT	0x6b
#define FSCSCY_REG_FAN0_STATE	0x62
#define FSCSCY_REG_FAN0_RIPPLE	0x6f

/* fan 1  */
#define FSCSCY_REG_FAN1_RPMMIN     FSCSCY_REG_FAN0_RPMMIN
#define FSCSCY_REG_FAN1_ACT     0x6c
#define FSCSCY_REG_FAN1_STATE   0x61
#define FSCSCY_REG_FAN1_RIPPLE  0x6f

/* fan 2  */
#define FSCSCY_REG_FAN2_RPMMIN     0x55
#define FSCSCY_REG_FAN2_ACT     0x0e
#define FSCSCY_REG_FAN2_STATE   0x0d
#define FSCSCY_REG_FAN2_RIPPLE  0x0f

/* fan 3  */
#define FSCSCY_REG_FAN3_RPMMIN     0xa5
#define FSCSCY_REG_FAN3_ACT     0xab
#define FSCSCY_REG_FAN3_STATE   0xa2
#define FSCSCY_REG_FAN3_RIPPLE  0xaf

/* fan 4  */
#define FSCSCY_REG_FAN4_RPMMIN     FSCSCY_REG_FAN2_RPMMIN
#define FSCSCY_REG_FAN4_ACT	0x5c
#define FSCSCY_REG_FAN4_STATE   0x52
#define FSCSCY_REG_FAN4_RIPPLE  0x0f

/* fan 5  */
#define FSCSCY_REG_FAN5_RPMMIN     FSCSCY_REG_FAN3_RPMMIN
#define FSCSCY_REG_FAN5_ACT     0xbb
#define FSCSCY_REG_FAN5_STATE   0xb2
#define FSCSCY_REG_FAN5_RIPPLE  0xbf

/* voltage supervision */
#define FSCSCY_REG_VOLT_12       0x45
#define FSCSCY_REG_VOLT_5        0x42
#define FSCSCY_REG_VOLT_BATT     0x48

/* temperatures */
/* sensor 0 */
#define FSCSCY_REG_TEMP0_ACT	0x64
#define FSCSCY_REG_TEMP0_STATE	0x71
#define FSCSCY_REG_TEMP0_LIM	0x76

/* sensor 1 */
#define FSCSCY_REG_TEMP1_ACT	0xD0
#define FSCSCY_REG_TEMP1_STATE	0xD1
#define FSCSCY_REG_TEMP1_LIM	0xD6

/* sensor 2 */
#define FSCSCY_REG_TEMP2_ACT	0x32
#define FSCSCY_REG_TEMP2_STATE	0x81
#define FSCSCY_REG_TEMP2_LIM	0x86

/* sensor3 */
#define FSCSCY_REG_TEMP3_ACT	0x35
#define FSCSCY_REG_TEMP3_STATE	0x91
#define FSCSCY_REG_TEMP3_LIM	0x96

/* PCI Load */
#define FSCSCY_REG_PCILOAD	0x1a

/* Intrusion Sensor */
#define FSCSCY_REG_INTR_STATE	0x13
#define FSCSCY_REG_INTR_CTRL	0x12

/* Conversions. Rounding and limit checking is only done on the TO_REG
   variants. Note that you should be a bit careful with which arguments
   these macros are called: arguments may be evaluated more than once.
   Fixing this is just not worth it. */

#define IN_TO_REG(val,nr) (SENSORS_LIMIT((val),0,255))
#define IN_FROM_REG(val,nr) (val)

/* Initial limits */

/* For each registered FSCSCY, we need to keep some data in memory. It is
   dynamically allocated, at the same time when a new fscscy client is
   allocated. */
struct fscscy_data {
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
	u8  volt_min[3];     /* minimum voltages over module "lifetime" */
	u8  volt_max[3];     /* maximum voltages over module "lifetime" */
	u8  temp_act[4];     /* temperature */
	u8  temp_status[4];  /* status of temp. sensor */
	u8  temp_lim[4];     /* limit temperature of temp. sensor */
	u8  temp_min[4];     /* minimum of temp. sensor, this is just calculated by the module */
	u8  temp_max[4];     /* maximum of temp. sensor, this is just calculsted by the module */
	u8  fan_act[6];      /* fans revolutions per second */
	u8  fan_status[6];   /* fan status */
	u8  fan_rpmmin[6];   /* fan min value for rps */
	u8  fan_ripple[6];   /* divider for rps */
	u8  fan_min[6];      /* minimum RPM over module "lifetime" */
	u8  fan_max[6];      /* maximum RPM over module "lifetime" */
	u8  pciload;	     /* PCILoad value */
	u8  intr_status;     /* Intrusion Status */
	u8  intr_control;    /* Intrusion Control */
};


static int fscscy_attach_adapter(struct i2c_adapter *adapter);
static int fscscy_detect(struct i2c_adapter *adapter, int address,
		       unsigned short flags, int kind);
static int fscscy_detach_client(struct i2c_client *client);

static int fscscy_read_value(struct i2c_client *client, u8 reg);
static int fscscy_write_value(struct i2c_client *client, u8 reg,
			    u8 value);
static void fscscy_update_client(struct i2c_client *client);
static void fscscy_init_client(struct i2c_client *client);


static void fscscy_in(struct i2c_client *client, int operation, int ctl_name,
		    	int *nrels_mag, long *results);
static void fscscy_fan(struct i2c_client *client, int operation,
		     	int ctl_name, int *nrels_mag, long *results);
static void fscscy_fan_internal(struct i2c_client *client, int operation,
		     	int ctl_name, int *nrels_mag, long *results, 
		     	int nr, int reg_state, int reg_min, int res_ripple);
static void fscscy_temp(struct i2c_client *client, int operation,
		      	int ctl_name, int *nrels_mag, long *results);
static void fscscy_volt(struct i2c_client *client, int operation,
		      	int ctl_name, int *nrels_mag, long *results);
static void fscscy_wdog(struct i2c_client *client, int operation,
		      	int ctl_name, int *nrels_mag, long *results);
static void fscscy_pciload(struct i2c_client *client, int operation,
		      	int ctl_name, int *nrels_mag, long *results);
static void fscscy_intrusion(struct i2c_client *client, int operation,
		      	int ctl_name, int *nrels_mag, long *results);

static struct i2c_driver fscscy_driver = {
	.name		= "FSCSCY sensor driver",
	.id		= I2C_DRIVERID_FSCSCY,
	.flags		= I2C_DF_NOTIFY,
	.attach_adapter	= fscscy_attach_adapter,
	.detach_client	= fscscy_detach_client,
};

/* The /proc/sys entries */

/* -- SENSORS SYSCTL START -- */
#define FSCSCY_SYSCTL_VOLT0    1000       /* 12 volt supply */
#define FSCSCY_SYSCTL_VOLT1    1001       /* 5 volt supply */
#define FSCSCY_SYSCTL_VOLT2    1002       /* batterie voltage*/
#define FSCSCY_SYSCTL_FAN0     1101       /* state, min, ripple, actual value fan 0 */
#define FSCSCY_SYSCTL_FAN1     1102       /* state, min, ripple, actual value fan 1 */
#define FSCSCY_SYSCTL_FAN2     1103       /* state, min, ripple, actual value fan 2 */
#define FSCSCY_SYSCTL_FAN3     1104       /* state, min, ripple, actual value fan 3 */
#define FSCSCY_SYSCTL_FAN4     1105       /* state, min, ripple, actual value fan 4 */
#define FSCSCY_SYSCTL_FAN5     1106       /* state, min, ripple, actual value fan 5 */
#define FSCSCY_SYSCTL_TEMP0    1201       /* state and value of sensor 0, cpu die */
#define FSCSCY_SYSCTL_TEMP1    1202       /* state and value of sensor 1, motherboard */
#define FSCSCY_SYSCTL_TEMP2    1203       /* state and value of sensor 2, chassis */
#define FSCSCY_SYSCTL_TEMP3    1204       /* state and value of sensor 3, chassis */
#define FSCSCY_SYSCTL_REV     2000        /* Revision */
#define FSCSCY_SYSCTL_EVENT   2001        /* global event status */
#define FSCSCY_SYSCTL_CONTROL 2002        /* global control byte */
#define FSCSCY_SYSCTL_WDOG     2003       /* state, min, ripple, actual value fan 2 */
#define FSCSCY_SYSCTL_PCILOAD  2004       /* PCILoad value */
#define FSCSCY_SYSCTL_INTRUSION 2005      /* state, control for intrusion sensor */

/* -- SENSORS SYSCTL END -- */

/* These files are created for each detected FSCSCY. This is just a template;
   though at first sight, you might think we could use a statically
   allocated list, we need some way to get back to the parent - which
   is done through one of the 'extra' fields which are initialized 
   when a new copy is allocated. */
static ctl_table fscscy_dir_table_template[] = {
	{FSCSCY_SYSCTL_REV, "rev", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &fscscy_in},
	{FSCSCY_SYSCTL_EVENT, "alarms", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &fscscy_in},
	{FSCSCY_SYSCTL_CONTROL, "control", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &fscscy_in},
	{FSCSCY_SYSCTL_TEMP0, "temp1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &fscscy_temp},
	{FSCSCY_SYSCTL_TEMP1, "temp2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &fscscy_temp},
	{FSCSCY_SYSCTL_TEMP2, "temp3", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &fscscy_temp},
	{FSCSCY_SYSCTL_TEMP3, "temp4", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &fscscy_temp},
	{FSCSCY_SYSCTL_VOLT0, "in0", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &fscscy_volt},
	{FSCSCY_SYSCTL_VOLT1, "in1", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &fscscy_volt},
	{FSCSCY_SYSCTL_VOLT2, "in2", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &fscscy_volt},
	{FSCSCY_SYSCTL_FAN0, "fan1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &fscscy_fan},
	{FSCSCY_SYSCTL_FAN1, "fan2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &fscscy_fan},
	{FSCSCY_SYSCTL_FAN2, "fan3", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &fscscy_fan},
	{FSCSCY_SYSCTL_FAN3, "fan4", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &fscscy_fan},
	{FSCSCY_SYSCTL_FAN4, "fan5", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &fscscy_fan},
	{FSCSCY_SYSCTL_FAN5, "fan6", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &fscscy_fan},
	{FSCSCY_SYSCTL_WDOG, "wdog", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &fscscy_wdog},
	{FSCSCY_SYSCTL_PCILOAD, "pciload", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &fscscy_pciload},
	{FSCSCY_SYSCTL_INTRUSION, "intrusion", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &fscscy_intrusion},
	{0}
};

static int fscscy_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_detect(adapter, &addr_data, fscscy_detect);
}

int fscscy_detect(struct i2c_adapter *adapter, int address,
		unsigned short flags, int kind)
{
	int i;
	struct i2c_client *new_client;
	struct fscscy_data *data;
	int err = 0;
	const char *type_name, *client_name;

	/* Make sure we aren't probing the ISA bus!! This is just a safety check
	   at this moment; i2c_detect really won't call us. */
#ifdef DEBUG
	if (i2c_is_isa_adapter(adapter)) {
		printk
		    ("fscscy.o: fscscy_detect called for an ISA bus adapter?!?\n");
		return 0;
	}
#endif

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		goto ERROR0;

	/* OK. For now, we presume we have a valid client. We now create the
	   client structure, even though we cannot fill it completely yet.
	   But it allows us to access fscscy_{read,write}_value. */
	if (!(data = kmalloc(sizeof(struct fscscy_data), GFP_KERNEL))) {
		err = -ENOMEM;
		goto ERROR0;
	}

	new_client = &data->client;
	new_client->addr = address;
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &fscscy_driver;
	new_client->flags = 0;

	/* Do the remaining detection unless force or force_fscscy parameter */
	if (kind < 0) {
		if (fscscy_read_value(new_client, FSCSCY_REG_IDENT_0) != 0x53)
			goto ERROR1;
		if (fscscy_read_value(new_client, FSCSCY_REG_IDENT_1) != 0x43)
			goto ERROR1;
		if (fscscy_read_value(new_client, FSCSCY_REG_IDENT_2) != 0x59)
			goto ERROR1;
	}

	kind = fscscy;

	type_name = "fscscy";
	client_name = "fsc scylla chip";

	/* Fill in the remaining client fields and put it into the global list */
	strcpy(new_client->name, client_name);
	data->valid = 0;
	init_MUTEX(&data->update_lock);

	/* Tell the I2C layer a new client has arrived */
	if ((err = i2c_attach_client(new_client)))
		goto ERROR3;

	/* Register a new directory entry with module sensors */
	if ((i = i2c_register_entry(new_client, type_name,
					fscscy_dir_table_template,
					THIS_MODULE)) < 0) {
		err = i;
		goto ERROR4;
	}
	data->sysctl_id = i;

	fscscy_init_client(new_client);
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

static int fscscy_detach_client(struct i2c_client *client)
{
	int err;

	i2c_deregister_entry(((struct fscscy_data *) (client->data))->
				 sysctl_id);

	if ((err = i2c_detach_client(client))) {
		printk
		    ("fscscy.o: Client deregistration failed, client not detached.\n");
		return err;
	}

	kfree(client->data);

	return 0;
}

static int fscscy_read_value(struct i2c_client *client, u8 reg)
{
#ifdef DEBUG
	printk("fscscy: read reg 0x%02x\n",reg);
#endif
	return i2c_smbus_read_byte_data(client, reg);
}

static int fscscy_write_value(struct i2c_client *client, u8 reg, u8 value)
{
#ifdef DEBUG
	printk("fscscy: write reg 0x%02x, val 0x%02x\n",reg, value);
#endif
	return i2c_smbus_write_byte_data(client, reg, value);
}

/* Called when we have found a new FSCSCY. */
static void fscscy_init_client(struct i2c_client *client)
{
	struct fscscy_data *data = client->data;

	/* read revision from chip */
	data->revision =  fscscy_read_value(client,FSCSCY_REG_REVISION);

        /* Initialize min/max values from chip */
	data->fan_min[0]  = data->fan_max[0]  = fscscy_read_value(client, FSCSCY_REG_FAN0_ACT);
	data->fan_min[1]  = data->fan_max[1]  = fscscy_read_value(client, FSCSCY_REG_FAN1_ACT);
	data->fan_min[2]  = data->fan_max[2]  = fscscy_read_value(client, FSCSCY_REG_FAN2_ACT);
	data->fan_min[3]  = data->fan_max[3]  = fscscy_read_value(client, FSCSCY_REG_FAN3_ACT);
	data->fan_min[4]  = data->fan_max[4]  = fscscy_read_value(client, FSCSCY_REG_FAN4_ACT);
	data->fan_min[4]  = data->fan_max[5]  = fscscy_read_value(client, FSCSCY_REG_FAN5_ACT);
        data->temp_min[0] = data->temp_max[0] = fscscy_read_value(client, FSCSCY_REG_TEMP0_ACT);
        data->temp_min[1] = data->temp_max[1] = fscscy_read_value(client, FSCSCY_REG_TEMP1_ACT);
        data->temp_min[2] = data->temp_max[2] = fscscy_read_value(client, FSCSCY_REG_TEMP2_ACT);
        data->temp_min[3] = data->temp_max[3] = fscscy_read_value(client, FSCSCY_REG_TEMP3_ACT);
	data->volt_min[0] = data->volt_max[0] = fscscy_read_value(client, FSCSCY_REG_VOLT_12);
	data->volt_min[1] = data->volt_max[1] = fscscy_read_value(client, FSCSCY_REG_VOLT_5);
	data->volt_min[2] = data->volt_max[2] = fscscy_read_value(client, FSCSCY_REG_VOLT_BATT);
}

static void fscscy_update_client(struct i2c_client *client)
{
	struct fscscy_data *data = client->data;

	down(&data->update_lock);

	if ((jiffies - data->last_updated > 2 * HZ) ||
	    (jiffies < data->last_updated) || !data->valid) {

#ifdef DEBUG
		printk("Starting fscscy update\n");
#endif
		data->temp_act[0] = fscscy_read_value(client, FSCSCY_REG_TEMP0_ACT);
		  if (data->temp_min[0] > data->temp_act[0]) data->temp_min[0] = data->temp_act[0];
		  if (data->temp_max[0] < data->temp_act[0]) data->temp_max[0] = data->temp_act[0];
		data->temp_act[1] = fscscy_read_value(client, FSCSCY_REG_TEMP1_ACT);
		  if (data->temp_min[1] > data->temp_act[1]) data->temp_min[1] = data->temp_act[1];
		  if (data->temp_max[1] < data->temp_act[1]) data->temp_max[1] = data->temp_act[1];
		data->temp_act[2] = fscscy_read_value(client, FSCSCY_REG_TEMP2_ACT);
		  if (data->temp_min[2] > data->temp_act[2]) data->temp_min[2] = data->temp_act[2];
		  if (data->temp_max[2] < data->temp_act[2]) data->temp_max[2] = data->temp_act[2];
		data->temp_act[3] = fscscy_read_value(client, FSCSCY_REG_TEMP3_ACT);
		  if (data->temp_min[3] > data->temp_act[3]) data->temp_min[3] = data->temp_act[3];
		  if (data->temp_max[3] < data->temp_act[3]) data->temp_max[3] = data->temp_act[3];
		data->temp_status[0] = fscscy_read_value(client, FSCSCY_REG_TEMP0_STATE);
		data->temp_status[1] = fscscy_read_value(client, FSCSCY_REG_TEMP1_STATE);
		data->temp_status[2] = fscscy_read_value(client, FSCSCY_REG_TEMP2_STATE);
		data->temp_status[3] = fscscy_read_value(client, FSCSCY_REG_TEMP3_STATE);
		data->temp_lim[0] = fscscy_read_value(client, FSCSCY_REG_TEMP0_LIM);
		data->temp_lim[1] = fscscy_read_value(client, FSCSCY_REG_TEMP1_LIM);
		data->temp_lim[2] = fscscy_read_value(client, FSCSCY_REG_TEMP2_LIM);
		data->temp_lim[3] = fscscy_read_value(client, FSCSCY_REG_TEMP3_LIM);

		data->volt[0] = fscscy_read_value(client, FSCSCY_REG_VOLT_12);
		  if (data->volt_min[0] > data->volt[0]) data->volt_min[0] = data->volt[0];
		  if (data->volt_max[0] < data->volt[0]) data->volt_max[0] = data->volt[0];
		data->volt[1] = fscscy_read_value(client, FSCSCY_REG_VOLT_5);
		  if (data->volt_min[1] > data->volt[1]) data->volt_min[1] = data->volt[1];
		  if (data->volt_max[1] < data->volt[1]) data->volt_max[1] = data->volt[1];
		data->volt[2] = fscscy_read_value(client, FSCSCY_REG_VOLT_BATT);
		  if (data->volt_min[2] > data->volt[2]) data->volt_min[2] = data->volt[2];
		  if (data->volt_max[2] < data->volt[2]) data->volt_max[2] = data->volt[2];

		data->fan_act[0] = fscscy_read_value(client, FSCSCY_REG_FAN0_ACT);
		  if (data->fan_min[0] > data->fan_act[0]) data->fan_min[0] = data->fan_act[0];
		  if (data->fan_max[0] < data->fan_act[0]) data->fan_max[0] = data->fan_act[0];
		data->fan_act[1] = fscscy_read_value(client, FSCSCY_REG_FAN1_ACT);
		  if (data->fan_min[1] > data->fan_act[1]) data->fan_min[1] = data->fan_act[1];
		  if (data->fan_max[1] < data->fan_act[1]) data->fan_max[1] = data->fan_act[1];
		data->fan_act[2] = fscscy_read_value(client, FSCSCY_REG_FAN2_ACT);
		  if (data->fan_min[2] > data->fan_act[2]) data->fan_min[2] = data->fan_act[2];
		  if (data->fan_max[2] < data->fan_act[2]) data->fan_max[2] = data->fan_act[2];
		data->fan_act[3] = fscscy_read_value(client, FSCSCY_REG_FAN3_ACT);
		  if (data->fan_min[3] > data->fan_act[3]) data->fan_min[3] = data->fan_act[3];
		  if (data->fan_max[3] < data->fan_act[3]) data->fan_max[3] = data->fan_act[3];
		data->fan_act[4] = fscscy_read_value(client, FSCSCY_REG_FAN4_ACT);
		  if (data->fan_min[4] > data->fan_act[4]) data->fan_min[4] = data->fan_act[4];
		  if (data->fan_max[4] < data->fan_act[4]) data->fan_max[4] = data->fan_act[4];
		data->fan_act[5] = fscscy_read_value(client, FSCSCY_REG_FAN5_ACT);
		  if (data->fan_min[5] > data->fan_act[5]) data->fan_min[5] = data->fan_act[5];
		  if (data->fan_max[5] < data->fan_act[5]) data->fan_max[5] = data->fan_act[5];
		data->fan_status[0] = fscscy_read_value(client, FSCSCY_REG_FAN0_STATE);
		data->fan_status[1] = fscscy_read_value(client, FSCSCY_REG_FAN1_STATE);
		data->fan_status[2] = fscscy_read_value(client, FSCSCY_REG_FAN2_STATE);
		data->fan_status[3] = fscscy_read_value(client, FSCSCY_REG_FAN3_STATE);
		data->fan_status[4] = fscscy_read_value(client, FSCSCY_REG_FAN4_STATE);
		data->fan_status[5] = fscscy_read_value(client, FSCSCY_REG_FAN5_STATE);
		data->fan_rpmmin[0] = fscscy_read_value(client, FSCSCY_REG_FAN0_RPMMIN);
		data->fan_rpmmin[1] = fscscy_read_value(client, FSCSCY_REG_FAN1_RPMMIN);
		data->fan_rpmmin[2] = fscscy_read_value(client, FSCSCY_REG_FAN2_RPMMIN);
		data->fan_rpmmin[3] = fscscy_read_value(client, FSCSCY_REG_FAN3_RPMMIN);
		data->fan_rpmmin[4] = fscscy_read_value(client, FSCSCY_REG_FAN4_RPMMIN);
		data->fan_rpmmin[5] = fscscy_read_value(client, FSCSCY_REG_FAN5_RPMMIN);
		data->fan_ripple[0] = fscscy_read_value(client, FSCSCY_REG_FAN0_RIPPLE);
		data->fan_ripple[1] = fscscy_read_value(client, FSCSCY_REG_FAN1_RIPPLE);
		data->fan_ripple[2] = fscscy_read_value(client, FSCSCY_REG_FAN2_RIPPLE);
		data->fan_ripple[3] = fscscy_read_value(client, FSCSCY_REG_FAN3_RIPPLE);
		data->fan_ripple[4] = fscscy_read_value(client, FSCSCY_REG_FAN4_RIPPLE);
		data->fan_ripple[5] = fscscy_read_value(client, FSCSCY_REG_FAN5_RIPPLE);

		data->watchdog[0] = fscscy_read_value(client, FSCSCY_REG_WDOG_PRESET);
		data->watchdog[1] = fscscy_read_value(client, FSCSCY_REG_WDOG_STATE);
		data->watchdog[2] = fscscy_read_value(client, FSCSCY_REG_WDOG_CONTROL);

		data->global_event = fscscy_read_value(client, FSCSCY_REG_EVENT_STATE);
		data->global_control = fscscy_read_value(client, FSCSCY_REG_CONTROL);
		data->pciload = fscscy_read_value(client, FSCSCY_REG_PCILOAD);
		data->intr_status = fscscy_read_value(client, FSCSCY_REG_INTR_STATE);
		data->intr_control = fscscy_read_value(client, FSCSCY_REG_INTR_CTRL);

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
void fscscy_in(struct i2c_client *client, int operation, int ctl_name,
	     int *nrels_mag, long *results)
{
	struct fscscy_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		fscscy_update_client(client);
		switch(ctl_name) {
			case FSCSCY_SYSCTL_REV:
				results[0] = data->revision ;
				break;
			case FSCSCY_SYSCTL_EVENT:
				results[0] = data->global_event & 0x9f; /* MKN */
				break;
			case FSCSCY_SYSCTL_CONTROL:
				results[0] = data->global_control & 0x19; /* MKN */
				break;
			default:
				printk("fscscy: ctl_name %d not supported\n",
					ctl_name);
				*nrels_mag = 0;
				return;
		}
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if((ctl_name == FSCSCY_SYSCTL_CONTROL) && (*nrels_mag >= 1)) {
			data->global_control = (data->global_control & 0x18) | (results[0] & 0x01); /* MKN */
			printk("fscscy: writing 0x%02x to global_control\n",
				data->global_control);
			fscscy_write_value(client,FSCSCY_REG_CONTROL,
				data->global_control);
		}
		else
			printk("fscscy: writing to chip not supported\n");
	}
}

#define TEMP_FROM_REG(val)    (val-128)


void fscscy_temp(struct i2c_client *client, int operation, int ctl_name,
	       int *nrels_mag, long *results)
{
	struct fscscy_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		fscscy_update_client(client);
		switch(ctl_name) {
			case FSCSCY_SYSCTL_TEMP0:
				results[0] = data->temp_status[0] & 0x03;
				results[1] = TEMP_FROM_REG(data->temp_act[0]);
				results[2] = TEMP_FROM_REG(data->temp_lim[0]);
				results[3] = TEMP_FROM_REG(data->temp_min[0]);
				results[4] = TEMP_FROM_REG(data->temp_max[0]);
				break;
			case FSCSCY_SYSCTL_TEMP1:
				results[0] = data->temp_status[1] & 0x03;
				results[1] = TEMP_FROM_REG(data->temp_act[1]);
				results[2] = TEMP_FROM_REG(data->temp_lim[1]);
				results[3] = TEMP_FROM_REG(data->temp_min[1]);
				results[4] = TEMP_FROM_REG(data->temp_max[1]);
				break;
			case FSCSCY_SYSCTL_TEMP2:
				results[0] = data->temp_status[2] & 0x03;
				results[1] = TEMP_FROM_REG(data->temp_act[2]);
				results[2] = TEMP_FROM_REG(data->temp_lim[2]);
				results[3] = TEMP_FROM_REG(data->temp_min[2]);
				results[4] = TEMP_FROM_REG(data->temp_max[2]);
				break;
			case FSCSCY_SYSCTL_TEMP3:
				results[0] = data->temp_status[3] & 0x03;
				results[1] = TEMP_FROM_REG(data->temp_act[3]);
				results[2] = TEMP_FROM_REG(data->temp_lim[3]);
				results[3] = TEMP_FROM_REG(data->temp_min[3]);
				results[4] = TEMP_FROM_REG(data->temp_max[3]);
				break;
			default:
				printk("fscscy: ctl_name %d not supported\n",
					ctl_name);
				*nrels_mag = 0;
				return;
		}
		*nrels_mag = 5;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if(*nrels_mag >= 1) {
			switch(ctl_name) {
				case FSCSCY_SYSCTL_TEMP0:
					data->temp_status[0] = 
						(data->temp_status[0] & ~0x02) 
						| (results[0] & 0x02);
					printk("fscscy: writing value 0x%02x "
						"to temp0_status\n",
						data->temp_status[0]);
					fscscy_write_value(client,
						FSCSCY_REG_TEMP0_STATE,
						data->temp_status[0] & 0x02);
					break;
				case FSCSCY_SYSCTL_TEMP1:
					data->temp_status[1] = (data->temp_status[1] & ~0x02) | (results[0] & 0x02);
					printk("fscscy: writing value 0x%02x to temp1_status\n", data->temp_status[1]);
					fscscy_write_value(client,FSCSCY_REG_TEMP1_STATE,
						data->temp_status[1] & 0x02);
					break;
				case FSCSCY_SYSCTL_TEMP2:
					data->temp_status[2] = (data->temp_status[2] & ~0x02) | (results[0] & 0x02);
					printk("fscscy: writing value 0x%02x to temp2_status\n", data->temp_status[2]);
					fscscy_write_value(client,FSCSCY_REG_TEMP2_STATE,
						data->temp_status[2] & 0x02);
					break;
				case FSCSCY_SYSCTL_TEMP3:
					data->temp_status[3] = (data->temp_status[3] & ~0x02) | (results[0] & 0x02);
					printk("fscscy: writing value 0x%02x to temp3_status\n", data->temp_status[3]);
					fscscy_write_value(client,FSCSCY_REG_TEMP3_STATE,
						data->temp_status[3] & 0x02);
					break;
				default:
					printk("fscscy: ctl_name %d not supported\n",ctl_name);
			}
		}
		else
			printk("fscscy: writing to chip not supported\n");
	}
}

#define VOLT_FROM_REG(val,mult)    (val*mult/255)

void fscscy_volt(struct i2c_client *client, int operation, int ctl_name,
	       int *nrels_mag, long *results)
{
	struct fscscy_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 2;
	else if (operation == SENSORS_PROC_REAL_READ) {
		fscscy_update_client(client);
		switch(ctl_name) {
			case FSCSCY_SYSCTL_VOLT0:
				results[0] = VOLT_FROM_REG(data->volt[0],1420);
				results[1] = VOLT_FROM_REG(data->volt_min[0],1420);
				results[2] = VOLT_FROM_REG(data->volt_max[0],1420);
				break;
			case FSCSCY_SYSCTL_VOLT1:
				results[0] = VOLT_FROM_REG(data->volt[1],660);
				results[1] = VOLT_FROM_REG(data->volt_min[1],660);
				results[2] = VOLT_FROM_REG(data->volt_max[1],660);
				break;
			case FSCSCY_SYSCTL_VOLT2:
				results[0] = VOLT_FROM_REG(data->volt[2],330);
				results[1] = VOLT_FROM_REG(data->volt_min[2],330);
				results[2] = VOLT_FROM_REG(data->volt_max[2],330);
				break;
			default:
				printk("fscscy: ctl_name %d not supported\n",
					ctl_name);
				*nrels_mag = 0;
				return;
		}
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
			printk("fscscy: writing to chip not supported\n");
	}
}

void fscscy_fan(struct i2c_client *client, int operation, int ctl_name,
	       int *nrels_mag, long *results)
{

	switch(ctl_name) {
		case FSCSCY_SYSCTL_FAN0:
			fscscy_fan_internal(client,operation,ctl_name,nrels_mag,results,
				0,FSCSCY_REG_FAN0_STATE,FSCSCY_REG_FAN0_RPMMIN,
				FSCSCY_REG_FAN0_RIPPLE);
			break;
		case FSCSCY_SYSCTL_FAN1:
			fscscy_fan_internal(client,operation,ctl_name,nrels_mag,results,
				1,FSCSCY_REG_FAN1_STATE,FSCSCY_REG_FAN1_RPMMIN,
				FSCSCY_REG_FAN1_RIPPLE);
			break;
		case FSCSCY_SYSCTL_FAN2:
			fscscy_fan_internal(client,operation,ctl_name,nrels_mag,results,
				2,FSCSCY_REG_FAN2_STATE,FSCSCY_REG_FAN2_RPMMIN,
				FSCSCY_REG_FAN2_RIPPLE);
			break;
		case FSCSCY_SYSCTL_FAN3:
			fscscy_fan_internal(client,operation,ctl_name,nrels_mag,results,
				3,FSCSCY_REG_FAN3_STATE,FSCSCY_REG_FAN3_RPMMIN,
				FSCSCY_REG_FAN3_RIPPLE);
			break;
		case FSCSCY_SYSCTL_FAN4:
			fscscy_fan_internal(client,operation,ctl_name,nrels_mag,results,
				4,FSCSCY_REG_FAN4_STATE,FSCSCY_REG_FAN4_RPMMIN,
				FSCSCY_REG_FAN4_RIPPLE);
			break;
		case FSCSCY_SYSCTL_FAN5:
			fscscy_fan_internal(client,operation,ctl_name,nrels_mag,results,
				5,FSCSCY_REG_FAN5_STATE,FSCSCY_REG_FAN5_RPMMIN,
				FSCSCY_REG_FAN5_RIPPLE);
			break;
		default:
			printk("fscscy: illegal fan nr %d\n",ctl_name);
	}
}
			
#define RPM_FROM_REG(val)   (val*60)

void fscscy_fan_internal(struct i2c_client *client, int operation, int ctl_name,
	       int *nrels_mag, long *results, int nr,
	       int reg_state, int reg_min, int reg_ripple )
{
	struct fscscy_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		fscscy_update_client(client);
		results[0] = data->fan_status[nr] & 0x0f; /* MKN */
		results[1] = data->fan_rpmmin[nr];
		results[2] = data->fan_ripple[nr] & 0x03;
		results[3] = RPM_FROM_REG(data->fan_act[nr]);
		results[4] = RPM_FROM_REG(data->fan_min[nr]);
		results[5] = RPM_FROM_REG(data->fan_max[nr]);
		*nrels_mag = 6;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if(*nrels_mag >= 1) {
			data->fan_status[nr] = (data->fan_status[nr] & 0x0b) | (results[0] & 0x04); /* MKN */
			printk("fscscy: writing value 0x%02x to fan%d_status\n",
				data->fan_status[nr],nr);
			fscscy_write_value(client,reg_state,
				data->fan_status[nr]);
		}
		if(*nrels_mag >= 2)  {
			if((results[1] & 0xff) == 0) {
				 printk("fscscy: fan%d rpmmin 0 not allowed for safety reasons\n",nr);
				 return;
			}
			data->fan_rpmmin[nr] = results[1];
			printk("fscscy: writing value 0x%02x to fan%d_min\n",
				data->fan_rpmmin[nr],nr);
			fscscy_write_value(client,reg_min,
				data->fan_rpmmin[nr]);
		}
		if(*nrels_mag >= 3) {
			if((results[2] & 0x03) == 0) {
				printk("fscscy: fan%d ripple 0 is nonsense/not allowed\n",nr);
				return;
			}
			data->fan_ripple[nr] = results[2] & 0x03;
			printk("fscscy: writing value 0x%02x to fan%d_ripple\n",
				data->fan_ripple[nr],nr);
			fscscy_write_value(client,reg_ripple,
				data->fan_ripple[nr]);
		}	
	}
}

void fscscy_wdog(struct i2c_client *client, int operation, int ctl_name,
	     int *nrels_mag, long *results)
{
	struct fscscy_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		fscscy_update_client(client);
		results[0] = data->watchdog[0] ;
		results[1] = data->watchdog[1] & 0x02;
		results[2] = data->watchdog[2] & 0xb0;
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->watchdog[0] = results[0] & 0xff;
			printk("fscscy: writing value 0x%02x to wdog_preset\n",
				data->watchdog[0]); 
			fscscy_write_value(client,FSCSCY_REG_WDOG_PRESET,
				data->watchdog[0]);
		} 
		if (*nrels_mag >= 2) {
			data->watchdog[1] = results[1] & 0x02;
			printk("fscscy: writing value 0x%02x to wdog_state\n",
				data->watchdog[1]); 
			fscscy_write_value(client,FSCSCY_REG_WDOG_STATE,
				data->watchdog[1]);
		}
		if (*nrels_mag >= 3) {
			data->watchdog[2] = results[2] & 0xb0;
			printk("fscscy: writing value 0x%02x to wdog_control\n",
				data->watchdog[2]); 
			fscscy_write_value(client,FSCSCY_REG_WDOG_CONTROL,
				data->watchdog[2]);
		}
	}
}

void fscscy_pciload(struct i2c_client *client, int operation, int ctl_name,
	       int *nrels_mag, long *results)
{
	struct fscscy_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		fscscy_update_client(client);
		results[0] = data->pciload;
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
			printk("fscscy: writing PCILOAD to chip not supported\n");
	}
}

void fscscy_intrusion(struct i2c_client *client, int operation, int ctl_name,
	     int *nrels_mag, long *results)
{
	struct fscscy_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		fscscy_update_client(client);
		results[0] = data->intr_control & 0x80;
		results[1] = data->intr_status & 0xc0;
		*nrels_mag = 2;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->intr_control = results[0] & 0x80;
			printk("fscscy: writing value 0x%02x to intr_control\n",
				data->intr_control); 
			fscscy_write_value(client,FSCSCY_REG_INTR_CTRL,
				data->intr_control);
		} 
	}
}

static int __init sm_fscscy_init(void)
{
	printk("fscscy.o version %s (%s)\n", LM_VERSION, LM_DATE);
	return i2c_add_driver(&fscscy_driver);
}

static void __exit sm_fscscy_exit(void)
{
	i2c_del_driver(&fscscy_driver);
}



MODULE_AUTHOR
    ("Martin Knoblauch <mkn@teraport.de> based on work (fscpos) from  Hermann Jung <hej@odn.de>");
MODULE_DESCRIPTION("fujitsu siemens scylla chip driver");

module_init(sm_fscscy_init);
module_exit(sm_fscscy_exit);
