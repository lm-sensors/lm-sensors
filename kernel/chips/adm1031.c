/*
  adm1031.c - Part of lm_sensors, Linux kernel modules for hardware
              monitoring
  Based on lm75.c and lm85.c
  Supports Analog Devices ADM1030 and ADM1031
  Copyright (C) 2004 Alexandre d'Alton <alex@alexdalton.org> and
                     Jean Delvare <khali@linux-fr.org>

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
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/i2c-proc.h>
#include "version.h"

/* Following macros take channel parameter starting from 0 */
#define ADM1031_REG_FAN_SPEED(nr)	(0x08 + (nr))

#define ADM1031_REG_FAN_DIV(nr)		(0x20 + (nr))
#define ADM1031_REG_FAN_MIN(nr)		(0x10 + (nr))

#define ADM1031_REG_TEMP_MAX(nr)	(0x14 + 4 * (nr))
#define ADM1031_REG_TEMP_MIN(nr)	(0x15 + 4 * (nr))
#define ADM1031_REG_TEMP_CRIT(nr)	(0x16 + 4 * (nr))

#define ADM1031_REG_TEMP(nr)		(0x0a + (nr))
#define ADM1031_REG_AUTO_TEMP(nr)	(0x24 + (nr))

#define ADM1031_REG_STATUS(nr)		(0x02 + (nr))
#define ADM1031_REG_FAN_PWM		0x22

#define ADM1031_REG_CONF1		0x00
#define ADM1031_REG_CONF2		0x01
#define ADM1031_REG_EXT_TEMP		0x06

#define ADM1031_CONF1_MONITOR_ENABLE	0x01 /* Monitoring enable */
#define ADM1031_CONF1_PWM_INVERT	0x08 /* PWM Invert (unused) */
#define ADM1031_CONF1_AUTO_MODE		0x80 /* Auto fan mode */

#define ADM1031_CONF2_PWM1_ENABLE	0x01
#define ADM1031_CONF2_PWM2_ENABLE	0x02
#define ADM1031_CONF2_TACH1_ENABLE	0x04
#define ADM1031_CONF2_TACH2_ENABLE	0x08
#define ADM1031_CONF2_TEMP_ENABLE(chan)	(0x10 << (chan))

