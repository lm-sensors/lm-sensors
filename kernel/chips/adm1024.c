/*
    adm1024.c - Part of lm_sensors, Linux kernel modules for hardware
             monitoring
    Add by Ken Bowley <ken@opnix.com> from the adm1025.c written by
    Gordon Wu <gwu@esoft.com> and from adm9240.c written by
    Copyright (c) 1999  Frodo Looijaard <frodol@dds.nl>
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

/* Supports the Analog Devices ADM1024. See doc/chips/adm1024 for details */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/i2c-proc.h>
#include <linux/init.h>
#include "version.h"

/* Addresses to scan */
static unsigned short normal_i2c[] = { SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { 0x2c, 0x2e, SENSORS_I2C_END };
static unsigned int normal_isa[] = { SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/* Insmod parameters */
SENSORS_INSMOD_1(adm1024);

/* Many ADM1024 constants specified below */

#define ADM1024_REG_IN_MAX(nr) (0x2b + (nr) * 2)
#define ADM1024_REG_IN_MIN(nr) (0x2c + (nr) * 2)
#define ADM1024_REG_IN(nr) (0x20 + (nr))

/* The ADM1024 registers */
#define ADM1024_REG_INT_TEMP_TRIP_SET 0x13
#define ADM1024_REG_EXT_TEMP_TRIP_SET 0x14
#define ADM1024_REG_TEST 0x15
#define ADM1024_REG_CHANNEL_MODE 0x16
#define ADM1024_REG_INT_TEMP_TRIP 0x17	/* read only */
#define ADM1024_REG_EXT_TEMP_TRIP 0x18	/* read only */
#define ADM1024_REG_ANALOG_OUT 0x19
#define ADM1024_REG_AIN1_LOW_LIMIT 0x1A
#define ADM1024_REG_AIN2_LOW_LIMIT 0x1B
/* These are all read-only */
#define ADM1024_REG_2_5V 0x20	/* 2.5V Measured Value/EXT Temp 2 */
#define ADM1024_REG_VCCP1 0x21
#define ADM1024_REG_3_3V 0x22	/* VCC Measured Value */
#define ADM1024_REG_5V 0x23
#define ADM1024_REG_12V 0x24
#define ADM1024_REG_VCCP2 0x25
#define ADM1024_REG_EXT_TEMP1 0x26
#define ADM1024_REG_TEMP 0x27
#define ADM1024_REG_FAN1 0x28	/* FAN1/AIN1 Value */
#define ADM1024_REG_FAN2 0x29	/* FAN2/AIN2 Value */
#define ADM1024_REG_COMPANY_ID 0x3E	/* 0x41 for ADM1024 */
#define ADM1024_REG_DIE_REV 0x3F
/* These are read/write */
#define ADM1024_REG_2_5V_HIGH 0x2B	/* 2.5V/Ext Temp2 High Limit */
#define ADM1024_REG_2_5V_LOW 0x2C	/* 2.5V/Ext Temp2 Low Limit */
#define ADM1024_REG_VCCP1_HIGH 0x2D
#define ADM1024_REG_VCCP1_LOW 0x2E
#define ADM1024_REG_3_3V_HIGH 0x2F	/* VCC High Limit */
#define ADM1024_REG_3_3V_LOW 0x30	/* VCC Low Limit */
#define ADM1024_REG_5V_HIGH 0x31
#define ADM1024_REG_5V_LOW 0x32
#define ADM1024_REG_12V_HIGH 0x33
#define ADM1024_REG_12V_LOW 0x34
#define ADM1024_REG_VCCP2_HIGH 0x35
#define ADM1024_REG_VCCP2_LOW 0x36
#define ADM1024_REG_EXT_TEMP1_HIGH 0x37
#define ADM1024_REG_EXT_TEMP1_LOW 0x38
#define ADM1024_REG_TOS 0x39
#define ADM1024_REG_THYST 0x3A
#define ADM1024_REG_FAN1_MIN 0x3B
#define ADM1024_REG_FAN2_MIN 0x3C

#define ADM1024_REG_CONFIG 0x40
#define ADM1024_REG_INT1_STAT 0x41
#define ADM1024_REG_INT2_STAT 0x42
#define ADM1024_REG_INT1_MASK 0x43
#define ADM1024_REG_INT2_MASK 0x44

#define ADM1024_REG_CHASSIS_CLEAR 0x46
#define ADM1024_REG_VID_FAN_DIV 0x47
#define ADM1024_REG_I2C_ADDR 0x48
#define ADM1024_REG_VID4 0x49
#define ADM1024_REG_CONFIG2 0x4A
#define ADM1024_REG_TEMP_CONFIG 0x4B
#define ADM1024_REG_EXTMODE1 0x4C	/* Interupt Status Register Mirror No. 1 */
#define ADM1024_REG_EXTMODE2 0x4D	/* Interupt Status Register Mirror No. 2 */

/* Conversions. Rounding and limit checking is only done on the TO_REG
   variants. Note that you should be a bit careful with which arguments
   these macros are called: arguments may be evaluated more than once.
   Fixing this is just not worth it. */
#define IN_TO_REG(val,nr) (SENSORS_LIMIT(((val) & 0xff),0,255))
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

#define TEMP_FROM_REG(temp) \
   ((temp)<256?((((temp)&0x1fe) >> 1) * 10)      + ((temp) & 1) * 5:  \
               ((((temp)&0x1fe) >> 1) -255) * 10 - ((temp) & 1) * 5)  \

#define EXT_TEMP_FROM_REG(temp) (((temp)>0x80?(temp)-0x100:(temp))*10)
   

#define TEMP_LIMIT_FROM_REG(val) (((val)>0x80?(val)-0x100:(val))*10)

#define TEMP_LIMIT_TO_REG(val) SENSORS_LIMIT(((val)<0?(((val)-5)/10):\
                                                      ((val)+5)/10), \
                                             0,255)

