/*
    adm9240.c - Part of lm_sensors, Linux kernel modules for hardware
             monitoring
    Copyright (C) 1999  Frodo Looijaard <frodol@dds.nl>
    and Philip Edelbrock <phil@netroedge.com>

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

/* Supports ADM9240, DS1780, and LM81. See doc/chips/adm9240 for details */

/* 
	A couple notes about the ADM9240:

* It claims to be 'LM7x' register compatible.  This must be in reference
  to only the LM78, because it is missing stuff to emulate LM75's as well. 
  (like the Winbond W83781 does)
 
* This driver was written from rev. 0 of the PDF, but it seems well 
  written and complete (unlike the W83781 which is horrible and has
  supposidly gone through a few revisions.. rev 0 of that one must
  have been in crayon on construction paper...)
  
* All analog inputs can range from 0 to 2.5, eventhough some inputs are
  marked as being 5V, 12V, etc.  I don't have any real voltages going 
  into my prototype, so I'm not sure that things are computed right, 
  but at least the limits seem to be working OK.
  
* Another curiousity is that the fan_div seems to be read-only.  I.e.,
  any written value to it doesn't seem to make any difference.  The
  fan_div seems to be 'stuck' at 2 (which isn't a bad value in most cases).
  
  
  --Phil

*/

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/i2c-proc.h>
#include <linux/init.h>
#include "version.h"

/* Addresses to scan */
static unsigned short normal_i2c[] = { SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { 0x2c, 0x2f, SENSORS_I2C_END };
static unsigned int normal_isa[] = { SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/* Insmod parameters */
SENSORS_INSMOD_3(adm9240, ds1780, lm81);

/* Many ADM9240 constants specified below */

#define ADM9240_REG_IN_MAX(nr) (0x2b + (nr) * 2)
#define ADM9240_REG_IN_MIN(nr) (0x2c + (nr) * 2)
#define ADM9240_REG_IN(nr) (0x20 + (nr))

/* The ADM9240 registers */
#define ADM9240_REG_TEST 0x15
#define ADM9240_REG_ANALOG_OUT 0x19
/* These are all read-only */
#define ADM9240_REG_2_5V 0x20
#define ADM9240_REG_VCCP1 0x21
#define ADM9240_REG_3_3V 0x22
#define ADM9240_REG_5V 0x23
#define ADM9240_REG_12V 0x24
#define ADM9240_REG_VCCP2 0x25
#define ADM9240_REG_TEMP 0x27
#define ADM9240_REG_FAN1 0x28
#define ADM9240_REG_FAN2 0x29
#define ADM9240_REG_COMPANY_ID 0x3E	/* 0x23 for ADM9240; 0xDA for DS1780 */
				     /* 0x01 for LM81 */
#define ADM9240_REG_DIE_REV 0x3F
/* These are read/write */
#define ADM9240_REG_2_5V_HIGH 0x2B
#define ADM9240_REG_2_5V_LOW 0x2C
#define ADM9240_REG_VCCP1_HIGH 0x2D
#define ADM9240_REG_VCCP1_LOW 0x2E
#define ADM9240_REG_3_3V_HIGH 0x2F
#define ADM9240_REG_3_3V_LOW 0x30
#define ADM9240_REG_5V_HIGH 0x31
#define ADM9240_REG_5V_LOW 0x32
#define ADM9240_REG_12V_HIGH 0x33
#define ADM9240_REG_12V_LOW 0x34
#define ADM9240_REG_VCCP2_HIGH 0x35
#define ADM9240_REG_VCCP2_LOW 0x36
#define ADM9240_REG_TCRIT_LIMIT 0x37	/* LM81 only - not supported */
#define ADM9240_REG_LOW_LIMIT 0x38	/* LM81 only - not supported */
#define ADM9240_REG_TOS 0x39
#define ADM9240_REG_THYST 0x3A
#define ADM9240_REG_FAN1_MIN 0x3B
#define ADM9240_REG_FAN2_MIN 0x3C

#define ADM9240_REG_CONFIG 0x40
#define ADM9240_REG_INT1_STAT 0x41
#define ADM9240_REG_INT2_STAT 0x42
#define ADM9240_REG_INT1_MASK 0x43
#define ADM9240_REG_INT2_MASK 0x44

#define ADM9240_REG_COMPAT 0x45	/* dummy compat. register for other drivers? */
#define ADM9240_REG_CHASSIS_CLEAR 0x46
#define ADM9240_REG_VID_FAN_DIV 0x47
#define ADM9240_REG_I2C_ADDR 0x48
#define ADM9240_REG_VID4 0x49
#define ADM9240_REG_TEMP_CONFIG 0x4B
#define ADM9240_REG_EXTMODE1 0x4C	/* LM81 only - not supported */
#define ADM9240_REG_EXTMODE2 0x4D	/* LM81 only - not supported */

/* Conversions. Rounding and limit checking is only done on the TO_REG
   variants. Note that you should be a bit careful with which arguments
   these macros are called: arguments may be evaluated more than once.
   Fixing this is just not worth it. */
#define IN_TO_REG(val,nr) (SENSORS_LIMIT((val), 0, 255))
#define IN_FROM_REG(val,nr) (val)

static inline u8 FAN_TO_REG(long rpm, int div)
{
	if (rpm == 0)
		return 255;
	rpm = SENSORS_LIMIT(rpm, 1, 1000000);
	return SENSORS_LIMIT((1350000 + rpm * div / 2) / (rpm * div), 1,
			     254);
}

#define FAN_FROM_REG(val,div) ((val)==0?-1:\
                               (val)==255?0:1350000/((div)*(val)))

#define TEMP_FROM_REG(temp) ((temp)<256 ? (temp) * 5 : \
                             ((temp) - 512) * 5)

