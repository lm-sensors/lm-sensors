/*
 * lm63.c - driver for the National Semiconductor LM63 temperature sensor
 *          with integrated fan control
 * Copyright (C) 2004  Jean Delvare <khali@linux-fr.org>
 * Based on the lm90 driver.
 *
 * The LM63 is a sensor chip made by National Semiconductor. It measures
 * two temperatures (its own and one external one) and the speed of one
 * fan, those speed it can additionally control. Complete datasheet can be
 * obtained from National's website at:
 *   http://www.national.com/pf/LM/LM63.html
 *
 * The LM63 is basically an LM86 with fan speed monitoring and control
 * capabilities added. It misses some of the LM86 features though:
 *  - No low limit for local temperature.
 *  - No critical limit for local temperature.
 *  - Critical limit for remote temperature can be changed only once. We
 *    will consider that the critical limit is read-only.
 *
 * The datasheet isn't very clear about what the tachometer reading is.
 * I had a explanation from National Semiconductor though. The two lower
 * bits of the read value have to be masked out. The value is still 16 bit
 * in width.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/i2c-proc.h>
#include <linux/init.h>
#include "version.h"

/*
 * Addresses to scan
 * Address is fully defined internally and cannot be changed.
 */

static unsigned short normal_i2c[] = { 0x4c, SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { SENSORS_I2C_END };
static unsigned int normal_isa[] = { SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };
/*
 * Insmod parameters
 */

SENSORS_INSMOD_1(lm63);

/*
 * The LM63 registers
 */

#define LM63_REG_CONFIG1		0x03
#define LM63_REG_CONFIG2		0xBF
#define LM63_REG_CONFIG_FAN		0x4A

#define LM63_REG_TACH_COUNT_MSB		0x47
#define LM63_REG_TACH_COUNT_LSB		0x46
#define LM63_REG_TACH_LIMIT_MSB		0x49
#define LM63_REG_TACH_LIMIT_LSB		0x48

#define LM63_REG_PWM_VALUE		0x4C
#define LM63_REG_PWM_FREQ		0x4D

#define LM63_REG_LOCAL_TEMP		0x00
#define LM63_REG_LOCAL_HIGH		0x05

#define LM63_REG_REMOTE_TEMP_MSB	0x01
#define LM63_REG_REMOTE_TEMP_LSB	0x10
#define LM63_REG_REMOTE_OFFSET_MSB	0x11
#define LM63_REG_REMOTE_OFFSET_LSB	0x12
#define LM63_REG_REMOTE_HIGH_MSB	0x07
#define LM63_REG_REMOTE_HIGH_LSB	0x13
#define LM63_REG_REMOTE_LOW_MSB		0x08
#define LM63_REG_REMOTE_LOW_LSB		0x14
#define LM63_REG_REMOTE_TCRIT		0x19
#define LM63_REG_REMOTE_TCRIT_HYST	0x21

#define LM63_REG_ALERT_STATUS		0x02
#define LM63_REG_ALERT_MASK		0x16

#define LM63_REG_MAN_ID			0xFE
#define LM63_REG_CHIP_ID		0xFF

/*
 * Conversions and various macros
 * For tachometer counts, the LM63 uses 16-bit values.
 * For local temperature and high limit, remote critical limit and hysteresis
 * value, it uses signed 8-bit values with LSB = 1 degree Celcius.
 * For remote temperature, low and high limits, it uses signed 11-bit values
 * with LSB = 0.125 degree Celcius, left-justified in 16-bit registers.
 */

#define FAN_FROM_REG(reg)	((reg) == 0xFFFC || (reg) == 0 ? 0 : \
				 5400000 / (reg))
#define FAN_TO_REG(val)		((val) <= 82 ? 0xFFFF : \
				 (5400000 / (val)) & 0xFFFC)
#define TEMP8_FROM_REG(reg)	(reg)
#define TEMP8_TO_REG(val)	((val) <= -128 ? -128 : \
				 (val) >= 127 ? 127 : \
				 val)
#define TEMP11_FROM_REG(reg)	((reg) / 32 * 125)
#define TEMP11_TO_REG(val)	((val) <= -128000 ? 0x8000 : \
				 (val) >= 127875 ? 0x7FE0 : \
				 (val) < 0 ? ((val) - 62) / 125 * 32 : \
				 ((val) + 62) / 125 * 32)
#define HYST_TO_REG(val)	((val) <= 0 ? 0 : \
				 (val) >= 127 ? 127 : \
				 val)

/*
 * Functions declaration
 */

