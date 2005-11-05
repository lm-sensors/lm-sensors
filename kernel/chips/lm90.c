/*
 * lm90.c - Part of lm_sensors, Linux kernel modules for hardware
 *          monitoring
 * Copyright (C) 2003-2005  Jean Delvare <khali@linux-fr.org>
 *
 * Based on the lm83 driver. The LM90 is a sensor chip made by National
 * Semiconductor. It reports up to two temperatures (its own plus up to
 * one external one) with a 0.125 deg resolution (1 deg for local
 * temperature) and a 3-4 deg accuracy. Complete datasheet can be
 * obtained from National's website at:
 *   http://www.national.com/pf/LM/LM90.html
 *
 * This driver also supports the LM89 and LM99, two other sensor chips
 * made by National Semiconductor. Both have an increased remote
 * temperature measurement accuracy (1 degree), and the LM99
 * additionally shifts remote temperatures (measured and limits) by 16
 * degrees, which allows for higher temperatures measurement. The
 * driver doesn't handle it since it can be done easily in user-space.
 * Complete datasheets can be obtained from National's website at:
 *   http://www.national.com/pf/LM/LM89.html
 *   http://www.national.com/pf/LM/LM99.html
 * Note that there is no way to differentiate between both chips.
 *
 * This driver also supports the LM86, another sensor chip made by
 * National Semiconductor. It is exactly similar to the LM90 except it
 * has a higher accuracy.
 * Complete datasheet can be obtained from National's website at:
 *   http://www.national.com/pf/LM/LM86.html
 *
 * This driver also supports the ADM1032, a sensor chip made by Analog
 * Devices. That chip is similar to the LM90, with a few differences
 * that are not handled by this driver. Complete datasheet can be
 * obtained from Analog's website at:
 *   http://products.analog.com/products/info.asp?product=ADM1032
 * Among others, it has a higher accuracy than the LM90, much like the
 * LM86 does.
 *
 * This driver also supports the MAX6657, MAX6658 and MAX6659 sensor
 * chips made by Maxim. These chips are similar to the LM86. Complete
 * datasheet can be obtained at Maxim's website at:
 *   http://www.maxim-ic.com/quick_view2.cfm/qv_pk/2578
 * Note that there is no easy way to differentiate between the three
 * variants. The extra address and features of the MAX6659 are not
 * supported by this driver.
 *
 * This driver also supports the ADT7461 chip from Analog Devices but
 * only in its "compatability mode". If an ADT7461 chip is found but
 * is configured in non-compatible mode (where its temperature
 * register values are decoded differently) it is ignored by this
 * driver. Complete datasheet can be obtained from Analog's website
 * at:
 *   http://products.analog.com/products/info.asp?product=ADT7461
 *
 * Since the LM90 was the first chipset supported by this driver, most
 * comments will refer to this chipset, but are actually general and
 * concern all supported chipsets, unless mentioned otherwise.
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

#ifndef I2C_DRIVERID_LM90
#define I2C_DRIVERID_LM90	1042
#endif

/*
 * Addresses to scan
 * Address is fully defined internally and cannot be changed except for
 * MAX6659.
 * LM86, LM89, LM90, LM99, ADM1032, ADM1032-1, ADT7461, MAX6657 and MAX6658
 * have address 0x4c.
 * ADM1032-2, ADT7461-2, LM89-1, and LM99-1 have address 0x4d.
 * MAX6659 can have address 0x4c, 0x4d or 0x4e (unsupported).
 */

