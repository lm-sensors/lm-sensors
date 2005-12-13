/*
 * f71805f.c - driver for the Fintek F71805F Super-I/O chip integrated
 *             hardware monitoring features
 * Copyright (C) 2005  Jean Delvare <khali@linux-fr.org>
 *
 * The F71805F is a LPC Super-I/O chip made by Fintek. It integrates
 * complete hardware monitoring features: voltage, fan and temperature
 * sensors, and manual and automatic fan speed control.
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
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/i2c.h>
#include <linux/i2c-proc.h>
#include <asm/io.h>
#include "version.h"

/* ISA address is read from Super-I/O configuration space */
static unsigned short normal_i2c[] = { SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { SENSORS_I2C_END };
static unsigned int normal_isa[] = { 0, SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

SENSORS_INSMOD_1(f71805f);

#define DRVNAME "f71805f"

/*
 * Super-I/O constants and functions
 */

#define F71805F_LD_HWM		0x04

#define SIO_REG_LDSEL		0x07	/* Logical device select */
#define SIO_REG_DEVID		0x20	/* Device ID (2 bytes) */
#define SIO_REG_DEVREV		0x22	/* Device revision */
#define SIO_REG_MANID		0x23	/* Fintek ID (2 bytes) */
#define SIO_REG_ENABLE		0x30	/* Logical device enable */
#define SIO_REG_ADDR		0x60	/* Logical device address (2 bytes) */

#define SIO_FINTEK_ID		0x1934
#define SIO_F71805F_ID		0x0406

static inline int
superio_inb(int base, int reg)
{
	outb(reg, base);
	return inb(base + 1);
}

static int
superio_inw(int base, int reg)
{
	int val;
	outb(reg++, base);
	val = inb(base + 1) << 8;
	outb(reg, base);
	val |= inb(base + 1);
	return val;
}

static inline void
superio_select(int base, int ld)
{
	outb(SIO_REG_LDSEL, base);
	outb(ld, base + 1);
}

static inline void
superio_enter(int base)
{
	outb(0x87, base);
	outb(0x87, base);
}

static inline void
superio_exit(int base)
{
	outb(0xaa, base);
}

/*
 * ISA constants
 */

#define REGION_LENGTH		2
#define ADDR_REG_OFFSET		0
#define DATA_REG_OFFSET		1

/*
 * Registers
 */

/* in nr from 0 to 8 (8-bit values) */
#define F71805F_REG_IN(nr)		(0x10 + (nr))
#define F71805F_REG_IN_HIGH(nr)		(0x40 + 2 * (nr))
#define F71805F_REG_IN_LOW(nr)		(0x41 + 2 * (nr))
/* fan nr from 0 to 2 (12-bit values, two registers) */
#define F71805F_REG_FAN(nr)		(0x20 + 2 * (nr))
#define F71805F_REG_FAN_LOW(nr)		(0x28 + 2 * (nr))
#define F71805F_REG_FAN_CTRL(nr)	(0x60 + 16 * (nr))
/* temp nr from 0 to 2 (8-bit values) */
#define F71805F_REG_TEMP(nr)		(0x1B + (nr))
#define F71805F_REG_TEMP_HIGH(nr)	(0x54 + 2 * (nr))
#define F71805F_REG_TEMP_HYST(nr)	(0x55 + 2 * (nr))
#define F71805F_REG_TEMP_MODE		0x01

#define F71805F_REG_START		0x00
/* status nr from 0 to 2 */
#define F71805F_REG_STATUS(nr)		(0x36 + (nr))

/*
 * Data structures and manipulation thereof
 */

struct f71805f_data {
	struct i2c_client client;
	struct semaphore lock;
	int sysctl_id;

	struct semaphore update_lock;
	char valid;		/* !=0 if following fields are valid */
	unsigned long last_updated;	/* In jiffies */
	unsigned long last_limits;	/* In jiffies */

	/* Register values */
	u8 in[9];
	u8 in_high[9];
	u8 in_low[9];
	u16 fan[3];
	u16 fan_low[3];
	u8 fan_enabled;		/* Read once at init time */
	u8 temp[3];
	u8 temp_high[3];
	u8 temp_hyst[3];
	u8 temp_mode;
	u8 alarms[3];
};

static inline long in_from_reg(u8 reg)
{
	return (reg * 8);
}

/* The 2 least significant bits are not used */
static inline u8 in_to_reg(long val)
{
	if (val <= 0)
		return 0;
	if (val >= 2016)
		return 0xfc;
	return (((val + 16) / 32) << 2);
}

/* in0 is downscaled by a factor 2 internally */
static inline long in0_from_reg(u8 reg)
{
	return (reg * 16);
}

static inline u8 in0_to_reg(long val)
{
	if (val <= 0)
		return 0;
	if (val >= 4032)
		return 0xfc;
	return (((val + 32) / 64) << 2);
}

/* The 4 most significant bits are not used */
static inline long fan_from_reg(u16 reg)
{
	reg &= 0xfff;
	if (!reg || reg == 0xfff)
		return 0;
	return (1500000 / reg);
}

static inline u16 fan_to_reg(long rpm)
{
	/* If the low limit is set below what the chip can measure,
	   store the largest possible 12-bit value in the registers,
	   so that no alarm will ever trigger. */
	if (rpm < 367)
		return 0xfff;
	return (1500000 / rpm);
}

static inline u8 temp_to_reg(long val)
{
	if (val < 0)
		val = 0;
	else if (val > 0xff)
		val = 0xff;
	return val;
}

/*
 * Driver and client management
 */

static u8 f71805f_read8(struct i2c_client *client, u8 reg)
{
	struct f71805f_data *data = client->data;
	u8 val;

	down(&data->lock);
	outb_p(reg, client->addr + ADDR_REG_OFFSET);
	val = inb_p(client->addr + DATA_REG_OFFSET);
	up(&data->lock);

	return val;
}

static void f71805f_write8(struct i2c_client *client, u8 reg, u8 val)
{
	struct f71805f_data *data = client->data;

	down(&data->lock);
	outb_p(reg, client->addr + ADDR_REG_OFFSET);
	outb_p(val, client->addr + DATA_REG_OFFSET);
	up(&data->lock);
}

/* It is important to read the MSB first, because doing so latches the
   value of the LSB, so we are sure both bytes belong to the same value. */
static u16 f71805f_read16(struct i2c_client *client, u8 reg)
{
	struct f71805f_data *data = client->data;
	u16 val;

	down(&data->lock);
	outb_p(reg, client->addr + ADDR_REG_OFFSET);
	val = inb_p(client->addr + DATA_REG_OFFSET) << 8;
	outb_p(++reg, client->addr + ADDR_REG_OFFSET);
	val |= inb_p(client->addr + DATA_REG_OFFSET);
	up(&data->lock);

	return val;
}

static void f71805f_write16(struct i2c_client *client, u8 reg, u16 val)
{
	struct f71805f_data *data = client->data;

	down(&data->lock);
	outb_p(reg, client->addr + ADDR_REG_OFFSET);
	outb_p(val >> 8, client->addr + DATA_REG_OFFSET);
	outb_p(++reg, client->addr + ADDR_REG_OFFSET);
	outb_p(val & 0xff, client->addr + DATA_REG_OFFSET);
	up(&data->lock);
}

static struct i2c_driver f71805f_driver;
static ctl_table f71805f_dir_table_template[];

static void f71805f_init_client(struct i2c_client *client)
{
	struct f71805f_data *data = client->data;
	u8 reg;
	int i;

	reg = f71805f_read8(client, F71805F_REG_START);
	if ((reg & 0x41) != 0x01) {
		printk(KERN_DEBUG DRVNAME ": Starting monitoring "
		       "operations\n");
		f71805f_write8(client,
			       F71805F_REG_START, (reg | 0x01) & ~0x40);
	}

	/* Fan monitoring can be disabled. If it is, we won't be polling
	   the register values, and instead set the cache to return 0 RPM. */
	for (i = 0; i < 3; i++) {
		reg = f71805f_read8(client, F71805F_REG_FAN_CTRL(i));
		if (!(reg & 0x80))
			data->fan_enabled |= (1 << i);
		else
			data->fan[i] = data->fan_low[i] = 0xfff;
	}
}

static int f71805f_detect(struct i2c_adapter *adapter, int address,
			  unsigned short flags, int kind)
{
	struct i2c_client *client;
	struct f71805f_data *data;
	int err;

	if (!request_region(address, REGION_LENGTH, f71805f_driver.name)) {
		err = -EBUSY;
		goto exit;
	}

	if (!(data = kmalloc(sizeof(struct f71805f_data), GFP_KERNEL))) {
		err = -ENOMEM;
		goto exit_release;
	}
	memset(data, 0, sizeof(struct f71805f_data));

	/* Fill in the client fields */
	client = &data->client;
	client->addr = address;
	client->data = data;
	client->adapter = adapter;
	client->driver = &f71805f_driver;
	strcpy(client->name, "F71805F chip");

	init_MUTEX(&data->lock);
	init_MUTEX(&data->update_lock);

	/* Tell the i2c core a new client has arrived */
	if ((err = i2c_attach_client(client)))
		goto exit_free;

	/* Initialize the F71805F chip */
	f71805f_init_client(client);

	/* Register a new directory entry in /proc */
	err = i2c_register_entry(client, "f71805f",
				 f71805f_dir_table_template, THIS_MODULE);
	if (err < 0)
		goto exit_detach;
	data->sysctl_id = err;

	return 0;

exit_detach:
	i2c_detach_client(client);
exit_free:
	kfree(data);
exit_release:
	release_region(address, REGION_LENGTH);
exit:
	return err;
}

static int f71805f_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_detect(adapter, &addr_data, f71805f_detect);
}

static int f71805f_detach_client(struct i2c_client *client)
{
	int err;
	struct f71805f_data *data = client->data;

	i2c_deregister_entry(data->sysctl_id);
	if ((err = i2c_detach_client(client))) {
		printk(KERN_ERR DRVNAME ": Client deregistration failed, "
		       "client not detached\n");
		return err;
	}

	release_region(client->addr, REGION_LENGTH);
	kfree(client->data);

	return 0;
}

static void f71805f_update_client(struct i2c_client *client)
{
	struct f71805f_data *data = client->data;
	int nr;

	down(&data->update_lock);

	/* Limit registers cache is refreshed after 60 seconds */
	if ((jiffies - data->last_limits > 60 * HZ)
	 || (jiffies < data->last_limits)
	 || !data->valid) {
		for (nr = 0; nr < 9; nr++) {
			data->in_high[nr] = f71805f_read8(client,
					    F71805F_REG_IN_HIGH(nr));
			data->in_low[nr] = f71805f_read8(client,
					   F71805F_REG_IN_LOW(nr));
		}
		for (nr = 0; nr < 3; nr++) {
			if (data->fan_enabled & (1 << nr))
				data->fan_low[nr] = f71805f_read16(client,
						    F71805F_REG_FAN_LOW(nr));
		}
		for (nr = 0; nr < 3; nr++) {
			data->temp_high[nr] = f71805f_read8(client,
					      F71805F_REG_TEMP_HIGH(nr));
			data->temp_hyst[nr] = f71805f_read8(client,
					      F71805F_REG_TEMP_HYST(nr));
		}
		data->temp_mode = f71805f_read8(client, F71805F_REG_TEMP_MODE);

		data->last_limits = jiffies;
	}

	/* Measurement registers cache is refreshed after 1 second */
	if ((jiffies - data->last_updated > HZ)
	 || (jiffies < data->last_updated)
	 || !data->valid) {
		for (nr = 0; nr < 9; nr++) {
			data->in[nr] = f71805f_read8(client,
				       F71805F_REG_IN(nr));
		}
		for (nr = 0; nr < 3; nr++) {
			if (data->fan_enabled & (1 << nr))
				data->fan[nr] = f71805f_read16(client,
						F71805F_REG_FAN(nr));
		}
		for (nr = 0; nr < 3; nr++) {
			data->temp[nr] = f71805f_read8(client,
					 F71805F_REG_TEMP(nr));
		}
		for (nr = 0; nr < 3; nr++) {
			data->alarms[nr] = f71805f_read8(client,
					   F71805F_REG_STATUS(nr));
		}

		data->last_updated = jiffies;
		data->valid = 1;
	}

	up(&data->update_lock);
}

/* -- SENSORS SYSCTL START -- */
#define F71805F_SYSCTL_IN0		1000
#define F71805F_SYSCTL_IN1		1001
#define F71805F_SYSCTL_IN2		1002
#define F71805F_SYSCTL_IN3		1003
#define F71805F_SYSCTL_IN4		1004
#define F71805F_SYSCTL_IN5		1005
#define F71805F_SYSCTL_IN6		1006
#define F71805F_SYSCTL_IN7		1007
#define F71805F_SYSCTL_IN8		1008
#define F71805F_SYSCTL_FAN1		1101
#define F71805F_SYSCTL_FAN2		1102
#define F71805F_SYSCTL_FAN3		1103
#define F71805F_SYSCTL_TEMP1		1201
#define F71805F_SYSCTL_TEMP2		1202
#define F71805F_SYSCTL_TEMP3		1203
#define F71805F_SYSCTL_SENSOR1		1211
#define F71805F_SYSCTL_SENSOR2		1212
#define F71805F_SYSCTL_SENSOR3		1213
#define F71805F_SYSCTL_ALARMS_IN	1090
#define F71805F_SYSCTL_ALARMS_FAN	1190
#define F71805F_SYSCTL_ALARMS_TEMP	1290
/* -- SENSORS SYSCTL END -- */

static void f71805f_in(struct i2c_client *client, int operation,
		       int ctl_name, int *nrels_mag, long *results)
{
	struct f71805f_data *data = client->data;
	int nr = ctl_name - F71805F_SYSCTL_IN0;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 3;
	else if (operation == SENSORS_PROC_REAL_READ) {
		f71805f_update_client(client);
		results[0] = in_from_reg(data->in_low[nr]);
		results[1] = in_from_reg(data->in_high[nr]);
		results[2] = in_from_reg(data->in[nr]);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag < 1)
			return;

		down(&data->update_lock);
		data->in_low[nr] = in_to_reg(results[0]);
		f71805f_write8(client, F71805F_REG_IN_LOW(nr),
			       data->in_low[nr]);

		if (*nrels_mag >= 2) {
			data->in_high[nr] = in_to_reg(results[1]);
			f71805f_write8(client, F71805F_REG_IN_HIGH(nr),
				       data->in_high[nr]);
		}
		up(&data->update_lock);
	}
}

