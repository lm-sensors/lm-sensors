/*
    w83627ehf - Driver for the hardware monitoring functionality of
                the Winbond W83627EHF Super-I/O chip
    Copyright (C) 2005, 2007  Jean Delvare <khali@linux-fr.org>
    Copyright (C) 2006  Rudolf Marek <r.marek@assembler.cz>
    Backported to kernel 2.4 by Yuan Mu (Winbond).

    Shamelessly ripped from the w83627hf driver
    Copyright (C) 2003  Mark Studebaker

    This driver also supports the W83627EHG, which is the lead-free
    version of the W83627EHF.

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

    Supports the following chips:

    Chip        #vin    #fan    #pwm    #temp
    w83627ehf   10      5       4       3
*/

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/i2c.h>
#include <linux/i2c-proc.h>
#include <linux/init.h>
#include <asm/io.h>
#include "version.h"
#include "sensors_vid.h"
#include "lm75.h"

/* The actual ISA address is read from Super-I/O configuration space */
static unsigned short normal_i2c[] = { SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { SENSORS_I2C_END };
static unsigned int normal_isa[] = { 0, SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/* Insmod parameters */
SENSORS_INSMOD_1(w83627ehf);

/*
 * Super-I/O constants and functions
 */

static int REG;			/* The register to read/write */
static int VAL;			/* The value to read/write */

#define W83627EHF_LD_HWM	0x0b

#define SIO_REG_LDSEL		0x07	/* Logical device select */
#define SIO_REG_DEVID		0x20	/* Device ID (2 bytes) */
#define SIO_REG_EN_VRM10	0x2C	/* GPIO3, GPIO4 selection */
#define SIO_REG_ENABLE		0x30	/* Logical device enable */
#define SIO_REG_ADDR		0x60	/* Logical device address (2 bytes) */
#define SIO_REG_VID_CTRL	0xF0	/* VID control */
#define SIO_REG_VID_DATA	0xF1	/* VID data */

#define SIO_W83627EHF_ID	0x8850
#define SIO_W83627EHG_ID	0x8860
#define SIO_ID_MASK		0xFFF0

static inline void superio_outb(int reg, int val)
{
	outb(reg, REG);
	outb(val, VAL);
}

static inline int superio_inb(int reg)
{
	outb(reg, REG);
	return inb(VAL);
}

static inline void superio_select(int ld)
{
	outb(SIO_REG_LDSEL, REG);
	outb(ld, VAL);
}

static inline void superio_enter(void)
{
	outb(0x87, REG);
	outb(0x87, REG);
}

static inline void superio_exit(void)
{
	outb(0x02, REG);
	outb(0x02, VAL);
}

/*
 * ISA constants
 */

#define REGION_ALIGNMENT	~7
#define REGION_OFFSET		5
#define REGION_LENGTH		2
#define ADDR_REG_OFFSET		5
#define DATA_REG_OFFSET		6

#define W83627EHF_REG_BANK		0x4E
#define W83627EHF_REG_CONFIG		0x40

static const u16 W83627EHF_REG_FAN[] = { 0x28, 0x29, 0x2a, 0x3f, 0x553 };
static const u16 W83627EHF_REG_FAN_MIN[] = { 0x3b, 0x3c, 0x3d, 0x3e, 0x55c };

/* The W83627EHF registers for nr=7,8,9 are in bank 5 */
#define W83627EHF_REG_IN_MAX(nr)	((nr < 7) ? (0x2b + (nr) * 2) : \
					 (0x554 + (((nr) - 7) * 2)))
#define W83627EHF_REG_IN_MIN(nr)	((nr < 7) ? (0x2c + (nr) * 2) : \
					 (0x555 + (((nr) - 7) * 2)))
#define W83627EHF_REG_IN(nr)		((nr < 7) ? (0x20 + (nr)) : \
					 (0x550 + (nr) - 7))

static const u16 W83627EHF_REG_TEMP[] = { 0x27, 0x150, 0x250 };
static const u16 W83627EHF_REG_TEMP_HYST[] = { 0x3a, 0x153, 0x253 };
static const u16 W83627EHF_REG_TEMP_OVER[] = { 0x39, 0x155, 0x255 };
static const u16 W83627EHF_REG_TEMP_CONFIG[] = { 0x152, 0x252 };

/* Fan clock dividers are spread over the following five registers */
#define W83627EHF_REG_FANDIV1		0x47
#define W83627EHF_REG_FANDIV2		0x4B
#define W83627EHF_REG_VBAT		0x5D
#define W83627EHF_REG_DIODE		0x59
#define W83627EHF_REG_SMI_OVT		0x4C

#define W83627EHF_REG_ALARM1		0x459
#define W83627EHF_REG_ALARM2		0x45A
#define W83627EHF_REG_ALARM3		0x45B

/* SmartFan registers */
/* DC or PWM output fan configuration */
static const u8 W83627EHF_REG_PWM_ENABLE[] = {
	0x04,			/* SYS FAN0 */
	0x04,			/* CPU FAN0 */
	0x12,			/* AUX FAN */
	0x62,			/* CPU FAN1 */
};

#if 0
static const u8 W83627EHF_PWM_MODE_SHIFT[4] = { 0, 1, 0, 6 };
#endif
static const u8 W83627EHF_PWM_ENABLE_SHIFT[4] = { 2, 4, 1, 4 };