static unsigned short normal_i2c[] = { 0x4c, 0x4d, SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { SENSORS_I2C_END };
static unsigned int normal_isa[] = { SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/*
 * Insmod parameters
 */

SENSORS_INSMOD_6(lm90, adm1032, lm99, lm86, max6657, adt7461);

/*
 * The LM90 registers
 */

#define LM90_REG_R_MAN_ID        0xFE
#define LM90_REG_R_CHIP_ID       0xFF
#define LM90_REG_R_CONFIG1       0x03
#define LM90_REG_W_CONFIG1       0x09
#define LM90_REG_R_CONFIG2       0xBF
#define LM90_REG_W_CONFIG2       0xBF
#define LM90_REG_R_CONVRATE      0x04
#define LM90_REG_W_CONVRATE      0x0A
#define LM90_REG_R_STATUS        0x02
#define LM90_REG_R_LOCAL_TEMP    0x00
#define LM90_REG_R_LOCAL_HIGH    0x05
#define LM90_REG_W_LOCAL_HIGH    0x0B
#define LM90_REG_R_LOCAL_LOW     0x06
#define LM90_REG_W_LOCAL_LOW     0x0C
#define LM90_REG_R_LOCAL_CRIT    0x20
#define LM90_REG_W_LOCAL_CRIT    0x20
#define LM90_REG_R_REMOTE_TEMPH  0x01
#define LM90_REG_R_REMOTE_TEMPL  0x10
#define LM90_REG_R_REMOTE_OFFSH  0x11
#define LM90_REG_W_REMOTE_OFFSH  0x11
#define LM90_REG_R_REMOTE_OFFSL  0x12
#define LM90_REG_W_REMOTE_OFFSL  0x12
#define LM90_REG_R_REMOTE_HIGHH  0x07
#define LM90_REG_W_REMOTE_HIGHH  0x0D
#define LM90_REG_R_REMOTE_HIGHL  0x13
#define LM90_REG_W_REMOTE_HIGHL  0x13
#define LM90_REG_R_REMOTE_LOWH   0x08
#define LM90_REG_W_REMOTE_LOWH   0x0E
#define LM90_REG_R_REMOTE_LOWL   0x14
#define LM90_REG_W_REMOTE_LOWL   0x14
#define LM90_REG_R_REMOTE_CRIT   0x19
#define LM90_REG_W_REMOTE_CRIT   0x19
#define LM90_REG_R_TCRIT_HYST    0x21
#define LM90_REG_W_TCRIT_HYST    0x21

/*
 * Conversions and various macros
 * For local temperatures and limits, critical limits and the hysteresis
 * value, the LM90 uses signed 8-bit values with LSB = 1 degree Celsius.
 * For remote temperatures and limits, it uses signed 11-bit values with
 * LSB = 0.125 degree Celsius, left-justified in 16-bit registers.
 */

#define TEMP1_FROM_REG(val)	(val)
#define TEMP1_TO_REG(val)	((val) <= -128 ? -128 : \
				 (val) >= 127 ? 127 : (val))
#define TEMP2_FROM_REG(val)	((val) / 32 * 125 / 100)
#define TEMP2_TO_REG(val)	((val) <= -1280 ? 0x8000 : \
				 (val) >= 1270 ? 0x7FE0 : \
				 ((val) * 100 / 125 * 32))
#define HYST_TO_REG(val)	((val) <= 0 ? 0 : \
				 (val) >= 31 ? 31 : (val))

/*
 * ADT7461 is almost identical to LM90 except that attempts to write
 * values that are outside the range 0 < temp < 127 are treated as
 * the boundary value.
 */

#define TEMP1_TO_REG_ADT7461(val) ((val) <= 0 ? 0 : \
				 (val) >= 127 ? 127 : (val))
#define TEMP2_TO_REG_ADT7461(val) ((val) <= 0 ? 0 : \
				 (val) >= 1277 ? 0x7FC0 : \
				 ((val) * 100 / 250 * 64))

/*
 * Functions declaration
 */

static int lm90_attach_adapter(struct i2c_adapter *adapter);
static int lm90_detect(struct i2c_adapter *adapter, int address,
	unsigned short flags, int kind);
static void lm90_init_client(struct i2c_client *client);
static int lm90_detach_client(struct i2c_client *client);
static void lm90_local_temp(struct i2c_client *client, int operation,
	int ctl_name, int *nrels_mag, long *results);
