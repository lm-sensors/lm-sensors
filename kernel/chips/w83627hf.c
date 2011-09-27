/*
    w83627hf.c - Part of lm_sensors, Linux kernel modules for hardware
                monitoring
    Copyright (c) 1998 - 2003  Frodo Looijaard <frodol@dds.nl>,
    Philip Edelbrock <phil@netroedge.com>,
    and Mark Studebaker <mdsxyz123@yahoo.com>

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

/*
    Supports following chips:

    Chip	#vin	#fanin	#pwm	#temp	wchipid	vendid	i2c	ISA
    w83627hf	9	3	2	3	0x20	0x5ca3	no	yes(LPC)
    w83627thf	7	3	3	3	0x90	0x5ca3	no	yes(LPC)
    w83637hf	7	3	3	3	0x80	0x5ca3	no	yes(LPC)
    w83687thf	7	3	3	3	0x90	0x5ca3	no	yes(LPC)
    w83697hf	8	2	2	2	0x60	0x5ca3	no	yes(LPC)

    For other winbond chips, and for i2c support in the above chips,
    use w83781d.c.

    Note: automatic ("cruise") fan control for 697, 637 & 627thf not
    supported yet.
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

static int force_addr;
MODULE_PARM(force_addr, "i");
MODULE_PARM_DESC(force_addr,
		 "Initialize the base address of the sensors");
static int force_i2c = 0x1f;
MODULE_PARM(force_i2c, "i");
MODULE_PARM_DESC(force_i2c,
		 "Initialize the i2c address of the sensors");

/* Addresses to scan */
static unsigned short normal_i2c[] = { SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { SENSORS_I2C_END };
static unsigned int normal_isa[] = { 0, SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/* Insmod parameters */
SENSORS_INSMOD_5(w83627hf, w83627thf, w83697hf, w83637hf, w83687thf);

static int init = 1;
MODULE_PARM(init, "i");
MODULE_PARM_DESC(init, "Set to zero to bypass chip initialization");

/* modified from kernel/include/traps.c */
static int REG;		/* The register to read/write */
#define	DEV	0x07	/* Register: Logical device select */
static int VAL;		/* The value to read/write */

/* logical device numbers for superio_select (below) */
#define W83627HF_LD_FDC		0x00
#define W83627HF_LD_PRT		0x01
#define W83627HF_LD_UART1	0x02
#define W83627HF_LD_UART2	0x03
#define W83627HF_LD_KBC		0x05
#define W83627HF_LD_CIR		0x06 /* w83627hf only */
#define W83627HF_LD_GAME	0x07
#define W83627HF_LD_MIDI	0x07
#define W83627HF_LD_GPIO1	0x07
#define W83627HF_LD_GPIO5	0x07 /* w83627thf only */
#define W83627HF_LD_GPIO2	0x08
#define W83627HF_LD_GPIO3	0x09
#define W83627HF_LD_GPIO4	0x09 /* w83627thf only */
#define W83627HF_LD_ACPI	0x0a
#define W83627HF_LD_HWM		0x0b

#define	DEVID	0x20	/* Register: Device ID */

#define W83627THF_GPIO5_EN	0x30 /* w83627thf only */
#define W83627THF_GPIO5_IOSR	0xf3 /* w83627thf only */
#define W83627THF_GPIO5_DR	0xf4 /* w83627thf only */

#define W83687THF_VID_EN	0x29 /* w83687thf only */
#define W83687THF_VID_CFG	0xF0 /* w83687thf only */
#define W83687THF_VID_DATA	0xF1 /* w83687thf only */

static inline void
superio_outb(int reg, int val)
{
	outb(reg, REG);
	outb(val, VAL);
}

static inline int
superio_inb(int reg)
{
	outb(reg, REG);
	return inb(VAL);
}

static inline void
superio_select(int ld)
{
	outb(DEV, REG);
	outb(ld, VAL);
}

static inline void
superio_enter(void)
{
	outb(0x87, REG);
	outb(0x87, REG);
}

static inline void
superio_exit(void)
{
	outb(0xAA, REG);
}

#define W627_DEVID 0x52
#define W627THF_DEVID 0x82
#define W697_DEVID 0x60
#define W637_DEVID 0x70
#define W687THF_DEVID 0x85
#define WINB_ACT_REG 0x30
#define WINB_BASE_REG 0x60
/* Constants specified below */

/* Length of ISA address segment */
#define WINB_EXTENT 8

/* Where are the ISA address/data registers relative to the base address */
#define W83781D_ADDR_REG_OFFSET 5
#define W83781D_DATA_REG_OFFSET 6

/* The W83781D registers */
/* The W83782D registers for nr=7,8 are in bank 5 */
#define W83781D_REG_IN_MAX(nr) ((nr < 7) ? (0x2b + (nr) * 2) : \
					   (0x554 + (((nr) - 7) * 2)))
#define W83781D_REG_IN_MIN(nr) ((nr < 7) ? (0x2c + (nr) * 2) : \
					   (0x555 + (((nr) - 7) * 2)))
#define W83781D_REG_IN(nr)     ((nr < 7) ? (0x20 + (nr)) : \
					   (0x550 + (nr) - 7))

#define W83781D_REG_FAN_MIN(nr) (0x3a + (nr))
#define W83781D_REG_FAN(nr) (0x27 + (nr))

#define W83781D_REG_TEMP2 0x0150
#define W83781D_REG_TEMP3 0x0250
#define W83781D_REG_TEMP2_HYST 0x153
#define W83781D_REG_TEMP3_HYST 0x253
#define W83781D_REG_TEMP2_CONFIG 0x152
#define W83781D_REG_TEMP3_CONFIG 0x252
#define W83781D_REG_TEMP2_OVER 0x155
#define W83781D_REG_TEMP3_OVER 0x255

#define W83781D_REG_TEMP 0x27
#define W83781D_REG_TEMP_OVER 0x39
#define W83781D_REG_TEMP_HYST 0x3A
#define W83781D_REG_BANK 0x4E

#define W83781D_REG_CONFIG 0x40
#define W83781D_REG_ALARM1 0x459
#define W83781D_REG_ALARM2 0x45A
#define W83781D_REG_ALARM3 0x45B

#define W83781D_REG_BEEP_CONFIG 0x4D
#define W83781D_REG_BEEP_INTS1 0x56
#define W83781D_REG_BEEP_INTS2 0x57
#define W83781D_REG_BEEP_INTS3 0x453

#define W83781D_REG_VID_FANDIV 0x47

#define W83781D_REG_CHIPID 0x49
#define W83781D_REG_WCHIPID 0x58
#define W83781D_REG_CHIPMAN 0x4F
#define W83781D_REG_PIN 0x4B

#define W83781D_REG_VBAT 0x5D

#define W83627HF_REG_PWM1 0x5A
#define W83627HF_REG_PWM2 0x5B

