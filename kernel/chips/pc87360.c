/*
 *  pc87360.c - Part of lm_sensors, Linux kernel modules
 *              for hardware monitoring
 *  Copyright (C) 2004 Jean Delvare <khali@linux-fr.org> 
 *
 *  Copied from smsc47m1.c:
 *  Copyright (C) 2002 Mark D. Studebaker <mdsxyz123@yahoo.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Supports the following chips:
 *
 *  Chip	#vin	#fan	#pwm	#temp	devid
 *  PC87360	-	2	2	-	0xE1
 *  PC87363	-	2	2	-	0xE8
 *  PC87364	-	3	3	-	0xE4
 *  PC87365	11	3	3	2	0xE5
 *  PC87366	11	3	3	3	0xE9
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/i2c.h>
#include <linux/i2c-proc.h>
#include <linux/init.h>
#include <asm/io.h>
#include "version.h"

static unsigned short normal_i2c[] = { SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { SENSORS_I2C_END };
static unsigned int normal_isa[] = { 0x0000, SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };
static u8 devid;
static unsigned int extra_isa[] = { 0x0000, 0x0000, 0x0000 };

SENSORS_INSMOD_5(pc87360, pc87363, pc87364, pc87365, pc87366);

/*
 * Super-I/O registers and operations
 */

#define REG	0x2e	/* The register to read/write */
#define VAL	0x2f	/* The value to read/write */

#define DEV	0x07	/* Register: Logical device select */
#define DEVID	0x20	/* Register: Device ID */
#define ACT	0x30	/* Register: Device activation */
#define BASE	0x60    /* Register: Base address */

#define FSCM	0x09	/* Logical device: fans */
#define VLM	0x0d	/* Logical device: voltages */
#define TMS	0x0e	/* Logical device: temperatures */
static const u8 logdev[3] = { FSCM, VLM, TMS };

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

static inline void superio_exit(void)
{
	outb(0x02, REG);
	outb(0x02, VAL);
}

#define PC87360_EXTENT		0x10

/*
 * Fan registers and conversions
 */

/* nr has to be 0 or 1 (PC87360/87363) or 2 (PC87364/87365/87366) */
#define PC87360_REG_PRESCALE(nr)	(0x00 + 2 * (nr))
#define PC87360_REG_PWM(nr)		(0x01 + 2 * (nr))
#define PC87360_REG_FAN_MIN(nr)		(0x06 + 3 * (nr))
#define PC87360_REG_FAN(nr)		(0x07 + 3 * (nr))
#define PC87360_REG_FAN_STATUS(nr)	(0x08 + 3 * (nr))

#define FAN_FROM_REG(val,div)		((val)==0?-1:(val)==255?0: \
					 480000/((val)*(div)))
#define FAN_TO_REG(val,div)		((val)<=0?255: \
					 480000/((val)*(div)))
#define FAN_DIV_FROM_REG(val)		(1 << ((val >> 5) & 0x03))
#define FAN_DIV_TO_REG(val)		((val)==8?0x60:(val)==4?0x40: \
					 (val)==1?0x00:0x20)
#define FAN_STATUS_FROM_REG(val)	((val) & 0x07)

/*
 * Voltage registers and conversions
 */

#define PC87365_REG_IN_BANK		0x09

/* nr has to be 0 to 11 (PC87365/87366) */
#define PC87365_REG_IN			0x0B
#define PC87365_REG_IN_MIN		0x0D
#define PC87365_REG_IN_MAX		0x0C
#define PC87365_REG_IN_STATUS		0x0A
#define PC87365_REG_IN_ALARMS1		0x00
#define PC87365_REG_IN_ALARMS2		0x01

#define IN_FROM_REG(val)		(((val) * 297 + 127) / 255)
#define IN_TO_REG(val)			((val)<0?0:(val)>297?255: \
					 ((val) * 255 + 148) / 297)
#define IN_STATUS_FROM_REG(val)		((val) & 0x86)