static void lm90_remote_temp(struct i2c_client *client, int operation,
	int ctl_name, int *nrels_mag, long *results);
static void lm90_local_tcrit(struct i2c_client *client, int operation,
	int ctl_name, int *nrels_mag, long *results);
static void lm90_remote_tcrit(struct i2c_client *client, int operation,
	int ctl_name, int *nrels_mag, long *results);
static void lm90_local_hyst(struct i2c_client *client, int operation,
	int ctl_name, int *nrels_mag, long *results);
static void lm90_remote_hyst(struct i2c_client *client, int operation,
	int ctl_name, int *nrels_mag, long *results);
static void lm90_alarms(struct i2c_client *client, int operation,
	int ctl_name, int *nrels_mag, long *results);
static void adm1032_pec(struct i2c_client *client, int operation,
	int ctl_name, int *nrels_mag, long *results);

/*
 * Driver data (common to all clients)
 */

static struct i2c_driver lm90_driver = {
	.name           = "LM90/ADM1032 sensor driver",
	.id             = I2C_DRIVERID_LM90,
	.flags          = I2C_DF_NOTIFY,
	.attach_adapter = lm90_attach_adapter,
	.detach_client  = lm90_detach_client
};

/*
 * Client data (each client gets its own)
 */

struct lm90_data {
	struct i2c_client client;
	int sysctl_id;

	struct semaphore update_lock;
	char valid; /* zero until following fields are valid */
	unsigned long last_updated; /* in jiffies */
	int kind;

	/* registers values */
	s8 local_temp, local_high, local_low;
	s16 remote_temp, remote_high, remote_low; /* combined */
	s8 local_crit, remote_crit;
	u8 hyst; /* linked to two sysctl files (hyst1 RW, hyst2 RO) */
	u8 alarms; /* bitvector */
};

/*
 * Proc entries
 * These files are created for each detected LM90.
 */

/* -- SENSORS SYSCTL START -- */

#define LM90_SYSCTL_LOCAL_TEMP    1200
#define LM90_SYSCTL_REMOTE_TEMP   1201
#define LM90_SYSCTL_LOCAL_TCRIT   1204
#define LM90_SYSCTL_REMOTE_TCRIT  1205
#define LM90_SYSCTL_LOCAL_HYST    1207
#define LM90_SYSCTL_REMOTE_HYST   1208
#define LM90_SYSCTL_ALARMS        1210
#define LM90_SYSCTL_PEC           1214

#define LM90_ALARM_LOCAL_HIGH     0x40
#define LM90_ALARM_LOCAL_LOW      0x20
#define LM90_ALARM_LOCAL_CRIT     0x01
#define LM90_ALARM_REMOTE_HIGH    0x10
#define LM90_ALARM_REMOTE_LOW     0x08
#define LM90_ALARM_REMOTE_CRIT    0x02
#define LM90_ALARM_REMOTE_OPEN    0x04

/* -- SENSORS SYSCTL END -- */


static ctl_table lm90_dir_table_template[] =
{
	{LM90_SYSCTL_LOCAL_TEMP, "temp1", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &lm90_local_temp},
	{LM90_SYSCTL_REMOTE_TEMP, "temp2", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &lm90_remote_temp},
	{LM90_SYSCTL_LOCAL_TCRIT, "tcrit1", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &lm90_local_tcrit},
	{LM90_SYSCTL_REMOTE_TCRIT, "tcrit2", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &lm90_remote_tcrit},
	{LM90_SYSCTL_LOCAL_HYST, "hyst1", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &lm90_local_hyst},
	{LM90_SYSCTL_REMOTE_HYST, "hyst2", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &lm90_remote_hyst},
	{LM90_SYSCTL_ALARMS, "alarms", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &lm90_alarms},
	{0}
};

