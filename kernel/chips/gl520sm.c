/*
    gl520sm.c - Part of lm_sensors, Linux kernel modules for hardware
                monitoring
    Copyright (c) 1998, 1999  Frodo Looijaard <frodol@dds.nl>,
                              Kyösti Mälkki <kmalkki@cc.hut.fi>

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
static unsigned short normal_i2c[] = { 0x2c, 0x2d, SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { SENSORS_I2C_END };
static unsigned int normal_isa[] = { SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/* Insmod parameters */
SENSORS_INSMOD_1(gl520sm);

/* Many GL520 constants specified below 
One of the inputs can be configured as either temp or voltage.
That's why _TEMP2 and _VIN4 access the same register 
*/

/* The GL520 registers */
#define GL520_REG_CHIP_ID 0x00
#define GL520_REG_REVISION 0x01
#define GL520_REG_VID 0x02
#define GL520_REG_CONF 0x03
#define GL520_REG_TEMP1 0x04
#define GL520_REG_TEMP1_OVER 0x05
#define GL520_REG_TEMP1_HYST 0x06
#define GL520_REG_FAN_COUNT 0x07
#define GL520_REG_FAN_LIMIT 0x08
#define GL520_REG_VIN1_LIMIT 0x09
#define GL520_REG_VIN2_LIMIT 0x0a
#define GL520_REG_VIN3_LIMIT 0x0b
#define GL520_REG_VDD_LIMIT 0x0c
#define GL520_REG_VIN3 0x0d
#define GL520_REG_VIN4 0x0e
#define GL520_REG_TEMP2 0x0e
#define GL520_REG_MISC 0x0f
#define GL520_REG_ALARM 0x10
#define GL520_REG_MASK 0x11
#define GL520_REG_INT 0x12
#define GL520_REG_VIN2 0x13
#define GL520_REG_VIN1 0x14
#define GL520_REG_VDD 0x15
#define GL520_REG_TEMP2_OVER 0x17
#define GL520_REG_VIN4_MAX 0x17
#define GL520_REG_TEMP2_HYST 0x18
#define GL520_REG_VIN4_MIN 0x18


/* Conversions. Rounding and limit checking is only done on the TO_REG
   variants. Note that you should be a bit careful with which arguments
   these macros are called: arguments may be evaluated more than once.
   Fixing this is just not worth it. */

#define TEMP_TO_REG(val) (SENSORS_LIMIT(((((val)<0?(val)-5:(val)+5) / 10)+130),\
                                        0,255))
#define TEMP_FROM_REG(val) (((val) - 130) * 10)

static inline u8 FAN_TO_REG(long rpm, int div)
{
	if (rpm == 0)
		return 255;
	rpm = SENSORS_LIMIT(rpm, 1, 1000000);
	return SENSORS_LIMIT((960000 + rpm * div / 2) / (rpm * div), 1,
			     254);
}

#define FAN_FROM_REG(val,div) \
 ( (val)==0 ? 0 : (val)==255 ? 0 : (960000/((val)*(div))) )

#define IN_TO_REG(val) (SENSORS_LIMIT((((val)*10+8)/19),0,255))
#define IN_FROM_REG(val) (((val)*19)/10)

#define VDD_TO_REG(val) (SENSORS_LIMIT((((val)*10+11)/23),0,255))
#define VDD_FROM_REG(val) (((val)*23)/10)

#define DIV_TO_REG(val) ((val)==8?3:(val)==4?2:(val)==1?0:1)
#define DIV_FROM_REG(val) (1 << (val))

#define ALARMS_FROM_REG(val) val

#define BEEP_ENABLE_TO_REG(val) ((val)?0:1)
#define BEEP_ENABLE_FROM_REG(val) ((val)?0:1)

#define BEEPS_TO_REG(val) (val)
#define BEEPS_FROM_REG(val) (val)

#define VID_FROM_REG(val) ((val)==0x1f?0:(val)>=0x10?510-(val)*10:\
                           205-(val)*5)

