/*
    asb100.c - Part of lm_sensors, Linux kernel modules for hardware
                monitoring

    Copyright (c) 2003 Mark M. Hoffman <mhoffman@lightlink.com>

	(derived from w83781d.c)

    Copyright (c) 1998 - 2003  Frodo Looijaard <frodol@dds.nl>,
    Philip Edelbrock <phil@netroedge.com>, and
    Mark Studebaker <mdsxyz123@yahoo.com>

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
    This driver supports the hardware sensor chips: Asus ASB100 and
    ASB100-A "BACH".

    ASB100-A supports pwm1, while plain ASB100 does not.  There is no known
    way for the driver to tell which one is there.

    Chip	#vin	#fanin	#pwm	#temp	wchipid	vendid	i2c	ISA
    asb100	7	3	1	4	0x31	0x0694	yes	no
*/

//#define DEBUG 1

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/i2c-proc.h>
#include <linux/init.h>
#include "version.h"
#include "sensors_vid.h"
#include "lm75.h"

#ifndef I2C_DRIVERID_ASB100
#define I2C_DRIVERID_ASB100		1043
#endif

/* I2C addresses to scan */
static unsigned short normal_i2c[] = { 0x2d, SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { SENSORS_I2C_END };

/* ISA addresses to scan (none) */
static unsigned int normal_isa[] = { SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/* default VRM to 9.0 instead of 8.2 */
#define ASB100_DEFAULT_VRM 90

/* Insmod parameters */
SENSORS_INSMOD_1(asb100);
SENSORS_MODULE_PARM(force_subclients, "List of subclient addresses: " \
	"{bus, clientaddr, subclientaddr1, subclientaddr2}");

/* Voltage IN registers 0-6 */
#define ASB100_REG_IN(nr)     (0x20 + (nr))
#define ASB100_REG_IN_MAX(nr) (0x2b + (nr * 2))
#define ASB100_REG_IN_MIN(nr) (0x2c + (nr * 2))

/* FAN IN registers 1-3 */
#define ASB100_REG_FAN(nr)     (0x27 + (nr))
#define ASB100_REG_FAN_MIN(nr) (0x3a + (nr))

/* TEMPERATURE registers 1-4 */
static const u16 asb100_reg_temp[]	= {0, 0x27, 0x150, 0x250, 0x17};
static const u16 asb100_reg_temp_max[]	= {0, 0x39, 0x155, 0x255, 0x18};
static const u16 asb100_reg_temp_hyst[]	= {0, 0x3a, 0x153, 0x253, 0x19};

#define ASB100_REG_TEMP(nr) (asb100_reg_temp[nr])
#define ASB100_REG_TEMP_MAX(nr) (asb100_reg_temp_max[nr])
#define ASB100_REG_TEMP_HYST(nr) (asb100_reg_temp_hyst[nr])

#define ASB100_REG_TEMP2_CONFIG	0x0152
#define ASB100_REG_TEMP3_CONFIG	0x0252


#define ASB100_REG_CONFIG	0x40
#define ASB100_REG_ALARM1	0x41
#define ASB100_REG_ALARM2	0x42
#define ASB100_REG_SMIM1	0x43
#define ASB100_REG_SMIM2	0x44
#define ASB100_REG_VID_FANDIV	0x47
#define ASB100_REG_I2C_ADDR	0x48
#define ASB100_REG_CHIPID	0x49
#define ASB100_REG_I2C_SUBADDR	0x4a
#define ASB100_REG_PIN		0x4b
#define ASB100_REG_IRQ		0x4c
#define ASB100_REG_BANK		0x4e
#define ASB100_REG_CHIPMAN	0x4f

#define ASB100_REG_WCHIPID	0x58

/* bit 7 -> enable, bits 0-3 -> duty cycle */
#define ASB100_REG_PWM1		0x59

/* CONVERSIONS
   Rounding and limit checking is only done on the TO_REG variants. */

/* These constants are a guess, consistent w/ w83781d */
#define ASB100_IN_MIN (  0)
#define ASB100_IN_MAX (408)

/* IN: 1/100 V (0V to 4.08V)
   REG: 16mV/bit */
static u8 IN_TO_REG(unsigned val)
{
	unsigned nval = SENSORS_LIMIT(val, ASB100_IN_MIN, ASB100_IN_MAX);
	return (nval * 10 + 8) / 16;
}

static unsigned IN_FROM_REG(u8 reg)
{
	return (reg * 16 + 5) / 10;
}

static u8 FAN_TO_REG(long rpm, int div)
{
	if (rpm == 0)
		return 255;
	rpm = SENSORS_LIMIT(rpm, 1, 1000000);
	return SENSORS_LIMIT((1350000 + rpm * div / 2) / (rpm * div), 1, 254);
}

static int FAN_FROM_REG(u8 val, int div)
{
	return val==0 ? -1 : val==255 ? 0 : 1350000/(val*div);
}

/* These constants are a guess, consistent w/ w83781d */
#define ASB100_TEMP_MIN (-1280)
#define ASB100_TEMP_MAX ( 1270)

/* TEMP: 1/10 degrees C (-128C to +127C)
   REG: 1C/bit, two's complement */
static u8 TEMP_TO_REG(int temp)
{
	int ntemp = SENSORS_LIMIT(temp, ASB100_TEMP_MIN, ASB100_TEMP_MAX);
	ntemp += (ntemp<0 ? -5 : 5);
	return (u8)(ntemp / 10);
}

static int TEMP_FROM_REG(u8 reg)
{
	return (s8)reg * 10;
}

/* PWM: 0 - 255 per sensors documentation
   REG: (6.25% duty cycle per bit) */
static u8 ASB100_PWM_TO_REG(int pwm)
{
	pwm = SENSORS_LIMIT(pwm, 0, 255);
	return (u8)(pwm / 16);
}

static int ASB100_PWM_FROM_REG(u8 reg)
{
	return reg * 16;
}

#define ALARMS_FROM_REG(val) (val)

#define DIV_FROM_REG(val) (1 << (val))

/* FAN DIV: 1, 2, 4, or 8 (defaults to 2)
   REG: 0, 1, 2, or 3 (respectively) (defaults to 1) */
static u8 DIV_TO_REG(long val)
{
	return val==8 ? 3 : val==4 ? 2 : val==1 ? 0 : 1;
}

/* For each registered client, we need to keep some data in memory. That
   data is pointed to by client->data. The structure itself is
   dynamically allocated, at the same time the client itself is allocated. */
struct asb100_data {
	struct i2c_client client;
	struct semaphore lock;
	int sysctl_id;
	enum chips type;

	struct semaphore update_lock;
	unsigned long last_updated;	/* In jiffies */

	/* array of 2 pointers to subclients */
	struct i2c_client *lm75[2];

	char valid;		/* !=0 if following fields are valid */
	u8 in[7];		/* Register value */
	u8 in_max[7];		/* Register value */
	u8 in_min[7];		/* Register value */
	u8 fan[3];		/* Register value */
	u8 fan_min[3];		/* Register value */
	u16 temp[4];		/* Register value (0 and 3 are u8 only) */
	u16 temp_max[4];	/* Register value (0 and 3 are u8 only) */
	u16 temp_hyst[4];	/* Register value (0 and 3 are u8 only) */
	u8 fan_div[3];		/* Register encoding, right justified */
	u8 pwm;			/* Register encoding */
	u8 vid;			/* Register encoding, combined */
	u32 alarms;		/* Register encoding, combined */
	u8 vrm;
};

static int asb100_attach_adapter(struct i2c_adapter *adapter);
static int asb100_detect(struct i2c_adapter *adapter, int address,
		unsigned short flags, int kind);
static int asb100_detach_client(struct i2c_client *client);

static int asb100_read_value(struct i2c_client *client, u16 reg);
static void asb100_write_value(struct i2c_client *client, u16 reg, u16 val);
static void asb100_update_client(struct i2c_client *client);
static void asb100_init_client(struct i2c_client *client);

static void asb100_in(struct i2c_client *client, int operation,
		int ctl_name, int *nrels_mag, long *results);
static void asb100_fan(struct i2c_client *client, int operation,
		int ctl_name, int *nrels_mag, long *results);
static void asb100_temp(struct i2c_client *client, int operation,
		int ctl_name, int *nrels_mag, long *results);
static void asb100_temp_add(struct i2c_client *client, int operation,
		int ctl_name, int *nrels_mag, long *results);
static void asb100_vid(struct i2c_client *client, int operation,
		int ctl_name, int *nrels_mag, long *results);
static void asb100_vrm(struct i2c_client *client, int operation,
		int ctl_name, int *nrels_mag, long *results);
static void asb100_alarms(struct i2c_client *client, int operation,
		int ctl_name, int *nrels_mag, long *results);
static void asb100_fan_div(struct i2c_client *client, int operation,
		int ctl_name, int *nrels_mag, long *results);
static void asb100_pwm(struct i2c_client *client, int operation,
		int ctl_name, int *nrels_mag, long *results);

static struct i2c_driver asb100_driver = {
	.name		= "asb100",
	.id		= I2C_DRIVERID_ASB100,
	.flags		= I2C_DF_NOTIFY,
	.attach_adapter	= asb100_attach_adapter,
	.detach_client	= asb100_detach_client,
};

/* The /proc/sys entries */
/* -- SENSORS SYSCTL START -- */

#define ASB100_SYSCTL_IN0	1000	/* Volts * 100 */
#define ASB100_SYSCTL_IN1	1001
#define ASB100_SYSCTL_IN2	1002
#define ASB100_SYSCTL_IN3	1003
#define ASB100_SYSCTL_IN4	1004
#define ASB100_SYSCTL_IN5	1005
#define ASB100_SYSCTL_IN6	1006

#define ASB100_SYSCTL_FAN1	1101	/* Rotations/min */
#define ASB100_SYSCTL_FAN2	1102
#define ASB100_SYSCTL_FAN3	1103

#define ASB100_SYSCTL_TEMP1	1200	/* Degrees Celcius * 10 */
#define ASB100_SYSCTL_TEMP2	1201
#define ASB100_SYSCTL_TEMP3	1202
#define ASB100_SYSCTL_TEMP4	1203

#define ASB100_SYSCTL_VID	1300	/* Volts * 1000 */
#define ASB100_SYSCTL_VRM	1301

#define ASB100_SYSCTL_PWM1	1401	/* 0-255 => 0-100% duty cycle */

#define ASB100_SYSCTL_FAN_DIV	2000	/* 1, 2, 4 or 8 */
#define ASB100_SYSCTL_ALARMS	2001	/* bitvector */

#define ASB100_ALARM_IN0	0x0001	/* ? */
#define ASB100_ALARM_IN1	0x0002	/* ? */
#define ASB100_ALARM_IN2	0x0004
#define ASB100_ALARM_IN3	0x0008
#define ASB100_ALARM_TEMP1	0x0010
#define ASB100_ALARM_TEMP2	0x0020
#define ASB100_ALARM_FAN1	0x0040
#define ASB100_ALARM_FAN2	0x0080
#define ASB100_ALARM_IN4	0x0100
#define ASB100_ALARM_IN5	0x0200	/* ? */
#define ASB100_ALARM_IN6	0x0400	/* ? */
#define ASB100_ALARM_FAN3	0x0800
#define ASB100_ALARM_CHAS	0x1000
#define ASB100_ALARM_TEMP3	0x2000

#define ASB100_ALARM_IN7	0x10000 /* ? */
#define ASB100_ALARM_IN8	0x20000	/* ? */

/* -- SENSORS SYSCTL END -- */

/* These files are created for each detected chip. This is just a template;
   though at first sight, you might think we could use a statically
   allocated list, we need some way to get back to the parent - which
   is done through one of the 'extra' fields which are initialized 
   when a new copy is allocated. */

/* no datasheet - but we did get some hints from someone who 
   claimed to have the datasheet */
#define ASB100_SYSCTL_IN(nr) {ASB100_SYSCTL_IN##nr, "in" #nr, NULL, 0, \
	0644, NULL, &i2c_proc_real, &i2c_sysctl_real, NULL, &asb100_in}
#define ASB100_SYSCTL_FAN(nr) {ASB100_SYSCTL_FAN##nr, "fan" #nr, NULL, 0, \
	0644, NULL, &i2c_proc_real, &i2c_sysctl_real, NULL, &asb100_fan}