static void f71805f_in0(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results)
{
	struct f71805f_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 3;
	else if (operation == SENSORS_PROC_REAL_READ) {
		f71805f_update_client(client);
		results[0] = in0_from_reg(data->in_low[0]);
		results[1] = in0_from_reg(data->in_high[0]);
		results[2] = in0_from_reg(data->in[0]);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag < 1)
			return;

		down(&data->update_lock);
		data->in_low[0] = in0_to_reg(results[0]);
		f71805f_write8(client, F71805F_REG_IN_LOW(0),
			       data->in_low[0]);

		if (*nrels_mag >= 2) {
			data->in_high[0] = in0_to_reg(results[1]);
			f71805f_write8(client, F71805F_REG_IN_HIGH(0),
				       data->in_high[0]);
		}
		up(&data->update_lock);
	}
}

static void f71805f_fan(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results)
{
	struct f71805f_data *data = client->data;
	int nr = ctl_name - F71805F_SYSCTL_FAN1;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		f71805f_update_client(client);
		results[0] = fan_from_reg(data->fan_low[nr]);
		results[1] = fan_from_reg(data->fan[nr]);
		*nrels_mag = 2;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag < 1)
			return;

		down(&data->update_lock);
		data->fan_low[nr] = fan_to_reg(results[0]);
		f71805f_write16(client, F71805F_REG_FAN_LOW(nr),
				data->fan_low[nr]);
		up(&data->update_lock);
	}
}