/* Each client has this additional data */
struct gl520_data {
	struct i2c_client client;
	int sysctl_id;
	enum chips type;

	struct semaphore update_lock;
	char valid;		/* !=0 if following fields are valid */
	unsigned long last_updated;	/* In jiffies */

	u8 voltage[5];		/* Register values; [0] = VDD */
	u8 voltage_min[5];	/* Register values; [0] = VDD */
	u8 voltage_max[5];	/* Register values; [0] = VDD */
	u8 fan[2];
	u8 fan_min[2];
	u8 temp[2];		/* Register values */
	u8 temp_over[2];	/* Register values */
	u8 temp_hyst[2];	/* Register values */
	u8 alarms, beeps, vid;	/* Register value */
	u8 alarm_mask;		/* Register value */
	u8 fan_div[2];		/* Register encoding, shifted right */
	u8 beep_enable;		/* Boolean */
	u8 two_temps;		/* Boolean */
};

static int gl520_attach_adapter(struct i2c_adapter *adapter);
static int gl520_detect(struct i2c_adapter *adapter, int address,
			unsigned short flags, int kind);
static void gl520_init_client(struct i2c_client *client);
static int gl520_detach_client(struct i2c_client *client);

static int gl520_read_value(struct i2c_client *client, u8 reg);
static int gl520_write_value(struct i2c_client *client, u8 reg, u16 value);
static void gl520_update_client(struct i2c_client *client);

static void gl520_vin(struct i2c_client *client, int operation,
		      int ctl_name, int *nrels_mag, long *results);
static void gl520_vid(struct i2c_client *client, int operation,
		      int ctl_name, int *nrels_mag, long *results);
static void gl520_fan(struct i2c_client *client, int operation,
		      int ctl_name, int *nrels_mag, long *results);
static void gl520_temp(struct i2c_client *client, int operation,
		       int ctl_name, int *nrels_mag, long *results);
static void gl520_fan_div(struct i2c_client *client, int operation,
			  int ctl_name, int *nrels_mag, long *results);
static void gl520_alarms(struct i2c_client *client, int operation,
			 int ctl_name, int *nrels_mag, long *results);
static void gl520_beep(struct i2c_client *client, int operation,
		       int ctl_name, int *nrels_mag, long *results);
static void gl520_fan1off(struct i2c_client *client, int operation,
			  int ctl_name, int *nrels_mag, long *results);
static void gl520_config(struct i2c_client *client, int operation,
			 int ctl_name, int *nrels_mag, long *results);

/* This is the driver that will be inserted */
static struct i2c_driver gl520_driver = {
	.name		= "GL520SM sensor chip driver",
	.id		= I2C_DRIVERID_GL520,
	.flags		= I2C_DF_NOTIFY,
	.attach_adapter	= gl520_attach_adapter,
	.detach_client	= gl520_detach_client,
};
/* -- SENSORS SYSCTL START -- */

#define GL520_SYSCTL_VDD  1000	/* Volts * 100 */
#define GL520_SYSCTL_VIN1 1001
#define GL520_SYSCTL_VIN2 1002
#define GL520_SYSCTL_VIN3 1003
#define GL520_SYSCTL_VIN4 1004
#define GL520_SYSCTL_FAN1 1101	/* RPM */
#define GL520_SYSCTL_FAN2 1102
#define GL520_SYSCTL_TEMP1 1200	/* Degrees Celcius * 10 */
#define GL520_SYSCTL_TEMP2 1201	/* Degrees Celcius * 10 */
#define GL520_SYSCTL_VID 1300
#define GL520_SYSCTL_FAN_DIV 2000	/* 1, 2, 4 or 8 */
#define GL520_SYSCTL_ALARMS 2001	/* bitvector */
#define GL520_SYSCTL_BEEP 2002	/* bitvector */
#define GL520_SYSCTL_FAN1OFF 2003
#define GL520_SYSCTL_CONFIG 2004