#define ASB100_SYSCTL_TEMP(nr, func) {ASB100_SYSCTL_TEMP##nr, "temp" #nr, \
	NULL, 0, 0644, NULL, &i2c_proc_real, &i2c_sysctl_real, NULL, func}
static ctl_table asb100_dir_table_template[] = {
	ASB100_SYSCTL_IN(0),
	ASB100_SYSCTL_IN(1),
	ASB100_SYSCTL_IN(2),
	ASB100_SYSCTL_IN(3),
	ASB100_SYSCTL_IN(4),
	ASB100_SYSCTL_IN(5),
	ASB100_SYSCTL_IN(6),

	ASB100_SYSCTL_FAN(1),
	ASB100_SYSCTL_FAN(2),
	ASB100_SYSCTL_FAN(3),

	ASB100_SYSCTL_TEMP(1, &asb100_temp),
	ASB100_SYSCTL_TEMP(2, &asb100_temp_add),
	ASB100_SYSCTL_TEMP(3, &asb100_temp_add),
	ASB100_SYSCTL_TEMP(4, &asb100_temp),

	{ASB100_SYSCTL_VID, "vid", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &asb100_vid},
	{ASB100_SYSCTL_VRM, "vrm", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &asb100_vrm},
	{ASB100_SYSCTL_FAN_DIV, "fan_div", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &asb100_fan_div},
	{ASB100_SYSCTL_ALARMS, "alarms", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &asb100_alarms},
	{ASB100_SYSCTL_PWM1, "pwm1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &asb100_pwm},
	{0}
};

/* This function is called when:
	asb100_driver is inserted (when this module is loaded), for each
		available adapter
	when a new adapter is inserted (and asb100_driver is still present)
 */
static int asb100_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_detect(adapter, &addr_data, asb100_detect);
}