static ctl_table adm1032_dir_table_template[] =
{
	{LM90_SYSCTL_LOCAL_TEMP, "temp1", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &lm90_local_temp},
	{LM90_SYSCTL_REMOTE_TEMP, "temp2", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &lm90_remote_temp},
	{LM90_SYSCTL_LOCAL_TCRIT, "tcrit1", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &lm90_local_tcrit},
	{LM90_SYSCTL_REMOTE_TCRIT, "tcrit2", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &lm90_remote_tcrit},
	{LM90_SYSCTL_LOCAL_HYST, "hyst1", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &lm90_local_hyst},
	{LM90_SYSCTL_REMOTE_HYST, "hyst2", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &lm90_remote_hyst},
	{LM90_SYSCTL_ALARMS, "alarms", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &lm90_alarms},
	{LM90_SYSCTL_PEC, "pec", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &adm1032_pec},
	{0}
};

/*
 * Real code
 */

/* The ADM1032 supports PEC but not on write byte transactions, so we need
   to explicitely ask for a transaction without PEC. */
static inline s32 adm1032_write_byte(struct i2c_client *client, u8 value)
{
	return i2c_smbus_xfer(client->adapter, client->addr,
			      client->flags & ~I2C_CLIENT_PEC,
			      I2C_SMBUS_WRITE, value, I2C_SMBUS_BYTE, NULL);
}

/* It is assumed that client->update_lock is held (unless we are in
   detection or initialization steps). This matters when PEC is enabled,
   because we don't want the address pointer to change between the write
   byte and the read byte transactions. */
static int lm90_read_reg(struct i2c_client* client, u8 reg, u8 *value)
{
	int err;

 	if (client->flags & I2C_CLIENT_PEC) {
 		err = adm1032_write_byte(client, reg);
 		if (err >= 0)
 			err = i2c_smbus_read_byte(client);
 	} else
 		err = i2c_smbus_read_byte_data(client, reg);

	if (err < 0) {
		printk(KERN_WARNING "lm90: Register 0x%02x read failed (%d)\n",
		       reg, err);
		return err;
	}
	*value = err;

	return 0;
}

static int lm90_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_detect(adapter, &addr_data, lm90_detect);
}

/*
 * The following function does more than just detection. If detection
 * succeeds, it also registers the new chip.
 */