#define GL520_ALARM_VDD 0x01
#define GL520_ALARM_VIN1 0x02
#define GL520_ALARM_VIN2 0x04
#define GL520_ALARM_VIN3 0x08
#define GL520_ALARM_TEMP1 0x10
#define GL520_ALARM_FAN1 0x20
#define GL520_ALARM_FAN2 0x40
#define GL520_ALARM_TEMP2 0x80
#define GL520_ALARM_VIN4 0x80

/* -- SENSORS SYSCTL END -- */

/* These files are created for each detected GL520. This is just a template;
   though at first sight, you might think we could use a statically
   allocated list, we need some way to get back to the parent - which
   is done through one of the 'extra' fields which are initialized
   when a new copy is allocated. */
static ctl_table gl520_dir_table_template[] = {
	{GL520_SYSCTL_VIN1, "vin1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &gl520_vin},
	{GL520_SYSCTL_VIN2, "vin2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &gl520_vin},
	{GL520_SYSCTL_VIN3, "vin3", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &gl520_vin},
	{GL520_SYSCTL_VIN4, "vin4", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &gl520_vin},
	{GL520_SYSCTL_VDD, "vdd", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &gl520_vin},
	{GL520_SYSCTL_VID, "vid", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &gl520_vid},
	{GL520_SYSCTL_FAN1, "fan1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &gl520_fan},
	{GL520_SYSCTL_FAN2, "fan2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &gl520_fan},
	{GL520_SYSCTL_TEMP1, "temp1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &gl520_temp},
	{GL520_SYSCTL_TEMP2, "temp2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &gl520_temp},
	{GL520_SYSCTL_FAN_DIV, "fan_div", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &gl520_fan_div},
	{GL520_SYSCTL_ALARMS, "alarms", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &gl520_alarms},
	{GL520_SYSCTL_BEEP, "beep", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &gl520_beep},
	{GL520_SYSCTL_FAN1OFF, "fan1off", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &gl520_fan1off},
	{GL520_SYSCTL_CONFIG, "config", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &gl520_config},
	{0}
};

static int gl520_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_detect(adapter, &addr_data, gl520_detect);
}

static int gl520_detect(struct i2c_adapter *adapter, int address,
			unsigned short flags, int kind)
{
	int i;
	struct i2c_client *new_client;
	struct gl520_data *data;
	int err = 0;
	const char *type_name = "";
	char client_name[32];

	/* Make sure we aren't probing the ISA bus!! This is just a safety check
	   at this moment; i2c_detect really won't call us. */
#ifdef DEBUG
	if (i2c_is_isa_adapter(adapter)) {
		printk
		    ("gl520sm.o: gl520_detect called for an ISA bus adapter?!?\n");
		return 0;
	}
#endif

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA |
				     I2C_FUNC_SMBUS_WORD_DATA))
		    goto ERROR0;

	/* OK. For now, we presume we have a valid client. We now create the
	   client structure, even though we cannot fill it completely yet.
	   But it allows us to access gl520_{read,write}_value. */

	if (!(data = kmalloc(sizeof(struct gl520_data), GFP_KERNEL))) {
		err = -ENOMEM;
		goto ERROR0;
	}

	new_client = &data->client;
	new_client->addr = address;
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &gl520_driver;
	new_client->flags = 0;

	/* Determine the chip type. */

	if (gl520_read_value(new_client, GL520_REG_CHIP_ID) != 0x20) {
		printk
		    ("gl520sm.o: Ignoring 'force' parameter for unknown chip at "
		     "adapter %d, address 0x%02x\n",
		     i2c_adapter_id(adapter), address);
		goto ERROR1;
	} else {
		kind = gl520sm;
	}

	i = gl520_read_value(new_client, GL520_REG_REVISION);
	if (kind == gl520sm) {
		type_name = "gl520sm";
		sprintf(client_name, "GL520SM Revision %02x chip", i);
	} else {
#ifdef DEBUG
		printk("gl520sm.o: Internal error: unknown kind (%d)?!?",
		       kind);
#endif
		goto ERROR1;
	}

	/* Fill in the remaining client fields and put it into the global list */
	strcpy(new_client->name, client_name);
	data->type = kind;
	data->valid = 0;
	init_MUTEX(&data->update_lock);

	/* Tell the I2C layer a new client has arrived */
	if ((err = i2c_attach_client(new_client)))
		goto ERROR3;

	/* Register a new directory entry with module sensors */
	if ((i = i2c_register_entry(new_client,
					type_name,
					gl520_dir_table_template,
					THIS_MODULE)) < 0) {
		err = i;
		goto ERROR4;
	}
	data->sysctl_id = i;

	/* Initialize the GL520SM chip */
	data->alarm_mask = 0xff;
	gl520_init_client(new_client);
	if (data->two_temps)
		data->voltage_max[4] = data->voltage_min[4] =
			data->voltage[4] = 0;
	else
		data->temp_hyst[1] = data->temp_over[1] =
			data->temp[1] = 0;

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


/* Called when we have found a new GL520SM. */
static void gl520_init_client(struct i2c_client *client)
{
	struct gl520_data *data = (struct gl520_data *)(client->data);
	u8 oldconf, conf;

	conf = oldconf = gl520_read_value(client, GL520_REG_CONF);
	data->two_temps = !(conf & 0x10);

	/* If IRQ# is disabled, we can safely force comparator mode */
	if (!(conf & 0x20))
		conf &= 0xf7;

	/* Enable monitoring if needed */
	conf |= 0x40;

	if (conf != oldconf)
		gl520_write_value(client, GL520_REG_CONF, conf);
}

static int gl520_detach_client(struct i2c_client *client)
{
	int err;

	i2c_deregister_entry(((struct gl520_data *) (client->data))->
				 sysctl_id);

	if ((err = i2c_detach_client(client))) {
		printk
		    ("gl520sm.o: Client deregistration failed, client not detached.\n");
		return err;
	}

	kfree(client->data);

	return 0;
}


/* Registers 0x07 to 0x0c are word-sized, others are byte-sized 
   GL520 uses a high-byte first convention, which is exactly opposite to
   the usual practice. */
static int gl520_read_value(struct i2c_client *client, u8 reg)
{
	if ((reg >= 0x07) && (reg <= 0x0c))
		return swab16(i2c_smbus_read_word_data(client, reg));
	else
		return i2c_smbus_read_byte_data(client, reg);
}

/* Registers 0x07 to 0x0c are word-sized, others are byte-sized 
   GL520 uses a high-byte first convention, which is exactly opposite to
   the usual practice. */
static int gl520_write_value(struct i2c_client *client, u8 reg, u16 value)
{
	if ((reg >= 0x07) && (reg <= 0x0c))
		return i2c_smbus_write_word_data(client, reg, swab16(value));
	else
		return i2c_smbus_write_byte_data(client, reg, value);
}

static void gl520_update_client(struct i2c_client *client)
{
	struct gl520_data *data = client->data;
	int val;

	down(&data->update_lock);

	if ((jiffies - data->last_updated > HZ + HZ / 2) ||
	    (jiffies < data->last_updated) || !data->valid) {

#ifdef DEBUG
		printk("Starting gl520 update\n");
#endif

		data->alarms = gl520_read_value(client, GL520_REG_INT);
		data->beeps = gl520_read_value(client, GL520_REG_ALARM);
		data->vid = gl520_read_value(client, GL520_REG_VID) & 0x1f;

		val = gl520_read_value(client, GL520_REG_VDD_LIMIT);
		data->voltage_min[0] = val & 0xff;
		data->voltage_max[0] = (val >> 8) & 0xff;
		val = gl520_read_value(client, GL520_REG_VIN1_LIMIT);
		data->voltage_min[1] = val & 0xff;
		data->voltage_max[1] = (val >> 8) & 0xff;
		val = gl520_read_value(client, GL520_REG_VIN2_LIMIT);
		data->voltage_min[2] = val & 0xff;
		data->voltage_max[2] = (val >> 8) & 0xff;
		val = gl520_read_value(client, GL520_REG_VIN3_LIMIT);
		data->voltage_min[3] = val & 0xff;
		data->voltage_max[3] = (val >> 8) & 0xff;

		val = gl520_read_value(client, GL520_REG_FAN_COUNT);
		data->fan[0] = (val >> 8) & 0xff;
		data->fan[1] = val & 0xff;

		val = gl520_read_value(client, GL520_REG_FAN_LIMIT);
		data->fan_min[0] = (val >> 8) & 0xff;
		data->fan_min[1] = val & 0xff;

		data->temp[0] = gl520_read_value(client, GL520_REG_TEMP1);
		data->temp_over[0] =
		    gl520_read_value(client, GL520_REG_TEMP1_OVER);
		data->temp_hyst[0] =
		    gl520_read_value(client, GL520_REG_TEMP1_HYST);

		val = gl520_read_value(client, GL520_REG_MISC);
		data->fan_div[0] = (val >> 6) & 0x03;
		data->fan_div[1] = (val >> 4) & 0x03;

		data->alarms &= data->alarm_mask;

		val = gl520_read_value(client, GL520_REG_CONF);
		data->beep_enable = (val >> 2) & 1;

		data->voltage[0] = gl520_read_value(client, GL520_REG_VDD);
		data->voltage[1] =
		    gl520_read_value(client, GL520_REG_VIN1);
		data->voltage[2] =
		    gl520_read_value(client, GL520_REG_VIN2);
		data->voltage[3] =
		    gl520_read_value(client, GL520_REG_VIN3);

		/* Temp1 and Vin4 are the same input */
		if (data->two_temps) {
			data->temp[1] =
			    gl520_read_value(client, GL520_REG_TEMP2);
			data->temp_over[1] =
			    gl520_read_value(client, GL520_REG_TEMP2_OVER);
			data->temp_hyst[1] =
			    gl520_read_value(client, GL520_REG_TEMP2_HYST);
		} else {
			data->voltage[4] =
			    gl520_read_value(client, GL520_REG_VIN4);
			data->voltage_min[4] =
			    gl520_read_value(client, GL520_REG_VIN4_MIN);
			data->voltage_max[4] =
			    gl520_read_value(client, GL520_REG_VIN4_MAX);
		}

		data->last_updated = jiffies;
		data->valid = 1;
	}

	up(&data->update_lock);
}

void gl520_temp(struct i2c_client *client, int operation, int ctl_name,
		int *nrels_mag, long *results)
{
	struct gl520_data *data = client->data;
	int nr = ctl_name - GL520_SYSCTL_TEMP1;
	int regnr;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 1;
	else if (operation == SENSORS_PROC_REAL_READ) {
		gl520_update_client(client);
		results[0] = TEMP_FROM_REG(data->temp_over[nr]);
		results[1] = TEMP_FROM_REG(data->temp_hyst[nr]);
		results[2] = TEMP_FROM_REG(data->temp[nr]);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if ((nr == 1) && (!data->two_temps))
			return;
		regnr =
		    nr == 0 ? GL520_REG_TEMP1_OVER : GL520_REG_TEMP2_OVER;
		if (*nrels_mag >= 1) {
			data->temp_over[nr] = TEMP_TO_REG(results[0]);
			gl520_write_value(client, regnr,
					  data->temp_over[nr]);
		}
		regnr =
		    nr == 0 ? GL520_REG_TEMP1_HYST : GL520_REG_TEMP2_HYST;
		if (*nrels_mag >= 2) {
			data->temp_hyst[nr] = TEMP_TO_REG(results[1]);
			gl520_write_value(client, regnr,
					  data->temp_hyst[nr]);
		}
	}
}

void gl520_vin(struct i2c_client *client, int operation, int ctl_name,
	       int *nrels_mag, long *results)
{
	struct gl520_data *data = client->data;
	int nr = ctl_name - GL520_SYSCTL_VDD;
	int regnr, old = 0;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 2;
	else if (operation == SENSORS_PROC_REAL_READ) {
		gl520_update_client(client);
		results[0] = nr ? IN_FROM_REG(data->voltage_min[nr]) :
		    VDD_FROM_REG(data->voltage_min[nr]);
		results[1] = nr ? IN_FROM_REG(data->voltage_max[nr]) :
		    VDD_FROM_REG(data->voltage_max[nr]);
		results[2] = nr ? IN_FROM_REG(data->voltage[nr]) :
		    VDD_FROM_REG(data->voltage[nr]);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (nr != 4) {
			regnr =
			    nr == 0 ? GL520_REG_VDD_LIMIT : nr ==
			    1 ? GL520_REG_VIN1_LIMIT : nr ==
			    2 ? GL520_REG_VIN2_LIMIT :
			    GL520_REG_VIN3_LIMIT;
			if (*nrels_mag == 1)
				old =
				    gl520_read_value(client,
						     regnr) & 0xff00;
			if (*nrels_mag >= 2) {
				data->voltage_max[nr] =
				    nr ? IN_TO_REG(results[1]) :
				    VDD_TO_REG(results[1]);
				old = data->voltage_max[nr] << 8;
			}
			if (*nrels_mag >= 1) {
				data->voltage_min[nr] =
				    nr ? IN_TO_REG(results[0]) :
				    VDD_TO_REG(results[0]);
				old |= data->voltage_min[nr];
				gl520_write_value(client, regnr, old);
			}
		} else if (!data->two_temps) {
			if (*nrels_mag == 1)
				gl520_write_value(client,
						  GL520_REG_VIN4_MIN,
						  IN_TO_REG(results[0]));
			if (*nrels_mag >= 2)
				gl520_write_value(client,
						  GL520_REG_VIN4_MAX,
						  IN_TO_REG(results[1]));
		}
	}
}


void gl520_fan(struct i2c_client *client, int operation, int ctl_name,
	       int *nrels_mag, long *results)
{
	struct gl520_data *data = client->data;
	int nr = ctl_name - GL520_SYSCTL_FAN1;
	int old;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		gl520_update_client(client);
		results[0] = FAN_FROM_REG(data->fan_min[nr],
					  DIV_FROM_REG(data->fan_div[nr]));
		results[1] =
		    FAN_FROM_REG(data->fan[nr],
				 DIV_FROM_REG(data->fan_div[nr]));
		*nrels_mag = 2;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->fan_min[nr] = FAN_TO_REG(results[0],
						       DIV_FROM_REG(data->
								    fan_div
								    [nr]));
			old =
			    gl520_read_value(client, GL520_REG_FAN_LIMIT);

			if (nr == 0) {
				old =
				    (old & 0x00ff) | (data->
						      fan_min[nr] << 8);
				if (results[0] == 0)
					data->alarm_mask &= ~0x20;
				else
					data->alarm_mask |= 0x20;
			} else {
				old = (old & 0xff00) | data->fan_min[nr];
				if (results[0] == 0)
					data->alarm_mask &= ~0x40;
				else
					data->alarm_mask |= 0x40;
			}
			gl520_write_value(client, GL520_REG_FAN_LIMIT,
					  old);
		}
	}
}


void gl520_alarms(struct i2c_client *client, int operation, int ctl_name,
		  int *nrels_mag, long *results)
{
	struct gl520_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		gl520_update_client(client);
		results[0] = ALARMS_FROM_REG(data->alarms);
		*nrels_mag = 1;
	}
}

void gl520_beep(struct i2c_client *client, int operation, int ctl_name,
		int *nrels_mag, long *results)
{
	struct gl520_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		gl520_update_client(client);
		results[0] = BEEP_ENABLE_FROM_REG(data->beep_enable);
		results[1] = BEEPS_FROM_REG(data->beeps);
		*nrels_mag = 2;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->beep_enable = BEEP_ENABLE_TO_REG(results[0]);
			gl520_write_value(client, GL520_REG_CONF,
					  (gl520_read_value(client,
							    GL520_REG_CONF)
					   & 0xfb) | (data->
						      beep_enable << 2));
		}
		if (*nrels_mag >= 2) {
			data->beeps =
			    BEEPS_TO_REG(results[1]) & data->alarm_mask;
			gl520_write_value(client, GL520_REG_ALARM,
					  data->beeps);
		}
	}
}