/* FAN Duty Cycle, be used to control */
static const u8 W83627EHF_REG_PWM[] = { 0x01, 0x03, 0x11, 0x61 };

/*
 * Conversions
 */

static inline unsigned int fan_from_reg(u8 reg, unsigned int div)
{
	if (reg == 0 || reg == 255)
		return 0;
	return 1350000U / (reg * div);
}

static inline unsigned int div_from_reg(u8 reg)
{
	return 1 << reg;
}

/* Some of analog inputs have internal scaling (2x), 8mV is ADC LSB */
static const u8 scale_in[10] = { 8, 8, 16, 16, 8, 8, 8, 16, 16, 8 };

static inline long in_from_reg(u8 reg, u8 nr)
{
	return reg * scale_in[nr];
}

static inline u8 in_to_reg(long val, u8 nr)
{
	return SENSORS_LIMIT((val + scale_in[nr] / 2) / scale_in[nr], 0, 255);
}

/*
 * Data structures and manipulation thereof
 */

struct w83627ehf_data {
	struct i2c_client client;
	struct class_device *class_dev;
	int sysctl_id;
	struct semaphore lock;
	struct semaphore update_lock;
	char valid;		/* !=0 if following fields are valid */
	unsigned long last_updated;	/* In jiffies */

	/* Register values */
	u8 in[10];		/* Register value */
	u8 in_max[10];		/* Register value */
	u8 in_min[10];		/* Register value */
	u8 fan[5];
	u8 fan_min[5];
	u8 fan_div[5];
	u8 has_fan;		/* some fan inputs can be disabled */
	u8 sensor[3];
	u16 temp[3];
	u16 temp_max[3];
	u16 temp_hyst[3];
	u32 alarms;

	u8 pwm_enable[4];	/* 1 for manual, 2+ for auto */
	u8 pwm[4];

	u8 vid;
	u8 vrm;
};

/* The /proc/sys entries */
/* -- SENSORS SYSCTL START -- */

#define W83627EHF_SYSCTL_IN0		1000	/* Volts * 1000 */
#define W83627EHF_SYSCTL_IN1		1001
#define W83627EHF_SYSCTL_IN2		1002
#define W83627EHF_SYSCTL_IN3		1003
#define W83627EHF_SYSCTL_IN4		1004
#define W83627EHF_SYSCTL_IN5		1005
#define W83627EHF_SYSCTL_IN6		1006
#define W83627EHF_SYSCTL_IN7		1007
#define W83627EHF_SYSCTL_IN8		1008
#define W83627EHF_SYSCTL_IN9		1009
#define W83627EHF_SYSCTL_FAN1		1101	/* Rotations/min */
#define W83627EHF_SYSCTL_FAN2		1102
#define W83627EHF_SYSCTL_FAN3		1103
#define W83627EHF_SYSCTL_FAN4		1104
#define W83627EHF_SYSCTL_FAN5		1105
#define W83627EHF_SYSCTL_TEMP1		1201	/* Degrees Celsius * 10 */
#define W83627EHF_SYSCTL_TEMP2		1202
#define W83627EHF_SYSCTL_TEMP3		1203
#define W83627EHF_SYSCTL_SENSOR1	1211
#define W83627EHF_SYSCTL_SENSOR2	1212
#define W83627EHF_SYSCTL_SENSOR3	1213
#define W83627EHF_SYSCTL_VID		1300	/* Volts * 1000 */
#define W83627EHF_SYSCTL_VRM		1301
#define W83627EHF_SYSCTL_PWM1		1401
#define W83627EHF_SYSCTL_PWM2		1402
#define W83627EHF_SYSCTL_PWM3		1403
#define W83627EHF_SYSCTL_PWM4		1404
#define W83627EHF_SYSCTL_FAN_DIV	1506
#define W83627EHF_SYSCTL_ALARMS		1507	/* bitvector */

#define W83627EHF_ALARM_IN0		(1 << 0)
#define W83627EHF_ALARM_IN1		(1 << 1)
#define W83627EHF_ALARM_IN2		(1 << 2)
#define W83627EHF_ALARM_IN3		(1 << 3)
#define W83627EHF_ALARM_IN4		(1 << 8)
#define W83627EHF_ALARM_IN5		(1 << 21)
#define W83627EHF_ALARM_IN6		(1 << 20)
#define W83627EHF_ALARM_IN7		(1 << 16)
#define W83627EHF_ALARM_IN8		(1 << 17)
#define W83627EHF_ALARM_IN9		(1 << 19)
#define W83627EHF_ALARM_TEMP1		(1 << 4)
#define W83627EHF_ALARM_TEMP2		(1 << 5)
#define W83627EHF_ALARM_TEMP3		(1 << 13)
#define W83627EHF_ALARM_FAN1		(1 << 6)
#define W83627EHF_ALARM_FAN2		(1 << 7)
#define W83627EHF_ALARM_FAN3		(1 << 11)
#define W83627EHF_ALARM_FAN4		(1 << 10)
#define W83627EHF_ALARM_FAN5		(1 << 23)

/* -- SENSORS SYSCTL END -- */