/*
 * Temperature registers and conversions
 */

#define PC87365_REG_TEMP_BANK		0x09

/* nr has to be 0 to 1 (PC87365) or 2 (PC87366) */
#define PC87365_REG_TEMP		0x0B
#define PC87365_REG_TEMP_MIN		0x0D
#define PC87365_REG_TEMP_MAX		0x0C
#define PC87365_REG_TEMP_CRIT		0x0D
#define PC87365_REG_TEMP_STATUS		0x0A
#define PC87365_REG_TEMP_ALARMS		0x00

#define TEMP_FROM_REG(val)		((val)&0x80 ? (val) : (val) - 128)
#define TEMP_TO_REG(val)		((val)<-128?0x80:(val)>127?0x7F: \
					 (val)<0?(val)+0x80:(val))
#define TEMP_STATUS_FROM_REG(val)	((val) & 0xCE)

struct pc87360_data {
	struct i2c_client client;
	struct semaphore lock;
	int sysctl_id;
	int address[3];

	struct semaphore update_lock;
	char valid;		/* !=0 if following fields are valid */
	unsigned long last_updated;	/* In jiffies */

	u8 fannr, innr, tempnr;

	u8 fan[3];		/* Register value */
	u8 fan_min[3];		/* Register value */
	u8 fan_status[3];	/* Register value */
	u8 pwm[3];		/* Register value */

	u8 in[11];		/* Register value */
	u8 in_min[11];		/* Register value */
	u8 in_max[11];		/* Register value */
	u8 in_status[11];	/* Register value */
	u16 in_alarms;		/* Register values, combined */

	u8 temp[3];		/* Register value */
	u8 temp_min[3];		/* Register value */
	u8 temp_max[3];		/* Register value */
	u8 temp_crit[3];	/* Register value */
	u8 temp_status[3];	/* Register value */
	u8 temp_alarms;		/* Register value */
};


static int pc87360_attach_adapter(struct i2c_adapter *adapter);
static int pc87360_detect(struct i2c_adapter *adapter, int address,
			  unsigned short flags, int kind);
static int pc87360_detach_client(struct i2c_client *client);

static int pc87360_read_value(struct pc87360_data *data, int ldi, u8 reg);
static int pc87360_write_value(struct pc87360_data *data, int ldi, u8 reg,
			       u8 value);
static void pc87360_update_client(struct i2c_client *client);
static int pc87360_find(u8 *devid, int *address);


void pc87365_alarms(struct i2c_client *client, int operation, int ctl_name,
		    int *nrels_mag, long *results);