void gl520_fan_div(struct i2c_client *client, int operation, int ctl_name,
		   int *nrels_mag, long *results)
{
	struct gl520_data *data = client->data;
	int old;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		gl520_update_client(client);
		results[0] = DIV_FROM_REG(data->fan_div[0]);
		results[1] = DIV_FROM_REG(data->fan_div[1]);
		*nrels_mag = 2;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		old = gl520_read_value(client, GL520_REG_MISC);
		if (*nrels_mag >= 2) {
			data->fan_div[1] = DIV_TO_REG(results[1]);
			old = (old & 0xcf) | (data->fan_div[1] << 4);
		}
		if (*nrels_mag >= 1) {
			data->fan_div[0] = DIV_TO_REG(results[0]);
			old = (old & 0x3f) | (data->fan_div[0] << 6);
		}
		gl520_write_value(client, GL520_REG_MISC, old);
	}
}

void gl520_vid(struct i2c_client *client, int operation, int ctl_name,
	       int *nrels_mag, long *results)
{
	struct gl520_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 2;
	else if (operation == SENSORS_PROC_REAL_READ) {
		gl520_update_client(client);
		results[0] = VID_FROM_REG(data->vid);
		*nrels_mag = 1;
	}
}

void gl520_fan1off(struct i2c_client *client, int operation, int ctl_name,
		   int *nrels_mag, long *results)
{
	int old;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		results[0] =
		    ((gl520_read_value(client, GL520_REG_MISC) & 0x04) !=
		     0);
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			old =
			    gl520_read_value(client,
					     GL520_REG_MISC) & 0xfb;
			if (results[0])
				old |= 0x04;
			gl520_write_value(client, GL520_REG_MISC, old);
		}
	}
}