/* Addresses to scan */
static unsigned short normal_i2c[] = { SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { 0x2c, 0x2e, SENSORS_I2C_END };
static unsigned int normal_isa[] = { SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/* Insmod parameters */
SENSORS_INSMOD_2(adm1030, adm1031);

/*
 * Proc entries
 * These files are created for each detected ADM1031.
 */

/* -- SENSORS SYSCTL START -- */

#define ADM1031_SYSCTL_TEMP1		1200
#define ADM1031_SYSCTL_TEMP2		1201
#define ADM1031_SYSCTL_TEMP3		1202

#define ADM1031_SYSCTL_FAN1		1210
#define ADM1031_SYSCTL_FAN2		1211

#define ADM1031_SYSCTL_FAN_DIV		1220

#define ADM1031_SYSCTL_ALARMS		1250

#define ADM1031_ALARM_FAN1_MIN		0x0001
#define ADM1031_ALARM_FAN1_FLT		0x0002
#define ADM1031_ALARM_TEMP2_HIGH	0x0004
#define ADM1031_ALARM_TEMP2_LOW		0x0008
#define ADM1031_ALARM_TEMP2_CRIT	0x0010
#define ADM1031_ALARM_TEMP2_DIODE	0x0020
#define ADM1031_ALARM_TEMP1_HIGH	0x0040
#define ADM1031_ALARM_TEMP1_LOW		0x0080
#define ADM1031_ALARM_FAN2_MIN		0x0100
#define ADM1031_ALARM_FAN2_FLT		0x0200
#define ADM1031_ALARM_TEMP3_HIGH	0x0400
#define ADM1031_ALARM_TEMP3_LOW		0x0800
#define ADM1031_ALARM_TEMP3_CRIT	0x1000
#define ADM1031_ALARM_TEMP3_DIODE	0x2000
#define ADM1031_ALARM_TEMP1_CRIT	0x4000
#define ADM1031_ALARM_THERMAL		0x8000


/* -- SENSORS SYSCTL END -- */
static void adm1031_alarms(struct i2c_client *client, int operation,
			   int ctl_name, int *nrels_mag, long *results);
static void adm1031_temp(struct i2c_client *client, int operation,
			 int ctl_name, int *nrels_mag, long *results);
static void adm1031_fan(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void adm1031_fan_div(struct i2c_client *client, int operation,
			    int ctl_name, int *nrels_mag, long *results);

static ctl_table adm1031_dir_table_template[] = {
	{ADM1031_SYSCTL_TEMP1, "temp1", NULL, 0, 0644, NULL,
	 &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm1031_temp},
	{ADM1031_SYSCTL_TEMP2, "temp2", NULL, 0, 0644, NULL,
	 &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm1031_temp},
	{ADM1031_SYSCTL_TEMP3, "temp3", NULL, 0, 0644, NULL,
	 &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm1031_temp},
	{ADM1031_SYSCTL_FAN1, "fan1", NULL, 0, 0644, NULL,
	 &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm1031_fan},
	{ADM1031_SYSCTL_FAN_DIV, "fan_div", NULL, 0, 0644, NULL,
	 &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm1031_fan_div},
	{ADM1031_SYSCTL_FAN2, "fan2", NULL, 0, 0644, NULL,
	 &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm1031_fan},
	{ADM1031_SYSCTL_ALARMS, "alarms", NULL, 0, 0444, NULL,
	 &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm1031_alarms},
	{0}
};

static ctl_table adm1030_dir_table_template[] = {
	{ADM1031_SYSCTL_TEMP1, "temp1", NULL, 0, 0644, NULL,
	 &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm1031_temp},
	{ADM1031_SYSCTL_TEMP2, "temp2", NULL, 0, 0644, NULL,
	 &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm1031_temp},
	{ADM1031_SYSCTL_FAN1, "fan1", NULL, 0, 0644, NULL,
	 &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm1031_fan},
	{ADM1031_SYSCTL_FAN_DIV, "fan_div", NULL, 0, 0644, NULL,
	 &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm1031_fan_div},
	{ADM1031_SYSCTL_ALARMS, "alarms", NULL, 0, 0444, NULL,
	 &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm1031_alarms},
	{0}
};


/* Each client has this additional data */
struct adm1031_data {
	struct i2c_client client;
	int sysctl_id;
	struct semaphore update_lock;
	int chip_type;
	char valid;		/* !=0 if following fields are valid */
	unsigned long last_updated;	/* In jiffies */

	u16 alarms;
	u8 conf1;
	u8 conf2;
	u8 fan[2];
	u8 fan_min[2];
	u8 fan_pwm[2];
	u8 fan_div[2];
	s8 temp[3];
	u8 auto_temp[3];
	u8 ext_temp[3];
	s8 temp_min[3];
	s8 temp_max[3];
	s8 temp_crit[3];
};

static int adm1031_attach_adapter(struct i2c_adapter *adapter);
static int adm1031_detect(struct i2c_adapter *adapter, int address,
			  unsigned short flags, int kind);
static void adm1031_init_client(struct i2c_client *client);
static int adm1031_detach_client(struct i2c_client *client);

static inline u8 adm1031_read_value(struct i2c_client *client, u8 reg);
static inline int adm1031_write_value(struct i2c_client *client, u8 reg,
				      unsigned int value);

static void adm1031_update_client(struct i2c_client *client);

/* This is the driver that will be inserted */
static struct i2c_driver adm1031_driver = {
	.name		= "ADM1031/ADM1030 sensor driver",
	.flags		= I2C_DF_NOTIFY,
	.attach_adapter	= adm1031_attach_adapter,
	.detach_client	= adm1031_detach_client,
};

#define TEMP_TO_REG(val)		((val) < 0 ? (((val) - 500) / 1000) : \
					 (((val) + 500) / 1000))

#define TEMP_FROM_REG(reg)		((reg) * 1000)

#define TEMP_FROM_REG_EXT(val,ext)	(TEMP_FROM_REG(val) + (ext) * 125)

#define FAN_FROM_REG(reg,div)		((reg) ? 675000 / ((reg) * (div)) : 0)

#define FAN_TO_REG(val,div)		((val) <= 0 ? 0 : \
					 (val) * (div) >= 675000 ? 1 : \
					 (val) * (div) <= 2647 ? 255 : \
					 675000 / ((val) * (div)))

#define FAN_DIV_TO_REG(val)		((val) == 8 ? 0xc0 : \
					 (val) == 4 ? 0x80 : \
					 (val) == 1 ? 0x00 : 0x40)