static void pc87360_fan(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void pc87360_fan_status(struct i2c_client *client, int operation,
			       int ctl_name, int *nrels_mag, long *results);
static void pc87360_fan_div(struct i2c_client *client, int operation,
			    int ctl_name, int *nrels_mag, long *results);
static void pc87360_pwm(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);

void pc87365_in(struct i2c_client *client, int operation, int ctl_name,
		int *nrels_mag, long *results);
void pc87365_in_status(struct i2c_client *client, int operation, int ctl_name,
		       int *nrels_mag, long *results);

void pc87365_temp(struct i2c_client *client, int operation, int ctl_name,
		  int *nrels_mag, long *results);
void pc87365_temp_status(struct i2c_client *client, int operation, int ctl_name,
			 int *nrels_mag, long *results);

static int pc87360_id = 0;

static struct i2c_driver pc87360_driver = {
	.owner		= THIS_MODULE,
	.name		= "PC8736x hardware monitor",
	.flags		= I2C_DF_NOTIFY,
	.attach_adapter	= pc87360_attach_adapter,
	.detach_client	= pc87360_detach_client,
};

/* -- SENSORS SYSCTL START -- */

#define PC87365_SYSCTL_ALARMS		100 /* bit field */

#define PC87360_SYSCTL_FAN1		1101 /* Rotations/min */
#define PC87360_SYSCTL_FAN2		1102
#define PC87360_SYSCTL_FAN3		1103 /* not for PC87360/PC87363 */
#define PC87360_SYSCTL_FAN_DIV		1201 /* 1, 2, 4 or 8 */
#define PC87360_SYSCTL_FAN1_STATUS	1301 /* bit field */
#define PC87360_SYSCTL_FAN2_STATUS	1302
#define PC87360_SYSCTL_FAN3_STATUS	1303 /* not for PC87360/PC87363 */
#define PC87360_SYSCTL_PWM1		1401 /* 0-255 */
#define PC87360_SYSCTL_PWM2		1402
#define PC87360_SYSCTL_PWM3		1403 /* not for PC87360/PC87363 */

#define PC87360_STATUS_FAN_READY	0x01
#define PC87360_STATUS_FAN_LOW		0x02
#define PC87360_STATUS_FAN_OVERFLOW	0x04

#define PC87365_SYSCTL_IN0		2100 /* mV */
#define PC87365_SYSCTL_IN1		2101
#define PC87365_SYSCTL_IN2		2102
#define PC87365_SYSCTL_IN3		2103
#define PC87365_SYSCTL_IN4		2104
#define PC87365_SYSCTL_IN5		2105
#define PC87365_SYSCTL_IN6		2106
#define PC87365_SYSCTL_IN7		2107
#define PC87365_SYSCTL_IN8		2108
#define PC87365_SYSCTL_IN9		2109
#define PC87365_SYSCTL_IN10		2110
#define PC87365_SYSCTL_IN0_STATUS	2300 /* bit field */
#define PC87365_SYSCTL_IN1_STATUS	2301
#define PC87365_SYSCTL_IN2_STATUS	2302
#define PC87365_SYSCTL_IN3_STATUS	2303
#define PC87365_SYSCTL_IN4_STATUS	2304
#define PC87365_SYSCTL_IN5_STATUS	2305
#define PC87365_SYSCTL_IN6_STATUS	2306
#define PC87365_SYSCTL_IN7_STATUS	2307
#define PC87365_SYSCTL_IN8_STATUS	2308
#define PC87365_SYSCTL_IN9_STATUS	2309
#define PC87365_SYSCTL_IN10_STATUS	2310

#define PC87365_STATUS_IN_MIN		0x02
#define PC87365_STATUS_IN_MAX		0x04

#define PC87365_SYSCTL_TEMP1		3101 /* degrees Celcius */
#define PC87365_SYSCTL_TEMP2		3102
#define PC87365_SYSCTL_TEMP3		3103 /* not for PC87365 */
#define PC87365_SYSCTL_TEMP1_STATUS	3101 /* bit field */
#define PC87365_SYSCTL_TEMP2_STATUS	3102
#define PC87365_SYSCTL_TEMP3_STATUS	3103 /* not for PC87365 */

#define PC87365_STATUS_TEMP_MIN		0x02
#define PC87365_STATUS_TEMP_MAX		0x04
#define PC87365_STATUS_TEMP_CRIT	0x08
#define PC87365_STATUS_TEMP_OPEN	0x40

/* -- SENSORS SYSCTL END -- */

static ctl_table pc87360_dir_table_template[] = { /* PC87363 and PC87364 too */
	{PC87360_SYSCTL_FAN1, "fan1", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87360_fan},
	{PC87360_SYSCTL_FAN2, "fan2", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87360_fan},
	{PC87360_SYSCTL_FAN3, "fan3", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87360_fan},
	{PC87360_SYSCTL_FAN_DIV, "fan_div", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87360_fan_div},
	{PC87360_SYSCTL_FAN1_STATUS, "fan1_status", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87360_fan_status},
	{PC87360_SYSCTL_FAN2_STATUS, "fan2_status", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87360_fan_status},
	{PC87360_SYSCTL_FAN3_STATUS, "fan3_status", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87360_fan_status},
	{PC87360_SYSCTL_PWM1, "pwm1", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87360_pwm},
	{PC87360_SYSCTL_PWM2, "pwm2", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87360_pwm},
	{PC87360_SYSCTL_PWM3, "pwm3", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87360_pwm},
	{0}
};

static ctl_table pc87365_dir_table_template[] = { /* PC87366 too */
	{PC87365_SYSCTL_ALARMS, "alarms", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87365_alarms},
	{PC87360_SYSCTL_FAN1, "fan1", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87360_fan},
	{PC87360_SYSCTL_FAN2, "fan2", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87360_fan},
	{PC87360_SYSCTL_FAN3, "fan3", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87360_fan},
	{PC87360_SYSCTL_FAN_DIV, "fan_div", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87360_fan_div},
	{PC87360_SYSCTL_FAN1_STATUS, "fan1_status", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87360_fan_status},
	{PC87360_SYSCTL_FAN2_STATUS, "fan2_status", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87360_fan_status},
	{PC87360_SYSCTL_FAN3_STATUS, "fan3_status", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87360_fan_status},
	{PC87360_SYSCTL_PWM1, "pwm1", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87360_pwm},
	{PC87360_SYSCTL_PWM2, "pwm2", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87360_pwm},
	{PC87360_SYSCTL_PWM3, "pwm3", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87360_pwm},
	{PC87365_SYSCTL_IN0, "in0", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87365_in},
	{PC87365_SYSCTL_IN1, "in1", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87365_in},
	{PC87365_SYSCTL_IN2, "in2", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87365_in},
	{PC87365_SYSCTL_IN3, "in3", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87365_in},
	{PC87365_SYSCTL_IN4, "in4", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87365_in},
	{PC87365_SYSCTL_IN5, "in5", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87365_in},
	{PC87365_SYSCTL_IN6, "in6", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87365_in},
	{PC87365_SYSCTL_IN7, "in7", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87365_in},
	{PC87365_SYSCTL_IN8, "in8", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87365_in},
	{PC87365_SYSCTL_IN9, "in9", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87365_in},
	{PC87365_SYSCTL_IN10, "in10", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87365_in},
	{PC87365_SYSCTL_IN0_STATUS, "in0_status", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87365_in_status},
	{PC87365_SYSCTL_IN1_STATUS, "in1_status", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87365_in_status},
	{PC87365_SYSCTL_IN2_STATUS, "in2_status", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87365_in_status},
	{PC87365_SYSCTL_IN3_STATUS, "in3_status", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87365_in_status},
	{PC87365_SYSCTL_IN4_STATUS, "in4_status", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87365_in_status},
	{PC87365_SYSCTL_IN5_STATUS, "in5_status", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87365_in_status},
	{PC87365_SYSCTL_IN6_STATUS, "in6_status", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87365_in_status},
	{PC87365_SYSCTL_IN7_STATUS, "in7_status", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87365_in_status},
	{PC87365_SYSCTL_IN8_STATUS, "in8_status", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87365_in_status},
	{PC87365_SYSCTL_IN9_STATUS, "in9_status", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87365_in_status},
	{PC87365_SYSCTL_IN10_STATUS, "in10_status", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87365_in_status},
	{PC87365_SYSCTL_TEMP1, "temp1", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87365_temp},
	{PC87365_SYSCTL_TEMP2, "temp2", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87365_temp},
	{PC87365_SYSCTL_TEMP3, "temp3", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87365_temp},
	{PC87365_SYSCTL_TEMP1_STATUS, "temp1_status", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87365_temp_status},
	{PC87365_SYSCTL_TEMP2_STATUS, "temp2_status", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87365_temp_status},
	{PC87365_SYSCTL_TEMP3_STATUS, "temp3_status", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87365_temp_status},
	{0}
};

static int pc87360_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_detect(adapter, &addr_data, pc87360_detect);
}