void gl520_config(struct i2c_client *client, int operation, int ctl_name,
		  int *nrels_mag, long *results)
{
	struct gl520_data *data = client->data;
	int old;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		results[0] =
		    ((gl520_read_value(client, GL520_REG_CONF) & 0x10) ==
		     0);
		data->two_temps = results[0];
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			old =
			    gl520_read_value(client,
					     GL520_REG_CONF) & 0xef;
			if (!results[1]) {
				old |= 0x10;
				data->two_temps = 0;
				data->temp_hyst[1] = data->temp_over[1] =
					data->temp[1] = 0;
			} else {
				data->two_temps = 1;
				data->voltage_max[4] = data->voltage_min[4] =
					data->voltage[4] = 0;
			}
			gl520_write_value(client, GL520_REG_CONF, old);
		}
	}
}

static int __init sm_gl520sm_init(void)
{
	printk("gl520sm.o version %s (%s)\n", LM_VERSION, LM_DATE);
	return i2c_add_driver(&gl520_driver);
}

static void __exit sm_gl520sm_exit(void)
{
	i2c_del_driver(&gl520_driver);
}



MODULE_AUTHOR
    ("Frodo Looijaard <frodol@dds.nl> and Kyösti Mälkki <kmalkki@cc.hut.fi>");
MODULE_DESCRIPTION("GL520SM driver");

module_init(sm_gl520sm_init);
module_exit(sm_gl520sm_exit);