static inline int is_word_sized(u16 reg)
{
	return (((reg & 0xff00) == 0x100 || (reg & 0xff00) == 0x200)
	     && ((reg & 0x00ff) == 0x50  || (reg & 0x00ff) == 0x53 ||
		 (reg & 0x00ff) == 0x55));
}

/* We assume that the default bank is 0, thus the following two functions do
   nothing for registers which live in bank 0. For others, they respectively
   set the bank register to the correct value (before the register is
   accessed), and back to 0 (afterwards). */
static inline void w83627ehf_set_bank(struct i2c_client *client, u16 reg)
{
	if (reg & 0xff00) {
		outb_p(W83627EHF_REG_BANK, client->addr + ADDR_REG_OFFSET);
		outb_p(reg >> 8, client->addr + DATA_REG_OFFSET);
	}
}

static inline void w83627ehf_reset_bank(struct i2c_client *client, u16 reg)
{
	if (reg & 0xff00) {
		outb_p(W83627EHF_REG_BANK, client->addr + ADDR_REG_OFFSET);
		outb_p(0, client->addr + DATA_REG_OFFSET);
	}
}

static u16 w83627ehf_read_value(struct i2c_client *client, u16 reg)
{
	struct w83627ehf_data *data = client->data;
	int res, word_sized = is_word_sized(reg);

	down(&data->lock);

	w83627ehf_set_bank(client, reg);
	outb_p(reg & 0xff, client->addr + ADDR_REG_OFFSET);
	res = inb_p(client->addr + DATA_REG_OFFSET);
	if (word_sized) {
		outb_p((reg & 0xff) + 1, client->addr + ADDR_REG_OFFSET);
		res = (res << 8) + inb_p(client->addr + DATA_REG_OFFSET);
	}
	w83627ehf_reset_bank(client, reg);

	up(&data->lock);
	return res;
}

static int w83627ehf_write_value(struct i2c_client *client, u16 reg, u16 value)
{
	struct w83627ehf_data *data = client->data;
	int word_sized = is_word_sized(reg);

	down(&data->lock);

	w83627ehf_set_bank(client, reg);
	outb_p(reg & 0xff, client->addr + ADDR_REG_OFFSET);
	if (word_sized) {
		outb_p(value >> 8, client->addr + DATA_REG_OFFSET);
		outb_p((reg & 0xff) + 1, client->addr + ADDR_REG_OFFSET);
	}
	outb_p(value & 0xff, client->addr + DATA_REG_OFFSET);
	w83627ehf_reset_bank(client, reg);

	up(&data->lock);
	return 0;
}

/* This function assumes that the caller holds data->update_lock */
static void w83627ehf_write_fan_div(struct i2c_client *client, int nr)
{
	struct w83627ehf_data *data = client->data;
	u8 reg;

	switch (nr) {
	case 0:
		reg = w83627ehf_read_value(client, W83627EHF_REG_FANDIV1)
		    & 0xcf;
		reg |= (data->fan_div[0] & 0x03) << 4;
		/* fan5 input control bit is write only, compute the value */
		reg |= (data->has_fan & (1 << 4)) ? 1 : 0;
		w83627ehf_write_value(client, W83627EHF_REG_FANDIV1, reg);
		reg = w83627ehf_read_value(client, W83627EHF_REG_VBAT)
		    & 0xdf;
		reg |= (data->fan_div[0] & 0x04) << 3;
		w83627ehf_write_value(client, W83627EHF_REG_VBAT, reg);
		break;
	case 1:
		reg = w83627ehf_read_value(client, W83627EHF_REG_FANDIV1)
		    & 0x3f;
		reg |= (data->fan_div[1] & 0x03) << 6;
		/* fan5 input control bit is write only, compute the value */
		reg |= (data->has_fan & (1 << 4)) ? 1 : 0;
		w83627ehf_write_value(client, W83627EHF_REG_FANDIV1, reg);
		reg = w83627ehf_read_value(client, W83627EHF_REG_VBAT)
		    & 0xbf;
		reg |= (data->fan_div[1] & 0x04) << 4;
		w83627ehf_write_value(client, W83627EHF_REG_VBAT, reg);
		break;
	case 2:
		reg = w83627ehf_read_value(client, W83627EHF_REG_FANDIV2)
		    & 0x3f;
		reg |= (data->fan_div[2] & 0x03) << 6;
		w83627ehf_write_value(client, W83627EHF_REG_FANDIV2, reg);
		reg = w83627ehf_read_value(client, W83627EHF_REG_VBAT)
		    & 0x7f;
		reg |= (data->fan_div[2] & 0x04) << 5;
		w83627ehf_write_value(client, W83627EHF_REG_VBAT, reg);
		break;
	case 3:
		reg = w83627ehf_read_value(client, W83627EHF_REG_DIODE)
		    & 0xfc;
		reg |= data->fan_div[3] & 0x03;
		w83627ehf_write_value(client, W83627EHF_REG_DIODE, reg);
		reg = w83627ehf_read_value(client, W83627EHF_REG_SMI_OVT)
		    & 0x7f;
		reg |= (data->fan_div[3] & 0x04) << 5;
		w83627ehf_write_value(client, W83627EHF_REG_SMI_OVT, reg);
		break;
	case 4:
		reg = w83627ehf_read_value(client, W83627EHF_REG_DIODE)
		    & 0x73;
		reg |= (data->fan_div[4] & 0x03) << 2;
		reg |= (data->fan_div[4] & 0x04) << 5;
		w83627ehf_write_value(client, W83627EHF_REG_DIODE, reg);
		break;
	}
}