#define FAN_DIV_FROM_REG(reg)		(1 << (((reg)&0xc0) >> 6))

#define AUTO_TEMP_MIN_FROM_REG(reg)	(((reg) >> 3) * 4)


static int adm1031_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_detect(adapter, &addr_data, adm1031_detect);
}

/* This function is called by i2c_detect */
static int
adm1031_detect(struct i2c_adapter *adapter, int address,
	       unsigned short flags, int kind)
{
	struct i2c_client *new_client;
	struct adm1031_data *data;
	int err = 0;
	const char *type_name = "";
	const char *client_name = "";

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		goto exit;

	if (!(data = kmalloc(sizeof(struct adm1031_data), GFP_KERNEL))) {
		err = -ENOMEM;
		goto exit;
	}
	memset(data, 0, sizeof(struct adm1031_data));

	new_client = &data->client;
	new_client->data = data;
	new_client->addr = address;
	new_client->adapter = adapter;
	new_client->driver = &adm1031_driver;
	new_client->flags = 0;

	if (kind < 0) {
		int id, co;
		id = i2c_smbus_read_byte_data(new_client, 0x3d);
		co = i2c_smbus_read_byte_data(new_client, 0x3e);

		if (((id != 0x31) || (id != 0x30)) && (co != 0x41))
			goto exit_free;
		kind = (id == 0x30) ? adm1030 : adm1031;
	}

	if (kind <= 0)
		kind = adm1031;

	/* Given the detected chip type, set the chip name and the
	 * auto fan control helper table. */
	if (kind == adm1030) {
		type_name = "adm1030";
		client_name = "ADM1030 chip";

	} else if (kind == adm1031) {
		type_name = "adm1031";
		client_name = "ADM1031 chip";
	}
	data->chip_type = kind;

	strcpy(new_client->name, client_name);
	data->valid = 0;
	init_MUTEX(&data->update_lock);

	/* Tell the I2C layer a new client has arrived */
	if ((err = i2c_attach_client(new_client)))
		goto exit_free;


	if (kind == adm1030) {
		if ((err = i2c_register_entry(new_client, type_name,
					      adm1030_dir_table_template,
					      THIS_MODULE)) < 0)
			goto exit_detach;
	} else if (kind == adm1031) {
		if ((err = i2c_register_entry(new_client, type_name,
					      adm1031_dir_table_template,
					      THIS_MODULE)) < 0)
			goto exit_detach;
	}

	data->sysctl_id = err;

	/* Initialize the ADM1031 chip */
	adm1031_init_client(new_client);

	return 0;

exit_detach:
	i2c_detach_client(new_client);
exit_free:
	kfree(new_client);
exit:
	return err;
}

static int adm1031_detach_client(struct i2c_client *client)
{
	int ret;
	struct adm1031_data *data = client->data;

	i2c_deregister_entry(data->sysctl_id);
	if ((ret = i2c_detach_client(client)) != 0) {
		return ret;
	}

	kfree(client);
	return 0;
}