#define W83627THF_REG_PWM1		0x01	/* 697HF/637HF/687THF too */
#define W83627THF_REG_PWM2		0x03	/* 697HF/637HF/687THF too */
#define W83627THF_REG_PWM3		0x11	/* 637HF/687THF too */

#define W83627THF_REG_VRM_OVT_CFG 	0x18	/* 637HF/687THF too */

static const u8 regpwm_627hf[] = { W83627HF_REG_PWM1, W83627HF_REG_PWM2 };
static const u8 regpwm[] = { W83627THF_REG_PWM1, W83627THF_REG_PWM2,
                             W83627THF_REG_PWM3 };
#define W836X7HF_REG_PWM(type, nr) (((type) == w83627hf) ? \
                                     regpwm_627hf[(nr) - 1] : regpwm[(nr) - 1])

#define W83781D_REG_I2C_ADDR 0x48
#define W83781D_REG_I2C_SUBADDR 0x4A

/* Sensor selection */
#define W83781D_REG_SCFG1 0x5D
static const u8 BIT_SCFG1[] = { 0x02, 0x04, 0x08 };
#define W83781D_REG_SCFG2 0x59
static const u8 BIT_SCFG2[] = { 0x10, 0x20, 0x40 };
#define W83781D_DEFAULT_BETA 3435

/* Conversions. Limit checking is only done on the TO_REG
   variants. Note that you should be a bit careful with which arguments
   these macros are called: arguments may be evaluated more than once.
   Fixing this is just not worth it. */
#define IN_TO_REG(val)  (SENSORS_LIMIT((((val) * 10 + 8)/16),0,255))
#define IN_FROM_REG(val) (((val) * 16 + 5) / 10)

#define IN_TO_REG_VRM9(val) \
	(SENSORS_LIMIT((((val) * 1000 - 70000 + 244) / 488), 0, 255))
#define IN_FROM_REG_VRM9(reg)	(((reg) * 488 + 70000 + 500) / 1000)

static inline u8 FAN_TO_REG(long rpm, int div)
{
	if (rpm == 0)
		return 255;
	rpm = SENSORS_LIMIT(rpm, 1, 1000000);
	return SENSORS_LIMIT((1350000 + rpm * div / 2) / (rpm * div), 1,
			     254);
}

#define TEMP_MIN (-1280)
#define TEMP_MAX ( 1270)

/* TEMP: 1/10 degrees C (-128C to +127C)
   REG: 1C/bit, two's complement */
static u8 TEMP_TO_REG(int temp)
{
	int ntemp = SENSORS_LIMIT(temp, TEMP_MIN, TEMP_MAX);
	ntemp += (ntemp<0 ? -5 : 5);
	return (u8)(ntemp / 10);
}

static int TEMP_FROM_REG(u8 reg)
{
	return (s8)reg * 10;
}

#define FAN_FROM_REG(val,div) ((val)==0?-1:(val)==255?0:1350000/((val)*(div)))

#define PWM_TO_REG(val) (SENSORS_LIMIT((val),0,255))
#define BEEPS_TO_REG(val) ((val) & 0xffffff)

#define BEEP_ENABLE_TO_REG(val)   ((val)?1:0)
#define BEEP_ENABLE_FROM_REG(val) ((val)?1:0)

#define DIV_FROM_REG(val) (1 << (val))

static inline u8 DIV_TO_REG(long val)
{
	int i;
	val = SENSORS_LIMIT(val, 1, 128) >> 1;
	for (i = 0; i < 7; i++) {
		if (val == 0)
			break;
		val >>= 1;
	}
	return ((u8) i);
}

/* For each registered chip, we need to keep some data in memory. It is
   dynamically allocated, at the same time when a new client is allocated. */
struct w83627hf_data {
	struct i2c_client client;
	struct semaphore lock;
	int sysctl_id;
	enum chips type;

	struct semaphore update_lock;
	char valid;		/* !=0 if following fields are valid */
	unsigned long last_updated;	/* In jiffies */

	u8 in[9];		/* Register value */
	u8 in_max[9];		/* Register value */
	u8 in_min[9];		/* Register value */
	u8 fan[3];		/* Register value */
	u8 fan_min[3];		/* Register value */
	u8 temp;
	u8 temp_over;		/* Register value */
	u8 temp_hyst;		/* Register value */
	u16 temp_add[2];	/* Register value */
	u16 temp_add_over[2];	/* Register value */
	u16 temp_add_hyst[2];	/* Register value */
	u8 fan_div[3];		/* Register encoding, shifted right */
	u8 vid;			/* Register encoding, combined */
	u32 alarms;		/* Register encoding, combined */
	u32 beeps;		/* Register encoding, combined */
	u8 beep_enable;		/* Boolean */
	u8 pwm[3];		/* Register value */
	u16 sens[3];		/* 782D/783S only.
				   1 = pentium diode; 2 = 3904 diode;
				   3000-5000 = thermistor beta.
				   Default = 3435.
				   Other Betas unimplemented */
	u8 vrm;
	u8 vrm_ovt;		/* Register value, 627THF/637HF/687THF only */
};


static int w83627hf_attach_adapter(struct i2c_adapter *adapter);
static int w83627hf_detect(struct i2c_adapter *adapter, int address,
			  unsigned short flags, int kind);
static int w83627hf_detach_client(struct i2c_client *client);

static int w83627hf_read_value(struct i2c_client *client, u16 reg);
static int w83627hf_write_value(struct i2c_client *client, u16 reg,
			       u16 value);
static void w83627hf_update_client(struct i2c_client *client);
static void w83627hf_init_client(struct i2c_client *client);


static void w83627hf_in(struct i2c_client *client, int operation,
		       int ctl_name, int *nrels_mag, long *results);