static void w83627ehf_update_client(struct i2c_client *client)
{
	struct w83627ehf_data *data = client->data;
	int pwmcfg = 0;	/* shut up the compiler */
	int i;

	down(&data->update_lock);
	if (time_after(jiffies, data->last_updated + HZ)
	    || !data->valid) {
		/* Fan clock dividers */
		i = w83627ehf_read_value(client, W83627EHF_REG_FANDIV1);
		data->fan_div[0] = (i >> 4) & 0x03;
		data->fan_div[1] = (i >> 6) & 0x03;
		i = w83627ehf_read_value(client, W83627EHF_REG_FANDIV2);
		data->fan_div[2] = (i >> 6) & 0x03;
		i = w83627ehf_read_value(client, W83627EHF_REG_VBAT);
		data->fan_div[0] |= (i >> 3) & 0x04;
		data->fan_div[1] |= (i >> 4) & 0x04;
		data->fan_div[2] |= (i >> 5) & 0x04;
		if (data->has_fan & ((1 << 3) | (1 << 4))) {
			i = w83627ehf_read_value(client, W83627EHF_REG_DIODE);
			data->fan_div[3] = i & 0x03;
			data->fan_div[4] = ((i >> 2) & 0x03)
					 | ((i >> 5) & 0x04);
		}
		if (data->has_fan & (1 << 3)) {
			i = w83627ehf_read_value(client, W83627EHF_REG_SMI_OVT);
			data->fan_div[3] |= (i >> 5) & 0x04;
		}

		/* Measured voltages and limits */
		for (i = 0; i < 10; i++) {
			data->in[i] = w83627ehf_read_value(client,
						W83627EHF_REG_IN(i));
			data->in_min[i] = w83627ehf_read_value(client,
						W83627EHF_REG_IN_MIN(i));
			data->in_max[i] = w83627ehf_read_value(client,
						W83627EHF_REG_IN_MAX(i));
		}

		/* Measured fan speeds and limits */
		for (i = 0; i < 5; i++) {
			if (!(data->has_fan & (1 << i)))
				continue;

			data->fan[i] = w83627ehf_read_value(client,
						W83627EHF_REG_FAN[i]);
			data->fan_min[i] = w83627ehf_read_value(client,
						W83627EHF_REG_FAN_MIN[i]);

			/* If we failed to measure the fan speed and clock
			   divider can be increased, let's try that for next
			   time */
			if (data->fan[i] == 0xff
			 && data->fan_div[i] < 0x07) {
#ifdef DEBUG
			 	printk(KERN_DEBUG "w83627ehf: Increasing "
				        "fan%d clock divider from %u to %u\n",
					i + 1, div_from_reg(data->fan_div[i]),
					div_from_reg(data->fan_div[i] + 1));
#endif
				data->fan_div[i]++;
				w83627ehf_write_fan_div(client, i);
				/* Preserve min limit if possible */
				if (data->fan_min[i] >= 2
				 && data->fan_min[i] != 255) {
					w83627ehf_write_value(client,
						W83627EHF_REG_FAN_MIN[i],
						(data->fan_min[i] /= 2));
				}
			}
		}

		for (i = 0; i < 4; i++) {
			/* pwmcfg mapped for i=0, i=1 to same reg */
			if (i != 1)
				pwmcfg = w83627ehf_read_value(client,
						W83627EHF_REG_PWM_ENABLE[i]);

			data->pwm_enable[i] = 1 +
				((pwmcfg >> W83627EHF_PWM_ENABLE_SHIFT[i]) & 3);
			data->pwm[i] = w83627ehf_read_value(client,
						W83627EHF_REG_PWM[i]);
		}

		/* Measured temperatures and limits */
		for (i = 0; i < 3; i++) {
			data->temp[i] = w83627ehf_read_value(client,
						W83627EHF_REG_TEMP[i]);
			data->temp_max[i] = w83627ehf_read_value(client,
						W83627EHF_REG_TEMP_OVER[i]);
			data->temp_hyst[i] = w83627ehf_read_value(client,
						W83627EHF_REG_TEMP_HYST[i]);
		}
		/* temp1 is 8-bit, align it */
		data->temp[0] <<= 8;
		data->temp_max[0] <<= 8;
		data->temp_hyst[0] <<= 8;

		data->alarms = w83627ehf_read_value(client,
						    W83627EHF_REG_ALARM1) |
		    (w83627ehf_read_value(client,
					  W83627EHF_REG_ALARM2) << 8) |
		    (w83627ehf_read_value(client, W83627EHF_REG_ALARM3) << 16);

		data->last_updated = jiffies;
		data->valid = 1;
	}

	up(&data->update_lock);
}