static inline u8 adm1031_read_value(struct i2c_client *client, u8 reg)
{
	return i2c_smbus_read_byte_data(client, reg);
}

static inline int adm1031_write_value(struct i2c_client *client, u8 reg,
				      unsigned int value)
{
	return i2c_smbus_write_byte_data(client, reg, value);
}

static void adm1031_init_client(struct i2c_client *client)
{
	unsigned int read_val;
	unsigned int mask;
	struct adm1031_data *data = client->data;

	mask = (ADM1031_CONF2_PWM1_ENABLE | ADM1031_CONF2_TACH1_ENABLE);
	if (data->chip_type == adm1031) {
		mask |= (ADM1031_CONF2_PWM2_ENABLE |
			 ADM1031_CONF2_TACH2_ENABLE);
	}
	/* Initialize the ADM1031 chip (enable fan speed reading) */
	read_val = adm1031_read_value(client, ADM1031_REG_CONF2);
	if ((read_val | mask) != read_val) {
		adm1031_write_value(client, ADM1031_REG_CONF2,
				    read_val | mask);
	}

	read_val = adm1031_read_value(client, ADM1031_REG_CONF1);
	if ((read_val | ADM1031_CONF1_MONITOR_ENABLE) != read_val) {
		adm1031_write_value(client, ADM1031_REG_CONF1, read_val |
				    ADM1031_CONF1_MONITOR_ENABLE);
	}
}

static void adm1031_update_client(struct i2c_client *client)
{
	struct adm1031_data *data = client->data;
	int chan;

	down(&data->update_lock);

	if ((jiffies - data->last_updated > HZ + HZ / 2) ||
	    (jiffies < data->last_updated) || !data->valid) {
#ifdef DEBUG
		printk(KERN_INFO "adm1031.o: Starting chip update\n");
#endif
		for (chan = 0;
		     chan < ((data->chip_type == adm1031) ? 3 : 2);
		     chan++) {
			u8 oldh, newh;

			oldh = adm1031_read_value(client,
						  ADM1031_REG_TEMP(chan));
			data->ext_temp[chan] = adm1031_read_value(client,
							ADM1031_REG_EXT_TEMP);
			newh = adm1031_read_value(client,
						  ADM1031_REG_TEMP(chan));
			if (newh != oldh) {
				data->ext_temp[chan] = adm1031_read_value(client,
							ADM1031_REG_EXT_TEMP);
#ifdef DEBUG
				oldh = adm1031_read_value(client,
							  ADM1031_REG_TEMP(chan));

				/* oldh is actually newer */
				if (newh != oldh)
					printk(KERN_INFO "adm1031.o: Remote "
					       "temperature may be wrong.\n");
#endif
			}
			data->temp[chan] = newh;

			data->temp_min[chan] = adm1031_read_value(client,
					       ADM1031_REG_TEMP_MIN(chan));
			data->temp_max[chan] = adm1031_read_value(client,
					       ADM1031_REG_TEMP_MAX(chan));
			data->temp_crit[chan] = adm1031_read_value(client,
						ADM1031_REG_TEMP_CRIT(chan));
			data->auto_temp[chan] = adm1031_read_value(client,
						ADM1031_REG_AUTO_TEMP(chan));
		}

		data->conf1 = adm1031_read_value(client, ADM1031_REG_CONF1);

		data->alarms = adm1031_read_value(client, ADM1031_REG_STATUS(0))
			     | (adm1031_read_value(client, ADM1031_REG_STATUS(1))
				<< 8);
		if (data->chip_type == adm1030) {
			data->alarms &= 0xc0ff;
		}

		for (chan = 0;
		     chan < (data->chip_type == adm1030 ? 1 : 2);
		     chan++) {
			data->fan_div[chan] = adm1031_read_value(client,
					      ADM1031_REG_FAN_DIV(chan));
			data->fan_min[chan] = adm1031_read_value(client,
					      ADM1031_REG_FAN_MIN(chan));
			data->fan[chan] = adm1031_read_value(client,
					  ADM1031_REG_FAN_SPEED(chan));
			data->fan_pwm[chan] = (adm1031_read_value(client,
					       ADM1031_REG_FAN_PWM) >> (4 * chan))
					      & 0x0f;
		}

		data->conf1 = adm1031_read_value(client, ADM1031_REG_CONF1);
		data->conf2 = adm1031_read_value(client, ADM1031_REG_CONF2);

		data->last_updated = jiffies;
		data->valid = 1;
	}

	up(&data->update_lock);
}