#define ALARMS_FROM_REG(val) (val)

#define DIV_FROM_REG(val) (1 << (val))
#define DIV_TO_REG(val) ((val)==1?0:((val)==8?3:((val)==4?2:1)))

#define VID_FROM_REG(val) ((val)==0x1f?0:(val)>=0x10?510-(val)*10:\
                           205-(val)*5)

/* For each registered ADM1024, we need to keep some data in memory. It is
   dynamically allocated, at the same time when a new adm1024 client is
   allocated. */
struct adm1024_data {
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
	int temp1;		/* Ext Temp 1 */
	u8 temp1_os_max;
	u8 temp1_os_hyst;
	int temp2;		/* Ext Temp 2 */
	u8 temp2_os_max;
	u8 temp2_os_hyst;
	u16 alarms;		/* Register encoding, combined */
	u8 analog_out;		/* Register value */
	u8 vid;			/* Register value combined */
};



static int adm1024_attach_adapter(struct i2c_adapter *adapter);
static int adm1024_detect(struct i2c_adapter *adapter, int address,
			  unsigned short flags, int kind);
static int adm1024_detach_client(struct i2c_client *client);

static int adm1024_read_value(struct i2c_client *client, u8 reg);
static int adm1024_write_value(struct i2c_client *client, u8 reg,
			       u8 value);
static void adm1024_update_client(struct i2c_client *client);
static void adm1024_init_client(struct i2c_client *client);


static void adm1024_in(struct i2c_client *client, int operation,
		       int ctl_name, int *nrels_mag, long *results);