static int pc87360_find(u8 *devid, int *address)
{
	u16 val;
	int i;
	int nrdev; /* logical device count */

	/* no superio_enter */

	/* identify device */
	val = superio_inb(DEVID);
	switch (val) {
       	case 0xE1: /* PC87360 */
	case 0xE8: /* PC87363 */
	case 0xE4: /* PC87364 */
		nrdev = 1;
		break;
	case 0xE5: /* PC87365 */
	case 0xE9: /* PC87366 */
		nrdev = 3;
		break;
	default:
		superio_exit();
		return -ENODEV;
	}
	/* remember the device id */
	*devid = val;

	for (i = 0; i < nrdev; i++) {
		/* select logical device */
		superio_outb(DEV, logdev[i]);

		val = superio_inb(ACT);
		if (!(val & 0x01)) {
			printk(KERN_INFO "pc87360.o: device 0x%02x not "
			       "activated\n", logdev[i]);
			continue;
		}

		val = (superio_inb(BASE) << 8)
		    | superio_inb(BASE + 1);
		if (!val) {
			printk(KERN_INFO "pc87360.o: base address not set for "
			       "device 0x%02x\n", logdev[i]);
			continue;
		}

		address[i] = val;
	}

	superio_exit();
	return 0;
}

/* We don't really care about the address.
   Read from extra_isa instead. */