static int lm90_detect(struct i2c_adapter *adapter, int address,
	unsigned short flags, int kind)
{
	struct i2c_client *new_client;
	struct lm90_data *data;
	int err = 0;
	const char *type_name = "";
	const char *client_name = "";

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
#ifdef DEBUG
		printk(KERN_DEBUG "lm90: adapter doesn't support byte mode, "
		       "skipping\n");
#endif
		return 0;
	}

	if (!(data = kmalloc(sizeof(struct lm90_data), GFP_KERNEL))) {
		printk(KERN_ERR "lm90: Out of memory in lm90_detect\n");
		return -ENOMEM;
	}

	/*
	 * The common I2C client data is placed right before the
	 * LM90-specific data. The LM90-specific data is pointed to by the
	 * data field from the I2C client data.
	 */

	new_client = &data->client;
	new_client->addr = address;
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &lm90_driver;
	new_client->flags = 0;

	/*
	 * Now we do the remaining detection. A negative kind means that
	 * the driver was loaded with no force parameter (default), so we
	 * must both detect and identify the chip. A zero kind means that
	 * the driver was loaded with the force parameter, the detection
	 * step shall be skipped. A positive kind means that the driver
	 * was loaded with the force parameter and a given kind of chip is
	 * requested, so both the detection and the identification steps
	 * are skipped.
	 */

	/* Default to an LM90 if forced */
	if (kind == 0)
		kind = lm90;

	if (kind < 0) { /* detection and identification */
		u8 man_id, chip_id, reg_config1, reg_convrate;

		if (lm90_read_reg(new_client, LM90_REG_R_MAN_ID,
				  &man_id) < 0
		 || lm90_read_reg(new_client, LM90_REG_R_CHIP_ID,
		 		  &chip_id) < 0
		 || lm90_read_reg(new_client, LM90_REG_R_CONFIG1,
		 		  &reg_config1) < 0
		 || lm90_read_reg(new_client, LM90_REG_R_CONVRATE,
		 		  &reg_convrate) < 0)
			goto exit_free;

		if (man_id == 0x01) { /* National Semiconductor */
			u8 reg_config2;

			if (lm90_read_reg(new_client, LM90_REG_R_CONFIG2,
					  &reg_config2) < 0)
				goto exit_free;

			if ((reg_config1 & 0x2A) == 0x00
			 && (reg_config2 & 0xF8) == 0x00
			 && reg_convrate <= 0x09) {
				if (address == 0x4C
				 && (chip_id & 0xF0) == 0x20) /* LM90 */
					kind = lm90;
				else if ((chip_id & 0xF0) == 0x30) /* LM89/LM99 */
					kind = lm99;
				else if (address == 0x4C
				 && (chip_id & 0xF0) == 0x10) /* LM86 */
					kind = lm99;
			}
		} else
		if (man_id == 0x41) { /* Analog Devices */
			if ((chip_id & 0xF0) == 0x40 /* ADM1032 */
			 && (reg_config1 & 0x3F) == 0x00
			 && reg_convrate <= 0x0A)
				kind = adm1032;
			else
			if (chip_id == 0x51 /* ADT7461 */
			 && (reg_config1 & 0x1F) == 0x00 /* check compat mode */
			 && reg_convrate <= 0x0A)
				kind = adt7461;
		} else
		if (man_id == 0x4D) { /* Maxim */
 			/*
 			 * The Maxim variants do NOT have a chip_id register.
 			 * Reading from that address will return the last read
 			 * value, which in our case is those of the man_id
 			 * register. Likewise, the config1 register seems to
 			 * lack a low nibble, so the value will be those of the
 			 * previous read, so in our case those of the man_id
 			 * register.
 			 */
			if (chip_id == man_id
			 && (reg_config1 & 0x1F) == (man_id & 0x0F)
			 && reg_convrate <= 0x09)
				kind = max6657;
		}
	}

	if (kind <= 0) { /* identification failed */
		printk(KERN_INFO "lm90: Unsupported chip\n");
		goto exit_free;
	}

	if (kind == lm90) {
		type_name = "lm90";
		client_name = "LM90 chip";
	} else if (kind == adm1032) {
		type_name = "adm1032";
		client_name = "ADM1032 chip";
		/* The ADM1032 supports PEC, but only if combined
		   transactions are not used. */
		if (i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE)) {
			new_client->flags |= I2C_CLIENT_PEC;
			printk(KERN_DEBUG "lm90: Enabling PEC for ADM1032\n");
		}
	} else if (kind == lm99) {
		type_name = "lm99";
		client_name = "LM99 chip";
	} else if (kind == lm86) {
		type_name = "lm86";
		client_name = "LM86 chip";
	} else if (kind == max6657) {
		type_name = "max6657";
		client_name = "MAX6657 chip";
	} else if (kind == adt7461) {
		type_name = "adt7461";
		client_name = "ADT7561 chip";
	} else {
		printk(KERN_ERR "lm90: Unknown kind %d\n", kind);
		goto exit_free;
	}

	/*
	 * OK, we got a valid chip so we can fill in the remaining client
	 * fields.
	 */

	strcpy(new_client->name, client_name);
	data->valid = 0;
	data->kind = kind;
	init_MUTEX(&data->update_lock);

	/*
	 * Tell the I2C layer a new client has arrived.
	 */

	if ((err = i2c_attach_client(new_client))) {
		printk(KERN_ERR "lm90: Failed to attach client (%d)\n", err);
		goto exit_free;
	}

	/*
	 * Register a new directory entry.
	 */

	if ((err = i2c_register_entry(new_client, type_name,
	     (new_client->flags & I2C_CLIENT_PEC) ?
	     adm1032_dir_table_template :
	     lm90_dir_table_template, THIS_MODULE)) < 0) {
		printk(KERN_ERR "lm90: Failed to register directory entry "
		       "(%d)\n", err);
		goto exit_detach;
	}
	data->sysctl_id = err;

	/*
	 * Initialize the LM90 chip.
	 */

	lm90_init_client(new_client);
	return 0;