#define TEMP_LIMIT_FROM_REG(val) (((val)>=0x80?(val)-0x100:(val))*10)

#define TEMP_LIMIT_TO_REG(val) SENSORS_LIMIT(((val)<0?(((val)-5)/10)+256:\
                                                      ((val)+5)/10), \
                                             0,255)

#define ALARMS_FROM_REG(val) (val)

#define DIV_FROM_REG(val) (1 << (val))
#define DIV_TO_REG(val) ((val)==1?0:((val)==8?3:((val)==4?2:1)))

#define VID_FROM_REG(val) ((val)==0x1f?0:(val)>=0x10?510-(val)*10:\
                           205-(val)*5)

/* For each registered ADM9240, we need to keep some data in memory. */
struct adm9240_data {
	struct i2c_client client;
	int sysctl_id;
	enum chips type;

	struct semaphore update_lock;
	char valid;		/* !=0 if following fields are valid */
	unsigned long last_updated;	/* In jiffies */

	u8 in[6];		/* Register value */
	u8 in_max[6];		/* Register value */
	u8 in_min[6];		/* Register value */
	u8 fan[2];		/* Register value */
	u8 fan_min[2];		/* Register value */
	u8 fan_div[2];		/* Register encoding, shifted right */
	int temp;		/* Temp, shifted right */
	u8 temp_os_max;		/* Register value */
	u8 temp_os_hyst;	/* Register value */
	u16 alarms;		/* Register encoding, combined */
	u8 analog_out;		/* Register value */
	u8 vid;			/* Register value combined */
};


static int adm9240_attach_adapter(struct i2c_adapter *adapter);
static int adm9240_detect(struct i2c_adapter *adapter, int address,
			  unsigned short flags, int kind);
static int adm9240_detach_client(struct i2c_client *client);

static int adm9240_read_value(struct i2c_client *client, u8 reg);
static int adm9240_write_value(struct i2c_client *client, u8 reg, u8 value);
static void adm9240_update_client(struct i2c_client *client);
static void adm9240_init_client(struct i2c_client *client);


static void adm9240_in(struct i2c_client *client, int operation,
		       int ctl_name, int *nrels_mag, long *results);