static int asb100_detect_subclients(struct i2c_adapter *adapter, int address,
		int kind, struct i2c_client *new_client)
{
	int i, id, err = 0;
	struct asb100_data *data = new_client->data;

	data->lm75[0] = kmalloc(sizeof(struct i2c_client), GFP_KERNEL);
	if (!(data->lm75[0])) {
		err = -ENOMEM;
		goto ERROR_SC_0;
	}
	memset(data->lm75[0], 0x00, sizeof(struct i2c_client));

	data->lm75[1] = kmalloc(sizeof(struct i2c_client), GFP_KERNEL);
	if (!(data->lm75[1])) {
		err = -ENOMEM;
		goto ERROR_SC_1;
	}
	memset(data->lm75[1], 0x00, sizeof(struct i2c_client));

	id = i2c_adapter_id(adapter);

	if (force_subclients[0] == id && force_subclients[1] == address) {
		for (i = 2; i <= 3; i++) {
			if (force_subclients[i] < 0x48 ||
				force_subclients[i] > 0x4f) {
				printk(KERN_ERR "asb100.o: invalid subclient "
					"address %d; must be 0x48-0x4f\n",
			        	force_subclients[i]);
				goto ERROR_SC_2;
			}
		}
		asb100_write_value(new_client, ASB100_REG_I2C_SUBADDR,
		                    (force_subclients[2] & 0x07) |
		                    ((force_subclients[3] & 0x07) <<4));
		data->lm75[0]->addr = force_subclients[2];
		data->lm75[1]->addr = force_subclients[3];
	} else {
		int val = asb100_read_value(new_client, ASB100_REG_I2C_SUBADDR);
		data->lm75[0]->addr = 0x48 + (val & 0x07);
		data->lm75[1]->addr = 0x48 + ((val >> 4) & 0x07);
	}

	if(data->lm75[0]->addr == data->lm75[1]->addr) {
		printk(KERN_ERR "asb100.o: duplicate addresses 0x%x "
				"for subclients\n", data->lm75[0]->addr);
		goto ERROR_SC_2;
	}

	for (i = 0; i <= 1; i++) {
		data->lm75[i]->data = NULL;
		data->lm75[i]->adapter = adapter;
		data->lm75[i]->driver = &asb100_driver;
		data->lm75[i]->flags = 0;
		strcpy(data->lm75[i]->name, "asb100 subclient");
	}

	if ((err = i2c_attach_client(data->lm75[0]))) {
		printk(KERN_ERR "asb100.o: Subclient %d registration "
			"at address 0x%x failed.\n", i, data->lm75[0]->addr);
		goto ERROR_SC_2;
	}

	if ((err = i2c_attach_client(data->lm75[1]))) {
		printk(KERN_ERR "asb100.o: Subclient %d registration "
			"at address 0x%x failed.\n", i, data->lm75[1]->addr);
		goto ERROR_SC_3;
	}

	return 0;

/* Undo inits in case of errors */
ERROR_SC_3:
	i2c_detach_client(data->lm75[0]);
ERROR_SC_2:
	kfree(data->lm75[1]);
ERROR_SC_1:
	kfree(data->lm75[0]);
ERROR_SC_0:
	return err;
}