static int __init sensors_adm1031_init(void)
{
	printk(KERN_INFO "adm1031.o version %s (%s)\n", LM_VERSION, LM_DATE);
	return i2c_add_driver(&adm1031_driver);
}

static void __exit sensors_adm1031_exit(void)
{
	i2c_del_driver(&adm1031_driver);
}


static void
adm1031_temp(struct i2c_client *client, int operation, int ctl_name,
	     int *nrels_mag, long *results)
{
	int ext;
	struct adm1031_data *data = client->data;
	int tmp;
	int nr = ctl_name - ADM1031_SYSCTL_TEMP1;

	ext = nr == 0 ?
		((data->ext_temp[nr] >> 6) & 0x3) * 2 :
		(((data->ext_temp[nr] >> ((nr - 1) * 3)) & 7));

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 3;
	else if (operation == SENSORS_PROC_REAL_READ) {
		adm1031_update_client(client);
		results[0] = TEMP_FROM_REG(data->temp_max[nr]);
		results[1] = TEMP_FROM_REG(data->temp_min[nr]);
		results[2] = TEMP_FROM_REG(data->temp_crit[nr]);
		results[3] = TEMP_FROM_REG_EXT(data->temp[nr], ext);
		*nrels_mag = 4;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			tmp = SENSORS_LIMIT(results[0], -55000, 127000);
			data->temp_max[nr] = TEMP_TO_REG(tmp);
			adm1031_write_value(client,
					    ADM1031_REG_TEMP_MAX(nr),
					    data->temp_max[nr]);
		}
		if (*nrels_mag >= 2) {
			tmp = SENSORS_LIMIT(results[1], -55000, 127000);
			data->temp_min[nr] = TEMP_TO_REG(tmp);
			adm1031_write_value(client,
					    ADM1031_REG_TEMP_MIN(nr),
					    data->temp_min[nr]);
		}
		if (*nrels_mag >= 3) {
			tmp = SENSORS_LIMIT(results[2], -55000, 127000);
			data->temp_crit[nr] = TEMP_TO_REG(tmp);
			adm1031_write_value(client,
					    ADM1031_REG_TEMP_CRIT(nr),
					    data->temp_crit[nr]);
		}
	}
}

/*
 * That function checks the cases where the fan reading is not
 * relevant.  It is used to provide 0 as fan reading when the fan is
 * not supposed to run.
 */
static int trust_fan_readings(struct adm1031_data * data, int chan)
{
	int res = 0;

	if (data->conf1 & ADM1031_CONF1_AUTO_MODE) {
		switch(data->conf1 & 0x60) {
		case 0x00: /* temp2 controls fan1, temp3 controls fan2 */
			res = data->temp[chan+1] >=
			      AUTO_TEMP_MIN_FROM_REG(data->auto_temp[chan+1]);
			break;
		case 0x20: /* temp2 controls both fans */
			res = data->temp[1] >=
			      AUTO_TEMP_MIN_FROM_REG(data->auto_temp[1]);
			break;
		case 0x40: /* temp3 controls both fans */
			res = data->temp[2] >=
			      AUTO_TEMP_MIN_FROM_REG(data->auto_temp[2]);
			break;
		case 0x60: /* max of temp1, temp2 and temp3 controls both
			      fans */
			res = data->temp[0] >=
			      AUTO_TEMP_MIN_FROM_REG(data->auto_temp[0])
			      || data->temp[1] >=
			      AUTO_TEMP_MIN_FROM_REG(data->auto_temp[1])
			      || (data->chip_type == adm1031
				  && data->temp[2] >=
				  AUTO_TEMP_MIN_FROM_REG(data->auto_temp[2]));
			break;
		}
	} else {
		res = data->fan_pwm[chan] > 0;
	}

	return res;
}