exit_detach:
	i2c_detach_client(new_client);
exit_free:
	kfree(data);
	return err;
}

static void lm90_init_client(struct i2c_client *client)
{
	u8 config;

	/*
	 * Start the conversions.
	 */

	i2c_smbus_write_byte_data(client, LM90_REG_W_CONVRATE,
		5); /* 2 Hz */
	if (lm90_read_reg(client, LM90_REG_R_CONFIG1, &config) < 0) {
		printk(KERN_ERR "lm90: Initialization failed!\n");
		return;
	}
	if (config & 0x40)
		i2c_smbus_write_byte_data(client, LM90_REG_W_CONFIG1,
			config & 0xBF); /* run */
}


static int lm90_detach_client(struct i2c_client *client)
{
	int err;

	i2c_deregister_entry(((struct lm90_data *) (client->data))->sysctl_id);
	if ((err = i2c_detach_client(client))) {
		printk(KERN_ERR "lm90: Client deregistration failed, client "
		       "not detached (%d)\n", err);
		return err;
	}

	kfree(client->data);
	return 0;
}

static void lm90_update_client(struct i2c_client *client)
{
	struct lm90_data *data = client->data;

	down(&data->update_lock);

	if ((jiffies - data->last_updated > HZ * 2) ||
	    (jiffies < data->last_updated) || !data->valid) {
		u8 oldh, newh, l;
#ifdef DEBUG
		printk(KERN_DEBUG "lm90: Updating register values\n");
#endif

		lm90_read_reg(client, LM90_REG_R_LOCAL_TEMP,
			      &data->local_temp);
		lm90_read_reg(client, LM90_REG_R_LOCAL_HIGH,
			      &data->local_high);
		lm90_read_reg(client, LM90_REG_R_LOCAL_LOW,
			      &data->local_low);
		lm90_read_reg(client, LM90_REG_R_LOCAL_CRIT,
			      &data->local_crit);
		lm90_read_reg(client, LM90_REG_R_REMOTE_CRIT,
			      &data->remote_crit);
		lm90_read_reg(client, LM90_REG_R_TCRIT_HYST,
			      &data->hyst);

		/*
		 * There is a trick here. We have to read two registers to
		 * have the remote sensor temperature, but we have to beware
		 * a conversion could occur inbetween the readings. The
		 * datasheet says we should either use the one-shot
		 * conversion register, which we don't want to do (disables
		 * hardware monitoring) or monitor the busy bit, which is
		 * impossible (we can't read the values and monitor that bit
		 * at the exact same time). So the solution used here is to
		 * read the high byte once, then the low byte, then the high
		 * byte again. If the new high byte matches the old one,
		 * then we have a valid reading. Else we have to read the low
		 * byte again, and now we believe we have a correct reading.
		 */

		if (lm90_read_reg(client, LM90_REG_R_REMOTE_TEMPH, &oldh) == 0
		 && lm90_read_reg(client, LM90_REG_R_REMOTE_TEMPL, &l) == 0
		 && lm90_read_reg(client, LM90_REG_R_REMOTE_TEMPH, &newh) == 0
		 && (newh == oldh
		  || lm90_read_reg(client, LM90_REG_R_REMOTE_TEMPL, &l) == 0))
			data->remote_temp = (newh << 8) | l;

		if (lm90_read_reg(client, LM90_REG_R_REMOTE_LOWH, &newh) == 0
		 && lm90_read_reg(client, LM90_REG_R_REMOTE_LOWL, &l) == 0)
			data->remote_low = (newh << 8) | l;
		if (lm90_read_reg(client, LM90_REG_R_REMOTE_HIGHH, &newh) == 0
		 && lm90_read_reg(client, LM90_REG_R_REMOTE_HIGHL, &l) == 0)
			data->remote_high = (newh << 8) | l;
		lm90_read_reg(client, LM90_REG_R_STATUS, &data->alarms);

		data->last_updated = jiffies;
		data->valid = 1;
	}

	up(&data->update_lock);
}