static void w83627hf_fan(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void w83627hf_temp(struct i2c_client *client, int operation,
			 int ctl_name, int *nrels_mag, long *results);
static void w83627hf_temp_add(struct i2c_client *client, int operation,
			     int ctl_name, int *nrels_mag, long *results);
static void w83627hf_vid(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void w83627hf_vrm(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void w83627hf_alarms(struct i2c_client *client, int operation,
			   int ctl_name, int *nrels_mag, long *results);
static void w83627hf_beep(struct i2c_client *client, int operation,
			 int ctl_name, int *nrels_mag, long *results);
static void w83627hf_fan_div(struct i2c_client *client, int operation,
			    int ctl_name, int *nrels_mag, long *results);
static void w83627hf_pwm(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void w83627hf_sens(struct i2c_client *client, int operation,
			 int ctl_name, int *nrels_mag, long *results);

static struct i2c_driver w83627hf_driver = {
	.name		= "W83627HF sensor driver",
	.id		= I2C_DRIVERID_W83627HF,
	.flags		= I2C_DF_NOTIFY,
	.attach_adapter	= w83627hf_attach_adapter,
	.detach_client	= w83627hf_detach_client,
};

/* The /proc/sys entries */
/* WARNING these are copied from w83781d.c and have not been renamed.
   Note that the 627hf is supported by both drivers.
   Do not make incompatible changes here or we will have errors
   in the generated file ../include/sensors.h !!!
*/
/* -- SENSORS SYSCTL START -- */

#define W83781D_SYSCTL_IN0 1000	/* Volts * 100 */
#define W83781D_SYSCTL_IN1 1001
#define W83781D_SYSCTL_IN2 1002
#define W83781D_SYSCTL_IN3 1003
#define W83781D_SYSCTL_IN4 1004
#define W83781D_SYSCTL_IN5 1005
#define W83781D_SYSCTL_IN6 1006
#define W83781D_SYSCTL_IN7 1007
#define W83781D_SYSCTL_IN8 1008
#define W83781D_SYSCTL_FAN1 1101	/* Rotations/min */
#define W83781D_SYSCTL_FAN2 1102
#define W83781D_SYSCTL_FAN3 1103
#define W83781D_SYSCTL_TEMP1 1200	/* Degrees Celsius * 10 */
#define W83781D_SYSCTL_TEMP2 1201	/* Degrees Celsius * 10 */
#define W83781D_SYSCTL_TEMP3 1202	/* Degrees Celsius * 10 */
#define W83781D_SYSCTL_VID 1300		/* Volts * 1000 */
#define W83781D_SYSCTL_VRM 1301
#define W83781D_SYSCTL_PWM1 1401
#define W83781D_SYSCTL_PWM2 1402
#define W83781D_SYSCTL_PWM3 1403
#define W83781D_SYSCTL_SENS1 1501	/* 1, 2, or Beta (3000-5000) */
#define W83781D_SYSCTL_SENS2 1502
#define W83781D_SYSCTL_SENS3 1503
#define W83781D_SYSCTL_FAN_DIV 2000	/* 1, 2, 4 or 8 */
#define W83781D_SYSCTL_ALARMS 2001	/* bitvector */
#define W83781D_SYSCTL_BEEP 2002	/* bitvector */

#define W83781D_ALARM_IN0 0x0001
#define W83781D_ALARM_IN1 0x0002
#define W83781D_ALARM_IN2 0x0004
#define W83781D_ALARM_IN3 0x0008
#define W83781D_ALARM_IN4 0x0100
#define W83781D_ALARM_IN5 0x0200
#define W83781D_ALARM_IN6 0x0400
#define W83782D_ALARM_IN7 0x10000
#define W83782D_ALARM_IN8 0x20000
#define W83781D_ALARM_FAN1 0x0040
#define W83781D_ALARM_FAN2 0x0080
#define W83781D_ALARM_FAN3 0x0800
#define W83781D_ALARM_TEMP1 0x0010
#define W83781D_ALARM_TEMP23 0x0020	/* 781D only */
#define W83781D_ALARM_TEMP2 0x0020	/* 782D/783S */
#define W83781D_ALARM_TEMP3 0x2000	/* 782D only */
#define W83781D_ALARM_CHAS 0x1000

/* -- SENSORS SYSCTL END -- */

/* These files are created for each detected chip. This is just a template;
   though at first sight, you might think we could use a statically
   allocated list, we need some way to get back to the parent - which
   is done through one of the 'extra' fields which are initialized
   when a new copy is allocated. */

/* without pwm3-4 */
static ctl_table w83627hf_dir_table_template[] = {
	{W83781D_SYSCTL_IN0, "in0", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_in},
	{W83781D_SYSCTL_IN1, "in1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_in},
	{W83781D_SYSCTL_IN2, "in2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_in},
	{W83781D_SYSCTL_IN3, "in3", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_in},
	{W83781D_SYSCTL_IN4, "in4", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_in},
	{W83781D_SYSCTL_IN5, "in5", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_in},
	{W83781D_SYSCTL_IN6, "in6", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_in},
	{W83781D_SYSCTL_IN7, "in7", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_in},
	{W83781D_SYSCTL_IN8, "in8", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_in},
	{W83781D_SYSCTL_FAN1, "fan1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_fan},
	{W83781D_SYSCTL_FAN2, "fan2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_fan},
	{W83781D_SYSCTL_FAN3, "fan3", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_fan},
	{W83781D_SYSCTL_TEMP1, "temp1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_temp},
	{W83781D_SYSCTL_TEMP2, "temp2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_temp_add},
	{W83781D_SYSCTL_TEMP3, "temp3", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_temp_add},
	{W83781D_SYSCTL_VID, "vid", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_vid},
	{W83781D_SYSCTL_VRM, "vrm", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_vrm},
	{W83781D_SYSCTL_FAN_DIV, "fan_div", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_fan_div},
	{W83781D_SYSCTL_ALARMS, "alarms", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_alarms},
	{W83781D_SYSCTL_BEEP, "beep", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_beep},
	{W83781D_SYSCTL_PWM1, "pwm1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_pwm},
	{W83781D_SYSCTL_PWM2, "pwm2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_pwm},
	{W83781D_SYSCTL_SENS1, "sensor1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_sens},
	{W83781D_SYSCTL_SENS2, "sensor2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_sens},
	{W83781D_SYSCTL_SENS3, "sensor3", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_sens},
	{0}
};

/* similar to w83782d but no fan3, no vid */
static ctl_table w83697hf_dir_table_template[] = {
	{W83781D_SYSCTL_IN0, "in0", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_in},
	/* no in1 to maintain compatibility with 781d and 782d. */
	{W83781D_SYSCTL_IN2, "in2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_in},
	{W83781D_SYSCTL_IN3, "in3", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_in},
	{W83781D_SYSCTL_IN4, "in4", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_in},
	{W83781D_SYSCTL_IN5, "in5", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_in},
	{W83781D_SYSCTL_IN6, "in6", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_in},
	{W83781D_SYSCTL_IN7, "in7", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_in},
	{W83781D_SYSCTL_IN8, "in8", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_in},
	{W83781D_SYSCTL_FAN1, "fan1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_fan},
	{W83781D_SYSCTL_FAN2, "fan2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_fan},
	{W83781D_SYSCTL_TEMP1, "temp1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_temp},
	{W83781D_SYSCTL_TEMP2, "temp2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_temp_add},
	{W83781D_SYSCTL_FAN_DIV, "fan_div", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_fan_div},
	{W83781D_SYSCTL_ALARMS, "alarms", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_alarms},
	{W83781D_SYSCTL_BEEP, "beep", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_beep},
	{W83781D_SYSCTL_PWM1, "pwm1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_pwm},
	{W83781D_SYSCTL_PWM2, "pwm2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_pwm},
	{W83781D_SYSCTL_SENS1, "sensor1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_sens},
	{W83781D_SYSCTL_SENS2, "sensor2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_sens},
	{0}
};

/* no in5 and in6 */
/* We use this one for W83637HF and W83687THF too */
static ctl_table w83627thf_dir_table_template[] = {
	{W83781D_SYSCTL_IN0, "in0", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_in},
	{W83781D_SYSCTL_IN1, "in1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_in},
	{W83781D_SYSCTL_IN2, "in2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_in},
	{W83781D_SYSCTL_IN3, "in3", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_in},
	{W83781D_SYSCTL_IN4, "in4", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_in},
	{W83781D_SYSCTL_IN7, "in7", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_in},
	{W83781D_SYSCTL_IN8, "in8", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_in},
	{W83781D_SYSCTL_FAN1, "fan1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_fan},
	{W83781D_SYSCTL_FAN2, "fan2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_fan},
	{W83781D_SYSCTL_FAN3, "fan3", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_fan},
	{W83781D_SYSCTL_TEMP1, "temp1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_temp},
	{W83781D_SYSCTL_TEMP2, "temp2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_temp_add},
	{W83781D_SYSCTL_TEMP3, "temp3", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_temp_add},
	{W83781D_SYSCTL_VID, "vid", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_vid},
	{W83781D_SYSCTL_VRM, "vrm", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_vrm},
	{W83781D_SYSCTL_FAN_DIV, "fan_div", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_fan_div},
	{W83781D_SYSCTL_ALARMS, "alarms", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_alarms},
	{W83781D_SYSCTL_BEEP, "beep", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_beep},
	{W83781D_SYSCTL_PWM1, "pwm1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_pwm},
	{W83781D_SYSCTL_PWM2, "pwm2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_pwm},
	{W83781D_SYSCTL_PWM3, "pwm3", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_pwm},
	{W83781D_SYSCTL_SENS1, "sensor1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_sens},
	{W83781D_SYSCTL_SENS2, "sensor2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_sens},
	{W83781D_SYSCTL_SENS3, "sensor3", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_sens},
	{0}
};


/* This function is called when:
     * w83627hf_driver is inserted (when this module is loaded), for each
       available adapter
     * when a new adapter is inserted (and w83627hf_driver is still present) */
static int w83627hf_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_detect(adapter, &addr_data, w83627hf_detect);
}

static int __init w83627hf_find(int sioaddr, int *address)
{
	u16 val;

	REG = sioaddr;
	VAL = sioaddr + 1;

	superio_enter();
	val= superio_inb(DEVID);
	if (val != W627_DEVID && val != W627THF_DEVID && val != W697_DEVID
	 && val != W637_DEVID && val != W687THF_DEVID) {
		superio_exit();
		return -ENODEV;
	}

	superio_select(W83627HF_LD_HWM);
	val = (superio_inb(WINB_BASE_REG) << 8) |
	       superio_inb(WINB_BASE_REG + 1);
	*address = val & ~(WINB_EXTENT - 1);
	if (*address == 0 && force_addr == 0) {
		printk("w83627hf.o: base address not set - use force_addr=0xaddr\n");
		superio_exit();
		return -ENODEV;
	}
	if (force_addr)
		*address = force_addr;	/* so detect will get called */

	superio_exit();
	return 0;
}

int w83627hf_detect(struct i2c_adapter *adapter, int address,
		   unsigned short flags, int kind)
{
	int i, val;
	struct i2c_client *new_client;
	struct w83627hf_data *data;
	int err = 0;
	const char *type_name = "";
	const char *client_name = "";

	if (!i2c_is_isa_adapter(adapter))
		return 0;

	if(force_addr)
		address = force_addr & ~(WINB_EXTENT - 1);
	if (check_region(address, WINB_EXTENT)) {
		printk("w83627hf.o: region 0x%x already in use!\n", address);
		return -ENODEV;
	}
	if(force_addr) {
		printk("w83627hf.o: forcing ISA address 0x%04X\n", address);
		superio_enter();
		superio_select(W83627HF_LD_HWM);
		superio_outb(WINB_BASE_REG, address >> 8);
		superio_outb(WINB_BASE_REG+1, address & 0xff);
		superio_exit();
	}

	superio_enter();
	val= superio_inb(DEVID);
	if(val == W627_DEVID)
		kind = w83627hf;
	else if(val == W697_DEVID)
		kind = w83697hf;
	else if(val == W627THF_DEVID)
		kind = w83627thf;
	else if(val == W637_DEVID)
		kind = w83637hf;
	else if (val == W687THF_DEVID)
		kind = w83687thf;

	superio_select(W83627HF_LD_HWM);
	if((val = 0x01 & superio_inb(WINB_ACT_REG)) == 0)
		superio_outb(WINB_ACT_REG, 1);
	superio_exit();

	/* OK. For now, we presume we have a valid client. We now create the
	   client structure, even though we cannot fill it completely yet.
	   But it allows us to access w83627hf_{read,write}_value. */

	if (!(data = kmalloc(sizeof(struct w83627hf_data), GFP_KERNEL))) {
		err = -ENOMEM;
		goto ERROR0;
	}

	new_client = &data->client;
	new_client->addr = address;
	init_MUTEX(&data->lock);
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &w83627hf_driver;
	new_client->flags = 0;


	if (kind == w83627hf) {
		type_name = "w83627hf";
		client_name = "W83627HF chip";
	} else if (kind == w83627thf) {
		type_name = "w83627thf";
		client_name = "W83627THF chip";
	} else if (kind == w83697hf) {
		type_name = "w83697hf";
		client_name = "W83697HF chip";
	} else if (kind == w83637hf) {
		type_name = "w83637hf";
		client_name = "W83637HF chip";
	} else if (kind == w83687thf) {
		type_name = "w83687thf";
		client_name = "W83687THF chip";
	} else {
		goto ERROR1;
	}

	request_region(address, WINB_EXTENT, type_name);

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
				(kind == w83697hf) ?
				   w83697hf_dir_table_template :
				(kind == w83627hf) ?
				   w83627hf_dir_table_template :
				   /* w83627thf table also used for 637HF
				      and 687THF */
				   w83627thf_dir_table_template,
				THIS_MODULE)) < 0) {
		err = i;
		goto ERROR7;
	}
	data->sysctl_id = i;

	/* Initialize the chip */
	w83627hf_init_client(new_client);
	return 0;

/* OK, this is not exactly good programming practice, usually. But it is
   very code-efficient in this case. */

ERROR7:
	i2c_detach_client(new_client);
ERROR3:
	release_region(address, WINB_EXTENT);
ERROR1:
	kfree(data);
ERROR0:
	return err;
}

static int w83627hf_detach_client(struct i2c_client *client)
{
	int err;

	i2c_deregister_entry(((struct w83627hf_data *) (client->data))->
				 sysctl_id);

	if ((err = i2c_detach_client(client))) {
		printk
		    (KERN_ERR "w83627hf.o: Client deregistration failed, client not detached.\n");
		return err;
	}

	release_region(client->addr, WINB_EXTENT);
	kfree(client->data);

	return 0;
}


/*
   ISA access must always be locked explicitly!
   We ignore the W83781D BUSY flag at this moment - it could lead to deadlocks,
   would slow down the W83781D access and should not be necessary.
   There are some ugly typecasts here, but the good news is - they should
   nowhere else be necessary! */
static int w83627hf_read_value(struct i2c_client *client, u16 reg)
{
	int res, word_sized;

	down(&(((struct w83627hf_data *) (client->data))->lock));
	word_sized = (((reg & 0xff00) == 0x100)
		      || ((reg & 0xff00) == 0x200))
	    && (((reg & 0x00ff) == 0x50)
		|| ((reg & 0x00ff) == 0x53)
		|| ((reg & 0x00ff) == 0x55));
	if (reg & 0xff00) {
		outb_p(W83781D_REG_BANK,
		       client->addr + W83781D_ADDR_REG_OFFSET);
		outb_p(reg >> 8,
		       client->addr + W83781D_DATA_REG_OFFSET);
	}
	outb_p(reg & 0xff, client->addr + W83781D_ADDR_REG_OFFSET);
	res = inb_p(client->addr + W83781D_DATA_REG_OFFSET);
	if (word_sized) {
		outb_p((reg & 0xff) + 1,
		       client->addr + W83781D_ADDR_REG_OFFSET);
		res =
		    (res << 8) + inb_p(client->addr +
				       W83781D_DATA_REG_OFFSET);
	}
	if (reg & 0xff00) {
		outb_p(W83781D_REG_BANK,
		       client->addr + W83781D_ADDR_REG_OFFSET);
		outb_p(0, client->addr + W83781D_DATA_REG_OFFSET);
	}
	up(&(((struct w83627hf_data *) (client->data))->lock));
	return res;
}

static int w83627thf_read_gpio5(struct i2c_client *client)
{
	int res = 0xff, sel;

	superio_enter();
	superio_select(W83627HF_LD_GPIO5);

	/* Make sure these GPIO pins are enabled */
	if (!(superio_inb(W83627THF_GPIO5_EN) & (1<<3))) {
#ifdef DEBUG
		printk(KERN_DEBUG "w83627hf: GPIO5 disabled, no VID "
		       "function\n");
#endif
		goto exit;
	}

	/* Make sure the pins are configured for input
	   There must be at least five (VRM 9), and possibly 6 (VRM 10) */
	sel = superio_inb(W83627THF_GPIO5_IOSR) & 0x3f;
	if ((sel & 0x1f) != 0x1f) {
#ifdef DEBUG
		printk(KERN_DEBUG "w83627hf: GPIO5 not configured for "
		       "VID function\n");
#endif
		goto exit;
	}

	printk(KERN_INFO "w83627hf: Reading VID from GPIO5\n");
	res = superio_inb(W83627THF_GPIO5_DR) & sel;

exit:
	superio_exit();
	return res;
}

static int w83687thf_read_vid(struct i2c_client *client)
{
	int res = 0xff;

	superio_enter();
	superio_select(W83627HF_LD_HWM);

	/* Make sure these GPIO pins are enabled */
	if (!(superio_inb(W83687THF_VID_EN) & (1 << 2))) {
#ifdef DEBUG
		printk(KERN_DEBUG "w83627hf: VID disabled, no VID "
		       "function\n");
#endif
		goto exit;
	}

	/* Make sure the pins are configured for input */
	if (!(superio_inb(W83687THF_VID_CFG) & (1 << 4))) {
#ifdef DEBUG
		printk(KERN_DEBUG "w83627hf: VID configured as output, "
		       "no VID function\n");
#endif
		goto exit;
	}

	res = superio_inb(W83687THF_VID_DATA) & 0x3f;

exit:
	superio_exit();
	return res;
}

static int w83627hf_write_value(struct i2c_client *client, u16 reg, u16 value)
{
	int word_sized;

	down(&(((struct w83627hf_data *) (client->data))->lock));
	word_sized = (((reg & 0xff00) == 0x100)
		      || ((reg & 0xff00) == 0x200))
	    && (((reg & 0x00ff) == 0x53)
		|| ((reg & 0x00ff) == 0x55));
	if (reg & 0xff00) {
		outb_p(W83781D_REG_BANK,
		       client->addr + W83781D_ADDR_REG_OFFSET);
		outb_p(reg >> 8,
		       client->addr + W83781D_DATA_REG_OFFSET);
	}
	outb_p(reg & 0xff, client->addr + W83781D_ADDR_REG_OFFSET);
	if (word_sized) {
		outb_p(value >> 8,
		       client->addr + W83781D_DATA_REG_OFFSET);
		outb_p((reg & 0xff) + 1,
		       client->addr + W83781D_ADDR_REG_OFFSET);
	}
	outb_p(value & 0xff,
	       client->addr + W83781D_DATA_REG_OFFSET);
	if (reg & 0xff00) {
		outb_p(W83781D_REG_BANK,
		       client->addr + W83781D_ADDR_REG_OFFSET);
		outb_p(0, client->addr + W83781D_DATA_REG_OFFSET);
	}
	up(&(((struct w83627hf_data *) (client->data))->lock));
	return 0;
}

/* Called when we have found a new W83781D. */
static void w83627hf_init_client(struct i2c_client *client)
{
	struct w83627hf_data *data = client->data;
	int i;
	int type = data->type;
	u8 tmp;

	/* Minimize conflicts with other winbond i2c-only clients...  */
	/* disable i2c subclients... how to disable main i2c client?? */
	/* force i2c address to relatively uncommon address */
	w83627hf_write_value(client, W83781D_REG_I2C_SUBADDR, 0x89);
	w83627hf_write_value(client, W83781D_REG_I2C_ADDR, force_i2c);

	/* Read VID only once */
	if (w83627hf == data->type || w83637hf == data->type) {
		int lo = w83627hf_read_value(client, W83781D_REG_VID_FANDIV);
		int hi = w83627hf_read_value(client, W83781D_REG_CHIPID);
		data->vid = (lo & 0x0f) | ((hi & 0x01) << 4);
	} else if (w83627thf == data->type) {
		data->vid = w83627thf_read_gpio5(client);
	} else if (w83687thf == data->type) {
		data->vid = w83687thf_read_vid(client);
	}

	/* Read VRM & OVT Config only once */
	if (w83627thf == data->type || w83637hf == data->type
	 || w83687thf == data->type)
		data->vrm_ovt =
			w83627hf_read_value(client, W83627THF_REG_VRM_OVT_CFG);
	else
		data->vrm_ovt = 0;

	/* Choose VRM based on "VRM & OVT" register */
	data->vrm = (data->vrm_ovt & 0x01) ? 90 : 82;

	tmp = w83627hf_read_value(client, W83781D_REG_SCFG1);
	for (i = 1; i <= 3; i++) {
		if (!(tmp & BIT_SCFG1[i - 1])) {
			data->sens[i - 1] = W83781D_DEFAULT_BETA;
		} else {
			if (w83627hf_read_value
			    (client,
			     W83781D_REG_SCFG2) & BIT_SCFG2[i - 1])
				data->sens[i - 1] = 1;
			else
				data->sens[i - 1] = 2;
		}
		if ((type == w83697hf) && (i == 2))
			break;
	}

	if(init) {
		/* Enable temp2 */
		tmp = w83627hf_read_value(client, W83781D_REG_TEMP2_CONFIG);
		if (tmp & 0x01) {
			printk(KERN_WARNING "w83627hf: temp2 was disabled, "
			       "readings might not make sense\n");
			w83627hf_write_value(client, W83781D_REG_TEMP2_CONFIG,
				tmp & 0xfe);
		}

		/* Enable temp3 */
		if (type != w83697hf) {
			tmp = w83627hf_read_value(client,
				W83781D_REG_TEMP3_CONFIG);
			if (tmp & 0x01) {
				printk(KERN_WARNING "w83627hf: temp3 was "
				       "disabled, readings might not make "
				       "sense\n");
				w83627hf_write_value(client,
					W83781D_REG_TEMP3_CONFIG, tmp & 0xfe);
			}
		}
	}

	/* Start monitoring */
	w83627hf_write_value(client, W83781D_REG_CONFIG,
			    (w83627hf_read_value(client,
						W83781D_REG_CONFIG) & 0xf7)
			    | 0x01);

	/* Enable VBAT monitoring if needed */
	tmp = w83627hf_read_value(client, W83781D_REG_VBAT);
	if (!(tmp & 0x01))
		w83627hf_write_value(client, W83781D_REG_VBAT, tmp | 0x01);
}

static void w83627hf_update_client(struct i2c_client *client)
{
	struct w83627hf_data *data = client->data;
	int i;

	down(&data->update_lock);

	if ((jiffies - data->last_updated > HZ + HZ / 2) ||
	    (jiffies < data->last_updated) || !data->valid) {
		for (i = 0; i <= 8; i++) {
			/* skip missing sensors */
			if (((data->type == w83697hf) && (i == 1)) ||
			    ((data->type == w83627thf || data->type == w83637hf
			      || data->type == w83687thf) && (i == 5 || i == 6)))
				continue;
			data->in[i] =
			    w83627hf_read_value(client, W83781D_REG_IN(i));
			data->in_min[i] =
			    w83627hf_read_value(client,
					       W83781D_REG_IN_MIN(i));
			data->in_max[i] =
			    w83627hf_read_value(client,
					       W83781D_REG_IN_MAX(i));
		}
		for (i = 1; i <= 3; i++) {
			data->fan[i - 1] =
			    w83627hf_read_value(client, W83781D_REG_FAN(i));
			data->fan_min[i - 1] =
			    w83627hf_read_value(client,
					       W83781D_REG_FAN_MIN(i));
		}
		for (i = 1; i <= 3; i++) {
			u8 tmp = w83627hf_read_value(client,
				W836X7HF_REG_PWM(data->type, i));
			if (data->type == w83627thf)
				tmp &= 0xf0; /* bits 0-3 are reserved  in 627THF */
			data->pwm[i - 1] = tmp;
			if(i == 2 && (data->type == w83627hf || data->type == w83697hf))
				break;
		}

		data->temp = w83627hf_read_value(client, W83781D_REG_TEMP);
		data->temp_over =
		    w83627hf_read_value(client, W83781D_REG_TEMP_OVER);
		data->temp_hyst =
		    w83627hf_read_value(client, W83781D_REG_TEMP_HYST);
		data->temp_add[0] =
		    w83627hf_read_value(client, W83781D_REG_TEMP2);
		data->temp_add_over[0] =
		    w83627hf_read_value(client, W83781D_REG_TEMP2_OVER);
		data->temp_add_hyst[0] =
		    w83627hf_read_value(client, W83781D_REG_TEMP2_HYST);
		if (data->type != w83697hf) {
			data->temp_add[1] =
			    w83627hf_read_value(client, W83781D_REG_TEMP3);
			data->temp_add_over[1] =
			    w83627hf_read_value(client, W83781D_REG_TEMP3_OVER);
			data->temp_add_hyst[1] =
			    w83627hf_read_value(client, W83781D_REG_TEMP3_HYST);
		}

		i = w83627hf_read_value(client, W83781D_REG_VID_FANDIV);
		data->fan_div[0] = (i >> 4) & 0x03;
		data->fan_div[1] = (i >> 6) & 0x03;
		if (data->type != w83697hf) {
			data->fan_div[2] = (w83627hf_read_value(client,
					       W83781D_REG_PIN) >> 6) & 0x03;
		}
		i = w83627hf_read_value(client, W83781D_REG_VBAT);
		data->fan_div[0] |= (i >> 3) & 0x04;
		data->fan_div[1] |= (i >> 4) & 0x04;
		if (data->type != w83697hf)
			data->fan_div[2] |= (i >> 5) & 0x04;
		data->alarms =
		    w83627hf_read_value(client, W83781D_REG_ALARM1) |
		    (w83627hf_read_value(client, W83781D_REG_ALARM2) << 8) |
		    (w83627hf_read_value(client, W83781D_REG_ALARM3) << 16);
		i = w83627hf_read_value(client, W83781D_REG_BEEP_INTS2);
		data->beep_enable = i >> 7;
		data->beeps = ((i & 0x7f) << 8) |
		    w83627hf_read_value(client, W83781D_REG_BEEP_INTS1) |
		    w83627hf_read_value(client, W83781D_REG_BEEP_INTS3) << 16;
		data->last_updated = jiffies;
		data->valid = 1;
	}

	up(&data->update_lock);
}

void w83627hf_in(struct i2c_client *client, int operation, int ctl_name,
		int *nrels_mag, long *results)
{
	struct w83627hf_data *data = client->data;
	int nr = ctl_name - W83781D_SYSCTL_IN0;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 2;
	else if (operation == SENSORS_PROC_REAL_READ) {
		w83627hf_update_client(client);

		if (nr == 0 && (data->vrm_ovt & 0x01)) {
			/* use VRM9 calculation */
			results[0] = IN_FROM_REG_VRM9(data->in_min[0]);
			results[1] = IN_FROM_REG_VRM9(data->in_max[0]);
			results[2] = IN_FROM_REG_VRM9(data->in[0]);

		} else {
			/* use VRM8 (standard) calculation */
			results[0] = IN_FROM_REG(data->in_min[nr]);
			results[1] = IN_FROM_REG(data->in_max[nr]);
			results[2] = IN_FROM_REG(data->in[nr]);
		}

		*nrels_mag = 3;

	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			if (nr == 0 && (data->vrm_ovt & 0x01))
				/* use VRM9 calculation */
				data->in_min[0] = IN_TO_REG_VRM9(results[0]);
			else
				/* use VRM8 (standard) calculation */
				data->in_min[nr] = IN_TO_REG(results[0]);

			w83627hf_write_value(client, W83781D_REG_IN_MIN(nr),
					    data->in_min[nr]);
		}
		if (*nrels_mag >= 2) {
			if (nr == 0 && (data->vrm_ovt & 0x01))
				/* use VRM9 calculation */
				data->in_max[0] = IN_TO_REG_VRM9(results[1]);
			else
				/* use VRM8 (standard) calculation */
				data->in_max[nr] = IN_TO_REG(results[1]);

			w83627hf_write_value(client, W83781D_REG_IN_MAX(nr),
					    data->in_max[nr]);
		}
	}
}

void w83627hf_fan(struct i2c_client *client, int operation, int ctl_name,
		 int *nrels_mag, long *results)
{
	struct w83627hf_data *data = client->data;
	int nr = ctl_name - W83781D_SYSCTL_FAN1 + 1;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		w83627hf_update_client(client);
		results[0] = FAN_FROM_REG(data->fan_min[nr - 1],
				  DIV_FROM_REG(data->fan_div[nr - 1]));
		results[1] = FAN_FROM_REG(data->fan[nr - 1],
			          DIV_FROM_REG(data->fan_div[nr - 1]));
		*nrels_mag = 2;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->fan_min[nr - 1] =
			     FAN_TO_REG(results[0],
			            DIV_FROM_REG(data->fan_div[nr-1]));
			w83627hf_write_value(client,
					    W83781D_REG_FAN_MIN(nr),
					    data->fan_min[nr - 1]);
		}
	}
}

void w83627hf_temp(struct i2c_client *client, int operation, int ctl_name,
		  int *nrels_mag, long *results)
{
	struct w83627hf_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 1;
	else if (operation == SENSORS_PROC_REAL_READ) {
		w83627hf_update_client(client);
		results[0] = TEMP_FROM_REG(data->temp_over);
		results[1] = TEMP_FROM_REG(data->temp_hyst);
		results[2] = TEMP_FROM_REG(data->temp);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->temp_over = TEMP_TO_REG(results[0]);
			w83627hf_write_value(client, W83781D_REG_TEMP_OVER,
					    data->temp_over);
		}
		if (*nrels_mag >= 2) {
			data->temp_hyst = TEMP_TO_REG(results[1]);
			w83627hf_write_value(client, W83781D_REG_TEMP_HYST,
					    data->temp_hyst);
		}
	}
}

void w83627hf_temp_add(struct i2c_client *client, int operation,
		      int ctl_name, int *nrels_mag, long *results)
{
	struct w83627hf_data *data = client->data;
	int nr = ctl_name - W83781D_SYSCTL_TEMP2;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 1;
	else if (operation == SENSORS_PROC_REAL_READ) {
		w83627hf_update_client(client);
			results[0] =
			    LM75_TEMP_FROM_REG(data->temp_add_over[nr]);
			results[1] =
			    LM75_TEMP_FROM_REG(data->temp_add_hyst[nr]);
			results[2] = LM75_TEMP_FROM_REG(data->temp_add[nr]);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
				data->temp_add_over[nr] =
				    LM75_TEMP_TO_REG(results[0]);
			w83627hf_write_value(client,
					    nr ? W83781D_REG_TEMP3_OVER :
					    W83781D_REG_TEMP2_OVER,
					    data->temp_add_over[nr]);
		}
		if (*nrels_mag >= 2) {
				data->temp_add_hyst[nr] =
				    LM75_TEMP_TO_REG(results[1]);
			w83627hf_write_value(client,
					    nr ? W83781D_REG_TEMP3_HYST :
					    W83781D_REG_TEMP2_HYST,
					    data->temp_add_hyst[nr]);
		}
	}
}

void w83627hf_vid(struct i2c_client *client, int operation, int ctl_name,
		 int *nrels_mag, long *results)
{
	struct w83627hf_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 3;
	else if (operation == SENSORS_PROC_REAL_READ) {
		if (data->vid == 0xff)
			results[0] = 0;
		else
			results[0] = vid_from_reg(data->vid, data->vrm);
		*nrels_mag = 1;
	}
}

void w83627hf_vrm(struct i2c_client *client, int operation, int ctl_name,
		 int *nrels_mag, long *results)
{
	struct w83627hf_data *data = client->data;
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

void w83627hf_alarms(struct i2c_client *client, int operation, int ctl_name,
		    int *nrels_mag, long *results)
{
	struct w83627hf_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		w83627hf_update_client(client);
		results[0] = data->alarms;
		*nrels_mag = 1;
	}
}

void w83627hf_beep(struct i2c_client *client, int operation, int ctl_name,
		  int *nrels_mag, long *results)
{
	struct w83627hf_data *data = client->data;
	int val;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		w83627hf_update_client(client);
		results[0] = BEEP_ENABLE_FROM_REG(data->beep_enable);
		results[1] = data->beeps;
		*nrels_mag = 2;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 2) {
			data->beeps = BEEPS_TO_REG(results[1]);
			w83627hf_write_value(client, W83781D_REG_BEEP_INTS1,
					    data->beeps & 0xff);
				w83627hf_write_value(client,
						    W83781D_REG_BEEP_INTS3,
						    ((data-> beeps) >> 16) &
						      0xff);
			val = (data->beeps >> 8) & 0x7f;
		} else if (*nrels_mag >= 1)
			val =
			    w83627hf_read_value(client,
					       W83781D_REG_BEEP_INTS2) &
			    0x7f;
		if (*nrels_mag >= 1) {
			data->beep_enable = BEEP_ENABLE_TO_REG(results[0]);
			w83627hf_write_value(client, W83781D_REG_BEEP_INTS2,
					    val | data->beep_enable << 7);
		}
	}
}

/* w83697hf only has two fans */
void w83627hf_fan_div(struct i2c_client *client, int operation,
		     int ctl_name, int *nrels_mag, long *results)
{
	struct w83627hf_data *data = client->data;
	int old, old2, old3 = 0;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		w83627hf_update_client(client);
		results[0] = DIV_FROM_REG(data->fan_div[0]);
		results[1] = DIV_FROM_REG(data->fan_div[1]);
		if (data->type == w83697hf) {
			*nrels_mag = 2;
		} else {
			results[2] = DIV_FROM_REG(data->fan_div[2]);
			*nrels_mag = 3;
		}
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		unsigned long min = 0;

		old = w83627hf_read_value(client, W83781D_REG_VID_FANDIV);
		/* w83627hf doesn't have extended divisor bits */
		old3 = w83627hf_read_value(client, W83781D_REG_VBAT);
		if (*nrels_mag >= 3 && data->type != w83697hf) {
			min = FAN_FROM_REG(data->fan_min[2],
				DIV_FROM_REG(data->fan_div[2]));
			data->fan_div[2] = DIV_TO_REG(results[2]);
			old2 = w83627hf_read_value(client, W83781D_REG_PIN);
			old2 = (old2 & 0x3f)
			     | ((data->fan_div[2] & 0x03) << 6);
			w83627hf_write_value(client, W83781D_REG_PIN, old2);
			old3 = (old3 & 0x7f)
			     | ((data->fan_div[2] & 0x04) << 5);
			data->fan_min[2] = FAN_TO_REG(min,
				DIV_FROM_REG(data->fan_div[2]));
			w83627hf_write_value(client, W83781D_REG_FAN_MIN(3),
				data->fan_min[2]);
		}
		if (*nrels_mag >= 2) {
			min = FAN_FROM_REG(data->fan_min[1],
				DIV_FROM_REG(data->fan_div[1]));
			data->fan_div[1] = DIV_TO_REG(results[1]);
			old = (old & 0x3f)
			    | ((data->fan_div[1] & 0x03) << 6);
			old3 = (old3 & 0xbf)
			     | ((data->fan_div[1] & 0x04) << 4);
			data->fan_min[1] = FAN_TO_REG(min,
				DIV_FROM_REG(data->fan_div[1]));
			w83627hf_write_value(client, W83781D_REG_FAN_MIN(2),
				data->fan_min[1]);
		}
		if (*nrels_mag >= 1) {
			min = FAN_FROM_REG(data->fan_min[0],
				DIV_FROM_REG(data->fan_div[0]));
			data->fan_div[0] = DIV_TO_REG(results[0]);
			old = (old & 0xcf)
			    | ((data->fan_div[0] & 0x03) << 4);
			w83627hf_write_value(client, W83781D_REG_VID_FANDIV,
					     old);
			old3 = (old3 & 0xdf)
			     | ((data->fan_div[0] & 0x04) << 3);
			w83627hf_write_value(client, W83781D_REG_VBAT,
					     old3);
			data->fan_min[0] = FAN_TO_REG(min,
				DIV_FROM_REG(data->fan_div[0]));
			w83627hf_write_value(client, W83781D_REG_FAN_MIN(1),
				data->fan_min[0]);
		}
	}
}

/* we do not currently support disabling PWM with 2nd argument;
  set first argument to 255 to disable */
void w83627hf_pwm(struct i2c_client *client, int operation, int ctl_name,
		 int *nrels_mag, long *results)
{
	struct w83627hf_data *data = client->data;
	int nr = 1 + ctl_name - W83781D_SYSCTL_PWM1;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		w83627hf_update_client(client);
		results[0] = data->pwm[nr - 1];
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			if (data->type == w83627thf) {
				/* bits 0-3 are reserved  in 627THF */
				data->pwm[nr - 1] = PWM_TO_REG(results[0]) & 0xf0;
				w83627hf_write_value(client,
						     W836X7HF_REG_PWM(data->type, nr),
						     data->pwm[nr - 1] |
						     (w83627hf_read_value(client,
						      W836X7HF_REG_PWM(data->type, nr)) & 0x0f));
			} else {
				data->pwm[nr - 1] = PWM_TO_REG(results[0]);
				w83627hf_write_value(client,
						     W836X7HF_REG_PWM(data->type, nr),
						     data->pwm[nr - 1]);
			}
		}
	}
}

void w83627hf_sens(struct i2c_client *client, int operation, int ctl_name,
		  int *nrels_mag, long *results)
{
	struct w83627hf_data *data = client->data;
	int nr = 1 + ctl_name - W83781D_SYSCTL_SENS1;
	u8 tmp;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		results[0] = data->sens[nr - 1];
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			switch (results[0]) {
			case 1:	/* PII/Celeron diode */
				tmp = w83627hf_read_value(client,
						       W83781D_REG_SCFG1);
				w83627hf_write_value(client,
						    W83781D_REG_SCFG1,
						    tmp | BIT_SCFG1[nr -
								    1]);
				tmp = w83627hf_read_value(client,
						       W83781D_REG_SCFG2);
				w83627hf_write_value(client,
						    W83781D_REG_SCFG2,
						    tmp | BIT_SCFG2[nr -
								    1]);
				data->sens[nr - 1] = results[0];
				break;
			case 2:	/* 3904 */
				tmp = w83627hf_read_value(client,
						       W83781D_REG_SCFG1);
				w83627hf_write_value(client,
						    W83781D_REG_SCFG1,
						    tmp | BIT_SCFG1[nr -
								    1]);
				tmp = w83627hf_read_value(client,
						       W83781D_REG_SCFG2);
				w83627hf_write_value(client,
						    W83781D_REG_SCFG2,
						    tmp & ~BIT_SCFG2[nr -
								     1]);
				data->sens[nr - 1] = results[0];
				break;
			case W83781D_DEFAULT_BETA:	/* thermistor */
				tmp = w83627hf_read_value(client,
						       W83781D_REG_SCFG1);
				w83627hf_write_value(client,
						    W83781D_REG_SCFG1,
						    tmp & ~BIT_SCFG1[nr -
								     1]);
				data->sens[nr - 1] = results[0];
				break;
			default:
				printk
				    (KERN_ERR "w83627hf.o: Invalid sensor type %ld; must be 1, 2, or %d\n",
				     results[0], W83781D_DEFAULT_BETA);
				break;
			}
		}
	}
}

static int __init sm_w83627hf_init(void)
{
	int addr;

	printk(KERN_INFO "w83627hf.o version %s (%s)\n", LM_VERSION, LM_DATE);
	if (w83627hf_find(0x2e, &addr)
	 && w83627hf_find(0x4e, &addr)) {
		printk("w83627hf.o: W83627/697 not detected, module not inserted.\n");
		return -ENODEV;
	}
	normal_isa[0] = addr;

	return i2c_add_driver(&w83627hf_driver);
}

static void __exit sm_w83627hf_exit(void)
{
	i2c_del_driver(&w83627hf_driver);
}



MODULE_AUTHOR("Frodo Looijaard <frodol@dds.nl>, "
	      "Philip Edelbrock <phil@netroedge.com>, "
	      "and Mark Studebaker <mdsxyz123@yahoo.com>");
MODULE_DESCRIPTION("W83627HF driver");
MODULE_LICENSE("GPL");

module_init(sm_w83627hf_init);
module_exit(sm_w83627hf_exit);