static int lm63_attach_adapter(struct i2c_adapter *adapter);
static int lm63_detect(struct i2c_adapter *adapter, int address,
	unsigned short flags, int kind);
static void lm63_init_client(struct i2c_client *client);
static int lm63_detach_client(struct i2c_client *client);

static void lm63_local_temp(struct i2c_client *client, int operation,
	int ctl_name, int *nrels_mag, long *results);
static void lm63_remote_temp(struct i2c_client *client, int operation,
	int ctl_name, int *nrels_mag, long *results);
static void lm63_remote_tcrit(struct i2c_client *client, int operation,
	int ctl_name, int *nrels_mag, long *results);
static void lm63_remote_tcrit_hyst(struct i2c_client *client, int operation,
	int ctl_name, int *nrels_mag, long *results);
static void lm63_alarms(struct i2c_client *client, int operation,
	int ctl_name, int *nrels_mag, long *results);
static void lm63_fan(struct i2c_client *client, int operation,
	int ctl_name, int *nrels_mag, long *results);
static void lm63_pwm(struct i2c_client *client, int operation,
	int ctl_name, int *nrels_mag, long *results);

/*
 * Driver data (common to all clients)
 */

static struct i2c_driver lm63_driver = {
	.name		= "LM63 sensor driver",
	.flags		= I2C_DF_NOTIFY,
	.attach_adapter	= lm63_attach_adapter,
	.detach_client	= lm63_detach_client,
};

/*
 * Client data (each client gets its own)
 */

struct lm63_data {
	struct i2c_client client;
	int sysctl_id;

	struct semaphore update_lock;
	char valid; /* zero until following fields are valid */
	unsigned long last_updated; /* in jiffies */

	/* registers values */
	u8 config, config_fan;
	u16 fan1_input;
	u16 fan1_low;
	u8 pwm1_freq;
	u8 pwm1_value;
	s8 temp1_input;
	s8 temp1_high;
	s16 temp2_input;
	s16 temp2_high;
	s16 temp2_low;
	s8 temp2_crit;
	u8 temp2_crit_hyst;
	u8 alarms;
};

/*
 * Proc entries
 * These files are created for each detected LM63.
 */

/* -- SENSORS SYSCTL START -- */

#define LM63_SYSCTL_TEMP1		1200
#define LM63_SYSCTL_TEMP2		1201
#define LM63_SYSCTL_TEMP2_TCRIT		1205
#define LM63_SYSCTL_TEMP2_TCRIT_HYST	1208
#define LM63_SYSCTL_ALARMS		1210
#define LM63_SYSCTL_FAN1		1220
#define LM63_SYSCTL_PWM1		1230

#define LM63_ALARM_LOCAL_HIGH		0x40
#define LM63_ALARM_REMOTE_HIGH		0x10
#define LM63_ALARM_REMOTE_LOW		0x08
#define LM63_ALARM_REMOTE_CRIT		0x02
#define LM63_ALARM_REMOTE_OPEN		0x04
#define LM63_ALARM_FAN_LOW		0x01

/* -- SENSORS SYSCTL END -- */

static ctl_table lm63_dir_table_template[] =
{
	{LM63_SYSCTL_TEMP1, "temp1", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &lm63_local_temp},
	{LM63_SYSCTL_TEMP2, "temp2", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &lm63_remote_temp},
	{LM63_SYSCTL_TEMP2_TCRIT, "temp2_crit", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &lm63_remote_tcrit},
	{LM63_SYSCTL_TEMP2_TCRIT_HYST, "temp2_crit_hyst", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &lm63_remote_tcrit_hyst},
	{LM63_SYSCTL_ALARMS, "alarms", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &lm63_alarms},
	{LM63_SYSCTL_FAN1, "fan1", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &lm63_fan},
	{LM63_SYSCTL_PWM1, "pwm1", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &lm63_pwm},
	{0}
};

/*
 * Real code
 */

static int lm63_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_detect(adapter, &addr_data, lm63_detect);
}

/*
 * The following function does more than just detection. If detection
 * succeeds, it also registers the new chip.
 */
