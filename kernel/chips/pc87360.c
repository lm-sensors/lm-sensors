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
 *  Chip        #vin    #fan    #pwm    #temp   devid
 *  PC87360     -       2       2       -       0xE1
 *  PC87363     -       2       2       -       0xE8
 *  PC87364     -       3       3       -       0xE4
 *  PC87365     11      3       3       2       0xE5
 *  PC87366     11      3       3       3-4     0xE9
 *
 *  This driver assumes that no more than one chip is present, and the
 *  standard Super-I/O address is used (0x2E/0x2F).
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

static unsigned short normal_i2c[] = { SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { SENSORS_I2C_END };
static unsigned int normal_isa[] = { 0x0000, SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };
static struct i2c_force_data forces[] = {{NULL}};
static u8 devid;
static unsigned int extra_isa[] = { 0x0000, 0x0000, 0x0000 };
static u8 confreg[4];

enum chips { any_chip, pc87360, pc87363, pc87364, pc87365, pc87366 };
static struct i2c_address_data addr_data = {
	.normal_i2c		= normal_i2c,
	.normal_i2c_range	= normal_i2c_range,
	.normal_isa		= normal_isa,
	.normal_isa_range	= normal_isa_range,
	.probe			= normal_i2c,		/* cheat */
	.probe_range		= normal_i2c_range,	/* cheat */
	.ignore			= normal_i2c,		/* cheat */
	.ignore_range		= normal_i2c_range,	/* cheat */
	.forces			= forces,
};

static int init = 1;
MODULE_PARM(init, "i");
MODULE_PARM_DESC(init,
	"Chip initialization level:\n"
	" 0: None\n"
	"*1: Forcibly enable internal voltage and temperature channels, except in9\n"
	" 2: Forcibly enable all voltage and temperature channels, except in9\n"
	" 3: Forcibly enable all voltage and temperature channels, including in9");

/*
 * Super-I/O registers and operations
 */

#define DEV	0x07	/* Register: Logical device select */
#define DEVID	0x20	/* Register: Device ID */
#define ACT	0x30	/* Register: Device activation */
#define BASE	0x60    /* Register: Base address */

#define FSCM	0x09	/* Logical device: fans */
#define VLM	0x0d	/* Logical device: voltages */
#define TMS	0x0e	/* Logical device: temperatures */
static const u8 logdev[3] = { FSCM, VLM, TMS };

#define LD_FAN		0
#define LD_IN		1
#define LD_TEMP		2

static inline void superio_outb(int sioaddr, int reg, int val)
{
	outb(reg, sioaddr);
	outb(val, sioaddr+1);
}

static inline int superio_inb(int sioaddr, int reg)
{
	outb(reg, sioaddr);
	return inb(sioaddr+1);
}

static inline void superio_exit(int sioaddr)
{
	outb(0x02, sioaddr);
	outb(0x02, sioaddr+1);
}

/*
 * Logical devices
 */

#define PC87360_EXTENT		0x10
#define PC87365_REG_BANK	0x09
#define NO_BANK			0xff

/*
 * Fan registers and conversions
 */

/* nr has to be 0 or 1 (PC87360/87363) or 2 (PC87364/87365/87366) */
#define PC87360_REG_PRESCALE(nr)	(0x00 + 2 * (nr))
#define PC87360_REG_PWM(nr)		(0x01 + 2 * (nr))
#define PC87360_REG_FAN_MIN(nr)		(0x06 + 3 * (nr))
#define PC87360_REG_FAN(nr)		(0x07 + 3 * (nr))
#define PC87360_REG_FAN_STATUS(nr)	(0x08 + 3 * (nr))

#define FAN_FROM_REG(val,div)		((val)==0?0: \
					 480000/((val)*(div)))
#define FAN_TO_REG(val,div)		((val)<=100?0: \
					 480000/((val)*(div)))
#define FAN_DIV_FROM_REG(val)		(1 << ((val >> 5) & 0x03))
#define FAN_DIV_TO_REG(val)		((val)==8?0x60:(val)==4?0x40: \
					 (val)==1?0x00:0x20)
#define FAN_STATUS_FROM_REG(val)	((val) & 0x07)