static void f71805f_temp(struct i2c_client *client, int operation,
			 int ctl_name, int *nrels_mag, long *results)
{
	struct f71805f_data *data = client->data;
	int nr = ctl_name - F71805F_SYSCTL_TEMP1;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		f71805f_update_client(client);
		results[0] = data->temp_high[nr];
		results[1] = data->temp_hyst[nr];
		results[2] = data->temp[nr];
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag < 1)
			return;

		down(&data->update_lock);
		data->temp_high[nr] = temp_to_reg(results[0]);
		f71805f_write8(client, F71805F_REG_TEMP_HIGH(nr),
			       data->temp_high[nr]);

		if (*nrels_mag >= 2) {
			data->temp_hyst[nr] = temp_to_reg(results[1]);
			f71805f_write8(client, F71805F_REG_TEMP_HYST(nr),
				       data->temp_hyst[nr]);
		}
		up(&data->update_lock);
	}
}

static void f71805f_sensor(struct i2c_client *client, int operation,
			   int ctl_name, int *nrels_mag, long *results)
{
	struct f71805f_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		int nr = ctl_name - F71805F_SYSCTL_SENSOR1;

		f71805f_update_client(client);
		results[0] = (data->temp_mode & (1 << nr)) ? 3 : 4;
		*nrels_mag = 1;
	}
}

