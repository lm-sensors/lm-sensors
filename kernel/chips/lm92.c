
/*
 * LM92 - Part of lm_sensors, Linux kernel modules for hardware
 *        monitoring
 *
 * Author: Abraham van der Merwe <abraham@2d3d.co.za>
 *
 * Linux support for the National Semiconductor LM92 Temperature
 * Sensor.
 *
 * Based on code from the lm-sensors project which is available
 * at http://www.lm-sensors.nu/. lm87.c have been particularly
 * helpful (:
 *
 * This source code is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/i2c-proc.h>
#include <linux/proc_fs.h>
#include <linux/sysctl.h>
#include <asm/semaphore.h>
#include "version.h"

/* if defined, 4 faults must occur consecutively to set alarm flags */
/* #define ENABLE_FAULT_QUEUE */

#define LM92_REG_TEMPERATURE		0x00	/* ro, 16-bit	*/
#define LM92_REG_CONFIGURATION		0x01	/* rw, 8-bit	*/
#define LM92_REG_TRIP_HYSTERESIS	0x02	/* rw, 16-bit	*/
#define LM92_REG_TRIP_CRITICAL		0x03	/* rw, 16-bit	*/
#define LM92_REG_TRIP_LOW			0x04	/* rw, 16-bit	*/
#define LM92_REG_TRIP_HIGH			0x05	/* rw, 16-bit	*/
#define LM92_REG_MANUFACTURER		0x07	/* ro, 16-bit	*/

#define LM92_MANUFACTURER_ID		0x8001

#define TEMP_MIN	(-4096)
#define TEMP_MAX	4095

#define LIMIT(x) do {							\
		if ((x) < TEMP_MIN) (x) = TEMP_MIN;		\
		if ((x) > TEMP_MAX) (x) = TEMP_MAX;		\
	} while (0)

#define PROC_TO_NATIVE(x) ((x) / 625)
#define NATIVE_TO_PROC(x) ((x) * 625)
#define CELSIUS(x) ((x) * 16)

static void lm92_temp (struct i2c_client *client,int operation,int ctl_name,int *nrels_mag,long *results);
static void lm92_alarms (struct i2c_client *client,int operation,int ctl_name,int *nrels_mag,long *results);

/* -- SENSORS SYSCTL START -- */
#define LM92_SYSCTL_ALARMS		2001	/* high, low, critical */
#define LM92_SYSCTL_TEMP		1200	/* high, low, critical, hysteresis, input */

#define LM92_ALARM_TEMP_HIGH	0x01
#define LM92_ALARM_TEMP_LOW		0x02
#define LM92_ALARM_TEMP_CRIT	0x04
#define LM92_TEMP_HIGH			0x08
#define LM92_TEMP_LOW			0x10
#define LM92_TEMP_CRIT			0x20
#define LM92_TEMP_HYST			0x40
#define LM92_TEMP_INPUT			0x80

/* -- SENSORS SYSCTL END -- */

static ctl_table lm92_dir_table[] = {
	{LM92_SYSCTL_TEMP, "temp", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &lm92_temp, NULL},
	{LM92_SYSCTL_ALARMS, "alarms", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &lm92_alarms, NULL},
	{0}
};

/* NOTE: all temperatures are degrees centigrade * 16 */
typedef struct {
	struct i2c_client client;
	int sysctl_id;
	unsigned long timestamp;
	struct {
		long high;
		long low;
		long crit;
		long hyst;
		long input;
	} temp;
	struct {
		long low;
		long high;
		long crit;
	} alarms;
} lm92_t;

/* this is needed for each client driver method */
static struct i2c_driver lm92_driver;

/* ensure exclusive access to chip and static variables */
static DECLARE_MUTEX (mutex);