static void adm1031_fan(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results)
{
	struct adm1031_data *data = client->data;
	int nr = ctl_name - ADM1031_SYSCTL_FAN1;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
	        adm1031_update_client(client);
	        results[0] = FAN_FROM_REG(data->fan_min[nr],
			     FAN_DIV_FROM_REG(data->fan_div[nr]));
		results[1] = trust_fan_readings(data, nr) ?
			     FAN_FROM_REG(data->fan[nr],
			     FAN_DIV_FROM_REG(data->fan_div[nr])) : 0;
		*nrels_mag = 2;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->fan_min[nr] = FAN_TO_REG(results[0],
					    FAN_DIV_FROM_REG(data->fan_div[nr]));
			adm1031_write_value(client,
					    ADM1031_REG_FAN_MIN(nr),
					    data->fan_min[nr]);
		}
	}
}

static void adm1031_fan_div(struct i2c_client *client, int operation,
			    int ctl_name, int *nrels_mag, long *results)
{
	struct adm1031_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		adm1031_update_client(client);
		results[0] = FAN_DIV_FROM_REG(data->fan_div[0]);
		*nrels_mag = 1;
		if(data->chip_type == adm1031) {
		    results[1] = FAN_DIV_FROM_REG(data->fan_div[1]);
		    *nrels_mag = 2;
		}
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		int old_div, new_min;
		if (*nrels_mag >= 1) {
			old_div = FAN_DIV_FROM_REG(data->fan_div[0]);
			data->fan_div[0] = FAN_DIV_TO_REG(results[0]);
			adm1031_write_value(client,
					    ADM1031_REG_FAN_DIV(0),
					    data->fan_div[0]);
			new_min = data->fan_min[0] * old_div /
				  FAN_DIV_FROM_REG(data->fan_div[0]);
			data->fan_min[0] = new_min > 0xff ? 0xff : new_min;
			adm1031_write_value(client,
					    ADM1031_REG_FAN_MIN(0),
					    data->fan_min[0]);
		}
		if (*nrels_mag >= 2) {
			old_div = FAN_DIV_FROM_REG(data->fan_div[1]);
			data->fan_div[1] = FAN_DIV_TO_REG(results[1]);
			adm1031_write_value(client,
					    ADM1031_REG_FAN_DIV(1),
					    data->fan_div[1]);
			new_min = data->fan_min[1] * old_div /
				  FAN_DIV_FROM_REG(data->fan_div[1]);
			data->fan_min[1] = new_min > 0xff ? 0xff : new_min;
			adm1031_write_value(client,
					    ADM1031_REG_FAN_MIN(1),
					    data->fan_min[1]);
		}
	}
}

static void adm1031_alarms(struct i2c_client *client, int operation,
			   int ctl_name, int *nrels_mag, long *results)
{
	struct adm1031_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		adm1031_update_client(client);
		results[0] = data->alarms;
		*nrels_mag = 1;
	}
}

MODULE_AUTHOR("Alexandre d'Alton <alex@alexdalton.org>");
MODULE_DESCRIPTION("ADM1031/ADM1030 driver");
MODULE_LICENSE("GPL");

module_init(sensors_adm1031_init);
module_exit(sensors_adm1031_exit);