static int lm63_detect(struct i2c_adapter *adapter, int address,
	unsigned short flags, int kind)
{
	struct i2c_client *new_client;
	struct lm63_data *data;
	int err = 0;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
#ifdef DEBUG
		printk(KERN_DEBUG "lm63: adapter doesn't support SMBus byte "
		       "data mode, skipping.\n");
#endif
		return 0;
	}

	if (!(data = kmalloc(sizeof(struct lm63_data), GFP_KERNEL))) {
		printk(KERN_ERR "lm63: Out of memory in lm63_detect\n");
		return -ENOMEM;
	}

	/*
	 * The common I2C client data is placed right before the
	 * LM63-specific data. The LM63-specific data is pointed to by the
	 * data field from the i2c_client structure.
	 */
	new_client = &data->client;
	new_client->addr = address;
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &lm63_driver;
	new_client->flags = 0;

	/* Default to an LM63 if forced */
	if (kind == 0)
		kind = lm63;

	if (kind < 0) { /* must identify */
		u8 man_id, chip_id, reg_config1, reg_config2;
		u8 reg_alert_status, reg_alert_mask;

		man_id = i2c_smbus_read_byte_data(new_client,
			 LM63_REG_MAN_ID);
		chip_id = i2c_smbus_read_byte_data(new_client,
			  LM63_REG_CHIP_ID);
		reg_config1 = i2c_smbus_read_byte_data(new_client,
			      LM63_REG_CONFIG1);
		reg_config2 = i2c_smbus_read_byte_data(new_client,
			      LM63_REG_CONFIG2);
		reg_alert_status = i2c_smbus_read_byte_data(new_client,
				   LM63_REG_ALERT_STATUS);
		reg_alert_mask = i2c_smbus_read_byte_data(new_client,
				 LM63_REG_ALERT_MASK);

		if (man_id == 0x01 /* National Semiconductor */
		 && chip_id == 0x41 /* LM63 */
		 && (reg_config1 & 0x18) == 0x00
		 && (reg_config2 & 0xF8) == 0x00
		 && (reg_alert_status & 0x20) == 0x00
		 && (reg_alert_mask & 0xA4) == 0xA4) {
			kind = lm63;
		} else {
#ifdef DEBUG
			printk(KERN_DEBUG "lm63: Unsupported chip "
			       "(man_id=0x%02X, chip_id=0x%02X)\n",
			       man_id, chip_id);
#endif			
			goto ERROR1;
		}
	}

	/* Fill in the remaining client fields */
	strcpy(new_client->name, "LM63 chip");
	data->valid = 0;
	init_MUTEX(&data->update_lock);

	/* Tell the I2C layer a new client has arrived */
	if ((err = i2c_attach_client(new_client))) {
		printk(KERN_ERR "lm63: Failed attaching client\n");
		goto ERROR1;
	}

	/* Register a new directory entry */
	if ((err = i2c_register_entry(new_client, "lm63",
	     lm63_dir_table_template, THIS_MODULE)) < 0) {
		printk(KERN_ERR "lm63: Failed registering directory entry\n");
		goto ERROR2;
	}
	data->sysctl_id = err;


	/* Initialize the LM63 chip */
	lm63_init_client(new_client);

	return 0;

ERROR2:
	i2c_detach_client(new_client);
ERROR1:
	kfree(data);
	return err;
}

/* Idealy we shouldn't have to initialize anything, since the BIOS
   should have taken care of everything */
static void lm63_init_client(struct i2c_client *client)
{
	struct lm63_data *data = client->data;

	data->config = i2c_smbus_read_byte_data(client, LM63_REG_CONFIG1);
	data->config_fan = i2c_smbus_read_byte_data(client,
						    LM63_REG_CONFIG_FAN);

	/* Start converting if needed */
	if (data->config & 0x40) { /* standby */
#ifdef DEBUG
		printk(KERN_DEBUG "lm63: Switching to operational mode");
#endif
		data->config &= 0xA7;
		i2c_smbus_write_byte_data(client, LM63_REG_CONFIG1,
					  data->config);
	}

	/* We may need pwm1_freq before ever updating the client data */
	data->pwm1_freq = i2c_smbus_read_byte_data(client, LM63_REG_PWM_FREQ);
	if (data->pwm1_freq == 0)
		data->pwm1_freq = 1;

#ifdef DEBUG
	/* Show some debug info about the LM63 configuration */
	printk(KERN_DEBUG "lm63: Alert/tach pin configured for %s\n",
		(data->config & 0x04) ? "tachometer input" :
		"alert output");
	printk(KERN_DEBUG "lm63: PWM clock %s kHz, output frequency %u Hz\n",
		(data->config_fan & 0x08) ? "1.4" : "360",
		((data->config_fan & 0x08) ? 700 : 180000) / data->pwm1_freq);
	printk(KERN_DEBUG "lm63: PWM output active %s, %s mode\n",
		(data->config_fan & 0x10) ? "low" : "high",
		(data->config_fan & 0x20) ? "manual" : "auto");
#endif
}