/* addresses to scan */
static unsigned short normal_i2c[] = { SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { 0x48, 0x4f, SENSORS_I2C_END };
static unsigned int normal_isa[] = { SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/* insmod parameters */
SENSORS_INSMOD_1 (lm92);

static inline int lm92_write8 (struct i2c_client *client,u8 reg,u8 value)
{
	return (i2c_smbus_write_byte_data (client,reg,value) < 0 ? -EIO : 0);
}

static inline int lm92_read16 (struct i2c_client *client,u8 reg,u16 *value)
{
	s32 tmp = i2c_smbus_read_word_data (client,reg);

	if (tmp < 0) return (-EIO);

	/* convert the data to little endian format */
	*value = swab16((u16) tmp);

	return (0);
}

static inline int lm92_write16 (struct i2c_client *client,u8 reg,u16 value)
{
	/* convert the data to big endian format */
	if (i2c_smbus_write_word_data(client, reg, swab16(value)) < 0)
		return -EIO;

	return 0;
}

static int lm92_read (struct i2c_client *client)
{
	lm92_t *data = (lm92_t *) client->data;
	u16 value[5];

	if ((jiffies - data->timestamp) > HZ) {
		if (lm92_read16 (client,LM92_REG_TEMPERATURE,value) < 0 ||
			lm92_read16 (client,LM92_REG_TRIP_HYSTERESIS,value + 1) < 0 ||
			lm92_read16 (client,LM92_REG_TRIP_CRITICAL,value + 2) < 0 ||
			lm92_read16 (client,LM92_REG_TRIP_LOW,value + 3) < 0 ||
			lm92_read16 (client,LM92_REG_TRIP_HIGH,value + 4) < 0)
			return (-EIO);

		data->temp.input = (s16) value[0] >> 3;
		data->temp.hyst = (s16) value[1] >> 3;
		data->temp.crit = (s16) value[2] >> 3;
		data->temp.low = (s16) value[3] >> 3;
		data->temp.high = (s16) value[4] >> 3;

		data->alarms.low = value[0] & 1;
		data->alarms.high = (value[0] & 2) >> 1;
		data->alarms.crit = (value[0] & 4) >> 2;

		data->timestamp = jiffies;
	}

	return (0);
}

static int lm92_write (struct i2c_client *client)
{
	lm92_t *data = (lm92_t *) client->data;

	LIMIT (data->temp.hyst);
	LIMIT (data->temp.crit);
	LIMIT (data->temp.low);
	LIMIT (data->temp.high);

	if (lm92_write16 (client,LM92_REG_TRIP_HYSTERESIS,((s16) data->temp.hyst << 3)) < 0 ||
		lm92_write16 (client,LM92_REG_TRIP_CRITICAL,((s16) data->temp.crit << 3)) < 0 ||
		lm92_write16 (client,LM92_REG_TRIP_LOW,((s16) data->temp.low << 3)) < 0 ||
		lm92_write16 (client,LM92_REG_TRIP_HIGH,((s16) data->temp.high << 3)) < 0)
		return (-EIO);

	return (0);
}

static void lm92_temp (struct i2c_client *client,int operation,int ctl_name,int *nrels_mag,long *results)
{
	if (!down_interruptible (&mutex)) {
		lm92_t *data = (lm92_t *) client->data;

		if (operation == SENSORS_PROC_REAL_READ) {
			lm92_read (client);
			results[0] = NATIVE_TO_PROC (data->temp.input);
			results[1] = NATIVE_TO_PROC (data->temp.high);
			results[2] = NATIVE_TO_PROC (data->temp.low);
			results[3] = NATIVE_TO_PROC (data->temp.crit);
			results[4] = NATIVE_TO_PROC (data->temp.hyst);
			*nrels_mag = 5;
		} else if (operation == SENSORS_PROC_REAL_WRITE && *nrels_mag == 4) {
			data->temp.high = PROC_TO_NATIVE (results[0]);
			data->temp.low = PROC_TO_NATIVE (results[1]);
			data->temp.crit = PROC_TO_NATIVE (results[2]);
			data->temp.hyst = PROC_TO_NATIVE (results[3]);
			lm92_write (client);
		} else if (operation == SENSORS_PROC_REAL_INFO) {
			*nrels_mag = 4;
		}

		up (&mutex);
	}
}

static void lm92_alarms (struct i2c_client *client,int operation,int ctl_name,int *nrels_mag,long *results)
{
	if (!down_interruptible (&mutex)) {
		lm92_t *data = (lm92_t *) client->data;

		if (operation == SENSORS_PROC_REAL_READ) {
			lm92_read (client);
			results[0] = data->alarms.high || (data->alarms.low << 1) || (data->alarms.crit << 2);
			*nrels_mag = 1;
		} else if (operation == SENSORS_PROC_REAL_INFO) {
			*nrels_mag = 0;
		}

		up (&mutex);
	}
}

static int max6635_check(struct i2c_client *client)
{
	int i;
	u16 temp_low, temp_high, temp_hyst, temp_crit;
	u8 conf;

	temp_low = i2c_smbus_read_word_data(client, LM92_REG_TRIP_LOW);
	temp_high = i2c_smbus_read_word_data(client, LM92_REG_TRIP_HIGH);
	temp_hyst = i2c_smbus_read_word_data(client, LM92_REG_TRIP_HYSTERESIS);
	temp_crit = i2c_smbus_read_word_data(client, LM92_REG_TRIP_CRITICAL);
	
	if ((temp_low & 0x7f00) || (temp_high & 0x7f00)
	 || (temp_hyst & 0x7f00) || (temp_crit & 0x7f00))
		return 0;

	conf = i2c_smbus_read_byte_data(client, LM92_REG_CONFIGURATION);

	for (i=0; i<128; i+=16) {
		if (temp_low != i2c_smbus_read_word_data(client, LM92_REG_TRIP_LOW + i)
		 || temp_high != i2c_smbus_read_word_data(client, LM92_REG_TRIP_HIGH + i)
		 || temp_hyst != i2c_smbus_read_word_data(client, LM92_REG_TRIP_HYSTERESIS + i)
		 || temp_crit != i2c_smbus_read_word_data(client, LM92_REG_TRIP_CRITICAL + i)
		 || conf != i2c_smbus_read_byte_data(client, LM92_REG_CONFIGURATION + i))
			return 0;
	}
	
	return 1;
}

static int lm92_init_client (struct i2c_client *client)
{
	lm92_t *data = (lm92_t *) client->data;
	u8 value = 0;
	int result;

	/* force reads to query the chip */
	data->timestamp = 0;

	/* setup the configuration register */

#ifdef ENABLE_FAULT_QUEUE
	value |= 0x10;
#endif	/* #ifdef ENABLE_FAULT_QUEUE */

	if (lm92_write8 (client,LM92_REG_CONFIGURATION,value) < 0)
		return (-ENODEV);

	/* set default alarm trigger values */

	data->temp.high = CELSIUS (64);
	data->temp.low = CELSIUS (10);
	data->temp.crit = CELSIUS (80);
	data->temp.hyst = CELSIUS (2);

	if ((result = lm92_write (client)) < 0)
		return (result);

	/* read everything once so that our cached data is updated */

	if ((result = lm92_read (client)) < 0)
		return (result);

	return (0);
}

static int lm92_detect (struct i2c_adapter *adapter,int address,unsigned short flags,int kind)
{
	struct i2c_client *client;
	lm92_t *data;
	int result = 0;
	u16 manufacturer;

	if (!i2c_check_functionality (adapter,I2C_FUNC_SMBUS_BYTE_DATA))
		return 0;

	if (!(data = kmalloc(sizeof(lm92_t), GFP_KERNEL)))
		return (-ENOMEM);

	client = &data->client;
	client->addr = address;
	client->data = data;
	client->adapter = adapter;
	client->driver = &lm92_driver;
	client->flags = 0;
	strcpy (client->name,lm92_driver.name);

	if (down_interruptible (&mutex)) {
		result = -ERESTARTSYS;
		goto ERROR1;
	}

	if (kind < 0) {
		/* Is it an lm92? */
		if (address < 0x4c
		 && (lm92_read16(client,LM92_REG_MANUFACTURER,&manufacturer) < 0
		  || manufacturer != LM92_MANUFACTURER_ID)) {
		  	/* Is it a MAX6635/MAX6635/MAX6635? */
			if (!max6635_check(client)) {
				goto ERROR2;
			}
		}
	}

	if ((result = i2c_attach_client (client))) {
		goto ERROR2;
	}

	if ((result = i2c_register_entry(client, client->name, lm92_dir_table,
					 THIS_MODULE)) < 0) {
		goto ERROR3;
	}
	data->sysctl_id = result;

	if ((result = lm92_init_client (client)) < 0) {
		goto ERROR4;
	}

	up (&mutex);

	return (0);

ERROR4:
	i2c_deregister_entry(data->sysctl_id);
ERROR3:
	i2c_detach_client(client);
ERROR2:
	up(&mutex);
ERROR1:
	kfree(data);
	return result;
}

static int lm92_attach_adapter (struct i2c_adapter *adapter)
{
	return i2c_detect (adapter,&addr_data,lm92_detect);
}

static int lm92_detach_client (struct i2c_client *client)
{
	int result;

	i2c_deregister_entry (((lm92_t *) (client->data))->sysctl_id);

	if ((result = i2c_detach_client (client)))
		return (result);

	kfree(client->data);

	return (0);
}


static struct i2c_driver lm92_driver = {
	.name		= "lm92",
	.id		= I2C_DRIVERID_LM92,
	.flags		= I2C_DF_NOTIFY,
	.attach_adapter	= lm92_attach_adapter,
	.detach_client	= lm92_detach_client,
};

static int __init sm_lm92_init(void)
{
	printk ("lm92.o version %s (%s)\n",LM_VERSION,LM_DATE);
	return i2c_add_driver(&lm92_driver);
}


static void __exit sm_lm92_exit(void)
{
	i2c_del_driver(&lm92_driver);
}



MODULE_AUTHOR ("Abraham van der Merwe <abraham@2d3d.co.za>");
MODULE_DESCRIPTION ("Linux support for LM92 Temperature Sensor");

MODULE_LICENSE ("GPL");

module_init(sm_lm92_init);
module_exit(sm_lm92_exit);