int pc87360_detect(struct i2c_adapter *adapter, int address,
		   unsigned short flags, int kind)
{
	int i;
	struct i2c_client *new_client;
	struct pc87360_data *data;
	int err = 0;
	const char *type_name = "pc87360";
	const char *client_name = "PC8736x chip";
	const ctl_table *template = pc87360_dir_table_template;

	if (!i2c_is_isa_adapter(adapter)) {
		return 0;
	}

	for (i = 0; i < 3; i++) {
		if (extra_isa[i]
		 && check_region(extra_isa[i], PC87360_EXTENT)) {
			printk(KERN_ERR "pc87360.o: region 0x%x already in "
			       "use!\n", address);
			return -ENODEV;
		}
	}

	if (!(data = kmalloc(sizeof(struct pc87360_data), GFP_KERNEL))) {
		return -ENOMEM;
	}

	new_client = &data->client;
	new_client->addr = address;
	init_MUTEX(&data->lock);
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &pc87360_driver;
	new_client->flags = 0;

	data->fannr = 2;
	data->innr = 0;
	data->tempnr = 0;
	
	switch (devid) {
	case 0xe8:
		type_name = "pc87363";
		break;
	case 0xe4:
		type_name = "pc87364";
		data->fannr = 3;
		break;
	case 0xe5:
		type_name = "pc87365";
		template = pc87365_dir_table_template;
		data->fannr = extra_isa[0] ? 3 : 0;
		data->innr = extra_isa[1] ? 11 : 0;
		data->tempnr = extra_isa[2] ? 2 : 0;
		break;
	case 0xe9:
		type_name = "pc87366";
		template = pc87365_dir_table_template;
		data->fannr = extra_isa[0] ? 3 : 0;
		data->innr = extra_isa[1] ? 11 : 0;
		data->tempnr = extra_isa[2] ? 3 : 0;
		break;
	}

	for (i = 0; i < 3; i++) {
		if ((data->address[i] = extra_isa[i])) {
			request_region(extra_isa[i], PC87360_EXTENT, "pc87360");
		}
	}
	strcpy(new_client->name, client_name);

	new_client->id = pc87360_id++;
	data->valid = 0;
	init_MUTEX(&data->update_lock);

	if ((err = i2c_attach_client(new_client)))
		goto ERROR1;

	if ((i = i2c_register_entry((struct i2c_client *) new_client,
				    type_name, template)) < 0) {
		err = i;
		goto ERROR2;
	}
	data->sysctl_id = i;

	return 0;

ERROR2:
	i2c_detach_client(new_client);
ERROR1:
	for (i = 0; i < 3; i++) {
		if (data->address[i]) {
			release_region(data->address[i], PC87360_EXTENT);
		}
	}
	kfree(data);
	return err;
}