static int asb100_detect(struct i2c_adapter *adapter, int address,
		unsigned short flags, int kind)
{
	int err = 0;
	struct i2c_client *new_client;
	struct asb100_data *data;

	/* asb100 is SMBus only */
	if (i2c_is_isa_adapter(adapter)) {
		pr_debug("asb100.o: detect failed, "
				"cannot attach to legacy adapter!\n");
		goto ERROR0;
	}

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		pr_debug("asb100.o: detect failed, "
				"smbus byte data not supported!\n");
		goto ERROR0;
	}

	/* OK. For now, we presume we have a valid client. We now create the
	   client structure, even though we cannot fill it completely yet.
	   But it allows us to access asb100_{read,write}_value. */

	if (!(data = kmalloc(sizeof(struct asb100_data), GFP_KERNEL))) {
		pr_debug("asb100.o: detect failed, kmalloc failed!\n");
		err = -ENOMEM;
		goto ERROR0;
	}

	new_client = &data->client;
	new_client->addr = address;
	init_MUTEX(&data->lock);
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &asb100_driver;
	new_client->flags = 0;

	/* Now, we do the remaining detection. */

	/* The chip may be stuck in some other bank than bank 0. This may
	   make reading other information impossible. Specify a force=... or
	   force_*=... parameter, and the chip will be reset to the right
	   bank. */
	if (kind < 0) {

		int val1 = asb100_read_value(new_client, ASB100_REG_BANK);
		int val2 = asb100_read_value(new_client, ASB100_REG_CHIPMAN);

		/* If we're in bank 0 */
		if ( (!(val1 & 0x07)) &&
				/* Check for ASB100 ID (low byte) */
				( ((!(val1 & 0x80)) && (val2 != 0x94)) ||
				/* Check for ASB100 ID (high byte ) */
				((val1 & 0x80) && (val2 != 0x06)) ) ) {
			pr_debug("asb100.o: detect failed, "
					"bad chip id 0x%02x!\n", val2);
			goto ERROR1;
		}

	} /* kind < 0 */

	/* We have either had a force parameter, or we have already detected
	   Winbond. Put it now into bank 0 and Vendor ID High Byte */
	asb100_write_value(new_client, ASB100_REG_BANK,
		(asb100_read_value(new_client, ASB100_REG_BANK) & 0x78) | 0x80);

	/* Determine the chip type. */
	if (kind <= 0) {
		int val1 = asb100_read_value(new_client, ASB100_REG_WCHIPID);
		int val2 = asb100_read_value(new_client, ASB100_REG_CHIPMAN);

		if ((val1 == 0x31) && (val2 == 0x06))
			kind = asb100;
		else {
			if (kind == 0)
				printk (KERN_WARNING "asb100.o: Ignoring "
					"'force' parameter for unknown chip "
					"at adapter %d, address 0x%02x.\n",
					i2c_adapter_id(adapter), address);
			goto ERROR1;
		}
	}

	/* Fill in remaining client fields and put it into the global list */
	strcpy(new_client->name, "ASB100 chip");
	data->type = kind;

	data->valid = 0;
	init_MUTEX(&data->update_lock);

	/* Tell the I2C layer a new client has arrived */
	if ((err = i2c_attach_client(new_client)))
		goto ERROR1;

	/* Attach secondary lm75 clients */
	if ((err = asb100_detect_subclients(adapter, address, kind,
			new_client)))
		goto ERROR2;

	/* Initialize the chip */
	asb100_init_client(new_client);

	/* Register a new directory entry with module sensors */
	if ((data->sysctl_id = i2c_register_entry(new_client, "asb100",
			asb100_dir_table_template, THIS_MODULE)) < 0) {
		err = data->sysctl_id;
		goto ERROR3;
	}

	return 0;