#define FAN_CONFIG_MONITOR(val,nr)	(((val) >> (2 + nr * 3)) & 1)
#define FAN_CONFIG_CONTROL(val,nr)	(((val) >> (3 + nr * 3)) & 1)
#define FAN_CONFIG_INVERT(val,nr)	(((val) >> (4 + nr * 3)) & 1)

#define PWM_FROM_REG(val,inv)		((inv) ? 255 - (val) : (val))
static inline u8 PWM_TO_REG(int val, int inv)
{
	if (inv)
		val = 255 - val;
	if (val < 0)
		return 0;
	if (val > 255)
		return 255;
	return val;
}

/*
 * Voltage registers and conversions
 */

#define PC87365_REG_IN_CONVRATE		0x07
#define PC87365_REG_IN_CONFIG		0x08
#define PC87365_REG_IN			0x0B
#define PC87365_REG_IN_MIN		0x0D
#define PC87365_REG_IN_MAX		0x0C
#define PC87365_REG_IN_STATUS		0x0A
#define PC87365_REG_IN_ALARMS1		0x00
#define PC87365_REG_IN_ALARMS2		0x01
#define PC87365_REG_VID			0x06

#define IN_FROM_REG(val,ref)		(((val) * (ref) + 128) / 256)
#define IN_TO_REG(val,ref)		((val)<0 ? 0 : \
					 (val)*256>=(ref)*255 ? 255: \
					 ((val) * 256 + (ref) / 2) / (ref))

/*
 * Temperature registers and conversions
 */

#define PC87365_REG_TEMP_CONFIG		0x08
#define PC87365_REG_TEMP		0x0B
#define PC87365_REG_TEMP_MIN		0x0D
#define PC87365_REG_TEMP_MAX		0x0C
#define PC87365_REG_TEMP_CRIT		0x0E
#define PC87365_REG_TEMP_STATUS		0x0A
#define PC87365_REG_TEMP_ALARMS		0x00

#define TEMP_FROM_REG(val)		((val)&0x80 ? (val) - 0x100 : (val))
#define TEMP_TO_REG(val)		((val)<-55 ? 201 : (val)>127 ? 0x7F : \
					 (val)<0 ? (val) + 0x100 : (val))

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
	u16 fan_conf;		/* Configuration register values, combined */

	u16 in_vref;		/* 10mV/bit */
	u8 in[14];		/* Register value */
	u8 in_min[14];		/* Register value */
	u8 in_max[14];		/* Register value */
	u8 in_crit[3];		/* Register value */
	u8 in_status[14];	/* Register value */
	u16 in_alarms;		/* Register values, combined, masked */
	u8 vid_conf;		/* Configuration register value */
	u8 vrm;
	u8 vid;			/* Register value */

	u8 temp[3];		/* Register value */
	u8 temp_min[3];		/* Register value */
	u8 temp_max[3];		/* Register value */
	u8 temp_crit[3];	/* Register value */
	u8 temp_status[3];	/* Register value */
	u8 temp_alarms;		/* Register value, masked */
};


static int pc87360_attach_adapter(struct i2c_adapter *adapter);
static int pc87360_detect(struct i2c_adapter *adapter, int address,
			  unsigned short flags, int kind);
static int pc87360_detach_client(struct i2c_client *client);

static int pc87360_read_value(struct pc87360_data *data, u8 ldi, u8 bank,
			      u8 reg);
static void pc87360_write_value(struct pc87360_data *data, u8 ldi, u8 bank,
				u8 reg, u8 value);
static void pc87360_init_client(struct i2c_client *client, int use_thermistors);
static void pc87360_update_client(struct i2c_client *client);


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
void pc87365_vid(struct i2c_client *client, int operation, int ctl_name,
		 int *nrels_mag, long *results);
void pc87365_vrm(struct i2c_client *client, int operation, int ctl_name,
		 int *nrels_mag, long *results);

void pc87365_temp(struct i2c_client *client, int operation, int ctl_name,
		  int *nrels_mag, long *results);
void pc87365_temp_status(struct i2c_client *client, int operation, int ctl_name,
			 int *nrels_mag, long *results);