static void adm9240_fan(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void adm9240_temp(struct i2c_client *client, int operation,
			 int ctl_name, int *nrels_mag, long *results);
static void adm9240_alarms(struct i2c_client *client, int operation,
			   int ctl_name, int *nrels_mag, long *results);
static void adm9240_fan_div(struct i2c_client *client, int operation,
			    int ctl_name, int *nrels_mag, long *results);
static void adm9240_analog_out(struct i2c_client *client, int operation,
			       int ctl_name, int *nrels_mag,
			       long *results);
static void adm9240_vid(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);

static struct i2c_driver adm9240_driver = {
	.name		= "ADM9240 sensor driver",
	.id		= I2C_DRIVERID_ADM9240,
	.flags		= I2C_DF_NOTIFY,
	.attach_adapter	= adm9240_attach_adapter,
	.detach_client	= adm9240_detach_client,
};

/* The /proc/sys entries */

/* -- SENSORS SYSCTL START -- */

#define ADM9240_SYSCTL_IN0 1000	/* Volts * 100 */
#define ADM9240_SYSCTL_IN1 1001
#define ADM9240_SYSCTL_IN2 1002
#define ADM9240_SYSCTL_IN3 1003
#define ADM9240_SYSCTL_IN4 1004
#define ADM9240_SYSCTL_IN5 1005
#define ADM9240_SYSCTL_FAN1 1101	/* Rotations/min */
#define ADM9240_SYSCTL_FAN2 1102
#define ADM9240_SYSCTL_TEMP 1250	/* Degrees Celcius * 100 */
#define ADM9240_SYSCTL_FAN_DIV 2000	/* 1, 2, 4 or 8 */
#define ADM9240_SYSCTL_ALARMS 2001	/* bitvector */
#define ADM9240_SYSCTL_ANALOG_OUT 2002
#define ADM9240_SYSCTL_VID 2003

#define ADM9240_ALARM_IN0 0x0001
#define ADM9240_ALARM_IN1 0x0002
#define ADM9240_ALARM_IN2 0x0004
#define ADM9240_ALARM_IN3 0x0008
#define ADM9240_ALARM_IN4 0x0100
#define ADM9240_ALARM_IN5 0x0200
#define ADM9240_ALARM_FAN1 0x0040
#define ADM9240_ALARM_FAN2 0x0080
#define ADM9240_ALARM_TEMP 0x0010
#define ADM9240_ALARM_CHAS 0x1000

/* -- SENSORS SYSCTL END -- */

/* These files are created for each detected ADM9240. This is just a template;
   though at first sight, you might think we could use a statically
   allocated list, we need some way to get back to the parent - which
   is done through one of the 'extra' fields which are initialized 
   when a new copy is allocated. */
static ctl_table adm9240_dir_table_template[] = {
	{ADM9240_SYSCTL_IN0, "in0", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm9240_in},
	{ADM9240_SYSCTL_IN1, "in1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm9240_in},
	{ADM9240_SYSCTL_IN2, "in2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm9240_in},
	{ADM9240_SYSCTL_IN3, "in3", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm9240_in},
	{ADM9240_SYSCTL_IN4, "in4", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm9240_in},
	{ADM9240_SYSCTL_IN5, "in5", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm9240_in},
	{ADM9240_SYSCTL_FAN1, "fan1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm9240_fan},
	{ADM9240_SYSCTL_FAN2, "fan2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm9240_fan},
	{ADM9240_SYSCTL_TEMP, "temp", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm9240_temp},
	{ADM9240_SYSCTL_FAN_DIV, "fan_div", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm9240_fan_div},
	{ADM9240_SYSCTL_ALARMS, "alarms", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm9240_alarms},
	{ADM9240_SYSCTL_ANALOG_OUT, "analog_out", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm9240_analog_out},
	{ADM9240_SYSCTL_VID, "vid", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm9240_vid},
	{0}
};

static int adm9240_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_detect(adapter, &addr_data, adm9240_detect);
}

static int adm9240_detect(struct i2c_adapter *adapter, int address,
			  unsigned short flags, int kind)
{
	int i;
	struct i2c_client *new_client;
	struct adm9240_data *data;
	int err = 0;
	const char *type_name = "";
	const char *client_name = "";

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		goto ERROR0;

	/* OK. For now, we presume we have a valid client. We now create the
	   client structure, even though we cannot fill it completely yet.
	   But it allows us to access adm9240_{read,write}_value. */

	if (!(data = kmalloc(sizeof(struct adm9240_data), GFP_KERNEL))) {
		err = -ENOMEM;
		goto ERROR0;
	}


	new_client = &data->client;
	new_client->addr = address;
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &adm9240_driver;
	new_client->flags = 0;

	/* Now, we do the remaining detection. */

	if (kind < 0) {
		if (
		    ((adm9240_read_value
		      (new_client, ADM9240_REG_CONFIG) & 0x80) != 0x00)
		    ||
		    (adm9240_read_value(new_client, ADM9240_REG_I2C_ADDR)
		     != address))
			goto ERROR1;
	}

	/* Determine the chip type. */
	if (kind <= 0) {
		i = adm9240_read_value(new_client, ADM9240_REG_COMPANY_ID);
		if (i == 0x23)
			kind = adm9240;
		else if (i == 0xda)
			kind = ds1780;
		else if (i == 0x01)
			kind = lm81;
		else {
			if (kind == 0)
				printk
				    ("adm9240.o: Ignoring 'force' parameter for unknown chip at "
				     "adapter %d, address 0x%02x\n",
				     i2c_adapter_id(adapter), address);
			goto ERROR1;
		}
	}

	if (kind == adm9240) {
		type_name = "adm9240";
		client_name = "ADM9240 chip";
	} else if (kind == ds1780) {
		type_name = "ds1780";
		client_name = "DS1780 chip";
	} else if (kind == lm81) {
		type_name = "lm81";
		client_name = "LM81 chip";
	} else {
#ifdef DEBUG
		printk("adm9240.o: Internal error: unknown kind (%d)?!?",
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
					adm9240_dir_table_template,
					THIS_MODULE)) < 0) {
		err = i;
		goto ERROR4;
	}
	data->sysctl_id = i;

	/* Initialize the ADM9240 chip */
	adm9240_init_client(new_client);
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

static int adm9240_detach_client(struct i2c_client *client)
{
	int err;

	i2c_deregister_entry(((struct adm9240_data *) (client->data))->
				 sysctl_id);

	if ((err = i2c_detach_client(client))) {
		printk
		    ("adm9240.o: Client deregistration failed, client not detached.\n");
		return err;
	}

	kfree(client->data);

	return 0;
}


static int adm9240_read_value(struct i2c_client *client, u8 reg)
{
	return i2c_smbus_read_byte_data(client, reg);
}

static int adm9240_write_value(struct i2c_client *client, u8 reg, u8 value)
{
	return i2c_smbus_write_byte_data(client, reg, value);
}

/* Called when we have found a new ADM9240. */
static void adm9240_init_client(struct i2c_client *client)
{
	/* Start monitoring */
	adm9240_write_value(client, ADM9240_REG_CONFIG, 0x01);
}

static void adm9240_update_client(struct i2c_client *client)
{
	struct adm9240_data *data = client->data;
	u8 i;

	down(&data->update_lock);

	if (
	    (jiffies - data->last_updated >
	     (data->type == adm9240 ? HZ / 2 : HZ * 2))
	    || (jiffies < data->last_updated) || !data->valid) {

#ifdef DEBUG
		printk("Starting adm9240 update\n");
#endif
		for (i = 0; i <= 5; i++) {
			data->in[i] =
			    adm9240_read_value(client, ADM9240_REG_IN(i));
			data->in_min[i] =
			    adm9240_read_value(client,
					       ADM9240_REG_IN_MIN(i));
			data->in_max[i] =
			    adm9240_read_value(client,
					       ADM9240_REG_IN_MAX(i));
		}
		data->fan[0] =
		    adm9240_read_value(client, ADM9240_REG_FAN1);
		data->fan_min[0] =
		    adm9240_read_value(client, ADM9240_REG_FAN1_MIN);
		data->fan[1] =
		    adm9240_read_value(client, ADM9240_REG_FAN2);
		data->fan_min[1] =
		    adm9240_read_value(client, ADM9240_REG_FAN2_MIN);
		data->temp =
		    (adm9240_read_value(client, ADM9240_REG_TEMP) << 1) +
		    ((adm9240_read_value
		      (client, ADM9240_REG_TEMP_CONFIG) & 0x80) >> 7);
		data->temp_os_max =
		    adm9240_read_value(client, ADM9240_REG_TOS);
		data->temp_os_hyst =
		    adm9240_read_value(client, ADM9240_REG_THYST);

		i = adm9240_read_value(client, ADM9240_REG_VID_FAN_DIV);
		data->fan_div[0] = (i >> 4) & 0x03;
		data->fan_div[1] = (i >> 6) & 0x03;
		data->vid = i & 0x0f;
		data->vid |=
		    (adm9240_read_value(client, ADM9240_REG_VID4) & 0x01)
		    << 4;

		data->alarms =
		    adm9240_read_value(client,
				       ADM9240_REG_INT1_STAT) +
		    (adm9240_read_value(client, ADM9240_REG_INT2_STAT) <<
		     8);
		data->analog_out =
		    adm9240_read_value(client, ADM9240_REG_ANALOG_OUT);
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
void adm9240_in(struct i2c_client *client, int operation, int ctl_name,
		int *nrels_mag, long *results)
{

	int scales[6] = { 250, 270, 330, 500, 1200, 270 };

	struct adm9240_data *data = client->data;
	int nr = ctl_name - ADM9240_SYSCTL_IN0;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 2;
	else if (operation == SENSORS_PROC_REAL_READ) {
		adm9240_update_client(client);
		results[0] =
		    IN_FROM_REG(data->in_min[nr], nr) * scales[nr] / 192;
		results[1] =
		    IN_FROM_REG(data->in_max[nr], nr) * scales[nr] / 192;
		results[2] =
		    IN_FROM_REG(data->in[nr], nr) * scales[nr] / 192;
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->in_min[nr] =
			    IN_TO_REG((results[0] * 192) / scales[nr], nr);
			adm9240_write_value(client, ADM9240_REG_IN_MIN(nr),
					    data->in_min[nr]);
		}
		if (*nrels_mag >= 2) {
			data->in_max[nr] =
			    IN_TO_REG((results[1] * 192) / scales[nr], nr);
			adm9240_write_value(client, ADM9240_REG_IN_MAX(nr),
					    data->in_max[nr]);
		}
	}
}

void adm9240_fan(struct i2c_client *client, int operation, int ctl_name,
		 int *nrels_mag, long *results)
{
	struct adm9240_data *data = client->data;
	int nr = ctl_name - ADM9240_SYSCTL_FAN1 + 1;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		adm9240_update_client(client);
		results[0] = FAN_FROM_REG(data->fan_min[nr - 1],
					  DIV_FROM_REG(data->
						       fan_div[nr - 1]));
		results[1] =
		    FAN_FROM_REG(data->fan[nr - 1],
				 DIV_FROM_REG(data->fan_div[nr - 1]));
		*nrels_mag = 2;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->fan_min[nr - 1] = FAN_TO_REG(results[0],
							   DIV_FROM_REG
							   (data->
							    fan_div[nr -
								    1]));
			adm9240_write_value(client,
					    nr ==
					    1 ? ADM9240_REG_FAN1_MIN :
					    ADM9240_REG_FAN2_MIN,
					    data->fan_min[nr - 1]);
		}
	}
}


void adm9240_temp(struct i2c_client *client, int operation, int ctl_name,
		  int *nrels_mag, long *results)
{
	struct adm9240_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 1;
	else if (operation == SENSORS_PROC_REAL_READ) {
		adm9240_update_client(client);
		results[0] = TEMP_LIMIT_FROM_REG(data->temp_os_max);
		results[1] = TEMP_LIMIT_FROM_REG(data->temp_os_hyst);
		results[2] = TEMP_FROM_REG(data->temp);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->temp_os_max = TEMP_LIMIT_TO_REG(results[0]);
			adm9240_write_value(client, ADM9240_REG_TOS,
					    data->temp_os_max);
		}
		if (*nrels_mag >= 2) {
			data->temp_os_hyst = TEMP_LIMIT_TO_REG(results[1]);
			adm9240_write_value(client, ADM9240_REG_THYST,
					    data->temp_os_hyst);
		}
	}
}

void adm9240_alarms(struct i2c_client *client, int operation, int ctl_name,
		    int *nrels_mag, long *results)
{
	struct adm9240_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		adm9240_update_client(client);
		results[0] = ALARMS_FROM_REG(data->alarms);
		*nrels_mag = 1;
	}
}

void adm9240_fan_div(struct i2c_client *client, int operation,
		     int ctl_name, int *nrels_mag, long *results)
{
	struct adm9240_data *data = client->data;
	int old;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		adm9240_update_client(client);
		results[0] = DIV_FROM_REG(data->fan_div[0]);
		results[1] = DIV_FROM_REG(data->fan_div[1]);
		*nrels_mag = 2;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		old = adm9240_read_value(client, ADM9240_REG_VID_FAN_DIV);
		if (*nrels_mag >= 2) {
			data->fan_div[1] = DIV_TO_REG(results[1]);
			old = (old & 0x3f) | (data->fan_div[1] << 6);
		}
		if (*nrels_mag >= 1) {
			data->fan_div[0] = DIV_TO_REG(results[0]);
			old = (old & 0xcf) | (data->fan_div[0] << 4);
			adm9240_write_value(client,
					    ADM9240_REG_VID_FAN_DIV, old);
		}
	}
}

void adm9240_analog_out(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results)
{
	struct adm9240_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		adm9240_update_client(client);
		results[0] = data->analog_out;
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->analog_out = results[0];
			adm9240_write_value(client, ADM9240_REG_ANALOG_OUT,
					    data->analog_out);
		}
	}
}

void adm9240_vid(struct i2c_client *client, int operation, int ctl_name,
		 int *nrels_mag, long *results)
{
	struct adm9240_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 2;
	else if (operation == SENSORS_PROC_REAL_READ) {
		adm9240_update_client(client);
		results[0] = VID_FROM_REG(data->vid);
		*nrels_mag = 1;
	}
}

static int __init sm_adm9240_init(void)
{
	printk("adm9240.o version %s (%s)\n", LM_VERSION, LM_DATE);
	return i2c_add_driver(&adm9240_driver);
}

static void __exit sm_adm9240_exit(void)
{
	i2c_del_driver(&adm9240_driver);
}



MODULE_AUTHOR
    ("Frodo Looijaard <frodol@dds.nl> and Philip Edelbrock <phil@netroedge.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ADM9240 driver");

module_init(sm_adm9240_init);
module_exit(sm_adm9240_exit);