static int pc87360_detach_client(struct i2c_client *client)
{
	struct pc87360_data *data = client->data;
	int i, err;

	i2c_deregister_entry(data->sysctl_id);

	if ((err = i2c_detach_client(client))) {
		printk(KERN_ERR "pc87360.o: Client deregistration failed, "
		       "client not detached.\n");
		return err;
	}

	for (i = 0; i < 3; i++) {
		if (data->address[i]) {
			release_region(data->address[i], PC87360_EXTENT);
		}
	}
	kfree(client->data);

	return 0;
}

/* ldi is the logical device index:
   0: fans
   1: voltages
   2: temperatures */
static int pc87360_read_value(struct pc87360_data *data, int ldi, u8 reg)
{
	int res;

	down(&(data->lock));
	res = inb_p(data->address[ldi] + reg);
	up(&(data->lock));
	return res;
}

static int pc87360_write_value(struct pc87360_data *data, int ldi, u8 reg,
			       u8 value)
{
	down(&(data->lock));
	outb_p(value, data->address[ldi] + reg);
	up(&(data->lock));
	return 0;
}

static void pc87360_update_client(struct i2c_client *client)
{
	struct pc87360_data *data = client->data;
	int i;

	down(&data->update_lock);

	if ((jiffies - data->last_updated > HZ + HZ / 2) ||
	    (jiffies < data->last_updated) || !data->valid) {
		for (i = 0; i < data->fannr; i++) {
			data->fan[i] = pc87360_read_value(data, 0,
				       PC87360_REG_FAN(i));
			data->fan_min[i] = pc87360_read_value(data, 0,
					   PC87360_REG_FAN_MIN(i));
			data->fan_status[i] = pc87360_read_value(data, 0,
					      PC87360_REG_FAN_STATUS(i));
			data->pwm[i] = pc87360_read_value(data, 0,
				       PC87360_REG_PWM(i));
		}

		for (i = 0; i < data->innr; i++) {
			pc87360_write_value(data, 1, PC87365_REG_IN_BANK, i);
			data->in_status[i] = pc87360_read_value(data, 1,
			                     PC87365_REG_IN_STATUS);
			if (data->in_status[i] & 0x01) {
				data->in[i] = pc87360_read_value(data, 1,
					      PC87365_REG_IN);
				data->in_min[i] = pc87360_read_value(data, 1,
						  PC87365_REG_IN_MIN);
				data->in_max[i] = pc87360_read_value(data, 1,
						  PC87365_REG_IN_MAX);
			}
			data->in_alarms = pc87360_read_value(data, 1,
					  PC87365_REG_IN_ALARMS1)
					| (pc87360_read_value(data, 1,
					   PC87365_REG_IN_ALARMS2) << 8);
		}

		for (i = 0; i < data->tempnr; i++) {
			pc87360_write_value(data, 2, PC87365_REG_TEMP_BANK, i);
			data->temp_status[i] = pc87360_read_value(data, 2,
					       PC87365_REG_TEMP_STATUS);
			if (data->temp_status[i] & 0x01) {
				data->temp[i] = pc87360_read_value(data, 2,
						PC87365_REG_TEMP);
				data->temp_min[i] = pc87360_read_value(data, 2,
						    PC87365_REG_TEMP_MIN);
				data->temp_max[i] = pc87360_read_value(data, 2,
						    PC87365_REG_TEMP_MAX);
				data->temp_crit[i] = pc87360_read_value(data, 2,
						     PC87365_REG_TEMP_CRIT);
			}
			data->temp_alarms = pc87360_read_value(data, 2,
					    PC87365_REG_TEMP_ALARMS);
		}

		data->last_updated = jiffies;
		data->valid = 1;
	}

	up(&data->update_lock);
}


void pc87365_alarms(struct i2c_client *client, int operation, int ctl_name,
		    int *nrels_mag, long *results)
{
	struct pc87360_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		pc87360_update_client(client);
		results[0] = data->in_alarms;
		results[1] = data->temp_alarms;
		*nrels_mag = 2;
	}
}