static void w83627ehf_in(struct i2c_client *client, int operation, int ctl_name,
			 int *nrels_mag, long *results)
{
	struct w83627ehf_data *data = client->data;
	int nr = ctl_name - W83627EHF_SYSCTL_IN0;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 3;
	else if (operation == SENSORS_PROC_REAL_READ) {
		w83627ehf_update_client(client);
		results[0] = in_from_reg(data->in_min[nr], nr);
		results[1] = in_from_reg(data->in_max[nr], nr);
		results[2] = in_from_reg(data->in[nr], nr);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		down(&data->update_lock);
		if (*nrels_mag >= 1) {
			/* Write low limit into register */
			data->in_min[nr] = in_to_reg(results[0], nr);
			w83627ehf_write_value(client, W83627EHF_REG_IN_MIN(nr),
					      data->in_min[nr]);
		}
		if (*nrels_mag >= 2) {
			/* Write high limit into register */
			data->in_max[nr] = in_to_reg(results[1], nr);
			w83627ehf_write_value(client, W83627EHF_REG_IN_MAX(nr),
					      data->in_max[nr]);
		}
		up(&data->update_lock);
	}
}

static void store_fan_min(struct i2c_client *client, long val, int nr)
{
	struct w83627ehf_data *data = client->data;
	unsigned int reg;
	u8 new_div;

	down(&data->update_lock);
	if (val <= 0) {
		/* No min limit, alarm disabled */
		data->fan_min[nr] = 255;
		new_div = data->fan_div[nr]; /* No change */
		printk(KERN_INFO "w83627ehf: fan%u low limit and alarm "
		       "disabled\n", nr + 1);
	} else if ((reg = 1350000U / val) >= 128 * 255) {
		/* Speed below this value cannot possibly be represented,
		   even with the highest divider (128) */
		data->fan_min[nr] = 254;
		new_div = 7; /* 128 == (1 << 7) */
		printk(KERN_WARNING "w83627ehf: fan%u low limit %lu below "
		       "minimum %u, set to minimum\n", nr + 1, val,
		       fan_from_reg(254, 128));
	} else if (!reg) {
		/* Speed above this value cannot possibly be represented,
		   even with the lowest divider (1) */
		data->fan_min[nr] = 1;
		new_div = 0; /* 1 == (1 << 0) */
		printk(KERN_WARNING "w83627ehf: fan%u low limit %lu above "
		       "maximum %u, set to maximum\n", nr + 1, val,
		       fan_from_reg(1, 1));
	} else {
		/* Automatically pick the best divider, i.e. the one such
		   that the min limit will correspond to a register value
		   in the 96..192 range */
		new_div = 0;
		while (reg > 192 && new_div < 7) {
			reg >>= 1;
			new_div++;
		}
		data->fan_min[nr] = reg;
	}

	/* Write both the fan clock divider (if it changed) and the new
	   fan min (unconditionally) */
	if (new_div != data->fan_div[nr]) {
		/* Preserve the fan speed reading */
		if (data->fan[nr] != 0xff) {
			if (new_div > data->fan_div[nr])
				data->fan[nr] >>= new_div - data->fan_div[nr];
			else if (data->fan[nr] & 0x80)
				data->fan[nr] = 0xff;
			else
				data->fan[nr] <<= data->fan_div[nr] - new_div;
		}

#ifdef DEBUG
		printk(KERN_DEBUG "w83627ehf: fan%u clock divider changed "
		       "from %u to %u\n", nr + 1,
		       div_from_reg(data->fan_div[nr]),
		       div_from_reg(new_div));
#endif
		data->fan_div[nr] = new_div;
		w83627ehf_write_fan_div(client, nr);
	}
	w83627ehf_write_value(client, W83627EHF_REG_FAN_MIN[nr],
			      data->fan_min[nr]);
	up(&data->update_lock);
}

static void w83627ehf_fan(struct i2c_client *client, int operation,
			  int ctl_name, int *nrels_mag, long *results)
{
	struct w83627ehf_data *data = client->data;
	int nr = ctl_name - W83627EHF_SYSCTL_FAN1;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		if (data->has_fan & (1 << nr)) {
			w83627ehf_update_client(client);
			results[0] = fan_from_reg(data->fan_min[nr],
					div_from_reg(data->fan_div[nr]));
			results[1] = fan_from_reg(data->fan[nr],
					div_from_reg(data->fan_div[nr]));
		} else {
			results[0] = 0;
			results[1] = 0;
		}
		*nrels_mag = 2;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if ((data->has_fan & (1 << nr))
		 && (*nrels_mag >= 1)) {
		 	store_fan_min(client, results[0], nr);
		}
	}
}

static void w83627ehf_temp(struct i2c_client *client, int operation,
			   int ctl_name, int *nrels_mag, long *results)
{
	struct w83627ehf_data *data = client->data;
	int nr = ctl_name - W83627EHF_SYSCTL_TEMP1;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 1;
	else if (operation == SENSORS_PROC_REAL_READ) {
		w83627ehf_update_client(client);
		results[0] = LM75_TEMP_FROM_REG(data->temp_max[nr]);
		results[1] = LM75_TEMP_FROM_REG(data->temp_hyst[nr]);
		results[2] = LM75_TEMP_FROM_REG(data->temp[nr]);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		down(&data->update_lock);
		if (*nrels_mag >= 1) {
			data->temp_max[nr] = LM75_TEMP_TO_REG(results[0]);
			w83627ehf_write_value(client,
					W83627EHF_REG_TEMP_OVER[nr],
					nr ? data->temp_max[nr] :
					data->temp_max[nr] >> 8);
		}
		if (*nrels_mag >= 2) {
			data->temp_hyst[nr] = LM75_TEMP_TO_REG(results[1]);
			w83627ehf_write_value(client,
					W83627EHF_REG_TEMP_HYST[nr],
					nr ? data->temp_hyst[nr] :
					data->temp_hyst[nr] >> 8);
		}
		up(&data->update_lock);
	}
}