ERROR3:
	i2c_detach_client(data->lm75[0]);
	kfree(data->lm75[1]);
	kfree(data->lm75[0]);
ERROR2:
	i2c_detach_client(new_client);
ERROR1:
	kfree(data);
ERROR0:
	return err;
}

static int asb100_detach_client(struct i2c_client *client)
{
	int err;
	struct asb100_data *data = client->data;

	/* remove sysctl table (primary client only) */
	if ((data))
		i2c_deregister_entry(data->sysctl_id);

	if ((err = i2c_detach_client(client))) {
		printk (KERN_ERR "asb100.o: Client deregistration failed; "
			"client not detached.\n");
		return err;
	}

	if (data) {
		/* primary client */
		kfree(data);
	} else {
		/* subclients */
		kfree(client);
	}

	return 0;
}

/* The SMBus locks itself, usually, but nothing may access the Winbond between
   bank switches. ISA access must always be locked explicitly! 
   We ignore the W83781D BUSY flag at this moment - it could lead to deadlocks,
   would slow down the W83781D access and should not be necessary. 
   There are some ugly typecasts here, but the good news is - they should
   nowhere else be necessary! */
static int asb100_read_value(struct i2c_client *client, u16 reg)
{
	struct asb100_data *data = client->data;
	struct i2c_client *cl;
	int res, bank;

	down(&data->lock);

	bank = (reg >> 8) & 0x0f;
	if (bank > 2)
		/* switch banks */
		i2c_smbus_write_byte_data(client, ASB100_REG_BANK, bank);

	if (bank == 0 || bank > 2) {
		res = i2c_smbus_read_byte_data(client, reg & 0xff);
	} else {
		/* switch to subclient */
		cl = data->lm75[bank - 1];

		/* convert from ISA to LM75 I2C addresses */
		switch (reg & 0xff) {
		case 0x50: /* TEMP */
			res = swab16(i2c_smbus_read_word_data (cl, 0));
			break;
		case 0x52: /* CONFIG */
			res = i2c_smbus_read_byte_data(cl, 1);
			break;
		case 0x53: /* HYST */
			res = swab16(i2c_smbus_read_word_data (cl, 2));
			break;
		case 0x55: /* MAX */
		default:
			res = swab16(i2c_smbus_read_word_data (cl, 3));
			break;
		}
	}

	if (bank > 2)
		i2c_smbus_write_byte_data(client, ASB100_REG_BANK, 0);

	up(&data->lock);

	return res;
}