static int lm63_detach_client(struct i2c_client *client)
{
	struct lm63_data *data = client->data;
	int err;

	i2c_deregister_entry(data->sysctl_id);
	if ((err = i2c_detach_client(client))) {
		printk(KERN_ERR "lm63: Client deregistration failed, client "
		       "not detached.\n");
		return err;
	}

	kfree(data);
	return 0;
}

static  void lm63_update_client(struct i2c_client *client)
{
	struct lm63_data *data = client->data;

	down(&data->update_lock);

	if ((jiffies - data->last_updated > HZ) ||
	    (jiffies < data->last_updated) ||
	    !data->valid) {
		if (data->config & 0x04) { /* tachometer enabled  */
			/* order matters for fan1_input */
			data->fan1_input = i2c_smbus_read_byte_data(client,
					   LM63_REG_TACH_COUNT_LSB) & 0xFC;
			data->fan1_input |= i2c_smbus_read_byte_data(client,
					    LM63_REG_TACH_COUNT_MSB) << 8;
			data->fan1_low = (i2c_smbus_read_byte_data(client,
					  LM63_REG_TACH_LIMIT_LSB) & 0xFC)
				       | (i2c_smbus_read_byte_data(client,
					  LM63_REG_TACH_LIMIT_MSB) << 8);
		}

		data->pwm1_freq = i2c_smbus_read_byte_data(client,
				  LM63_REG_PWM_FREQ);
		if (data->pwm1_freq == 0)
			data->pwm1_freq = 1;
		data->pwm1_value = i2c_smbus_read_byte_data(client,
				   LM63_REG_PWM_VALUE);

		data->temp1_input = i2c_smbus_read_byte_data(client,
				    LM63_REG_LOCAL_TEMP);
		data->temp1_high = i2c_smbus_read_byte_data(client,
				   LM63_REG_LOCAL_HIGH);

		/* order matters for temp2_input */
		data->temp2_input = i2c_smbus_read_byte_data(client,
				    LM63_REG_REMOTE_TEMP_MSB) << 8;
		data->temp2_input |= i2c_smbus_read_byte_data(client,
				     LM63_REG_REMOTE_TEMP_LSB);
		data->temp2_high = (i2c_smbus_read_byte_data(client,
				   LM63_REG_REMOTE_HIGH_MSB) << 8)
				 | i2c_smbus_read_byte_data(client,
				   LM63_REG_REMOTE_HIGH_LSB);
		data->temp2_low = (i2c_smbus_read_byte_data(client,
				  LM63_REG_REMOTE_LOW_MSB) << 8)
				| i2c_smbus_read_byte_data(client,
				  LM63_REG_REMOTE_LOW_LSB);
		data->temp2_crit = i2c_smbus_read_byte_data(client,
				   LM63_REG_REMOTE_TCRIT);
		data->temp2_crit_hyst = i2c_smbus_read_byte_data(client,
					LM63_REG_REMOTE_TCRIT_HYST);

		/* Mask out Busy bit in status register */
		data->alarms = i2c_smbus_read_byte_data(client,
			       LM63_REG_ALERT_STATUS) & 0x7F;

		data->last_updated = jiffies;
		data->valid = 1;
	}

	up(&data->update_lock);
}

static void lm63_local_temp(struct i2c_client *client, int operation,
	int ctl_name, int *nrels_mag, long *results)
{
	struct lm63_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0; /* magnitude */
	else if (operation == SENSORS_PROC_REAL_READ) {
		lm63_update_client(client);
		results[0] = TEMP8_FROM_REG(data->temp1_high);
		results[1] = TEMP8_FROM_REG(data->temp1_input);
		*nrels_mag = 2;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->temp1_high = TEMP8_TO_REG(results[0]);
			i2c_smbus_write_byte_data(client, LM63_REG_LOCAL_HIGH,
				data->temp1_high);
		}
	}
}

static void lm63_remote_temp(struct i2c_client *client, int operation,
	int ctl_name, int *nrels_mag, long *results)
{
	struct lm63_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 3; /* magnitude */
	else if (operation == SENSORS_PROC_REAL_READ) {
		lm63_update_client(client);
		results[0] = TEMP11_FROM_REG(data->temp2_high);
		results[1] = TEMP11_FROM_REG(data->temp2_low);
		results[2] = TEMP11_FROM_REG(data->temp2_input);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->temp2_high = TEMP11_TO_REG(results[0]);
			i2c_smbus_write_byte_data(client,
				LM63_REG_REMOTE_HIGH_MSB,
				data->temp2_high >> 8);
			i2c_smbus_write_byte_data(client,
				LM63_REG_REMOTE_HIGH_LSB,
				data->temp2_high & 0xFF);
		}
		if (*nrels_mag >= 2) {
			data->temp2_low = TEMP11_TO_REG(results[1]);
			i2c_smbus_write_byte_data(client,
				LM63_REG_REMOTE_LOW_MSB,
				data->temp2_low >> 8);
			i2c_smbus_write_byte_data(client,
				LM63_REG_REMOTE_LOW_LSB,
				data->temp2_low & 0xFF);
		}
	}
}