void pc87360_fan(struct i2c_client *client, int operation, int ctl_name,
		 int *nrels_mag, long *results)
{
	struct pc87360_data *data = client->data;
	int nr = ctl_name - PC87360_SYSCTL_FAN1;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		pc87360_update_client(client);
		results[0] = FAN_FROM_REG(data->fan_min[nr],
			     FAN_DIV_FROM_REG(data->fan_status[nr]));
		results[1] = FAN_FROM_REG(data->fan[nr],
			     FAN_DIV_FROM_REG(data->fan_status[nr]));
		*nrels_mag = 2;
	}
	/* We ignore National's recommendation */
	else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (nr >= data->fannr)
			return;
		if (*nrels_mag >= 1) {
			data->fan_min[nr] = FAN_TO_REG(results[0],
					    FAN_DIV_FROM_REG(data->fan_status[nr]));
			pc87360_write_value(data, 0, PC87360_REG_FAN_MIN(nr),
					    data->fan_min[nr]);
		}
	}
}

void pc87360_fan_div(struct i2c_client *client, int operation,
		     int ctl_name, int *nrels_mag, long *results)
{
	struct pc87360_data *data = client->data;
	int i;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		pc87360_update_client(client);
		for (i = 0; i < data->fannr; i++) {
			results[i] = FAN_DIV_FROM_REG(data->fan_status[i]);
		}
		*nrels_mag = data->fannr;
	}
	/* We ignore National's recommendation */
	else if (operation == SENSORS_PROC_REAL_WRITE) {
		for (i = 0; i < data->fannr && i < *nrels_mag; i++) {
			/* Preserve fan min */
			int fan_min = FAN_FROM_REG(data->fan_min[i],
				      FAN_DIV_FROM_REG(data->fan_status[i]));
			data->fan_status[i] = (data->fan_status[i] & 0x9F)
					    | FAN_DIV_TO_REG(results[i]);
			pc87360_write_value(data, 0, PC87360_REG_FAN_STATUS(i),
					    data->fan_status[i]);
			data->fan_min[i] = FAN_TO_REG(fan_min,
					   FAN_DIV_FROM_REG(data->fan_status[i]));
			pc87360_write_value(data, 0, PC87360_REG_FAN_MIN(i),
					    data->fan_min[i]);
		}
	}
}

void pc87360_pwm(struct i2c_client *client, int operation, int ctl_name,
		 int *nrels_mag, long *results)
{
	struct pc87360_data *data = client->data;
	int nr = ctl_name - PC87360_SYSCTL_PWM1;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		pc87360_update_client(client);
		results[0] = data->pwm[nr];
		*nrels_mag = 1;
	}
	else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1)
		{
			data->pwm[nr] = SENSORS_LIMIT(results[0], 0, 255);
			pc87360_write_value(data, 0, PC87360_REG_PWM(nr),
					    data->pwm[nr]);
		}
	}
}

void pc87360_fan_status(struct i2c_client *client, int operation, int ctl_name,
			int *nrels_mag, long *results)
{
	struct pc87360_data *data = client->data;
	int nr = ctl_name - PC87360_SYSCTL_FAN1_STATUS;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		pc87360_update_client(client);
		results[0] = FAN_STATUS_FROM_REG(data->fan_status[nr]);
		*nrels_mag = 1;
	}
}

void pc87365_in(struct i2c_client *client, int operation, int ctl_name,
		int *nrels_mag, long *results)
{
	struct pc87360_data *data = client->data;
	int nr = ctl_name - PC87365_SYSCTL_IN0;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 2;
	else if (operation == SENSORS_PROC_REAL_READ) {
		pc87360_update_client(client);
		results[0] = IN_FROM_REG(data->in_min[nr]);
		results[1] = IN_FROM_REG(data->in_max[nr]);
		results[2] = IN_FROM_REG(data->in[nr]);
		*nrels_mag = 3;
	}
	else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->in_min[nr] = IN_TO_REG(results[0]);
			pc87360_write_value(data, 1, PC87365_REG_IN_BANK, nr);
			pc87360_write_value(data, 1, PC87365_REG_IN_MIN,
					    data->in_min[nr]);
		}
		if (*nrels_mag >= 2) {
			data->in_max[nr] = IN_TO_REG(results[1]);
			pc87360_write_value(data, 1, PC87365_REG_IN_MAX,
					    data->in_max[nr]);
		}
	}
}