static struct i2c_driver pc87360_driver = {
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
#define PC87365_SYSCTL_TEMP4		2111 /* not for PC87365 */
#define PC87365_SYSCTL_TEMP5		2112 /* not for PC87365 */
#define PC87365_SYSCTL_TEMP6		2113 /* not for PC87365 */
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
#define PC87365_SYSCTL_TEMP4_STATUS	2311 /* not for PC87365 */
#define PC87365_SYSCTL_TEMP5_STATUS	2312 /* not for PC87365 */
#define PC87365_SYSCTL_TEMP6_STATUS	2313 /* not for PC87365 */

#define PC87365_SYSCTL_VID		2400
#define PC87365_SYSCTL_VRM		2401

#define PC87365_STATUS_IN_MIN		0x02
#define PC87365_STATUS_IN_MAX		0x04

#define PC87365_SYSCTL_TEMP1		3101 /* degrees Celcius */
#define PC87365_SYSCTL_TEMP2		3102
#define PC87365_SYSCTL_TEMP3		3103 /* not for PC87365 */
#define PC87365_SYSCTL_TEMP1_STATUS	3301 /* bit field */
#define PC87365_SYSCTL_TEMP2_STATUS	3302
#define PC87365_SYSCTL_TEMP3_STATUS	3303 /* not for PC87365 */

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
	{PC87365_SYSCTL_TEMP4, "temp4", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87365_in},
	{PC87365_SYSCTL_TEMP5, "temp5", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87365_in},
	{PC87365_SYSCTL_TEMP6, "temp6", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87365_in},
	{PC87365_SYSCTL_TEMP1_STATUS, "temp1_status", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87365_temp_status},
	{PC87365_SYSCTL_TEMP2_STATUS, "temp2_status", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87365_temp_status},
	{PC87365_SYSCTL_TEMP3_STATUS, "temp3_status", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87365_temp_status},
	{PC87365_SYSCTL_TEMP4_STATUS, "temp4_status", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87365_in_status},
	{PC87365_SYSCTL_TEMP5_STATUS, "temp5_status", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87365_in_status},
	{PC87365_SYSCTL_TEMP6_STATUS, "temp6_status", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87365_in_status},
	{PC87365_SYSCTL_VID, "vid", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87365_vid},
	{PC87365_SYSCTL_VRM, "vrm", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87365_vrm},
	{0}
};

static int pc87360_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_detect(adapter, &addr_data, pc87360_detect);
}