static void f71805f_alarms_in(struct i2c_client *client, int operation,
			      int ctl_name, int *nrels_mag, long *results)
{
	struct f71805f_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		f71805f_update_client(client);
		results[0] = data->alarms[0]
			   | ((data->alarms[1] & 0x01) << 8);
		*nrels_mag = 1;
	}
}

static void f71805f_alarms_fan(struct i2c_client *client, int operation,
			       int ctl_name, int *nrels_mag, long *results)
{
	struct f71805f_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		f71805f_update_client(client);
		results[0] = data->alarms[2] & 0x07;
		*nrels_mag = 1;
	}
}

static void f71805f_alarms_temp(struct i2c_client *client, int operation,
				int ctl_name, int *nrels_mag, long *results)
{
	struct f71805f_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		f71805f_update_client(client);
		results[0] = (data->alarms[1] >> 3) & 0x07;
		*nrels_mag = 1;
	}
}

static ctl_table f71805f_dir_table_template[] = {
	{ F71805F_SYSCTL_IN0, "in0", NULL, 0, 0644, NULL,
	  &i2c_proc_real, &i2c_sysctl_real, NULL, &f71805f_in0 },
	{ F71805F_SYSCTL_IN1, "in1", NULL, 0, 0644, NULL,
	  &i2c_proc_real, &i2c_sysctl_real, NULL, &f71805f_in },
	{ F71805F_SYSCTL_IN2, "in2", NULL, 0, 0644, NULL,
	  &i2c_proc_real, &i2c_sysctl_real, NULL, &f71805f_in },
	{ F71805F_SYSCTL_IN3, "in3", NULL, 0, 0644, NULL,
	  &i2c_proc_real, &i2c_sysctl_real, NULL, &f71805f_in },
	{ F71805F_SYSCTL_IN4, "in4", NULL, 0, 0644, NULL,
	  &i2c_proc_real, &i2c_sysctl_real, NULL, &f71805f_in },
	{ F71805F_SYSCTL_IN5, "in5", NULL, 0, 0644, NULL,
	  &i2c_proc_real, &i2c_sysctl_real, NULL, &f71805f_in },
	{ F71805F_SYSCTL_IN6, "in6", NULL, 0, 0644, NULL,
	  &i2c_proc_real, &i2c_sysctl_real, NULL, &f71805f_in },
	{ F71805F_SYSCTL_IN7, "in7", NULL, 0, 0644, NULL,
	  &i2c_proc_real, &i2c_sysctl_real, NULL, &f71805f_in },
	{ F71805F_SYSCTL_IN8, "in8", NULL, 0, 0644, NULL,
	  &i2c_proc_real, &i2c_sysctl_real, NULL, &f71805f_in },
	{ F71805F_SYSCTL_FAN1, "fan1", NULL, 0, 0644, NULL,
	  &i2c_proc_real, &i2c_sysctl_real, NULL, &f71805f_fan },
	{ F71805F_SYSCTL_FAN2, "fan2", NULL, 0, 0644, NULL,
	  &i2c_proc_real, &i2c_sysctl_real, NULL, &f71805f_fan },
	{ F71805F_SYSCTL_FAN3, "fan3", NULL, 0, 0644, NULL,
	  &i2c_proc_real, &i2c_sysctl_real, NULL, &f71805f_fan },
	{ F71805F_SYSCTL_TEMP1, "temp1", NULL, 0, 0644, NULL,
	  &i2c_proc_real, &i2c_sysctl_real, NULL, &f71805f_temp },
	{ F71805F_SYSCTL_TEMP2, "temp2", NULL, 0, 0644, NULL,
	  &i2c_proc_real, &i2c_sysctl_real, NULL, &f71805f_temp },
	{ F71805F_SYSCTL_TEMP3, "temp3", NULL, 0, 0644, NULL,
	  &i2c_proc_real, &i2c_sysctl_real, NULL, &f71805f_temp },
	{ F71805F_SYSCTL_SENSOR1, "sensor1", NULL, 0, 0444, NULL,
	  &i2c_proc_real, &i2c_sysctl_real, NULL, &f71805f_sensor },
	{ F71805F_SYSCTL_SENSOR2, "sensor2", NULL, 0, 0444, NULL,
	  &i2c_proc_real, &i2c_sysctl_real, NULL, &f71805f_sensor },
	{ F71805F_SYSCTL_SENSOR3, "sensor3", NULL, 0, 0444, NULL,
	  &i2c_proc_real, &i2c_sysctl_real, NULL, &f71805f_sensor },
	{ F71805F_SYSCTL_ALARMS_IN, "alarms_in", NULL, 0, 0444, NULL,
	  &i2c_proc_real, &i2c_sysctl_real, NULL, &f71805f_alarms_in },
	{ F71805F_SYSCTL_ALARMS_FAN, "alarms_fan", NULL, 0, 0444, NULL,
	  &i2c_proc_real, &i2c_sysctl_real, NULL, &f71805f_alarms_fan },
	{ F71805F_SYSCTL_ALARMS_TEMP, "alarms_temp", NULL, 0, 0444, NULL,
	  &i2c_proc_real, &i2c_sysctl_real, NULL, &f71805f_alarms_temp },
	{ 0 }
};