static void asb100_write_value(struct i2c_client *client, u16 reg, u16 value)
{
	struct asb100_data *data = client->data;
	struct i2c_client *cl;
	int bank;

	down(&data->lock);

	bank = (reg >> 8) & 0x0f;
	if (bank > 2)
		/* switch banks */
		i2c_smbus_write_byte_data(client, ASB100_REG_BANK, bank);

	if (bank == 0 || bank > 2) {
		i2c_smbus_write_byte_data(client, reg & 0xff, value & 0xff);
	} else {
		/* switch to subclient */
		cl = data->lm75[bank - 1];

		/* convert from ISA to LM75 I2C addresses */
		switch (reg & 0xff) {
		case 0x52: /* CONFIG */
			i2c_smbus_write_byte_data(cl, 1, value & 0xff);
			break;
		case 0x53: /* HYST */
			i2c_smbus_write_word_data(cl, 2, swab16(value));
			break;
		case 0x55: /* MAX */
			i2c_smbus_write_word_data(cl, 3, swab16(value));
			break;
		}
	}

	if (bank > 2)
		i2c_smbus_write_byte_data(client, ASB100_REG_BANK, 0);

	up(&data->lock);
}

static void asb100_init_client(struct i2c_client *client)
{
	struct asb100_data *data = client->data;
	int vid = 0;

	vid = asb100_read_value(client, ASB100_REG_VID_FANDIV) & 0x0f;
	vid |= (asb100_read_value(client, ASB100_REG_CHIPID) & 0x01) << 4;
	data->vrm = ASB100_DEFAULT_VRM;
	vid = vid_from_reg(vid, data->vrm);

	/* Start monitoring */
	asb100_write_value(client, ASB100_REG_CONFIG, 
		(asb100_read_value(client, ASB100_REG_CONFIG) & 0xf7) | 0x01);
}