static void lm90_local_temp(struct i2c_client *client, int operation,
	int ctl_name, int *nrels_mag, long *results)
{
	struct lm90_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0; /* magnitude */
	else if (operation == SENSORS_PROC_REAL_READ) {
		lm90_update_client(client);
		results[0] = TEMP1_FROM_REG(data->local_high);
		results[1] = TEMP1_FROM_REG(data->local_low);
		results[2] = TEMP1_FROM_REG(data->local_temp);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			if (data->kind == adt7461)
				data->local_high = TEMP1_TO_REG_ADT7461(results[0]);
			else
				data->local_high = TEMP1_TO_REG(results[0]);
			i2c_smbus_write_byte_data(client, LM90_REG_W_LOCAL_HIGH,
				data->local_high);
		}
		if (*nrels_mag >= 2) {
			if (data->kind == adt7461)
				data->local_low = TEMP1_TO_REG_ADT7461(results[1]);
			else
				data->local_low = TEMP1_TO_REG(results[1]);
			i2c_smbus_write_byte_data(client, LM90_REG_W_LOCAL_LOW,
				data->local_low);
		}
	}
}

static void lm90_remote_temp(struct i2c_client *client, int operation,
	int ctl_name, int *nrels_mag, long *results)
{
	struct lm90_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 1; /* magnitude */
	else if (operation == SENSORS_PROC_REAL_READ) {
		lm90_update_client(client);
		results[0] = TEMP2_FROM_REG(data->remote_high);
		results[1] = TEMP2_FROM_REG(data->remote_low);
		results[2] = TEMP2_FROM_REG(data->remote_temp);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			if (data->kind == adt7461)
				data->remote_high = TEMP2_TO_REG_ADT7461(results[0]);
			else
				data->remote_high = TEMP2_TO_REG(results[0]);
			i2c_smbus_write_byte_data(client, LM90_REG_W_REMOTE_HIGHH,
				data->remote_high >> 8);
			i2c_smbus_write_byte_data(client, LM90_REG_W_REMOTE_HIGHL,
				data->remote_high & 0xFF);
		}
		if (*nrels_mag >= 2) {
			if (data->kind == adt7461)
				data->remote_low = TEMP2_TO_REG_ADT7461(results[1]);
			else
				data->remote_low = TEMP2_TO_REG(results[1]);
			i2c_smbus_write_byte_data(client, LM90_REG_W_REMOTE_LOWH,
				data->remote_low >> 8);
			i2c_smbus_write_byte_data(client, LM90_REG_W_REMOTE_LOWL,
				data->remote_low & 0xFF);
		}
	}
}

static void lm90_local_tcrit(struct i2c_client *client, int operation,
	int ctl_name, int *nrels_mag, long *results)
{
	struct lm90_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0; /* magnitude */
	else if (operation == SENSORS_PROC_REAL_READ) {
		lm90_update_client(client);
		results[0] = TEMP1_FROM_REG(data->local_crit);
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			if (data->kind == adt7461)
				data->local_crit = TEMP1_TO_REG_ADT7461(results[0]);
			else
				data->local_crit = TEMP1_TO_REG(results[0]);
			i2c_smbus_write_byte_data(client, LM90_REG_W_LOCAL_CRIT,
				data->local_crit);
		}
	}
}