static void w83627ehf_alarms(struct i2c_client *client, int operation,
			     int ctl_name, int *nrels_mag, long *results)
{
	struct w83627ehf_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		w83627ehf_update_client(client);
		results[0] = data->alarms;
		*nrels_mag = 1;
	}
}

static void w83627ehf_fan_div(struct i2c_client *client, int operation,
			      int ctl_name, int *nrels_mag, long *results)
{
	struct w83627ehf_data *data = client->data;
	int i;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		w83627ehf_update_client(client);
		for (i = 0; i < 5; i++)
			results[i] = div_from_reg(data->fan_div[i]);
		*nrels_mag = 5;
	}
}

/* This function assumes that the caller holds data->update_lock */
static void store_pwm_enable(struct i2c_client *client, long val, int nr)
{
	struct w83627ehf_data *data = client->data;
	u16 reg;
	long modemax;

	/* Only pwm2 and pwm4 support Smart Fan III */
	modemax = (nr == 1 || nr == 3) ? 4 : 3;
	if (val < 1 || val > modemax)
		return;

	data->pwm_enable[nr] = val;
	reg = w83627ehf_read_value(client, W83627EHF_REG_PWM_ENABLE[nr]);
	reg &= ~(0x03 << W83627EHF_PWM_ENABLE_SHIFT[nr]);
	reg |= (val - 1) << W83627EHF_PWM_ENABLE_SHIFT[nr];
	w83627ehf_write_value(client, W83627EHF_REG_PWM_ENABLE[nr], reg);
}

static void w83627ehf_pwm(struct i2c_client *client, int operation,
			  int ctl_name, int *nrels_mag, long *results)
{
	struct w83627ehf_data *data = client->data;
	int nr = ctl_name - W83627EHF_SYSCTL_PWM1;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		w83627ehf_update_client(client);
		results[0] = data->pwm[nr];
		results[1] = data->pwm_enable[nr];
		*nrels_mag = 2;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		down(&data->update_lock);
		if (*nrels_mag >= 1) {
			data->pwm[nr] = SENSORS_LIMIT(results[0], 0, 255);
			w83627ehf_write_value(client, W83627EHF_REG_PWM[nr],
					      data->pwm[nr]);
		}
		if (*nrels_mag >= 2) {
			store_pwm_enable(client, results[1], nr);
		}
		up(&data->update_lock);
	}
}

static void w83627ehf_vid(struct i2c_client *client, int operation,
			  int ctl_name, int *nrels_mag, long *results)
{
	struct w83627ehf_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 3;
	else if (operation == SENSORS_PROC_REAL_READ) {
		results[0] = vid_from_reg(data->vid, data->vrm);
		*nrels_mag = 1;
	}
}

/* Change the VID input level, and read VID value again
   This function assumes that the caller holds data->update_lock */
static void w83627ehf_en_vrm10(struct w83627ehf_data *data, int enable)
{
	u8 reg;

	superio_enter();
	reg = superio_inb(SIO_REG_EN_VRM10) & ~0x08;
	if (enable)
		reg |= 0x08;
	superio_outb(SIO_REG_EN_VRM10, reg);

	superio_select(W83627EHF_LD_HWM);
	if (superio_inb(SIO_REG_VID_CTRL) & 0x80)	/* VID input mode */
		data->vid = superio_inb(SIO_REG_VID_DATA) & 0x3f;
	superio_exit();
}

static void w83627ehf_vrm(struct i2c_client *client, int operation,
			  int ctl_name, int *nrels_mag, long *results)
{
	struct w83627ehf_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 1;
	else if (operation == SENSORS_PROC_REAL_READ) {
		results[0] = data->vrm;
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			down(&data->update_lock);
			/* We may have to change the VID input level */
			if (data->vrm/10 != 10 && results[0]/10 == 10)
				w83627ehf_en_vrm10(data, 1);
			else if (data->vrm/10 == 10 && results[0]/10 != 10)
				w83627ehf_en_vrm10(data, 0);
			data->vrm = results[0];
			up(&data->update_lock);
		}
	}
}

static void w83627ehf_sensor(struct i2c_client *client, int operation,
			     int ctl_name, int *nrels_mag, long *results)
{
	struct w83627ehf_data *data = client->data;
	int nr = ctl_name - W83627EHF_SYSCTL_SENSOR1;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		w83627ehf_update_client(client);
		results[0] = data->sensor[nr];
		*nrels_mag = 1;
	}
}