static void asb100_update_client(struct i2c_client *client)
{
	struct asb100_data *data = client->data;
	int i;

	down(&data->update_lock);

	if (time_after(jiffies - data->last_updated, HZ + HZ / 2) ||
		time_before(jiffies, data->last_updated) || !data->valid) {

		pr_debug("asb100.o: starting device update...\n");

		/* 7 voltage inputs */
		for (i = 0; i < 7; i++) {
			data->in[i] = asb100_read_value(client,
				ASB100_REG_IN(i));
			data->in_min[i] = asb100_read_value(client,
				ASB100_REG_IN_MIN(i));
			data->in_max[i] = asb100_read_value(client,
				ASB100_REG_IN_MAX(i));
		}

		/* 3 fan inputs */
		for (i = 1; i <= 3; i++) {
			data->fan[i-1] = asb100_read_value(client,
					ASB100_REG_FAN(i));
			data->fan_min[i-1] = asb100_read_value(client,
					ASB100_REG_FAN_MIN(i));
		}

		/* 4 temperature inputs */
		for (i = 1; i <= 4; i++) {
			data->temp[i-1] = asb100_read_value(client,
					ASB100_REG_TEMP(i));
			data->temp_max[i-1] = asb100_read_value(client,
					ASB100_REG_TEMP_MAX(i));
			data->temp_hyst[i-1] = asb100_read_value(client,
					ASB100_REG_TEMP_HYST(i));
		}

		/* VID and fan divisors */
		i = asb100_read_value(client, ASB100_REG_VID_FANDIV);
		data->vid = i & 0x0f;
		data->vid |= (asb100_read_value(client,
				ASB100_REG_CHIPID) & 0x01) << 4;
		data->fan_div[0] = (i >> 4) & 0x03;
		data->fan_div[1] = (i >> 6) & 0x03;
		data->fan_div[2] = (asb100_read_value(client,
				ASB100_REG_PIN) >> 6) & 0x03;

		/* PWM */
		data->pwm = asb100_read_value(client, ASB100_REG_PWM1);

		/* alarms */
		data->alarms = asb100_read_value(client, ASB100_REG_ALARM1) +
			(asb100_read_value(client, ASB100_REG_ALARM2) << 8);

		data->last_updated = jiffies;
		data->valid = 1;

		pr_debug("asb100.o: ... update complete.\n");
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
static void asb100_in(struct i2c_client *client, int operation, int ctl_name,
               int *nrels_mag, long *results)
{
	struct asb100_data *data = client->data;
	int nr = ctl_name - ASB100_SYSCTL_IN0;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 2;
	else if (operation == SENSORS_PROC_REAL_READ) {
		asb100_update_client(client);
		results[0] = IN_FROM_REG(data->in_min[nr]);
		results[1] = IN_FROM_REG(data->in_max[nr]);
		results[2] = IN_FROM_REG(data->in[nr]);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->in_min[nr] = IN_TO_REG(results[0]);
			asb100_write_value(client, ASB100_REG_IN_MIN(nr),
					    data->in_min[nr]);
		}
		if (*nrels_mag >= 2) {
			data->in_max[nr] = IN_TO_REG(results[1]);
			asb100_write_value(client, ASB100_REG_IN_MAX(nr),
					    data->in_max[nr]);
		}
	}
}

void asb100_fan(struct i2c_client *client, int operation, int ctl_name,
		 int *nrels_mag, long *results)
{
	struct asb100_data *data = client->data;
	int nr = ctl_name - ASB100_SYSCTL_FAN1 + 1;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		asb100_update_client(client);
		results[0] = FAN_FROM_REG(data->fan_min[nr - 1],
				  DIV_FROM_REG(data->fan_div[nr - 1]));
		results[1] = FAN_FROM_REG(data->fan[nr - 1],
			          DIV_FROM_REG(data->fan_div[nr - 1]));
		*nrels_mag = 2;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->fan_min[nr - 1] =
			     FAN_TO_REG(results[0],
			            DIV_FROM_REG(data->fan_div[nr-1]));
			asb100_write_value(client,
					    ASB100_REG_FAN_MIN(nr),
					    data->fan_min[nr - 1]);
		}
	}
}

void asb100_temp(struct i2c_client *client, int operation, int ctl_name,
		  int *nrels_mag, long *results)
{
	struct asb100_data *data = client->data;
	int nr = ctl_name - ASB100_SYSCTL_TEMP1;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 1;

	else if (operation == SENSORS_PROC_REAL_READ) {
		asb100_update_client(client);
		results[0] = TEMP_FROM_REG(data->temp_max[nr]);
		results[1] = TEMP_FROM_REG(data->temp_hyst[nr]);
		results[2] = TEMP_FROM_REG(data->temp[nr]);
		*nrels_mag = 3;

	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->temp_max[nr] = TEMP_TO_REG(results[0]);
			asb100_write_value(client, ASB100_REG_TEMP_MAX(nr+1),
				data->temp_max[nr]);
		}
		if (*nrels_mag >= 2) {
			data->temp_hyst[nr] = TEMP_TO_REG(results[1]);
			asb100_write_value(client, ASB100_REG_TEMP_HYST(nr+1),
				data->temp_hyst[nr]);
		}
	}
}

