
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
#include <linux/version.h>
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

/* if defined, 4 faults must occur consecutively to set alarm flags */
/* #define ENABLE_FAULT_QUEUE */

#include "version.h"
#include "sensors.h"

#define LM92_REG_TEMPERATURE		0x00	/* ro, 16-bit	*/
#define LM92_REG_CONFIGURATION		0x01	/* rw, 8-bit	*/
#define LM92_REG_TRIP_HYSTERISIS	0x02	/* rw, 16-bit	*/
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

#define ENTRY(name,proc,perm,callback)		\
	{										\
		ctl_name:		name,				\
		procname:		proc,				\
		data:			NULL,				\
		maxlen:			0,					\
		mode:			perm,				\
		child:			NULL,				\
		proc_handler:	&i2c_proc_real,		\
		strategy:		&i2c_sysctl_real,	\
		de:				NULL,				\
		extra1:			callback,			\
		extra2:			NULL				\
	}

/* NOTE: all temperatures are degrees centigrade * 16 */
typedef struct {
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
static unsigned short normal_i2c_range[] = { 0x48, 0x4b, SENSORS_I2C_END };
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
	*value = ((u16) tmp >> 8) | (u16) ((u16) tmp << 8);

	return (0);
}

static inline int lm92_write16 (struct i2c_client *client,u8 reg,u16 value)
{
	/* convert the data to big endian format */
	u16 be = (value >> 8) | (u16) (value << 8);
	return (i2c_smbus_write_word_data (client,reg,be) < 0 ? -EIO : 0);
}

static int lm92_read (struct i2c_client *client)
{
	lm92_t *data = (lm92_t *) client->data;
	u16 value[5];

	if ((jiffies - data->timestamp) > HZ) {
		if (lm92_read16 (client,LM92_REG_TEMPERATURE,value) < 0 ||
			lm92_read16 (client,LM92_REG_TRIP_HYSTERISIS,value + 1) < 0 ||
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

	if (lm92_write16 (client,LM92_REG_TRIP_HYSTERISIS,((s16) data->temp.hyst << 3)) < 0 ||
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
	static ctl_table dir_table[] = {
		ENTRY (LM92_SYSCTL_TEMP,"temp",0644,&lm92_temp),
		ENTRY (LM92_SYSCTL_ALARMS,"alarms",0444,&lm92_alarms),
		{ 0 }
	};
	static int id = 0;
	struct i2c_client *client;
	lm92_t *data;
	int result;
	u16 manufacturer;

	if (!i2c_check_functionality (adapter,I2C_FUNC_SMBUS_BYTE_DATA))
		return (-ENODEV);

	if ((client = kmalloc (sizeof (struct i2c_client) + sizeof (lm92_t),GFP_KERNEL)) == NULL)
		return (-ENOMEM);

	data = (lm92_t *) (client + 1);
	client->addr = address;
	client->data = data;
	client->adapter = adapter;
	client->driver = &lm92_driver;
	client->flags = 0;
	strcpy (client->name,lm92_driver.name);

	if (down_interruptible (&mutex)) {
		kfree (client);
		return (-ERESTARTSYS);
	}

	if ((kind < 0 && lm92_read16 (client,LM92_REG_MANUFACTURER,&manufacturer) < 0) ||
		manufacturer != LM92_MANUFACTURER_ID) {
		kfree (client);
		up (&mutex);
		return (-ENODEV);
	}

	if ((result = i2c_attach_client (client))) {
		kfree (client);
		up (&mutex);
		return (result);
	}

	if ((result = i2c_register_entry (client,client->name,dir_table,THIS_MODULE)) < 0) {
		i2c_detach_client (client);
		kfree (client);
		up (&mutex);
		return (result);
	}
	data->sysctl_id = result;

	if ((result = lm92_init_client (client)) < 0) {
		i2c_deregister_entry (data->sysctl_id);
		i2c_detach_client (client);
		kfree (client);
		up (&mutex);
		return (result);
	}

	client->id = id++;

	up (&mutex);

	return (0);
}

static int lm92_attach_adapter (struct i2c_adapter *adapter)
{
	int result;
	struct i2c_client_address_data lm92_client_data = {
		normal_i2c:			addr_data.normal_i2c,
		normal_i2c_range:	addr_data.normal_i2c_range,
		probe:				addr_data.probe,
		probe_range:		addr_data.probe_range,
		ignore:				addr_data.ignore,
		ignore_range:		addr_data.ignore_range,
		force:				addr_data.forces->force
	};

	if (!(result = i2c_probe (adapter,&lm92_client_data,lm92_detect)))
		result = i2c_detect (adapter,&addr_data,lm92_detect);

	return (result);
}

static int lm92_detach_client (struct i2c_client *client)
{
	int result;

	i2c_deregister_entry (((lm92_t *) (client->data))->sysctl_id);

	if ((result = i2c_detach_client (client)))
		return (result);

	kfree (client);

	return (0);
}

static int lm92_command (struct i2c_client *client,unsigned int cmd,void *arg)
{
	return (0);
}

static void lm92_inc_use (struct i2c_client *client)
{
#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif	/* #ifdef MODULE */
}

static void lm92_dec_use (struct i2c_client *client)
{
#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif	/* #ifdef MODULE */
}

#ifndef I2C_DRIVERID_LM92
#define I2C_DRIVERID_LM92 1033
#endif	/* #ifndef I2C_DRIVERID_LM92 */

static struct i2c_driver lm92_driver = {
	name:			"lm92",
	id:				I2C_DRIVERID_LM92,
	flags:			I2C_DF_NOTIFY,
	attach_adapter:	lm92_attach_adapter,
	detach_client:	lm92_detach_client,
	command:		lm92_command,
	inc_use:		lm92_inc_use,
	dec_use:		lm92_dec_use
};

static int __init sensors_lm92_init (void)
{
	int result;

	if ((result = i2c_add_driver (&lm92_driver)))
		return (result);

	printk ("lm92.o version %s (%s)\n",LM_VERSION,LM_DATE);

	return (0);
}

static void __exit sensors_lm92_exit (void)
{
	i2c_del_driver (&lm92_driver);
}

EXPORT_NO_SYMBOLS;

MODULE_AUTHOR ("Abraham van der Merwe <abraham@2d3d.co.za>");
MODULE_DESCRIPTION ("Linux support for LM92 Temperature Sensor");

#ifdef MODULE_LICENSE
MODULE_LICENSE ("GPL");
#endif	/* #ifdef MODULE_LICENSE */

module_init (sensors_lm92_init);
module_exit (sensors_lm92_exit);