void pc87365_in_status(struct i2c_client *client, int operation, int ctl_name,
		       int *nrels_mag, long *results)
{
	struct pc87360_data *data = client->data;
	int nr = ctl_name - PC87365_SYSCTL_IN0_STATUS;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		pc87360_update_client(client);
		results[0] = IN_STATUS_FROM_REG(data->in_status[nr]);
		*nrels_mag = 1;
	}
}

void pc87365_temp(struct i2c_client *client, int operation, int ctl_name,
		  int *nrels_mag, long *results)
{
	struct pc87360_data *data = client->data;
	int nr = ctl_name - PC87365_SYSCTL_TEMP1;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		pc87360_update_client(client);
		results[0] = TEMP_FROM_REG(data->temp_max[nr]);
		results[1] = TEMP_FROM_REG(data->temp_min[nr]);
		results[1] = TEMP_FROM_REG(data->temp_crit[nr]);
		results[2] = TEMP_FROM_REG(data->temp[nr]);
		*nrels_mag = 4;
	}
	else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (nr >= data->tempnr)
			return;
		if (*nrels_mag >= 1) {
			pc87360_write_value(data, 2, PC87365_REG_TEMP_BANK, nr);
			data->temp_max[nr] = TEMP_TO_REG(results[0]);
			pc87360_write_value(data, 2, PC87365_REG_TEMP_MAX,
					    data->temp_max[nr]);
		}
		if (*nrels_mag >= 2) {
			data->temp_min[nr] = TEMP_TO_REG(results[1]);
			pc87360_write_value(data, 2, PC87365_REG_TEMP_MAX,
					    data->temp_max[nr]);
		}
		if (*nrels_mag >= 3) {
			data->temp_crit[nr] = TEMP_TO_REG(results[2]);
			pc87360_write_value(data, 2, PC87365_REG_TEMP_CRIT,
					    data->temp_crit[nr]);
		}
	}
}

void pc87365_temp_status(struct i2c_client *client, int operation, int ctl_name,
			 int *nrels_mag, long *results)
{
	struct pc87360_data *data = client->data;
	int nr = ctl_name - PC87365_SYSCTL_TEMP1_STATUS;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		pc87360_update_client(client);
		results[0] = TEMP_STATUS_FROM_REG(data->temp_status[nr]);
		*nrels_mag = 1;
	}
}


static int __init pc87360_init(void)
{
	int i;

	printk(KERN_INFO "pc87360.o version %s (%s)\n", LM_VERSION, LM_DATE);

	if (pc87360_find(&devid, extra_isa)) {
		printk(KERN_WARNING "pc87360.o: PC8736x not detected, "
		       "module not inserted.\n");
		return -ENODEV;
	}

	/* Arbitrarily pick one of the addresses */
	for (i = 0; i < 3; i++) {
		if (extra_isa[i] != 0x0000) {
			normal_isa[0] = extra_isa[i];
			break;
		}
	}

	if (normal_isa[0] == 0x0000) {
		printk(KERN_WARNING "pc87360.o: No active logical device, "
		       "module not inserted.\n");
		return -ENODEV;
	
	}

	return i2c_add_driver(&pc87360_driver);
}

static void __exit pc87360_exit(void)
{
	i2c_del_driver(&pc87360_driver);
}


MODULE_AUTHOR("Jean Delvare <khali@linux-fr.org>");
MODULE_DESCRIPTION("PC8736x hardware monitor");
MODULE_LICENSE("GPL");

module_init(pc87360_init);
module_exit(pc87360_exit);