void asb100_temp_add(struct i2c_client *client, int operation,
		      int ctl_name, int *nrels_mag, long *results)
{
	struct asb100_data *data = client->data;
	int nr = ctl_name - ASB100_SYSCTL_TEMP1;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 1;

	else if (operation == SENSORS_PROC_REAL_READ) {
		asb100_update_client(client);

		results[0] = LM75_TEMP_FROM_REG(data->temp_max[nr]);
		results[1] = LM75_TEMP_FROM_REG(data->temp_hyst[nr]);
		results[2] = LM75_TEMP_FROM_REG(data->temp[nr]);
		*nrels_mag = 3;

	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->temp_max[nr] =
				LM75_TEMP_TO_REG(results[0]);
			asb100_write_value(client, ASB100_REG_TEMP_MAX(nr+1),
				data->temp_max[nr]);
		}
		if (*nrels_mag >= 2) {
			data->temp_hyst[nr] =
				LM75_TEMP_TO_REG(results[1]);
			asb100_write_value(client, ASB100_REG_TEMP_HYST(nr+1),
				data->temp_hyst[nr]);
		}
	}
}

void asb100_vid(struct i2c_client *client, int operation, int ctl_name,
		 int *nrels_mag, long *results)
{
	struct asb100_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 3;
	else if (operation == SENSORS_PROC_REAL_READ) {
		asb100_update_client(client);
		results[0] = vid_from_reg(data->vid, data->vrm);
		*nrels_mag = 1;
	}
}

void asb100_vrm(struct i2c_client *client, int operation, int ctl_name,
		 int *nrels_mag, long *results)
{
	struct asb100_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 1;
	else if (operation == SENSORS_PROC_REAL_READ) {
		results[0] = data->vrm;
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1)
			data->vrm = results[0];
	}
}

void asb100_alarms(struct i2c_client *client, int operation, int ctl_name,
		    int *nrels_mag, long *results)
{
	struct asb100_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		asb100_update_client(client);
		results[0] = ALARMS_FROM_REG(data->alarms);
		*nrels_mag = 1;
	}
}

void asb100_fan_div(struct i2c_client *client, int operation,
		     int ctl_name, int *nrels_mag, long *results)
{
	struct asb100_data *data = client->data;
	int old, old2;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;

	else if (operation == SENSORS_PROC_REAL_READ) {
		asb100_update_client(client);
		results[0] = DIV_FROM_REG(data->fan_div[0]);
		results[1] = DIV_FROM_REG(data->fan_div[1]);
		results[2] = DIV_FROM_REG(data->fan_div[2]);
		*nrels_mag = 3;

	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		old = asb100_read_value(client, ASB100_REG_VID_FANDIV);
		if (*nrels_mag >= 3) {
			data->fan_div[2] = DIV_TO_REG(results[2]);
			old2 = asb100_read_value(client, ASB100_REG_PIN);
			old2 = (old2 & 0x3f) | ((data->fan_div[2] & 0x03) << 6);
			asb100_write_value(client, ASB100_REG_PIN, old2);
		}
		if (*nrels_mag >= 2) {
			data->fan_div[1] = DIV_TO_REG(results[1]);
			old = (old & 0x3f) | ((data->fan_div[1] & 0x03) << 6);
		}
		if (*nrels_mag >= 1) {
			data->fan_div[0] = DIV_TO_REG(results[0]);
			old = (old & 0xcf) | ((data->fan_div[0] & 0x03) << 4);
			asb100_write_value(client, ASB100_REG_VID_FANDIV, old);
		}
	}
}

void asb100_pwm(struct i2c_client *client, int operation, int ctl_name,
		int *nrels_mag, long *results)
{
	struct asb100_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		asb100_update_client(client);
		results[0] = ASB100_PWM_FROM_REG(data->pwm & 0x0f);
		results[1] = (data->pwm & 0x80) ? 1 : 0;
		*nrels_mag = 2;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		u8 val = data->pwm;
		if (*nrels_mag >= 1) {
			val = 0x0f & ASB100_PWM_TO_REG(results[0]);
			if (*nrels_mag >= 2) {
				if (results[1])
					val |= 0x80;
				else
					val &= ~0x80;
			}
			asb100_write_value(client, ASB100_REG_PWM1, val);
		}
	}
}

static int __init asb100_init(void)
{
	printk(KERN_INFO "asb100.o version %s (%s)\n", LM_VERSION, LM_DATE);
	return i2c_add_driver(&asb100_driver);
}

static void __exit asb100_exit(void)
{
	i2c_del_driver(&asb100_driver);
}

MODULE_AUTHOR(	"Mark M. Hoffman <mhoffman@lightlink.com>, "
		"Frodo Looijaard <frodol@dds.nl>, "
		"Philip Edelbrock <phil@netroedge.com>, and"
		"Mark Studebaker <mdsxyz123@yahoo.com>");

MODULE_DESCRIPTION("ASB100 'Bach' driver");
MODULE_LICENSE("GPL");

module_init(asb100_init);
module_exit(asb100_exit);