static void lm90_remote_tcrit(struct i2c_client *client, int operation,
	int ctl_name, int *nrels_mag, long *results)
{
	struct lm90_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0; /* magnitude */
	else if (operation == SENSORS_PROC_REAL_READ) {
		lm90_update_client(client);
		results[0] = TEMP1_FROM_REG(data->remote_crit);
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			if (data->kind == adt7461)
				data->remote_crit = TEMP1_TO_REG_ADT7461(results[0]);
			else
				data->remote_crit = TEMP1_TO_REG(results[0]);
			i2c_smbus_write_byte_data(client, LM90_REG_W_REMOTE_CRIT,
				data->remote_crit);
		}
	}
}

/*
 * One quick note about hysteresis. Internally, the hysteresis value
 * is held in a single register by the LM90, as a relative value.
 * This relative value applies to both the local critical temperature
 * and the remote critical temperature. Since all temperatures exported
 * through procfs have to be absolute, we have to do some conversions.
 * The solution retained here is to export two absolute values, one for
 * each critical temperature. In order not to confuse the users too
 * much, only one file is writable. Would we fail to do so, users
 * would probably attempt to write to both files, as if they were
 * independant, and since they aren't, they wouldn't understand why
 * setting one affects the other one (and would probably claim there's
 * a bug in the driver).
 */

static void lm90_local_hyst(struct i2c_client *client, int operation,
	int ctl_name, int *nrels_mag, long *results)
{
	struct lm90_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0; /* magnitude */
	else if (operation == SENSORS_PROC_REAL_READ) {
		lm90_update_client(client);
		results[0] = TEMP1_FROM_REG(data->local_crit) -
			TEMP1_FROM_REG(data->hyst);
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->hyst = HYST_TO_REG(data->local_crit - results[0]);
			i2c_smbus_write_byte_data(client, LM90_REG_W_TCRIT_HYST,
				data->hyst);
		}
	}
}

static void lm90_remote_hyst(struct i2c_client *client, int operation,
	int ctl_name, int *nrels_mag, long *results)
{
	struct lm90_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0; /* magnitude */
	else if (operation == SENSORS_PROC_REAL_READ) {
		lm90_update_client(client);
		results[0] = TEMP1_FROM_REG(data->remote_crit) -
			TEMP1_FROM_REG(data->hyst);
		*nrels_mag = 1;
	}
}

static void lm90_alarms(struct i2c_client *client, int operation,
	int ctl_name, int *nrels_mag, long *results)
{
	struct lm90_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0; /* magnitude */
	else if (operation == SENSORS_PROC_REAL_READ) {
		lm90_update_client(client);
		results[0] = data->alarms;
		*nrels_mag = 1;
	}
}

/* pec used for ADM1032 only */
static void adm1032_pec(struct i2c_client *client, int operation,
	int ctl_name, int *nrels_mag, long *results)
{
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0; /* magnitude */
	else if (operation == SENSORS_PROC_REAL_READ) {
		results[0] = !!(client->flags & I2C_CLIENT_PEC);
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			switch (results[0]) {
			case 0:
				client->flags &= ~I2C_CLIENT_PEC;
				break;
			case 1:
				client->flags |= I2C_CLIENT_PEC;
				break;
			}
		}
	}
}

static int __init sm_lm90_init(void)
{
	printk(KERN_INFO "lm90 driver version %s (%s)\n", LM_VERSION,
	       LM_DATE);
	return i2c_add_driver(&lm90_driver);
}

static void __exit sm_lm90_exit(void)
{
	i2c_del_driver(&lm90_driver);
}

MODULE_AUTHOR("Jean Delvare <khali@linux-fr.org>");
MODULE_DESCRIPTION("LM90/ADM1032 sensor driver");
MODULE_LICENSE("GPL");

module_init(sm_lm90_init);
module_exit(sm_lm90_exit);