static int pc87360_find(int sioaddr, u8 *devid, int *address)
{
	u16 val;
	int i;
	int nrdev; /* logical device count */

	/* No superio_enter */

	/* Identify device */
	val = superio_inb(sioaddr, DEVID);
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
		superio_exit(sioaddr);
		return -ENODEV;
	}
	/* Remember the device id */
	*devid = val;

	for (i = 0; i < nrdev; i++) {
		/* select logical device */
		superio_outb(sioaddr, DEV, logdev[i]);

		val = superio_inb(sioaddr, ACT);
		if (!(val & 0x01)) {
			printk(KERN_INFO "pc87360.o: Device 0x%02x not "
			       "activated\n", logdev[i]);
			continue;
		}

		val = (superio_inb(sioaddr, BASE) << 8)
		    | superio_inb(sioaddr, BASE + 1);
		if (!val) {
			printk(KERN_INFO "pc87360.o: Base address not set for "
			       "device 0x%02x\n", logdev[i]);
			continue;
		}

		address[i] = val;

		if (i==0) { /* Fans */
			confreg[0] = superio_inb(sioaddr, 0xF0);
			confreg[1] = superio_inb(sioaddr, 0xF1);
			
#ifdef DEBUG
			printk(KERN_DEBUG "pc87360.o: Fan 1: mon=%d "
			       "ctrl=%d inv=%d\n", (confreg[0]>>2)&1,
			       (confreg[0]>>3)&1, (confreg[0]>>4)&1);
			printk(KERN_DEBUG "pc87360.o: Fan 2: mon=%d "
			       "ctrl=%d inv=%d\n", (confreg[0]>>5)&1,
			       (confreg[0]>>6)&1, (confreg[0]>>7)&1);
			printk(KERN_DEBUG "pc87360.o: Fan 3: mon=%d "
			       "ctrl=%d inv=%d\n", confreg[1]&1,
			       (confreg[1]>>1)&1, (confreg[1]>>2)&1);
#endif
		} else if (i==1) { /* Voltages */
			/* Are we using thermistors? */
			if (*devid == 0xE9) { /* PC87366 */
				/* These registers are not logical-device
				   specific, just that we won't need them if
				   we don't use the VLM device */
				confreg[2] = superio_inb(sioaddr, 0x2B);
				confreg[3] = superio_inb(sioaddr, 0x25);

				if (confreg[2] & 0x40) {
					printk(KERN_INFO "pc87360.o: Using "
					       "thermistors for temperature "
					       "monitoring\n");
				}
				if (confreg[3] & 0xE0) {
					printk(KERN_INFO "pc87360.o: VID "
					       "inputs routed (mode %u)\n",
					       	confreg[3] >> 5);
				}
			}
		}
	}

	superio_exit(sioaddr);
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
	ctl_table *template = pc87360_dir_table_template;
	int use_thermistors = 0;

	if (!i2c_is_isa_adapter(adapter)) {
		return 0;
	}

	for (i = 0; i < 3; i++) {
		if (extra_isa[i]
		 && check_region(extra_isa[i], PC87360_EXTENT)) {
			printk(KERN_ERR "pc87360.o: Region 0x%x-0x%x already "
			       "in use!\n", extra_isa[i],
			       extra_isa[i]+PC87360_EXTENT-1);
			return -ENODEV;
		}
	}

	if (!(data = kmalloc(sizeof(struct pc87360_data), GFP_KERNEL))) {
		return -ENOMEM;
	}
	memset(data, 0x00, sizeof(struct pc87360_data));

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
		data->innr = extra_isa[1] ? 14 : 0;
		data->tempnr = extra_isa[2] ? 3 : 0;
		break;
	}

	/* Retrieve the fans configuration from Super-I/O space */
	if (data->fannr)
		data->fan_conf = confreg[0] | (confreg[1] << 8);

	for (i = 0; i < 3; i++) {
		if ((data->address[i] = extra_isa[i])) {
			request_region(extra_isa[i], PC87360_EXTENT, "pc87360");
		}
	}
	strcpy(new_client->name, client_name);
	data->valid = 0;
	init_MUTEX(&data->update_lock);

	if ((err = i2c_attach_client(new_client)))
		goto ERROR1;

	/* Use the correct reference voltage
	   Unless both the VLM and the TMS logical devices agree to
	   use an external Vref, the internal one is used. */
	if (data->innr) {
		i = pc87360_read_value(data, LD_IN, NO_BANK,
				       PC87365_REG_IN_CONFIG);
		if (data->tempnr) {
		 	i &= pc87360_read_value(data, LD_TEMP, NO_BANK,
						PC87365_REG_TEMP_CONFIG);
		}
		data->in_vref = (i&0x02) ? 3025 : 2966;
#ifdef DEBUG
		printk(KERN_DEBUG "pc87360.o: Using %s reference voltage\n",
		       (i&0x02) ? "external" : "internal");
#endif

		data->vid_conf = confreg[3];
		data->vrm = 90;
	}

	/* Fan clock dividers may be needed before any data is read */
	for (i = 0; i < data->fannr; i++) {
		if (FAN_CONFIG_MONITOR(data->fan_conf, i))
			data->fan_status[i] = pc87360_read_value(data,
					      LD_FAN, NO_BANK,
					      PC87360_REG_FAN_STATUS(i));
	}

	if (init > 0) {
		if (devid == 0xe9 && data->address[1]) /* PC87366 */
			use_thermistors = confreg[2] & 0x40;

		pc87360_init_client(new_client, use_thermistors);
	}

	if ((i = i2c_register_entry((struct i2c_client *) new_client,
				    type_name, template, THIS_MODULE)) < 0) {
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

/* ldi is the logical device index
   bank is for voltages and temperatures only */
static int pc87360_read_value(struct pc87360_data *data, u8 ldi, u8 bank,
			      u8 reg)
{
	int res;

	down(&(data->lock));
	if (bank != NO_BANK) {
		outb_p(bank, data->address[ldi] + PC87365_REG_BANK);
	}
	res = inb_p(data->address[ldi] + reg);
	up(&(data->lock));
	return res;
}

static void pc87360_write_value(struct pc87360_data *data, u8 ldi, u8 bank,
				u8 reg, u8 value)
{
	down(&(data->lock));
	if (bank != NO_BANK) {
		outb_p(bank, data->address[ldi] + PC87365_REG_BANK);
	}
	outb_p(value, data->address[ldi] + reg);
	up(&(data->lock));
}

static void pc87360_init_client(struct i2c_client *client, int use_thermistors)
{
	struct pc87360_data *data = client->data;
	int i, nr;
	const u8 init_in[14] = { 2, 2, 2, 2, 2, 2, 2, 1, 1, 3, 1, 2, 2, 2 };
	const u8 init_temp[3] = { 2, 2, 1 };
	u8 reg;

	if (init >= 2 && data->innr) {
		reg = pc87360_read_value(data, LD_IN, NO_BANK,
					 PC87365_REG_IN_CONVRATE);
		printk(KERN_INFO "pc87360.o: VLM conversion set to"
		       "1s period, 160us delay\n");
		pc87360_write_value(data, LD_IN, NO_BANK,
				    PC87365_REG_IN_CONVRATE,
				    (reg & 0xC0) | 0x11);
	}

	nr = data->innr < 11 ? data->innr : 11;
	for (i=0; i<nr; i++) {
		if (init >= init_in[i]) {
			/* Forcibly enable voltage channel */
			reg = pc87360_read_value(data, LD_IN, i,
						 PC87365_REG_IN_STATUS);
			if (!(reg & 0x01)) {
#ifdef DEBUG
				printk(KERN_DEBUG "pc87360.o: Forcibly "
				       "enabling in%d\n", i);
#endif
				pc87360_write_value(data, LD_IN, i,
						    PC87365_REG_IN_STATUS,
						    (reg & 0x68) | 0x87);
			}
		}
	}

	/* We can't blindly trust the Super-I/O space configuration bit,
	   most BIOS won't set it properly */
	for (i=11; i<data->innr; i++) {
		reg = pc87360_read_value(data, LD_IN, i,
					 PC87365_REG_TEMP_STATUS);
		use_thermistors = use_thermistors || (reg & 0x01);
	}

	i = use_thermistors ? 2 : 0;
	for (; i<data->tempnr; i++) {
		if (init >= init_temp[i]) {
			/* Forcibly enable temperature channel */
			reg = pc87360_read_value(data, LD_TEMP, i,
						 PC87365_REG_TEMP_STATUS);
			if (!(reg & 0x01)) {
#ifdef DEBUG
				printk(KERN_DEBUG "pc87360.o: Forcibly "
				       "enabling temp%d\n", i+1);
#endif
				pc87360_write_value(data, LD_TEMP, i,
						    PC87365_REG_TEMP_STATUS,
						    0xCF);
			}
		}
	}

	if (use_thermistors) {
		for (i=11; i<data->innr; i++) {
			if (init >= init_in[i]) {
				/* The pin may already be used by thermal
				   diodes */
				reg = pc87360_read_value(data, LD_TEMP, (i-11)/2,
							 PC87365_REG_TEMP_STATUS);
				if (reg & 0x01) {
#ifdef DEBUG
					printk(KERN_DEBUG "pc87360.o: Skipping "
					       "temp%d, pin already in use by "
					       "temp%d\n", i-7, (i-11)/2);
#endif
					continue;
				}
			
				/* Forcibly enable thermistor channel */
				reg = pc87360_read_value(data, LD_IN, i,
							 PC87365_REG_IN_STATUS);
				if (!(reg & 0x01)) {
#ifdef DEBUG
					printk(KERN_DEBUG "pc87360.o: Forcibly "
					       "enabling temp%d\n", i-7);
#endif
					pc87360_write_value(data, LD_IN, i,
							    PC87365_REG_TEMP_STATUS,
							    (reg & 0x60) | 0x8F);
				}
			}
		}
	}

	if (data->innr) {
		reg = pc87360_read_value(data, LD_IN, NO_BANK,
					 PC87365_REG_IN_CONFIG);
		if (reg & 0x01) {
#ifdef DEBUG
			printk(KERN_DEBUG "pc87360.o: Forcibly "
			       "enabling monitoring (VLM)\n");
#endif
			pc87360_write_value(data, LD_IN, NO_BANK,
					    PC87365_REG_IN_CONFIG,
					    reg & 0xFE);
		}
	}

	if (data->tempnr) {
		reg = pc87360_read_value(data, LD_TEMP, NO_BANK,
					 PC87365_REG_TEMP_CONFIG);
		if (reg & 0x01) {
#ifdef DEBUG
			printk(KERN_DEBUG "pc87360.o: Forcibly "
			       "enabling monitoring (TMS)\n");
#endif
			pc87360_write_value(data, LD_TEMP, NO_BANK,
					    PC87365_REG_TEMP_CONFIG,
					    reg & 0xFE);
		}

		if (init >= 2) {
			/* Chip config as documented by National Semi. */
			pc87360_write_value(data, LD_TEMP, 0xF, 0xA, 0x08);
			/* We voluntarily omit the bank here, in case the
			   sequence itself matters. It shouldn't be a problem,
			   since nobody else is supposed to access the
			   device at that point. */
			pc87360_write_value(data, LD_TEMP, NO_BANK, 0xB, 0x04);
			pc87360_write_value(data, LD_TEMP, NO_BANK, 0xC, 0x35);
			pc87360_write_value(data, LD_TEMP, NO_BANK, 0xD, 0x05);
			pc87360_write_value(data, LD_TEMP, NO_BANK, 0xE, 0x05);
		}
	}
}

static void pc87360_autodiv(struct pc87360_data *data, int nr)
{
	u8 old_min = data->fan_min[nr];

	/* Increase clock divider if needed and possible */
	if ((data->fan_status[nr] & 0x04) /* overflow flag */
	 || (data->fan[nr] >= 224)) { /* next to overflow */
		if ((data->fan_status[nr] & 0x60) != 0x60) {
			data->fan_status[nr] += 0x20;
			data->fan_min[nr] >>= 1;
			data->fan[nr] >>= 1;
#ifdef DEBUG
			printk(KERN_DEBUG "pc87360.o: Increasing "
			       "clock divider to %d for fan %d\n",
			       FAN_DIV_FROM_REG(data->fan_status[nr]),
			       nr+1);
#endif
		}
	} else {
		/* Decrease clock divider if possible */
		while (!(data->fan_min[nr] & 0x80) /* fan min "nails" divider */
		 && data->fan[nr] < 85 /* bad accuracy */
		 && (data->fan_status[nr] & 0x60) != 0x00) {
			data->fan_status[nr] -= 0x20;
			data->fan_min[nr] <<= 1;
			data->fan[nr] <<= 1;
#ifdef DEBUG
			printk(KERN_DEBUG "pc87360.o: Decreasing "
			       "clock divider to %d for fan %d\n",
			       FAN_DIV_FROM_REG(data->fan_status[nr]),
			       nr+1);
#endif
		}
	}

	/* Write new fan min if it changed */
	if (old_min != data->fan_min[nr]) {
		pc87360_write_value(data, LD_FAN, NO_BANK,
				    PC87360_REG_FAN_MIN(nr),
				    data->fan_min[nr]);
	}
}

static void pc87360_update_client(struct i2c_client *client)
{
	struct pc87360_data *data = client->data;
	u8 i;

	down(&data->update_lock);

	if ((jiffies - data->last_updated > HZ * 2) ||
	    (jiffies < data->last_updated) || !data->valid) {
#ifdef DEBUG
		printk(KERN_DEBUG "pc87360.o: Data update\n");
#endif

		/* Fans */
		for (i = 0; i < data->fannr; i++) {
			if (FAN_CONFIG_MONITOR(data->fan_conf, i)) {
				data->fan_status[i] = pc87360_read_value(data,
						      LD_FAN, NO_BANK,
						      PC87360_REG_FAN_STATUS(i));
				data->fan[i] = pc87360_read_value(data, LD_FAN,
					       NO_BANK, PC87360_REG_FAN(i));
				data->fan_min[i] = pc87360_read_value(data,
						   LD_FAN, NO_BANK,
						   PC87360_REG_FAN_MIN(i));
				/* Change clock divider if needed */
				pc87360_autodiv(data, i);
				/* Clear bits and write new divider */
				pc87360_write_value(data, LD_FAN, NO_BANK,
						    PC87360_REG_FAN_STATUS(i),
						    data->fan_status[i]);
			}
			data->pwm[i] = pc87360_read_value(data, LD_FAN,
				       NO_BANK, PC87360_REG_PWM(i));
		}

		/* Voltages */
		for (i = 0; i < data->innr; i++) {
			data->in_status[i] = pc87360_read_value(data, LD_IN, i,
					     PC87365_REG_IN_STATUS);
			/* Clear bits */
			pc87360_write_value(data, LD_IN, i,
					    PC87365_REG_IN_STATUS,
					    data->in_status[i]);
			if ((data->in_status[i] & 0x81) == 0x81) {
				data->in[i] = pc87360_read_value(data, LD_IN,
					      i, PC87365_REG_IN);
			}
			if (data->in_status[i] & 0x01) {
				data->in_min[i] = pc87360_read_value(data,
						  LD_IN, i,
						  PC87365_REG_IN_MIN);
				data->in_max[i] = pc87360_read_value(data,
						  LD_IN, i,
						  PC87365_REG_IN_MAX);
				if (i >= 11)
					data->in_crit[i-11] =
						pc87360_read_value(data, LD_IN,
						i, PC87365_REG_TEMP_CRIT);
			}
		}
		if (data->innr) {
			data->in_alarms = pc87360_read_value(data, LD_IN,
					  NO_BANK, PC87365_REG_IN_ALARMS1)
					| ((pc87360_read_value(data, LD_IN,
					    NO_BANK, PC87365_REG_IN_ALARMS2)
					    & 0x07) << 8);
			data->vid = (data->vid_conf & 0xE0) ?
				    pc87360_read_value(data, LD_IN,
				    NO_BANK, PC87365_REG_VID) : 0x1F;
		}

		/* Temperatures */
		for (i = 0; i < data->tempnr; i++) {
			data->temp_status[i] = pc87360_read_value(data,
					       LD_TEMP, i,
					       PC87365_REG_TEMP_STATUS);
			/* Clear bits */
			pc87360_write_value(data, LD_TEMP, i,
					    PC87365_REG_TEMP_STATUS,
					    data->temp_status[i]);
			if ((data->temp_status[i] & 0x81) == 0x81) {
				data->temp[i] = pc87360_read_value(data,
						LD_TEMP, i,
						PC87365_REG_TEMP);
			}
			if (data->temp_status[i] & 0x01) {
				data->temp_min[i] = pc87360_read_value(data,
						    LD_TEMP, i,
						    PC87365_REG_TEMP_MIN);
				data->temp_max[i] = pc87360_read_value(data,
						    LD_TEMP, i,
						    PC87365_REG_TEMP_MAX);
				data->temp_crit[i] = pc87360_read_value(data,
						     LD_TEMP, i,
						     PC87365_REG_TEMP_CRIT);
			}
		}
		if (data->tempnr) {
			data->temp_alarms = pc87360_read_value(data, LD_TEMP,
					    NO_BANK, PC87365_REG_TEMP_ALARMS)
					  & 0x3F;
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
			int fan_min = FAN_TO_REG(results[0],
				      FAN_DIV_FROM_REG(data->fan_status[nr]));
			/* If it wouldn't fit, change clock divisor */
			while (fan_min > 255
			    && (data->fan_status[nr] & 0x60) != 0x60) {
				fan_min >>= 1;
				data->fan[nr] >>= 1;
				data->fan_status[nr] += 0x20;
			}
			data->fan_min[nr] = fan_min > 255 ? 255 : fan_min;
			pc87360_write_value(data, LD_FAN, NO_BANK,
					    PC87360_REG_FAN_MIN(nr),
					    data->fan_min[nr]);
			/* Write new divider, preserve alarm bits */
			pc87360_write_value(data, LD_FAN, NO_BANK,
					    PC87360_REG_FAN_STATUS(nr),
					    data->fan_status[nr] & 0xF9);
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
		for (; i < 3; i++) {
			results[i] = 0;
		}
		*nrels_mag = 3;
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
		results[0] = PWM_FROM_REG(data->pwm[nr],
			     FAN_CONFIG_INVERT(data->fan_conf, nr));
		results[1] = FAN_CONFIG_CONTROL(data->fan_conf, nr);
		*nrels_mag = 2;
	}
	else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (nr >= data->fannr)
			return;
		if (*nrels_mag >= 1) {
			data->pwm[nr] = PWM_TO_REG(results[0],
					FAN_CONFIG_INVERT(data->fan_conf, nr));
			pc87360_write_value(data, LD_FAN, NO_BANK,
					    PC87360_REG_PWM(nr),
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
		*nrels_mag = 3;
	else if (operation == SENSORS_PROC_REAL_READ) {
		pc87360_update_client(client);
		results[0] = IN_FROM_REG(data->in_min[nr], data->in_vref);
		results[1] = IN_FROM_REG(data->in_max[nr], data->in_vref);
		if (nr < 11) {
			*nrels_mag = 3;
		} else {
			results[2] = IN_FROM_REG(data->in_crit[nr-11],
						 data->in_vref);
			*nrels_mag = 4;
		}
		results[(*nrels_mag)-1] = IN_FROM_REG(data->in[nr],
						      data->in_vref);
	}
	else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->in_min[nr] = IN_TO_REG(results[0],
						     data->in_vref);
			pc87360_write_value(data, LD_IN, nr,
					    PC87365_REG_IN_MIN,
					    data->in_min[nr]);
		}
		if (*nrels_mag >= 2) {
			data->in_max[nr] = IN_TO_REG(results[1],
						     data->in_vref);
			pc87360_write_value(data, LD_IN, nr,
					    PC87365_REG_IN_MAX,
					    data->in_max[nr]);
		}
		if (*nrels_mag >= 3 && nr >= 11) {
			data->in_crit[nr-11] = IN_TO_REG(results[2],
							 data->in_vref);
			pc87360_write_value(data, LD_IN, nr,
					    PC87365_REG_TEMP_CRIT,
					    data->in_crit[nr-11]);
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
		results[0] = data->in_status[nr];
		*nrels_mag = 1;
	}
}

void pc87365_vid(struct i2c_client *client, int operation, int ctl_name,
		 int *nrels_mag, long *results)
{
	struct pc87360_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 3;
	else if (operation == SENSORS_PROC_REAL_READ) {
		pc87360_update_client(client);
		results[0] = vid_from_reg(data->vid & 0x1f, data->vrm);
		*nrels_mag = 1;
	}
}

void pc87365_vrm(struct i2c_client *client, int operation, int ctl_name,
		 int *nrels_mag, long *results)
{
	struct pc87360_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 1;
	else if (operation == SENSORS_PROC_REAL_READ) {
		results[0] = data->vrm;
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1)
			data->vrm = results[0];
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
		results[2] = TEMP_FROM_REG(data->temp_crit[nr]);
		results[3] = TEMP_FROM_REG(data->temp[nr]);
		*nrels_mag = 4;
	}
	else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (nr >= data->tempnr)
			return;
		if (*nrels_mag >= 1) {
			data->temp_max[nr] = TEMP_TO_REG(results[0]);
			pc87360_write_value(data, LD_TEMP, nr,
					    PC87365_REG_TEMP_MAX,
					    data->temp_max[nr]);
		}
		if (*nrels_mag >= 2) {
			data->temp_min[nr] = TEMP_TO_REG(results[1]);
			pc87360_write_value(data, LD_TEMP, nr,
					    PC87365_REG_TEMP_MIN,
					    data->temp_min[nr]);
		}
		if (*nrels_mag >= 3) {
			data->temp_crit[nr] = TEMP_TO_REG(results[2]);
			pc87360_write_value(data, LD_TEMP, nr,
					    PC87365_REG_TEMP_CRIT,
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
		results[0] = data->temp_status[nr];
		*nrels_mag = 1;
	}
}


static int __init pc87360_init(void)
{
	int i;

	printk(KERN_INFO "pc87360.o version %s (%s)\n", LM_VERSION, LM_DATE);

	if (pc87360_find(0x2e, &devid, extra_isa)
	 && pc87360_find(0x4e, &devid, extra_isa)) {
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