static void adm1024_fan(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void adm1024_temp(struct i2c_client *client, int operation,
			 int ctl_name, int *nrels_mag, long *results);
static void adm1024_temp1(struct i2c_client *client, int operation,
			 int ctl_name, int *nrels_mag, long *results);
static void adm1024_temp2(struct i2c_client *client, int operation,
			 int ctl_name, int *nrels_mag, long *results);
static void adm1024_alarms(struct i2c_client *client, int operation,
			   int ctl_name, int *nrels_mag, long *results);
static void adm1024_fan_div(struct i2c_client *client, int operation,
			    int ctl_name, int *nrels_mag, long *results);
static void adm1024_analog_out(struct i2c_client *client, int operation,
			       int ctl_name, int *nrels_mag,
			       long *results);
static void adm1024_vid(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);

static struct i2c_driver adm1024_driver = {
	.name		= "ADM1024 sensor driver",
	.id		= I2C_DRIVERID_ADM1024,
	.flags		= I2C_DF_NOTIFY,
	.attach_adapter	= adm1024_attach_adapter,
	.detach_client	= adm1024_detach_client,
};

/* The /proc/sys entries */
/* -- SENSORS SYSCTL START -- */

#define ADM1024_SYSCTL_IN0 1000	/* Volts * 100 */
#define ADM1024_SYSCTL_IN1 1001
#define ADM1024_SYSCTL_IN2 1002
#define ADM1024_SYSCTL_IN3 1003
#define ADM1024_SYSCTL_IN4 1004
#define ADM1024_SYSCTL_IN5 1005
#define ADM1024_SYSCTL_FAN1 1101	/* Rotations/min */
#define ADM1024_SYSCTL_FAN2 1102
#define ADM1024_SYSCTL_TEMP 1250	/* Degrees Celsius * 100 */
#define ADM1024_SYSCTL_TEMP1 1290	/* Degrees Celsius */
#define ADM1024_SYSCTL_TEMP2 1295	/* Degrees Celsius */
#define ADM1024_SYSCTL_FAN_DIV 2000	/* 1, 2, 4 or 8 */
#define ADM1024_SYSCTL_ALARMS 2001	/* bitvector */
#define ADM1024_SYSCTL_ANALOG_OUT 2002
#define ADM1024_SYSCTL_VID 2003

#define ADM1024_ALARM_IN0 0x0001
#define ADM1024_ALARM_IN1 0x0002
#define ADM1024_ALARM_IN2 0x0004
#define ADM1024_ALARM_IN3 0x0008
#define ADM1024_ALARM_IN4 0x0100
#define ADM1024_ALARM_IN5 0x0200
#define ADM1024_ALARM_FAN1 0x0040
#define ADM1024_ALARM_FAN2 0x0080
#define ADM1024_ALARM_TEMP 0x0010
#define ADM1024_ALARM_TEMP1 0x0020
#define ADM1024_ALARM_TEMP2 0x0001
#define ADM1024_ALARM_CHAS 0x1000

/* -- SENSORS SYSCTL END -- */

/* These files are created for each detected ADM1024. This is just a template;
   though at first sight, you might think we could use a statically
   allocated list, we need some way to get back to the parent - which
   is done through one of the 'extra' fields which are initialized 
   when a new copy is allocated. */
static ctl_table adm1024_dir_table_template[] = {
	{ADM1024_SYSCTL_IN0, "in0", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm1024_in},
	{ADM1024_SYSCTL_IN1, "in1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm1024_in},
	{ADM1024_SYSCTL_IN2, "in2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm1024_in},
	{ADM1024_SYSCTL_IN3, "in3", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm1024_in},
	{ADM1024_SYSCTL_IN4, "in4", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm1024_in},
	{ADM1024_SYSCTL_IN5, "in5", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm1024_in},
	{ADM1024_SYSCTL_FAN1, "fan1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm1024_fan},
	{ADM1024_SYSCTL_FAN2, "fan2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm1024_fan},
	{ADM1024_SYSCTL_TEMP, "temp1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm1024_temp},
	{ADM1024_SYSCTL_TEMP1, "temp2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm1024_temp1},
	{ADM1024_SYSCTL_TEMP2, "temp3", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm1024_temp2},
	{ADM1024_SYSCTL_FAN_DIV, "fan_div", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm1024_fan_div},
	{ADM1024_SYSCTL_ALARMS, "alarms", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm1024_alarms},
	{ADM1024_SYSCTL_ANALOG_OUT, "analog_out", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm1024_analog_out},
	{ADM1024_SYSCTL_VID, "vid", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm1024_vid},
	{0}
};

static int adm1024_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_detect(adapter, &addr_data, adm1024_detect);
}

static int adm1024_detect(struct i2c_adapter *adapter, int address,
			  unsigned short flags, int kind)
{
	int i;
	struct i2c_client *new_client;
	struct adm1024_data *data;
	int err = 0;
	const char *type_name = "";
	const char *client_name = "";

	/* Make sure we aren't probing the ISA bus!! This is just a safety check
	   at this moment; i2c_detect really won't call us. */
#ifdef DEBUG
	if (i2c_is_isa_adapter(adapter)) {
		printk
		    ("adm1024.o: adm1024_detect called for an ISA bus adapter?!?\n");
		return 0;
	}
#endif

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		goto ERROR0;

	/* OK. For now, we presume we have a valid client. We now create the
	   client structure, even though we cannot fill it completely yet.
	   But it allows us to access adm1024_{read,write}_value. */

	if (!(data = kmalloc(sizeof(struct adm1024_data), GFP_KERNEL))) {
		err = -ENOMEM;
		goto ERROR0;
	}

	new_client = &data->client;
	new_client->addr = address;
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &adm1024_driver;
	new_client->flags = 0;

	/* Now, we do the remaining detection. */

	if (kind < 0) {
		if((adm1024_read_value(new_client, ADM1024_REG_CONFIG) & 0x80) != 0x00)
			goto ERROR1;
	}

	/* Determine the chip type. */
	if (kind <= 0) {
		i = adm1024_read_value(new_client, ADM1024_REG_COMPANY_ID);
		if (i == 0x41)
			kind = adm1024;
		else {
			if (kind == 0)
				printk
				    ("adm1024.o: Ignoring 'force' parameter for unknown chip at "
				     "adapter %d, address 0x%02x\n",
				     i2c_adapter_id(adapter), address);
			goto ERROR1;
		}
	}

	if (kind == adm1024) {
		type_name = "adm1024";
		client_name = "ADM1024 chip";
	} else {
#ifdef DEBUG
		printk("adm1024.o: Internal error: unknown kind (%d)\n",
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
					adm1024_dir_table_template,
					THIS_MODULE)) < 0) {
		err = i;
		goto ERROR4;
	}
	data->sysctl_id = i;

	/* Initialize the ADM1024 chip */
	adm1024_init_client(new_client);
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

static int adm1024_detach_client(struct i2c_client *client)
{
	int err;

	i2c_deregister_entry(((struct adm1024_data *) (client->data))->
				 sysctl_id);

	if ((err = i2c_detach_client(client))) {
		printk
		    ("adm1024.o: Client deregistration failed, client not detached.\n");
		return err;
	}

	kfree(client->data);

	return 0;
}

static int adm1024_read_value(struct i2c_client *client, u8 reg)
{
	return 0xFF & i2c_smbus_read_byte_data(client, reg);
}

static int adm1024_write_value(struct i2c_client *client, u8 reg, u8 value)
{
	return i2c_smbus_write_byte_data(client, reg, value);
}

static void adm1024_init_client(struct i2c_client *client)
{
	/* Enable temperature channel 2 */
	adm1024_write_value(client, ADM1024_REG_CHANNEL_MODE, adm1024_read_value(client, ADM1024_REG_CHANNEL_MODE) | 0x04);

	/* Start monitoring */
	adm1024_write_value(client, ADM1024_REG_CONFIG, 0x07);
}

static void adm1024_update_client(struct i2c_client *client)
{
	struct adm1024_data *data = client->data;
	u8 i;

	down(&data->update_lock);

	if (
	    (jiffies - data->last_updated >
	     (data->type == adm1024 ? HZ / 2 : HZ * 2))
	    || (jiffies < data->last_updated) || !data->valid) {

#ifdef DEBUG
		printk("Starting adm1024 update\n");
#endif
		for (i = 0; i <= 5; i++) {
			data->in[i] =
			    adm1024_read_value(client, ADM1024_REG_IN(i));
			data->in_min[i] =
			    adm1024_read_value(client,
					       ADM1024_REG_IN_MIN(i));
			data->in_max[i] =
			    adm1024_read_value(client,
					       ADM1024_REG_IN_MAX(i));
		}
		data->fan[0] =
		    adm1024_read_value(client, ADM1024_REG_FAN1);
		data->fan_min[0] =
		    adm1024_read_value(client, ADM1024_REG_FAN1_MIN);
		data->fan[1] =
		    adm1024_read_value(client, ADM1024_REG_FAN2);
		data->fan_min[1] =
		    adm1024_read_value(client, ADM1024_REG_FAN2_MIN);
		data->temp =
		    (adm1024_read_value(client, ADM1024_REG_TEMP) << 1) +
		    ((adm1024_read_value
		      (client, ADM1024_REG_TEMP_CONFIG) & 0x80) >> 7);
		data->temp_os_max =
		    adm1024_read_value(client, ADM1024_REG_TOS);
		data->temp_os_hyst =
		    adm1024_read_value(client, ADM1024_REG_THYST);
		data->temp1 =
		    adm1024_read_value(client, ADM1024_REG_EXT_TEMP1);
		data->temp1_os_max =
		    adm1024_read_value(client, ADM1024_REG_EXT_TEMP1_HIGH);
		data->temp1_os_hyst =
		    adm1024_read_value(client, ADM1024_REG_EXT_TEMP1_LOW);
		data->temp2 =
		    adm1024_read_value(client, ADM1024_REG_2_5V);
		data->temp2_os_max =
		    adm1024_read_value(client, ADM1024_REG_2_5V_HIGH);
		data->temp2_os_hyst =
		    adm1024_read_value(client, ADM1024_REG_2_5V_LOW);

		i = adm1024_read_value(client, ADM1024_REG_VID_FAN_DIV);
		data->fan_div[0] = (i >> 4) & 0x03;
		data->fan_div[1] = (i >> 6) & 0x03;
		data->vid = i & 0x0f;
		data->vid |=
		    (adm1024_read_value(client, ADM1024_REG_VID4) & 0x01)
		    << 4;

		data->alarms =
		    adm1024_read_value(client,
				       ADM1024_REG_INT1_STAT) +
		    (adm1024_read_value(client, ADM1024_REG_INT2_STAT) <<
		     8);
		data->analog_out =
		    adm1024_read_value(client, ADM1024_REG_ANALOG_OUT);
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
void adm1024_in(struct i2c_client *client, int operation, int ctl_name,
		int *nrels_mag, long *results)
{

	int scales[6] = { 250, 225, 330, 500, 1200, 270 };

	struct adm1024_data *data = client->data;
	int nr = ctl_name - ADM1024_SYSCTL_IN0;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 2;
	else if (operation == SENSORS_PROC_REAL_READ) {
		adm1024_update_client(client);
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
			adm1024_write_value(client, ADM1024_REG_IN_MIN(nr),
					    data->in_min[nr]);
		}
		if (*nrels_mag >= 2) {
			data->in_max[nr] =
			    IN_TO_REG((results[1] * 192) / scales[nr], nr);
			adm1024_write_value(client, ADM1024_REG_IN_MAX(nr),
					    data->in_max[nr]);
		}
	}
}

void adm1024_fan(struct i2c_client *client, int operation, int ctl_name,
		 int *nrels_mag, long *results)
{
	struct adm1024_data *data = client->data;
	int nr = ctl_name - ADM1024_SYSCTL_FAN1 + 1;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		adm1024_update_client(client);
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
			adm1024_write_value(client,
					    nr ==
					    1 ? ADM1024_REG_FAN1_MIN :
					    ADM1024_REG_FAN2_MIN,
					    data->fan_min[nr - 1]);
		}
	}
}


void adm1024_temp(struct i2c_client *client, int operation, int ctl_name,
		  int *nrels_mag, long *results)
{
	struct adm1024_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 1;
	else if (operation == SENSORS_PROC_REAL_READ) {
		adm1024_update_client(client);
		results[0] = TEMP_LIMIT_FROM_REG(data->temp_os_max);
		results[1] = TEMP_LIMIT_FROM_REG(data->temp_os_hyst);
		results[2] = TEMP_FROM_REG(data->temp);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->temp_os_max = TEMP_LIMIT_TO_REG(results[0]);
			adm1024_write_value(client, ADM1024_REG_TOS,
					    data->temp_os_max);
		}
		if (*nrels_mag >= 2) {
			data->temp_os_hyst = TEMP_LIMIT_TO_REG(results[1]);
			adm1024_write_value(client, ADM1024_REG_THYST,
					    data->temp_os_hyst);
		}
	}
}

void adm1024_temp1(struct i2c_client *client, int operation, int ctl_name,
		  int *nrels_mag, long *results)
{
	struct adm1024_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 1;
	else if (operation == SENSORS_PROC_REAL_READ) {
		adm1024_update_client(client);
		results[0] = TEMP_LIMIT_FROM_REG(data->temp1_os_max);
		results[1] = TEMP_LIMIT_FROM_REG(data->temp1_os_hyst);
		results[2] = EXT_TEMP_FROM_REG(data->temp1);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->temp1_os_max = TEMP_LIMIT_TO_REG(results[0]);
			adm1024_write_value(client, ADM1024_REG_EXT_TEMP1_HIGH,
					    data->temp1_os_max);
		}
		if (*nrels_mag >= 2) {
			data->temp1_os_hyst = TEMP_LIMIT_TO_REG(results[1]);
			adm1024_write_value(client, ADM1024_REG_EXT_TEMP1_LOW,
					    data->temp1_os_hyst);
		}
	}
}

void adm1024_temp2(struct i2c_client *client, int operation, int ctl_name,
		  int *nrels_mag, long *results)
{
	struct adm1024_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 1;
	else if (operation == SENSORS_PROC_REAL_READ) {
		adm1024_update_client(client);
		results[0] = TEMP_LIMIT_FROM_REG(data->temp2_os_max);
		results[1] = TEMP_LIMIT_FROM_REG(data->temp2_os_hyst);
		results[2] = EXT_TEMP_FROM_REG(data->temp2);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->temp2_os_max = TEMP_LIMIT_TO_REG(results[0]);
			adm1024_write_value(client, ADM1024_REG_2_5V_HIGH,
					    data->temp2_os_max);
		}
		if (*nrels_mag >= 2) {
			data->temp2_os_hyst = TEMP_LIMIT_TO_REG(results[1]);
			adm1024_write_value(client, ADM1024_REG_2_5V_LOW,
					    data->temp2_os_hyst);
		}
	}
}

void adm1024_alarms(struct i2c_client *client, int operation, int ctl_name,
		    int *nrels_mag, long *results)
{
	struct adm1024_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		adm1024_update_client(client);
		results[0] = ALARMS_FROM_REG(data->alarms);
		*nrels_mag = 1;
	}
}

void adm1024_fan_div(struct i2c_client *client, int operation,
		     int ctl_name, int *nrels_mag, long *results)
{
	struct adm1024_data *data = client->data;
	int old;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		adm1024_update_client(client);
		results[0] = DIV_FROM_REG(data->fan_div[0]);
		results[1] = DIV_FROM_REG(data->fan_div[1]);
		*nrels_mag = 2;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		old = adm1024_read_value(client, ADM1024_REG_VID_FAN_DIV);
		if (*nrels_mag >= 2) {
			data->fan_div[1] = DIV_TO_REG(results[1]);
			old = (old & 0x3f) | (data->fan_div[1] << 6);
		}
		if (*nrels_mag >= 1) {
			data->fan_div[0] = DIV_TO_REG(results[0]);
			old = (old & 0xcf) | (data->fan_div[0] << 4);
			adm1024_write_value(client,
					    ADM1024_REG_VID_FAN_DIV, old);
		}
	}
}

void adm1024_analog_out(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results)
{
	struct adm1024_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		adm1024_update_client(client);
		results[0] = data->analog_out;
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->analog_out = results[0];
			adm1024_write_value(client, ADM1024_REG_ANALOG_OUT,
					    data->analog_out);
		}
	}
}

void adm1024_vid(struct i2c_client *client, int operation, int ctl_name,
		 int *nrels_mag, long *results)
{
	struct adm1024_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 2;
	else if (operation == SENSORS_PROC_REAL_READ) {
		adm1024_update_client(client);
		results[0] = VID_FROM_REG(data->vid);
		*nrels_mag = 1;
	}
}

static int __init sm_adm1024_init(void)
{
	printk("adm1024.o version %s (%s)\n", LM_VERSION, LM_DATE);
	return i2c_add_driver(&adm1024_driver);
}

static void __exit sm_adm1024_exit(void)
{
	i2c_del_driver(&adm1024_driver);
}



MODULE_AUTHOR
    ("Frodo Looijaard <frodol@dds.nl> and Philip Edelbrock <phil@netroedge.com>");
MODULE_DESCRIPTION("ADM1024 driver");

MODULE_LICENSE("GPL");

module_init(sm_adm1024_init);
module_exit(sm_adm1024_exit);