static void lm63_remote_tcrit(struct i2c_client *client, int operation,
	int ctl_name, int *nrels_mag, long *results)
{
	struct lm63_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0; /* magnitude */
	else if (operation == SENSORS_PROC_REAL_READ) {
		lm63_update_client(client);
		results[0] = TEMP8_FROM_REG(data->temp2_crit);
		*nrels_mag = 1;
	}
}

static void lm63_remote_tcrit_hyst(struct i2c_client *client, int operation,
	int ctl_name, int *nrels_mag, long *results)
{
	struct lm63_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0; /* magnitude */
	else if (operation == SENSORS_PROC_REAL_READ) {
		lm63_update_client(client);
		results[0] = TEMP8_FROM_REG(data->temp2_crit) -
			     TEMP8_FROM_REG(data->temp2_crit_hyst);
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->temp2_crit_hyst = HYST_TO_REG(data->temp2_crit -
						results[0]);
			i2c_smbus_write_byte_data(client,
				LM63_REG_REMOTE_TCRIT_HYST,
				data->temp2_crit_hyst);
		}
	}
}

static void lm63_alarms(struct i2c_client *client, int operation,
	int ctl_name, int *nrels_mag, long *results)
{
	struct lm63_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0; /* magnitude */
	else if (operation == SENSORS_PROC_REAL_READ) {
		lm63_update_client(client);
		results[0] = data->alarms;
		*nrels_mag = 1;
	}
}

static void lm63_fan(struct i2c_client *client, int operation,
	int ctl_name, int *nrels_mag, long *results)
{
	struct lm63_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0; /* magnitude */
	else if (operation == SENSORS_PROC_REAL_READ) {
		if (!(data->config & 0x04)) { /* tachometer disabled */
			results[0] = 0;
			*nrels_mag = 1;
			return;
		}

		lm63_update_client(client);
		results[0] = FAN_FROM_REG(data->fan1_low);
		results[1] = FAN_FROM_REG(data->fan1_input);
		*nrels_mag = 2;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (!(data->config & 0x04)) /* tachometer disabled */
			return;

		if (*nrels_mag >= 1) {
			data->fan1_low = FAN_TO_REG(results[0]);
			i2c_smbus_write_byte_data(client,
				LM63_REG_TACH_LIMIT_LSB,
				data->fan1_low & 0xFF);
			i2c_smbus_write_byte_data(client,
				LM63_REG_TACH_LIMIT_MSB,
				data->fan1_low >> 8);
		}
	}
}

static void lm63_pwm(struct i2c_client *client, int operation,
	int ctl_name, int *nrels_mag, long *results)
{
	struct lm63_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0; /* magnitude */
	else if (operation == SENSORS_PROC_REAL_READ) {
		lm63_update_client(client);
		results[0] = data->pwm1_value >= 2 * data->pwm1_freq ? 255 :
			     (data->pwm1_value * 255 + data->pwm1_freq) /
			     (2 * data->pwm1_freq);
		results[1] = data->config_fan & 0x20 ? 1 : 2;
		*nrels_mag = 2;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1 && (data->config_fan & 0x20)) {
			data->pwm1_value = results[0] <= 0 ? 0 :
				results[0] >= 255 ? 2 * data->pwm1_freq :
				(results[0] * data->pwm1_freq * 2 + 127) / 255;
			i2c_smbus_write_byte_data(client, LM63_REG_PWM_VALUE,
				data->pwm1_value);
		}
	}
}

static int __init lm63_init(void)
{
	printk(KERN_INFO "lm63 version %s (%s)\n", LM_VERSION, LM_DATE);
	return i2c_add_driver(&lm63_driver);
}

static void __exit lm63_exit(void)
{
	i2c_del_driver(&lm63_driver);
}

MODULE_AUTHOR("Jean Delvare <khali@linux-fr.org>");
MODULE_DESCRIPTION("LM63 sensor driver");
MODULE_LICENSE("GPL");

module_init(lm63_init);
module_exit(lm63_exit);