static ctl_table w83627ehf_dir_table_template[] = {
	{W83627EHF_SYSCTL_IN0, "in0", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627ehf_in},
	{W83627EHF_SYSCTL_IN1, "in1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627ehf_in},
	{W83627EHF_SYSCTL_IN2, "in2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627ehf_in},
	{W83627EHF_SYSCTL_IN3, "in3", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627ehf_in},
	{W83627EHF_SYSCTL_IN4, "in4", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627ehf_in},
	{W83627EHF_SYSCTL_IN5, "in5", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627ehf_in},
	{W83627EHF_SYSCTL_IN6, "in6", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627ehf_in},
	{W83627EHF_SYSCTL_IN7, "in7", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627ehf_in},
	{W83627EHF_SYSCTL_IN8, "in8", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627ehf_in},
	{W83627EHF_SYSCTL_IN9, "in9", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627ehf_in},

	{W83627EHF_SYSCTL_FAN1, "fan1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627ehf_fan},
	{W83627EHF_SYSCTL_FAN2, "fan2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627ehf_fan},
	{W83627EHF_SYSCTL_FAN3, "fan3", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627ehf_fan},
	{W83627EHF_SYSCTL_FAN4, "fan4", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627ehf_fan},
	{W83627EHF_SYSCTL_FAN5, "fan5", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627ehf_fan},

	{W83627EHF_SYSCTL_TEMP1, "temp1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627ehf_temp},
	{W83627EHF_SYSCTL_TEMP2, "temp2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627ehf_temp},
	{W83627EHF_SYSCTL_TEMP3, "temp3", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627ehf_temp},

	/* manual adjust fan divisor not allowed */
	{W83627EHF_SYSCTL_FAN_DIV, "fan_div", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &w83627ehf_fan_div},
	{W83627EHF_SYSCTL_ALARMS, "alarms", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627ehf_alarms},

	{W83627EHF_SYSCTL_PWM1, "pwm1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627ehf_pwm},
	{W83627EHF_SYSCTL_PWM2, "pwm2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627ehf_pwm},
	{W83627EHF_SYSCTL_PWM3, "pwm3", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627ehf_pwm},
	{W83627EHF_SYSCTL_PWM4, "pwm4", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627ehf_pwm},

	{W83627EHF_SYSCTL_VID, "vid", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627ehf_vid},
	{W83627EHF_SYSCTL_VRM, "vrm", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627ehf_vrm},

	{W83627EHF_SYSCTL_SENSOR1, "sensor1", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627ehf_sensor},
	{W83627EHF_SYSCTL_SENSOR2, "sensor2", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627ehf_sensor},
	{W83627EHF_SYSCTL_SENSOR3, "sensor3", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627ehf_sensor},
	{0}
};

/*
 * Driver and client management
 */

static struct i2c_driver w83627ehf_driver;

static void w83627ehf_init_client(struct i2c_client *client)
{
	struct w83627ehf_data *data = client->data;
	int i;
	u8 tmp, diode;

	/* Start monitoring if needed */
	tmp = w83627ehf_read_value(client, W83627EHF_REG_CONFIG);
	if (!(tmp & 0x01))
		w83627ehf_write_value(client, W83627EHF_REG_CONFIG,
				      tmp | 0x01);

	/* Enable temp2 and temp3 if needed */
	for (i = 0; i < 2; i++) {
		tmp = w83627ehf_read_value(client,
					   W83627EHF_REG_TEMP_CONFIG[i]);
		if (tmp & 0x01)
			w83627ehf_write_value(client,
					      W83627EHF_REG_TEMP_CONFIG[i],
					      tmp & 0xfe);
	}

	/* Enable VBAT monitoring if needed */
	tmp = w83627ehf_read_value(client, W83627EHF_REG_VBAT);
	if (!(tmp & 0x01)) {
		printk(KERN_INFO "w83627ehf: Enabling VBAT monitoring\n");
		w83627ehf_write_value(client, W83627EHF_REG_VBAT, tmp | 0x01);
	}

	/* Get thermal sensor types */
	diode = w83627ehf_read_value(client, W83627EHF_REG_DIODE);
	for (i = 0; i < 3; i++) {
		if ((tmp & (0x02 << i)))
			data->sensor[i] = (diode & (0x10 << i)) ? 1 : 2;
		else
			data->sensor[i] = 4; /* thermistor */
	}
}

static int w83627ehf_detect(struct i2c_adapter *adapter, int address,
			    unsigned short flags, int kind)
{
	struct i2c_client *client;
	struct w83627ehf_data *data;
	u8 fan4pin, fan5pin;
	int i, err;

	if (!i2c_is_isa_adapter(adapter))
		return -ENODEV;

	if (!request_region(address + REGION_OFFSET, REGION_LENGTH,
			    "w83627ehf")) {
		printk(KERN_ERR "w83627ehf: Region 0x%x-0x%x already in "
		       "use!\n", address + REGION_OFFSET,
		       address + REGION_OFFSET + REGION_LENGTH - 1);
		err = -EBUSY;
		goto exit;
	}

	if (!(data = kmalloc(sizeof(struct w83627ehf_data), GFP_KERNEL))) {
		err = -ENOMEM;
		goto exit_release;
	}
	memset(data, 0, sizeof(struct w83627ehf_data));

	client = &data->client;
	client->addr = address;
	client->data = data;
	client->adapter = adapter;
	client->driver = &w83627ehf_driver;
	strcpy(client->name, "W83627EHF chip");
	init_MUTEX(&data->lock);
	init_MUTEX(&data->update_lock);

	/* Tell the i2c layer a new client has arrived */
	if ((err = i2c_attach_client(client)))
		goto exit_free;

	w83627ehf_init_client(client);

	/* Read VID value */
	superio_enter();
	superio_select(W83627EHF_LD_HWM);
	if (superio_inb(SIO_REG_VID_CTRL) & 0x80)	/* VID input mode */
		data->vid = superio_inb(SIO_REG_VID_DATA) & 0x3f;
	else {
		printk(KERN_NOTICE "w83627ehf: VID pins in output mode, CPU "
		       "VID not available\n");
		data->vid = 0x3f;
	}

	/* Wild guess, might not be correct */
	if (superio_inb(SIO_REG_EN_VRM10) & 0x08)
		data->vrm = 100;
	else
		data->vrm = 91;

	/* fan4 and fan5 share some pins with the GPIO and serial flash */
	fan5pin = superio_inb(0x24) & 0x2;
	fan4pin = superio_inb(0x29) & 0x6;
	superio_exit();

	/* It looks like fan4 and fan5 pins can be alternatively used
	   as fan on/off switches, but fan5 control is write only :/
	   We assume that if the serial interface is disabled, designers
	   connected fan5 as input unless they are emitting log 1, which
	   is not the default. */

	data->has_fan = 0x07;	/* fan1, fan2 and fan3 */
	i = w83627ehf_read_value(client, W83627EHF_REG_FANDIV1);
	if ((i & (1 << 2)) && (!fan4pin))
		data->has_fan |= (1 << 3);
	if (!(i & (1 << 1)) && (!fan5pin))
		data->has_fan |= (1 << 4);

	/* Register a new directory entry with module sensors */
	if ((i = i2c_register_entry(client,
				    "w83627ehf",
				    w83627ehf_dir_table_template,
				    THIS_MODULE)) < 0) {
		err = i;
		goto exit_detach;
	}
	data->sysctl_id = i;

	return 0;

 exit_detach:
	i2c_detach_client(client);
 exit_free:
	kfree(data);
 exit_release:
	release_region(address + REGION_OFFSET, REGION_LENGTH);
 exit:
	return err;

}

static int w83627ehf_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_detect(adapter, &addr_data, w83627ehf_detect);
}

static int w83627ehf_detach_client(struct i2c_client *client)
{
	struct w83627ehf_data *data = client->data;
	int err;

	i2c_deregister_entry(data->sysctl_id);

	if ((err = i2c_detach_client(client))) {
		printk(KERN_ERR "w83627ehf: Client deregistration failed\n");
		return err;
	}

	release_region(client->addr + REGION_OFFSET, REGION_LENGTH);
	kfree(client->data);

	return 0;
}

static struct i2c_driver w83627ehf_driver = {
	.name		= "w83627ehf",
	.flags		= I2C_DF_NOTIFY,
	.attach_adapter	= w83627ehf_attach_adapter,
	.detach_client	= w83627ehf_detach_client,
};

static int __init w83627ehf_find(int sioaddr, unsigned int *addr)
{
	u16 val;
	const char *devname;

	REG = sioaddr;
	VAL = sioaddr + 1;
	superio_enter();

	val = (superio_inb(SIO_REG_DEVID) << 8)
	    | superio_inb(SIO_REG_DEVID + 1);
	switch (val & SIO_ID_MASK) {
	case SIO_W83627EHF_ID:
		devname = "W83627EHF";
		break;
	case SIO_W83627EHG_ID:
		devname = "W83627EHG";
		break;
	default:
		superio_exit();
		return -ENODEV;
	}

	superio_select(W83627EHF_LD_HWM);
	val = (superio_inb(SIO_REG_ADDR) << 8)
	    | superio_inb(SIO_REG_ADDR + 1);
	*addr = val & REGION_ALIGNMENT;
	if (*addr == 0) {
		printk(KERN_ERR "w83627ehf: HWM I/O area not set\n");
		superio_exit();
		return -ENODEV;
	}

	printk(KERN_INFO "w83627ehf: Found %s device at 0x%x\n",
	       devname, *addr);

	/* Activate logical device if needed */
	val = superio_inb(SIO_REG_ENABLE);
	if (!(val & 0x01)) {
		printk(KERN_WARNING "w83627ehf: HWM disabled, enabling\n");
		superio_outb(SIO_REG_ENABLE, val | 0x01);
	}

	superio_exit();
	return 0;
}

static int __init sensors_w83627ehf_init(void)
{
	printk(KERN_INFO "w83627ehf: Driver version %s (%s)\n",
	       LM_VERSION, LM_DATE);
	if (w83627ehf_find(0x2e, &normal_isa[0])
	 && w83627ehf_find(0x4e, &normal_isa[0]))
		return -ENODEV;

	return i2c_add_driver(&w83627ehf_driver);
}

static void __exit sensors_w83627ehf_exit(void)
{
	i2c_del_driver(&w83627ehf_driver);
}

MODULE_AUTHOR("Yuan Mu (Winbond), Jean Delvare <khali@linux-fr.org>");
MODULE_DESCRIPTION("W83627EHF harware monitoring driver");
MODULE_LICENSE("GPL");

module_init(sensors_w83627ehf_init);
module_exit(sensors_w83627ehf_exit);