static struct i2c_driver f71805f_driver = {
	.name		= "F71805F sensor driver",
	.flags		= I2C_DF_NOTIFY,
	.attach_adapter	= f71805f_attach_adapter,
	.detach_client	= f71805f_detach_client,
};

static int __init f71805f_find(int sioaddr, unsigned int *address)
{
	int err = -ENODEV;
	u16 devid;

	superio_enter(sioaddr);

	devid = superio_inw(sioaddr, SIO_REG_MANID);
	if (devid != SIO_FINTEK_ID)
		goto exit;

	devid = superio_inw(sioaddr, SIO_REG_DEVID);
	if (devid != SIO_F71805F_ID) {
		printk(KERN_INFO DRVNAME ": Unsupported Fintek device, "
		       "skipping\n");
		goto exit;
	}

	superio_select(sioaddr, F71805F_LD_HWM);
	if (!(superio_inb(sioaddr, SIO_REG_ENABLE) & 0x01)) {
		printk(KERN_WARNING DRVNAME ": Device not activated, "
		       "skipping\n");
		goto exit;
	}

	*address = superio_inw(sioaddr, SIO_REG_ADDR);
	if (*address == 0) {
		printk(KERN_WARNING DRVNAME ": Base address not set, "
		       "skipping\n");
		goto exit;
	}

	err = 0;
	printk(KERN_INFO DRVNAME ": Found F71805F chip at %#x, revision %u\n",
	       *address, superio_inb(sioaddr, SIO_REG_DEVREV));

exit:
	superio_exit(sioaddr);
	return err;
}

static int __init f71805f_init(void)
{
	printk("%s: Driver version %s (%s)\n", DRVNAME, LM_VERSION, LM_DATE);

	if (f71805f_find(0x2e, &normal_isa[0])
	 && f71805f_find(0x4e, &normal_isa[0]))
		return -ENODEV;

	return i2c_add_driver(&f71805f_driver);
}

static void __exit f71805f_exit(void)
{
	i2c_del_driver(&f71805f_driver);
}

MODULE_AUTHOR("Jean Delvare <khali@linux-fr>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("F71805F hardware monitoring driver");

module_init(f71805f_init);
module_exit(f71805f_exit);
