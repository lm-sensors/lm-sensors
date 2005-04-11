/*
    w83792d.c - Part of lm_sensors, Linux kernel modules for hardware
                monitoring
    Copyright (c) 2004, 2005 Winbond Electronics Corp.
                  Chunhao Huang <huang0@winbond.com.tw>

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

    Note:
    1. This driver is only for 2.4 kernel(2.4.10 or later), 2.6 kernel
       need a different driver.
    2. This driver is only for Winbond W83792D C version device, there
       are also some motherboards with B version W83792D device. The 
       calculation method to in6-in7(measured value, limits) is a little
       different between C and B version. C or B version can be identified
       by CR[0x49h].
    3. Maybe there is some bug in temp3, because temp3 measured value
       usually seems wrong.
    4. The function of chassis open detection need further test.
    5. The function of vid and vrm has not been finished, because I'm NOT
       very familiar with them. If someone can finish it, that's good,
       then please delete this note 5.
*/

/*
    Supports following chips:

    Chip	#vin	#fanin	#pwm	#temp	wchipid	vendid	i2c	ISA
    w83792d	9	7	3	3	0x7a	0x5ca3	yes	no
*/

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/i2c-proc.h>
#include <linux/init.h>
#include "version.h"
#include "sensors_vid.h"

/* Addresses to scan */
static unsigned short normal_i2c[] = { SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { 0x2c, 0x2f, SENSORS_I2C_END };
static unsigned int normal_isa[] = { SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/* Insmod parameters */
SENSORS_INSMOD_1(w83792d);
SENSORS_MODULE_PARM(force_subclients, "List of subclient addresses: " \
                      "{bus, clientaddr, subclientaddr1, subclientaddr2}");

static int init;
MODULE_PARM(init, "i");
MODULE_PARM_DESC(init, "Set to one for chip initialization");

/* Enable/Disable w83792d debugging output */
/* #define W83792D_DEBUG 1 */

/* Constants specified below */
#define W83792D_REG_CONFIG 0x40
#define W83792D_REG_I2C_ADDR 0x48
#define W83792D_REG_CHIPID 0x49   /* contains version ID: A/B/C */
#define W83792D_REG_I2C_SUBADDR 0x4A
#define W83792D_REG_IRQ 0x4C
#define W83792D_REG_BANK 0x4E
#define W83792D_REG_CHIPMAN 0x4F  /* contains the vendor ID */
#define W83792D_REG_WCHIPID 0x58  /* contains the chip ID */
#define W83792D_REG_VID_IN_B 0x17 /* ctroll in0/in1 's limit modifiability */

static const u8 W83792D_REG_IN[9] = {
	0x20,	/* Vcore A in DataSheet */
	0x21,	/* Vcore B in DataSheet */
	0x22,	/* VIN0 in DataSheet */
	0x23,	/* VIN1 in DataSheet */
	0x24,	/* VIN2 in DataSheet */
	0x25,	/* VIN3 in DataSheet */
	0x26,	/* 5VCC in DataSheet */
	0xB0,	/* 5VSB in DataSheet */
	0xB1	/* VBAT in DataSheet */
};
#define W83792D_REG_LOW_BITS1 0x3E  /* Low Bits I in DataSheet */
#define W83792D_REG_LOW_BITS2 0x3F  /* Low Bits II in DataSheet */
static const u8 W83792D_REG_IN_MAX[9] = {
	0x2B,	/* Vcore A High Limit in DataSheet */
	0x2D,	/* Vcore B High Limit in DataSheet */
	0x2F,	/* VIN0 High Limit in DataSheet */
	0x31,	/* VIN1 High Limit in DataSheet */
	0x33,	/* VIN2 High Limit in DataSheet */
	0x35,	/* VIN3 High Limit in DataSheet */
	0x37,	/* 5VCC High Limit in DataSheet */
	0xB4,	/* 5VSB High Limit in DataSheet */
	0xB6	/* VBAT High Limit in DataSheet */
};
static const u8 W83792D_REG_IN_MIN[9] = {
	0x2C,	/* Vcore A Low Limit in DataSheet */
	0x2E,	/* Vcore B Low Limit in DataSheet */
	0x30,	/* VIN0 Low Limit in DataSheet */
	0x32,	/* VIN1 Low Limit in DataSheet */
	0x34,	/* VIN2 Low Limit in DataSheet */
	0x36,	/* VIN3 Low Limit in DataSheet */
	0x38,	/* 5VCC Low Limit in DataSheet */
	0xB5,	/* 5VSB Low Limit in DataSheet */
	0xB7	/* VBAT Low Limit in DataSheet */
};

static const u8 W83792D_REG_FAN[7] = {
	0x28,	/* FAN 1 Count in DataSheet */
	0x29,	/* FAN 2 Count in DataSheet */
	0x2A,	/* FAN 3 Count in DataSheet */
	0xB8,	/* FAN 4 Count in DataSheet */
	0xB9,	/* FAN 5 Count in DataSheet */
	0xBA,	/* FAN 6 Count in DataSheet */
	0xBE	/* FAN 7 Count in DataSheet */
};
static const u8 W83792D_REG_FAN_MIN[7] = {
	0x3B,	/* FAN 1 Count Low Limit in DataSheet */
	0x3C,	/* FAN 2 Count Low Limit in DataSheet */
	0x3D,	/* FAN 3 Count Low Limit in DataSheet */
	0xBB,	/* FAN 4 Count Low Limit in DataSheet */
	0xBC,	/* FAN 5 Count Low Limit in DataSheet */
	0xBD,	/* FAN 6 Count Low Limit in DataSheet */
	0xBF	/* FAN 7 Count Low Limit in DataSheet */
};
#define W83792D_REG_FAN_CFG 0x84    /* FAN Configuration in DataSheet */
static const u8 W83792D_REG_PWM[7] = {
	0x81,	/* FAN 1 Duty Cycle, be used to control */
	0x83,	/* FAN 2 Duty Cycle, be used to control */
	0x94,	/* FAN 3 Duty Cycle, be used to control */
	0xA3,	/* FAN 4 Duty Cycle, be used to control */
	0xA4,	/* FAN 5 Duty Cycle, be used to control */
	0xA5,	/* FAN 6 Duty Cycle, be used to control */
	0xA6	/* FAN 7 Duty Cycle, be used to control */
};

#define W83792D_REG_TEMP1 0x27		/* TEMP 1 in DataSheet */
#define W83792D_REG_TEMP1_OVER 0x39	/* TEMP 1 High Limit in DataSheet */
#define W83792D_REG_TEMP1_HYST 0x3A	/* TEMP 1 Low Limit in DataSheet */
static const u8 W83792D_REG_TEMP_ADD[2][7] = {
	{ 0xC0,		/* TEMP 2 in DataSheet */
	  0xC1,		/* TEMP 2(0.5 deg) in DataSheet */
	  0xC5,		/* TEMP 2 Over High part in DataSheet */
	  0xC6,		/* TEMP 2 Over Low part in DataSheet */
	  0xC3,		/* TEMP 2 Thyst High part in DataSheet */
	  0xC4,		/* TEMP 2 Thyst Low part in DataSheet */
	  0xC2 },	/* TEMP 2 Config in DataSheet */
	{ 0xC8,		/* TEMP 3 in DataSheet */
	  0xC9,		/* TEMP 3(0.5 deg) in DataSheet */
	  0xCD,		/* TEMP 3 Over High part in DataSheet */
	  0xCE,		/* TEMP 3 Over Low part in DataSheet */
	  0xCB,		/* TEMP 3 Thyst High part in DataSheet */
	  0xCC,		/* TEMP 3 Thyst Low part in DataSheet */
	  0xCA }	/* TEMP 3 Config in DataSheet */
};

static const u8 W83792D_REG_FAN_DIV[4] = {
	0x47,	/* contains FAN2 and FAN1 Divisor */
	0x5B,	/* contains FAN4 and FAN3 Divisor */
	0x5C,	/* contains FAN6 and FAN5 Divisor */
	0x9E	/* contains FAN7 Divisor. */
};

#define W83792D_REG_CASE_OPEN 0x42	/* Bit 5: Case Open status bit */
#define W83792D_REG_CASE_OPEN_CLR 0x44	/* Bit 7: Case Open CLR_CHS/Reset bit */

static const u8 W83792D_REG_THERMAL[3] = {
	0x85,	/* SmartFanI: Fan1 target value */
	0x86,	/* SmartFanI: Fan2 target value */
	0x96	/* SmartFanI: Fan3 target value */
};

static const u8 W83792D_REG_FAN_TOL[3] = {
	0x87,	/* (bit3-0)SmartFan Fan1 tolerance */
	0x87,	/* (bit7-4)SmartFan Fan2 tolerance */
	0x97	/* (bit3-0)SmartFan Fan3 tolerance */
};

static const u8 W83792D_REG_POINTS[3][4] = {
	{ 0x85,		/* SmartFanII: Fan1 temp point 1 */
	  0xE3,		/* SmartFanII: Fan1 temp point 2 */
	  0xE4,		/* SmartFanII: Fan1 temp point 3 */
	  0xE5 },	/* SmartFanII: Fan1 temp point 4 */
	{ 0x86,		/* SmartFanII: Fan2 temp point 1 */
	  0xE6,		/* SmartFanII: Fan2 temp point 2 */
	  0xE7,		/* SmartFanII: Fan2 temp point 3 */
	  0xE8 },	/* SmartFanII: Fan2 temp point 4 */
	{ 0x96,		/* SmartFanII: Fan3 temp point 1 */
	  0xE9,		/* SmartFanII: Fan3 temp point 2 */
	  0xEA,		/* SmartFanII: Fan3 temp point 3 */
	  0xEB }	/* SmartFanII: Fan3 temp point 4 */
};

static const u8 W83792D_REG_LEVELS[3][4] = {
	{ 0x88,		/* (bit3-0) SmartFanII: Fan1 Non-Stop */
	  0x88,		/* (bit7-4) SmartFanII: Fan1 Level 1 */
	  0xE0,		/* (bit7-4) SmartFanII: Fan1 Level 2 */
	  0xE0 },	/* (bit3-0) SmartFanII: Fan1 Level 3 */
	{ 0x89,		/* (bit3-0) SmartFanII: Fan2 Non-Stop */
	  0x89,		/* (bit7-4) SmartFanII: Fan2 Level 1 */
	  0xE1,		/* (bit7-4) SmartFanII: Fan2 Level 2 */
	  0xE1 },	/* (bit3-0) SmartFanII: Fan2 Level 3 */
	{ 0x98,		/* (bit3-0) SmartFanII: Fan3 Non-Stop */
	  0x98,		/* (bit7-4) SmartFanII: Fan3 Level 1 */
	  0xE2,		/* (bit7-4) SmartFanII: Fan3 Level 2 */
	  0xE2 }	/* (bit3-0) SmartFanII: Fan3 Level 3 */
};

static inline u8 FAN_TO_REG(long rpm, int div)
{
	if (rpm == 0)
		return 255;
	rpm = SENSORS_LIMIT(rpm, 1, 1000000);
	return SENSORS_LIMIT(1350000/(rpm * div), 1, 254);
}

#define IN_FROM_REG(nr,val) (((nr)<=1)?(val*2): \
                             ((((nr)==6)||((nr)==7))?(val*6):(val*4)))
#define IN_TO_REG(nr,val) (((nr)<=1)?(val/2): \
                           ((((nr)==6)||((nr)==7))?(val/6):(val/4)))
#define TEMP_FROM_REG(val) (((val)>0x80?(val)-0x100:(val))*10)
#define TEMP_TO_REG(val) (SENSORS_LIMIT((val>=0)?((val)/10):((val)/10+256), 0, 255))
#define FAN_FROM_REG(val,div) ((val)==0?-1:(val)==255?0:1350000/((val)*(div)))
#define DIV_FROM_REG(val) (1 << (val))

#ifdef W83792D_DEBUG
#define ENTER()	printk(KERN_DEBUG "w83792d: ENTERING %s, line: %d\n", __FUNCTION__, __LINE__);
#define LEAVE()	printk(KERN_DEBUG "w83792d: LEAVING %s, line: %d\n", __FUNCTION__, __LINE__);
#else
#define ENTER()
#define LEAVE()
#endif

struct w83792d_data {
	struct i2c_client client;
	struct semaphore lock;
	int sysctl_id;
	enum chips type;

	struct semaphore update_lock;
	char valid;		/* !=0 if following fields are valid */
	unsigned long last_updated;	/* In jiffies */

	struct i2c_client *lm75;	/* for secondary I2C addresses */
	/* pointer to array of 2 subclients */

	u8 in[9];		/* Register value */
	u8 in_max[9];		/* Register value */
	u8 in_min[9];		/* Register value */
	u8 low_bits[2];		/* Register value */
	u8 fan[7];		/* Register value */
	u8 fan_min[7];		/* Register value */
	u8 fan_cfg;		/* Configure Fan Mode */
	u8 temp1[3];		/* Register value */
	u8 temp_add[2][7];	/* Register value */
	u8 fan_div[7];		/* Fan Divisor */
	/*u8 vid;		Register encoding, combined */
	u8 pwm[7];		/* We only consider the first 3 set of pwm,
				   although 792 chip has 7 set of pwm. */
	u8 pwm_flag[7];		/* indicates PWM or DC mode: 1->PWM; 0->DC */
	/* u8 vrm;		 VRM version */
	u8 chassis[2];		/* [0]->Chassis status, [1]->CLR_CHS */
	u8 thermal_cruise[3];	/* Smart FanI: Fan1,2,3 target value */
	u8 fan_tolerance[3];	/* Fan1,2,3 tolerance(Smart Fan I/II) */
	u8 sf2_points[3][4];	/* Smart FanII: Fan1,2,3 temperature points */
	u8 sf2_levels[3][4];	/* Smart FanII: Fan1,2,3 duty cycle levels */
};


static int w83792d_attach_adapter(struct i2c_adapter *adapter);
static int w83792d_detect(struct i2c_adapter *adapter, int address,
			  unsigned short flags, int kind);
static int w83792d_detach_client(struct i2c_client *client);

static int w83792d_read_value(struct i2c_client *client, u8 register);
static int w83792d_write_value(struct i2c_client *client, u8 register,
			       u8 value);
static void w83792d_init_client(struct i2c_client *client);
static void w83792d_update_client(struct i2c_client *client);
#ifdef W83792D_DEBUG
static void w83792d_print_debug(struct w83792d_data *data);
#endif
static void w83792d_in(struct i2c_client *client, int operation,
		       int ctl_name, int *nrels_mag, long *results);
static void w83792d_fan(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void w83792d_temp(struct i2c_client *client, int operation,
			 int ctl_name, int *nrels_mag, long *results);
static void w83792d_temp_add(struct i2c_client *client, int operation,
			     int ctl_name, int *nrels_mag, long *results);
/*static void w83792d_vid(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void w83792d_vrm(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results); */
static void w83792d_fan_div(struct i2c_client *client, int operation,
			    int ctl_name, int *nrels_mag, long *results);
static void w83792d_chassis(struct i2c_client *client, int operation,
			    int ctl_name, int *nrels_mag, long *results);
static void w83792d_pwm(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void w83792d_pwm_flag(struct i2c_client *client, int operation,
			     int ctl_name, int *nrels_mag, long *results);
static void w83792d_fan_cfg(struct i2c_client *client, int operation,
			    int ctl_name, int *nrels_mag, long *results);
static void w83792d_thermal_cruise(struct i2c_client *client, int operation,
				   int ctl_name, int *nrels_mag, long *results);
static void w83792d_fan_tolerance(struct i2c_client *client, int operation,
				  int ctl_name, int *nrels_mag, long *results);
static void w83792d_sf2_points(struct i2c_client *client, int operation,
				int ctl_name, int *nrels_mag, long *results);
static void w83792d_sf2_levels(struct i2c_client *client, int operation,
				int ctl_name, int *nrels_mag, long *results);

static struct i2c_driver w83792d_driver = {
	.name		= "W83792D sensor driver",
	.flags		= I2C_DF_NOTIFY,
	.attach_adapter	= w83792d_attach_adapter,
	.detach_client	= w83792d_detach_client,
};

/* The /proc/sys entries */
/* -- SENSORS SYSCTL START -- */

#define W83792D_SYSCTL_IN0 1000
#define W83792D_SYSCTL_IN1 1001
#define W83792D_SYSCTL_IN2 1002
#define W83792D_SYSCTL_IN3 1003
#define W83792D_SYSCTL_IN4 1004
#define W83792D_SYSCTL_IN5 1005
#define W83792D_SYSCTL_IN6 1006
#define W83792D_SYSCTL_IN7 1007
#define W83792D_SYSCTL_IN8 1008
#define W83792D_SYSCTL_FAN1 1101
#define W83792D_SYSCTL_FAN2 1102
#define W83792D_SYSCTL_FAN3 1103
#define W83792D_SYSCTL_FAN4 1104
#define W83792D_SYSCTL_FAN5 1105
#define W83792D_SYSCTL_FAN6 1106
#define W83792D_SYSCTL_FAN7 1107

#define W83792D_SYSCTL_TEMP1 1200
#define W83792D_SYSCTL_TEMP2 1201
#define W83792D_SYSCTL_TEMP3 1202
/*#define W83792D_SYSCTL_VID 1300	
#define W83792D_SYSCTL_VRM 1301*/
#define W83792D_SYSCTL_PWM_FLAG 1400
#define W83792D_SYSCTL_PWM1 1401
#define W83792D_SYSCTL_PWM2 1402
#define W83792D_SYSCTL_PWM3 1403
#define W83792D_SYSCTL_FAN_CFG 1500	/* control Fan Mode */
#define W83792D_SYSCTL_FAN_DIV 1501
#define W83792D_SYSCTL_CHASSIS 1502	/* control Case Open */

#define W83792D_SYSCTL_THERMAL_CRUISE 1600	/* Smart Fan I: target value */
#define W83792D_SYSCTL_FAN_TOLERANCE 1601	/* Smart Fan I/II: tolerance */
#define W83792D_SYSCTL_SF2_POINTS_FAN1 1602	/* Smart Fan II: Fan1 points */
#define W83792D_SYSCTL_SF2_POINTS_FAN2 1603	/* Smart Fan II: Fan2 points */
#define W83792D_SYSCTL_SF2_POINTS_FAN3 1604	/* Smart Fan II: Fan3 points */
#define W83792D_SYSCTL_SF2_LEVELS_FAN1 1605	/* Smart Fan II: Fan1 levels */
#define W83792D_SYSCTL_SF2_LEVELS_FAN2 1606	/* Smart Fan II: Fan2 levels */
#define W83792D_SYSCTL_SF2_LEVELS_FAN3 1607	/* Smart Fan II: Fan3 levels */

/* -- SENSORS SYSCTL END -- */

/* These files are created for detected chip,
   W83792D has 9 voltages 7 fans and 3 temperatures. */
static ctl_table w83792d_dir_table_template[] =
{
	{W83792D_SYSCTL_IN0, "in0", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83792d_in},
	{W83792D_SYSCTL_IN1, "in1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83792d_in},
	{W83792D_SYSCTL_IN2, "in2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83792d_in},
	{W83792D_SYSCTL_IN3, "in3", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83792d_in},
	{W83792D_SYSCTL_IN4, "in4", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83792d_in},
	{W83792D_SYSCTL_IN5, "in5", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83792d_in},
	{W83792D_SYSCTL_IN6, "in6", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83792d_in},
	{W83792D_SYSCTL_IN7, "in7", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83792d_in},
	{W83792D_SYSCTL_IN8, "in8", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83792d_in},
	{W83792D_SYSCTL_FAN1, "fan1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83792d_fan},
	{W83792D_SYSCTL_FAN2, "fan2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83792d_fan},
	{W83792D_SYSCTL_FAN3, "fan3", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83792d_fan},
	{W83792D_SYSCTL_FAN4, "fan4", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83792d_fan},
	{W83792D_SYSCTL_FAN5, "fan5", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83792d_fan},
	{W83792D_SYSCTL_FAN6, "fan6", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83792d_fan},
	{W83792D_SYSCTL_FAN7, "fan7", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83792d_fan},
	{W83792D_SYSCTL_TEMP1, "temp1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83792d_temp},
	{W83792D_SYSCTL_TEMP2, "temp2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83792d_temp_add},
	{W83792D_SYSCTL_TEMP3, "temp3", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83792d_temp_add},
	/*{W83792D_SYSCTL_VID, "vid", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83792d_vid}, */
	{W83792D_SYSCTL_FAN_DIV, "fan_div", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83792d_fan_div},
	{W83792D_SYSCTL_CHASSIS, "chassis", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &w83792d_chassis},
	{W83792D_SYSCTL_PWM1, "pwm1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83792d_pwm},
	{W83792D_SYSCTL_PWM2, "pwm2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83792d_pwm},
	{W83792D_SYSCTL_PWM3, "pwm3", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83792d_pwm},
	{W83792D_SYSCTL_PWM_FLAG, "pwm_flag", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83792d_pwm_flag},
	{W83792D_SYSCTL_FAN_CFG, "fan_cfg", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83792d_fan_cfg},
	/*{W83792D_SYSCTL_VRM, "vrm", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83792d_vrm},  */
	{W83792D_SYSCTL_THERMAL_CRUISE, "thermal_cruise", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &w83792d_thermal_cruise},
	{W83792D_SYSCTL_FAN_TOLERANCE, "fan_tolerance", NULL, 0, 0644,
	 NULL, &i2c_proc_real, &i2c_sysctl_real, NULL, &w83792d_fan_tolerance},
	{W83792D_SYSCTL_SF2_POINTS_FAN1, "sf2_points_fan1", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &w83792d_sf2_points},
	{W83792D_SYSCTL_SF2_POINTS_FAN2, "sf2_points_fan2", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &w83792d_sf2_points},
	{W83792D_SYSCTL_SF2_POINTS_FAN3, "sf2_points_fan3", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &w83792d_sf2_points},
	{W83792D_SYSCTL_SF2_LEVELS_FAN1, "sf2_levels_fan1", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &w83792d_sf2_levels},
	{W83792D_SYSCTL_SF2_LEVELS_FAN2, "sf2_levels_fan2", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &w83792d_sf2_levels},
	{W83792D_SYSCTL_SF2_LEVELS_FAN3, "sf2_levels_fan3", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &w83792d_sf2_levels},
	{0}
};

/* This function is called when:
     * w83792d_driver is inserted (when this module is loaded), for each
       available adapter
     * when a new adapter is inserted (and w83792d_driver is still present) */
static int w83792d_attach_adapter(struct i2c_adapter *adapter)
{
	int i_tmp;
	ENTER()
	i_tmp = i2c_detect(adapter, &addr_data, w83792d_detect);
	LEAVE()
	return i_tmp;
}

static int w83792d_detect(struct i2c_adapter *adapter, int address,
			  unsigned short flags, int kind)
{
	int i, val1 = 0, val2 = 0, id;
	struct i2c_client *new_client;
	struct w83792d_data *data;
	int err = 0;
	const char *type_name = "";
	const char *client_name = "";

	ENTER()

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		LEAVE()
		goto ERROR0;
	}

	/* OK. For now, we presume we have a valid client. We now create the
	   client structure, even though we cannot fill it completely yet.
	   But it allows us to access w83792d_{read,write}_value. */

	if (!(data = kmalloc(sizeof(struct w83792d_data), GFP_KERNEL))) {
		printk(KERN_ERR "w83792d: Out of memory in w83792d_detect (new_client).\n");
		err = -ENOMEM;
		LEAVE()
		goto ERROR0;
	}

	new_client = &data->client;
	new_client->addr = address;
	init_MUTEX(&data->lock);
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &w83792d_driver;
	new_client->flags = 0;

	/* Now, we do the remaining detection. */
	if (kind < 0) {
		if (w83792d_read_value(new_client, W83792D_REG_CONFIG)&0x80) {
			LEAVE()
			goto ERROR1;
		}
		val1 = w83792d_read_value(new_client, W83792D_REG_BANK);
		val2 = w83792d_read_value(new_client, W83792D_REG_CHIPMAN);
#ifdef W83792D_DEBUG
		printk(KERN_DEBUG "w83792d: val1 is: %d, val2 is: %d\n",
						val1, val2);
#endif
		/* Check for Winbond ID if in bank 0 */
		if (!(val1 & 0x07)) {  /* is Bank0 */
			if (((!(val1 & 0x80)) && (val2 != 0xa3)) ||
			     ((val1 & 0x80) && (val2 != 0x5c))) {
				LEAVE()
				goto ERROR1;
			}
		}
		/* check address at 0x48. */
		if (w83792d_read_value(new_client, W83792D_REG_I2C_ADDR)
				!= address) {
			LEAVE()
			goto ERROR1;
		}
	}

	/* We have either had a force parameter, or we have already detected
	   the Winbond. Put it now into bank 0 and Vendor ID High Byte */
	w83792d_write_value(new_client, W83792D_REG_BANK,
			    (w83792d_read_value(new_client,
						W83792D_REG_BANK) & 0x78) |
			    0x80);

	/* Determine the chip type. */
	if (kind <= 0) {
		/* get vendor ID */
		val2 = w83792d_read_value(new_client, W83792D_REG_CHIPMAN);
		if (val2 != 0x5c) {  /* the vendor is NOT Winbond */
			LEAVE()
			goto ERROR1;
		}
		val1 = w83792d_read_value(new_client, W83792D_REG_WCHIPID);
		if (val1 == 0x7a && address >= 0x2c) {
			kind = w83792d;
		} else {
			if (kind == 0)
				printk(KERN_WARNING "w83792d: Ignoring 'force' parameter for unknown chip at"
				     "adapter %d, address 0x%02x\n",
				     i2c_adapter_id(adapter), address);
			LEAVE()
			goto ERROR1;
		}
	}

	if (kind == w83792d) {
 		type_name = "w83792d";
 		client_name = "W83792D chip";
	} else {
		printk(KERN_ERR "w83792d: Internal error: unknown kind (%d)?!?",
		       kind);
		LEAVE()
		goto ERROR1;
	}

	/* Fill in the remaining client fields and put it into the global list */
	strcpy(new_client->name, client_name);
	data->type = kind;
	data->valid = 0;
	init_MUTEX(&data->update_lock);

	/* Tell the I2C layer a new client has arrived */
	if ((err = i2c_attach_client(new_client))) {
		LEAVE()
		goto ERROR1;
	}

	/* attach secondary i2c lm75-like clients */
	if (!(data->lm75 = kmalloc(2 * sizeof(struct i2c_client),
				   GFP_KERNEL))) {
		err = -ENOMEM;
		goto ERROR2;
	}
	id = i2c_adapter_id(adapter);
	if(force_subclients[0] == id && force_subclients[1] == address) {
		if(force_subclients[2] < 0x48 || force_subclients[2] > 0x4b) {
			printk(KERN_ERR "w83792d.o: Invalid subclient address %d; must be 0x48-0x4b\n",
			        force_subclients[2]);
			goto ERROR5;
		}
		if(force_subclients[3] < 0x4c || force_subclients[3] > 0x4f) {
			printk(KERN_ERR "w83792d.o: Invalid subclient address %d; must be 0x4c-0x4f\n",
			        force_subclients[3]);
			goto ERROR5;
		}
		w83792d_write_value(new_client,
		                    W83792D_REG_I2C_SUBADDR,
		                    0x40 | (force_subclients[2] & 0x03) |
		                    ((force_subclients[3] & 0x03) <<4));
		data->lm75[0].addr = force_subclients[2];
		data->lm75[1].addr = force_subclients[3];
	} else {
		val1 = w83792d_read_value(new_client,
				          W83792D_REG_I2C_SUBADDR);
		data->lm75[0].addr = 0x48 + (val1 & 0x07);
		data->lm75[1].addr = 0x48 + ((val1 >> 4) & 0x07);
		if (data->lm75[0].addr == data->lm75[1].addr)
			printk(KERN_WARNING "w83792d: Subclients have the same "
			       "address (0x%02x)! Use force_subclients.\n",
			       data->lm75[0].addr);
	}
	client_name = "W83792D subclient";


	for (i = 0; i <= 1; i++) {
		data->lm75[i].data = NULL;	/* store all data in w83792d */
		data->lm75[i].adapter = adapter;
		data->lm75[i].driver = &w83792d_driver;
		data->lm75[i].flags = 0;
		strcpy(data->lm75[i].name, client_name);
		if ((err = i2c_attach_client(&(data->lm75[i])))) {
			printk(KERN_ERR "w83792d.o: Subclient %d registration at address 0x%x failed.\n",
			       i, data->lm75[i].addr);
			if (i == 1)
				goto ERROR6;
			goto ERROR5;
		}
	}

	/* Register a new directory entry with module sensors */
	if ((i = i2c_register_entry(new_client, type_name,
				    w83792d_dir_table_template, THIS_MODULE)) < 0) {
		err = i;
		goto ERROR7;
	}
	data->sysctl_id = i;

	/* Initialize the chip */
	w83792d_init_client(new_client);
	LEAVE()
	return 0;

      ERROR7:
	i2c_detach_client(&
			  (((struct
			     w83792d_data *) (new_client->data))->
			   lm75[1]));
      ERROR6:
	i2c_detach_client(&
			  (((struct
			     w83792d_data *) (new_client->data))->
			   lm75[0]));
      ERROR5:
	kfree(((struct w83792d_data *) (new_client->data))->lm75);
      ERROR2:
	i2c_detach_client(new_client);
      ERROR1:
	kfree(data);
      ERROR0:

	LEAVE()
	return err;
}

static int w83792d_detach_client(struct i2c_client *client)
{
	int err;
	struct w83792d_data *data = client->data;
	ENTER()

	i2c_deregister_entry(data->sysctl_id);

	if ((err = i2c_detach_client(client))) {
		printk(KERN_ERR "w83792d: Client deregistration failed, client not detached.\n");
		LEAVE()
		return err;
	}
	i2c_detach_client(&(data->lm75[0]));
	i2c_detach_client(&(data->lm75[1]));
	kfree(data->lm75);
	kfree(data);

	LEAVE()
	return 0;
}

/* Read the w83792d register value, only use bank 0 of the 792 chip */
static int w83792d_read_value(struct i2c_client *client, u8 reg)
{
	int res = 0;
	res = i2c_smbus_read_byte_data(client, reg);
	return res;
}

/* Write value into the w83792d registers, only use bank 0 of the 792 chip */
static int w83792d_write_value(struct i2c_client *client, u8 reg, u8 value)
{
	i2c_smbus_write_byte_data(client, reg, value);
	return 0;
}

/* Called when we have found a new W83792D. It should set limits, etc. */
static void w83792d_init_client(struct i2c_client *client)
{
	int temp2_cfg, temp3_cfg, i;
	u8 vid_in_b;

	ENTER()

	if (init) {
		w83792d_write_value(client, W83792D_REG_CONFIG, 0x80);
	}
	/* data->vrm = 90; */ /* maybe need to be modified! */

	/* Clear the bit6 of W83792D_REG_VID_IN_B(set it into 0):
	   W83792D_REG_VID_IN_B bit6 = 0: the high/low limit of
	     vin0/vin1 can be modified by user;
	   W83792D_REG_VID_IN_B bit6 = 1: the high/low limit of
	     vin0/vin1 auto-updated, can NOT be modified by user. */
	vid_in_b = w83792d_read_value(client, W83792D_REG_VID_IN_B);
	w83792d_write_value(client, W83792D_REG_VID_IN_B,
			    vid_in_b & 0xbf);

	temp2_cfg = w83792d_read_value(client, W83792D_REG_TEMP_ADD[0][6]);
	temp3_cfg = w83792d_read_value(client, W83792D_REG_TEMP_ADD[1][6]);
	w83792d_write_value(client, W83792D_REG_TEMP_ADD[0][6],
			    temp2_cfg & 0xe6);
	w83792d_write_value(client, W83792D_REG_TEMP_ADD[1][6],
			    temp3_cfg & 0xe6);

	/* enable comparator mode for temp2 and temp3 so
	   alarm indication will work correctly */
	i = w83792d_read_value(client, W83792D_REG_IRQ);
	if (!(i & 0x40))
		w83792d_write_value(client, W83792D_REG_IRQ, i|0x40);

	/* Start monitoring */
	w83792d_write_value(client, W83792D_REG_CONFIG, (w83792d_read_value(
				client,	W83792D_REG_CONFIG) & 0xf7) | 0x01);
	LEAVE()
}

static void w83792d_update_client(struct i2c_client *client)
{
	struct w83792d_data *data = client->data;
	int i, j;
	u8 reg_array_tmp[4], pwm_array_tmp[7], reg_tmp;

	down(&data->update_lock);

	if (time_after(jiffies - data->last_updated, HZ * 3) ||
	    time_before(jiffies, data->last_updated) || !data->valid) {
		pr_debug(KERN_DEBUG "Starting device update\n");

		/* Update the voltages measured value and limits */
		for (i = 0; i < 9; i++) {
			data->in[i] = w83792d_read_value(client,
						W83792D_REG_IN[i]);
			data->in_max[i] = w83792d_read_value(client,
						W83792D_REG_IN_MAX[i]);
			data->in_min[i] = w83792d_read_value(client,
						W83792D_REG_IN_MIN[i]);
		}
		data->low_bits[0] = w83792d_read_value(client,
						W83792D_REG_LOW_BITS1);
		data->low_bits[1] = w83792d_read_value(client,
						W83792D_REG_LOW_BITS2);

		for (i = 0; i < 7; i++) {
			/* Update the Fan measured value and limits */
			data->fan[i] = w83792d_read_value(client,
						W83792D_REG_FAN[i]);
			data->fan_min[i] = w83792d_read_value(client,
						W83792D_REG_FAN_MIN[i]);
			/* Update the PWM/DC Value and PWM/DC flag */
			pwm_array_tmp[i] = w83792d_read_value(client,
						W83792D_REG_PWM[i]);
			data->pwm[i] = pwm_array_tmp[i] & 0x0f;
			data->pwm_flag[1] = (pwm_array_tmp[i] >> 7) & 0x01;
		}
		data->fan_cfg = w83792d_read_value(client, W83792D_REG_FAN_CFG);

		/* Update the Fan Divisor */
		for (i = 0; i < 4; i++) {
			reg_array_tmp[i] = w83792d_read_value(client, W83792D_REG_FAN_DIV[i]);
		}
		data->fan_div[0] = reg_array_tmp[0] & 0x07;
		data->fan_div[1] = (reg_array_tmp[0] >> 4) & 0x07;
		data->fan_div[2] = reg_array_tmp[1] & 0x07;
		data->fan_div[3] = (reg_array_tmp[1] >> 4) & 0x07;
		data->fan_div[4] = reg_array_tmp[2] & 0x07;
		data->fan_div[5] = (reg_array_tmp[2] >> 4) & 0x07;
		data->fan_div[6] = reg_array_tmp[3] & 0x07;

		/* Update the Temperature1 measured value and limits */
		data->temp1[0] = w83792d_read_value(client, W83792D_REG_TEMP1);
		data->temp1[1] = w83792d_read_value(client, W83792D_REG_TEMP1_OVER);
		data->temp1[2] = w83792d_read_value(client, W83792D_REG_TEMP1_HYST);

		/* Update the Temperature2/3 measured value and limits */
		for (i = 0; i < 7; i++) {
			data->temp_add[0][i] = w83792d_read_value(client,
						W83792D_REG_TEMP_ADD[0][i]);
			data->temp_add[1][i] = w83792d_read_value(client,
						W83792D_REG_TEMP_ADD[1][i]);
		}

		/* Update the VID */
		/* i = w83792d_read_value(client, W83792D_REG_FAN_DIV[0]);
		data->vid = i & 0x0f;
		data->vid |=
		    (w83792d_read_value(client, W83792D_REG_CHIPID) & 0x01)
		    << 4;   */

		/* Update CaseOpen status and it's CLR_CHS. */
		data->chassis[0] = (w83792d_read_value(client, 
					W83792D_REG_CASE_OPEN)
				    >> 5) & 0x01;
		data->chassis[1] = (w83792d_read_value(client,
					W83792D_REG_CASE_OPEN_CLR)
				    >> 7) & 0x01;

		/* Update Thermal Cruise/Smart Fan I target value */
		for (i = 0; i < 3; i++) {
			data->thermal_cruise[i] =
				w83792d_read_value(client,
					W83792D_REG_THERMAL[i]) & 0x7f;
		}

		/* Update Smart Fan I/II tolerance */
		reg_tmp = w83792d_read_value(client, W83792D_REG_FAN_TOL[0]);
		data->fan_tolerance[0] = reg_tmp & 0x0f;
		data->fan_tolerance[1] = (reg_tmp >> 4) & 0x0f;
		data->fan_tolerance[2] =
		    w83792d_read_value(client, W83792D_REG_FAN_TOL[2]) & 0x0f;

		/* Update Smart Fan II temperature points */
		for (i = 0; i < 3; i++) {
			for (j = 0; j < 4; j++) {
				data->sf2_points[i][j] = w83792d_read_value(
					client,W83792D_REG_POINTS[i][j]) & 0x7f;
			}
		}

		/* Update Smart Fan II duty cycle levels */
		for (i = 0; i < 3; i++) {
			reg_tmp = w83792d_read_value(client,
						W83792D_REG_LEVELS[i][0]);
			data->sf2_levels[i][0] = reg_tmp & 0x0f;
			data->sf2_levels[i][1] = (reg_tmp >> 4) & 0x0f;
			reg_tmp = w83792d_read_value(client,
						W83792D_REG_LEVELS[i][2]);
			data->sf2_levels[i][2] = (reg_tmp >> 4) & 0x0f;
			data->sf2_levels[i][3] = reg_tmp & 0x0f;
		}
		data->last_updated = jiffies;
		data->valid = 1;
#ifdef W83792D_DEBUG
		w83792d_print_debug(data);
#endif
	}
	up(&data->update_lock);
}

/* This is a function used to debug the message. */
#ifdef W83792D_DEBUG
static void w83792d_print_debug(struct w83792d_data *data)
{
	int i=0, j=0;
	printk(KERN_DEBUG "==========The following is the debug message...========\n");
	printk(KERN_DEBUG "9 set of Voltages: =====>\n");
	for (i=0; i<=8; i++) {
		printk(KERN_DEBUG "vin[%d] is: 0x%x\n", i, data->in[i]);
		printk(KERN_DEBUG "vin[%d] max is: 0x%x\n", i, data->in_max[i]);
		printk(KERN_DEBUG "vin[%d] min is: 0x%x\n", i, data->in_min[i]);
	}
	printk(KERN_DEBUG "Low Bit1 is: 0x%x\n", data->low_bits[0]);
	printk(KERN_DEBUG "Low Bit2 is: 0x%x\n", data->low_bits[1]);
	printk(KERN_DEBUG "7 set of Fan Counts and 3 set of Duty Cycles: =====>\n");
	printk(KERN_DEBUG "fan_cfg is: 0x%x\n", data->fan_cfg);
	for (i=0; i<=6; i++) {
		printk(KERN_DEBUG "fan[%d] is: 0x%x\n", i, data->fan[i]);
		printk(KERN_DEBUG "fan[%d] min is: 0x%x\n", i, data->fan_min[i]);
		if (i<3) {
			printk(KERN_DEBUG "pwm[%d]     is: 0x%x\n", i, data->pwm[i]);
			printk(KERN_DEBUG "pwm_flag[%d] is: 0x%x\n", i, data->pwm_flag[i]);
		}
	}
	printk(KERN_DEBUG "3 set of Temperatures: =====>\n");
	printk(KERN_DEBUG "temp1 is: 0x%x\n", data->temp1[0]);
	printk(KERN_DEBUG "temp1 high limit is: 0x%x\n", data->temp1[1]);
	printk(KERN_DEBUG "temp1 low limit is: 0x%x\n", data->temp1[2]);
	for (i=0; i<2; i++) {
		for (j=0; j<7; j++) {
			printk(KERN_DEBUG "temp_add[%d][%d] is: 0x%x\n", i, j,
					data->temp_add[i][j]);
		}
	}
	for (i=0; i<=6; i++) {
		printk(KERN_DEBUG "fan_div[%d] is: 0x%x\n", i, data->fan_div[i]);
	}
	printk(KERN_DEBUG "==========End of the debug message...==================\n\n");
}
#endif

/* The next few functions are the call-back functions of the /proc/sys and
   sysctl files. Which function is used is defined in the ctl_table in
   the extra1 field.
   Each function must return the magnitude (power of 10 to divide the date
   with) if it is called with operation==SENSORS_PROC_REAL_INFO. It must
   put a maximum of *nrels elements in results reflecting the data of this
   file, and set *nrels to the number it actually put in it, if operation==
   SENSORS_PROC_REAL_READ. Finally, it must get upto *nrels elements from
   results and write them to the chip, if operations==SENSORS_PROC_REAL_WRITE.
   Note that on SENSORS_PROC_REAL_READ, I do not check whether results is
   large enough (by checking the incoming value of *nrels). This is not very
   good practice, but as long as you put less than about 5 values in results,
   you can assume it is large enough. */

/* read/write voltage meaured value and limits */
static void w83792d_in(struct i2c_client *client, int operation, int ctl_name,
			int *nrels_mag, long *results)
{
	struct w83792d_data *data = client->data;
	int nr = ctl_name - W83792D_SYSCTL_IN0;
	u16 vol_max_tmp = 0;
	u16 vol_min_tmp = 0;
	u16 vol_count = 0;
	u16 low_bits = 0;

	/* result[0]: low limit, result[1]: high limit,
	   result[2]: measured value */
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 3;
	else if (operation == SENSORS_PROC_REAL_READ) {
		w83792d_update_client(client);
		/* Read High/Low limit. */
		vol_min_tmp = data->in_min[nr];
		vol_max_tmp = data->in_max[nr];
		results[0] = IN_FROM_REG(nr, vol_min_tmp*4);
		results[1] = IN_FROM_REG(nr, vol_max_tmp*4);

		/* Read voltage measured value. */
		vol_count = data->in[nr];
		vol_count = (vol_count << 2);
		low_bits = 0;
		switch (nr)
		{
		case 0:  /* vin0 */
			low_bits = (data->low_bits[0]) & 0x03;
			break;
		case 1:  /* vin1 */
			low_bits = ((data->low_bits[0]) & 0x0c) >> 2;
			break;
		case 2:  /* vin2 */
			low_bits = ((data->low_bits[0]) & 0x30) >> 4;
			break;
		case 3:  /* vin3 */
			low_bits = ((data->low_bits[0]) & 0xc0) >> 6;
			break;
		case 4:  /* vin4 */
			low_bits = (data->low_bits[1]) & 0x03;
			break;
		case 5:  /* vin5 */
			low_bits = ((data->low_bits[1]) & 0x0c) >> 2;
			break;
		case 6:  /* vin6 */
			low_bits = ((data->low_bits[1]) & 0x30) >> 4;
		default:
			break;
		}
		vol_count = vol_count | low_bits;
		results[2] = IN_FROM_REG(nr, vol_count);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			/* Write Low limit into register. */
			data->in_min[nr] = SENSORS_LIMIT(IN_TO_REG(nr,results[0])/4,
							0, 255);
			w83792d_write_value(client, W83792D_REG_IN_MIN[nr],
							data->in_min[nr]);
		}
		if (*nrels_mag >= 2) {
			/* Write High limit into register. */
			data->in_max[nr] = SENSORS_LIMIT(IN_TO_REG(nr,results[1])/4,
							0, 255);
			w83792d_write_value(client, W83792D_REG_IN_MAX[nr],
							data->in_max[nr]);
		}
	}
}

/* read/write fan meaured value and limits */
void w83792d_fan(struct i2c_client *client, int operation, int ctl_name,
		 int *nrels_mag, long *results)
{
	struct w83792d_data *data = client->data;
	int nr = ctl_name - W83792D_SYSCTL_FAN1;
	u8 tmp_reg, tmp_fan_div;

	/* result[0]: low limit, result[1]: measured value */
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		w83792d_update_client(client);
		results[0] = FAN_FROM_REG(data->fan_min[nr],
				  DIV_FROM_REG(data->fan_div[nr]));
		/* adjust Fan Divisor, then change RPM */
		do {
			w83792d_update_client(client);
			if ((data->fan[nr]>0x50) && (data->fan[nr]<0xff)) {
			/* optimal case. 0x50 and 0xff are experience data */
				results[1] = FAN_FROM_REG(data->fan[nr],
						DIV_FROM_REG(data->fan_div[nr]));
				break; /* go out of the do-while loop. */
			} else {
				if (((data->fan_div[nr])>=0x07 &&
					(data->fan[nr])==0xff) ||
				    ((data->fan_div[nr])<=0 &&
					(data->fan[nr])<0x78)) {
					results[1] = 0;
					break;
				} else if ((data->fan_div[nr])<0x07 &&
					 (data->fan[nr])==0xff) {
					(data->fan_div[nr])++;
					results[1] = FAN_FROM_REG(data->fan[nr],
					             DIV_FROM_REG(data->fan_div[nr]));
				} else if ((data->fan_div[nr])>0 &&
					  (data->fan[nr])<0x78) {
					(data->fan_div[nr])--;
					results[1] = FAN_FROM_REG(data->fan[nr],
					             DIV_FROM_REG(data->fan_div[nr]));
				}

				tmp_reg = w83792d_read_value(client,
						W83792D_REG_FAN_DIV[nr/2]);
				tmp_reg &= (nr%2 == 0) ? 0xf8 : 0x8f;
				tmp_fan_div = (nr%2 == 0) ? (data->fan_div[nr])
					: (((data->fan_div[nr])<<4)&0x70);
				w83792d_write_value(client,
						    W83792D_REG_FAN_DIV[nr/2],
						    tmp_reg|tmp_fan_div);
			}
		} while (0);
		*nrels_mag = 2;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->fan_min[nr] = FAN_TO_REG(results[0],
					    DIV_FROM_REG(data->fan_div[nr]));
			w83792d_write_value(client,
					     W83792D_REG_FAN_MIN[nr],
					     data->fan_min[nr]);
		}
	}
}

/* read/write temperature1 meaured value and limits */
void w83792d_temp(struct i2c_client *client, int operation, int ctl_name,
		  int *nrels_mag, long *results)
{
	struct w83792d_data *data = client->data;

	/* result[0]: high limit, result[1]: low limit
	   result[2]: measured value, the order is different with voltage(in) */
	if (operation == SENSORS_PROC_REAL_INFO) {
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_READ) {
		w83792d_update_client(client);
		results[0] = TEMP_FROM_REG(data->temp1[1]);
		results[1] = TEMP_FROM_REG(data->temp1[2]);
		results[2] = TEMP_FROM_REG(data->temp1[0]);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->temp1[1] = TEMP_TO_REG(results[0]);
			w83792d_write_value(client, W83792D_REG_TEMP1_OVER,
					    data->temp1[1]);
		}
		if (*nrels_mag >= 2) {
			data->temp1[2] = TEMP_TO_REG(results[1]);
			w83792d_write_value(client, W83792D_REG_TEMP1_HYST,
					    data->temp1[2]);
		}
	}
}

/* read/write temperature2,3 meaured value and limits */
void w83792d_temp_add(struct i2c_client *client, int operation,
		      int ctl_name, int *nrels_mag, long *results)
{
	struct w83792d_data *data = client->data;
	int nr = ctl_name - W83792D_SYSCTL_TEMP2;
	int i=0, j=0;

	/* result[0]: high limit, result[1]: low limit
	   result[2]: measured value, the order is different with voltage(in) */
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 1;
	else if (operation == SENSORS_PROC_REAL_READ) {
		w83792d_update_client(client);
		for (i=0; i<3; i++) {
			j = (i==0) ? 2 : ((i==1)?0:1);
			if (((data->temp_add[nr][i*2+1]) && 0x80) == 0) {
				results[j] = TEMP_FROM_REG(data->temp_add[nr][i*2]);
			} else {
				results[j] = TEMP_FROM_REG(data->temp_add[nr][i*2]) + 5;
			}
		}
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		data->temp_add[nr][2] = TEMP_TO_REG(results[0]);
		data->temp_add[nr][4] = TEMP_TO_REG(results[1]);
		w83792d_write_value(client,
				     W83792D_REG_TEMP_ADD[nr][2],
				     data->temp_add[nr][2]);
		w83792d_write_value(client,
				     W83792D_REG_TEMP_ADD[nr][4],
				     data->temp_add[nr][4]);
		if ((results[0]%10) == 0) {
			w83792d_write_value(client,
				W83792D_REG_TEMP_ADD[nr][3],
				w83792d_read_value(client,
					W83792D_REG_TEMP_ADD[nr][3])&0x7f);
		} else { /* consider the 0.5 degree */
			w83792d_write_value(client,
				W83792D_REG_TEMP_ADD[nr][3],
				w83792d_read_value(client,
					W83792D_REG_TEMP_ADD[nr][3])|0x80);
		}
		if ((results[1]%10) == 0) {
			w83792d_write_value(client,
				W83792D_REG_TEMP_ADD[nr][5],
				w83792d_read_value(client,
					W83792D_REG_TEMP_ADD[nr][5])&0x7f);
		} else { /* consider the 0.5 degree */
			w83792d_write_value(client,
				W83792D_REG_TEMP_ADD[nr][5],
				w83792d_read_value(client,
					W83792D_REG_TEMP_ADD[nr][5])|0x80);
		}
	}
}

/*
void w83792d_vid(struct i2c_client *client, int operation, int ctl_name,
		 int *nrels_mag, long *results)
{
	struct w83792d_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 3;
	else if (operation == SENSORS_PROC_REAL_READ) {
		w83792d_update_client(client);
		results[0] = vid_from_reg(data->vid, data->vrm);
		*nrels_mag = 1;
	}
}

void w83792d_vrm(struct i2c_client *client, int operation, int ctl_name,
		 int *nrels_mag, long *results)
{
	struct w83792d_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 1;
	else if (operation == SENSORS_PROC_REAL_READ) {
		results[0] = data->vrm;
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->vrm = results[0];
		}
	}
} */

/* Read/Write Fan Divisor */
void w83792d_fan_div(struct i2c_client *client, int operation,
		     int ctl_name, int *nrels_mag, long *results)
{
	struct w83792d_data *data = client->data;
	int i=0, j=0;
	u8 temp_reg=0, k=1, fan_div_reg=0;
	u8 tmp_fan_div;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		w83792d_update_client(client);
		for (i=0; i<7; i++) {
			results[i] = DIV_FROM_REG(data->fan_div[i]);
		}
		*nrels_mag = 7;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag < 7) {
			return;
		}
		for (i=0; i<7; i++) {
			temp_reg = SENSORS_LIMIT(results[i], 1, 128);
			for (k=0,j=0; j<7; j++) {
				temp_reg = temp_reg>>1;
				if (temp_reg == 0)
					break;
				k++;
			}
			fan_div_reg = w83792d_read_value(client,
					W83792D_REG_FAN_DIV[i/2]);
			fan_div_reg &= (i%2 == 0) ? 0xf8 : 0x8f;
			tmp_fan_div = (i%2 == 0) ? (k&0x07)
					: ((k<<4)&0x70);
			w83792d_write_value(client,
					W83792D_REG_FAN_DIV[i/2],
					fan_div_reg|tmp_fan_div);
		}
	}
}


/* Under Smart Fan I mode: read/write the Fan1/2/3 target temperature */
void w83792d_thermal_cruise(struct i2c_client *client, int operation,
			    int ctl_name, int *nrels_mag, long *results)
{
	struct w83792d_data *data = client->data;
	int i=0;
	u8 target_tmp=0, target_mask=0;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		w83792d_update_client(client);
		for (i=0; i<3; i++) {
			results[i] = data->thermal_cruise[i];
		}
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		for (i=0; i<3; i++) {
			if (*nrels_mag < (i+1)) {
				return;
			}
			target_tmp = results[i];
			target_tmp = target_tmp & 0x7f;
			target_mask = w83792d_read_value(client,
						W83792D_REG_THERMAL[i]) & 0x80;
			data->thermal_cruise[i] = SENSORS_LIMIT(target_tmp, 0, 255);
			w83792d_write_value(client, W83792D_REG_THERMAL[i],
					     (data->thermal_cruise[i])|target_mask);
		}
	}
}

/* The tolerance of fan1/fan2/fan3, when using Thermal Cruise(Smart Fan I)
   or Smart Fan II mode. */
void w83792d_fan_tolerance(struct i2c_client *client, int operation,
				 int ctl_name, int *nrels_mag, long *results)
{
	struct w83792d_data *data = client->data;
	int i=0;
	u8 tol_tmp, tol_mask;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		w83792d_update_client(client);
		for (i=0; i<3; i++) {
			results[i] = data->fan_tolerance[i];
		}
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		for (i=0; i<3; i++) {
			if (*nrels_mag < (i+1)) {
				return;
			}
			tol_mask = w83792d_read_value(client,
				W83792D_REG_FAN_TOL[i]) & ((i==1)?0x0f:0xf0);
			tol_tmp = SENSORS_LIMIT(results[0], 0, 15);
			tol_tmp &= 0x0f;
			data->fan_tolerance[i] = tol_tmp;
			if (i==1) {
				tol_tmp <<= 4;
			}
			w83792d_write_value(client, W83792D_REG_FAN_TOL[i],
					     tol_mask|tol_tmp);
		}
	}
}

/* Under Smart Fan II mode: read/write the Fan1/2/3 temperature points */
void w83792d_sf2_points(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results)
{
	struct w83792d_data *data = client->data;
	int nr = ctl_name - W83792D_SYSCTL_SF2_POINTS_FAN1;
	int j=0;
	u8 mask_tmp = 0;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		w83792d_update_client(client);
		for (j=0; j<4; j++) {
			results[j] = data->sf2_points[nr][j];
		}
		*nrels_mag = 4;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		for (j=0; j<4; j++) {
			if (*nrels_mag < (j+1)) {
				return;
			}
			data->sf2_points[nr][j] = SENSORS_LIMIT(results[j],
							0, 127);
			mask_tmp = w83792d_read_value(client,
					W83792D_REG_POINTS[nr][j]) & 0x80;
			w83792d_write_value(client, W83792D_REG_POINTS[nr][j],
					mask_tmp|data->sf2_points[nr][j]);
		}
	}
}

/* Smart Fan II Duty Cycle1/2/3 of Fan1/2/3.
   Notice that: The Non-Stop can NOT be modified by user,
   because it is related with some physical characters, 
   usually set by BIOS. User's modification to it may lead to
   Fan's stop, then bring danger. */
void w83792d_sf2_levels(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results)
{
	struct w83792d_data *data = client->data;
	int nr = ctl_name - W83792D_SYSCTL_SF2_LEVELS_FAN1;
	int j = 0;
	u8 mask_tmp = 0, level_tmp = 0;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		w83792d_update_client(client);
		for (j=0; j<4; j++) {
			results[j] = (data->sf2_levels[nr][j] * 100) / 15;
		}
		*nrels_mag = 4;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		for (j=1; j<4; j++) {  /* start with 1: need ignore Non-Stop */
			if (*nrels_mag < j) {
				return;
			}
			data->sf2_levels[nr][j] =
				SENSORS_LIMIT((results[j]*15)/100, 0, 15);
			mask_tmp = w83792d_read_value(client, W83792D_REG_LEVELS[nr][j])
						& ((j==3) ? 0xf0 : 0x0f);
			if (j==3) {
				level_tmp = data->sf2_levels[nr][j];
			} else {
				level_tmp = data->sf2_levels[nr][j] << 4;
			}
			w83792d_write_value(client, W83792D_REG_LEVELS[nr][j],
						level_tmp | mask_tmp);
		}
	}
}

/* Read/Write Chassis status and Reset Chassis. */
void w83792d_chassis(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results)
{
	struct w83792d_data *data = client->data;
	u8 temp1 = 0, temp2 = 0;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		w83792d_update_client(client);
		results[0] = data->chassis[0];
		results[1] = data->chassis[1];
		*nrels_mag = 2;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		data->chassis[1] = SENSORS_LIMIT(results[1], 0 ,1);
		temp1 = ((data->chassis[1]) << 7) & 0x80;
		temp2 = w83792d_read_value(client,
				W83792D_REG_CASE_OPEN_CLR) & 0x7f;
		w83792d_write_value(client,
				W83792D_REG_CASE_OPEN_CLR,
				temp1|temp2);
	}
}

/* Read/Write PWM/DC value of Fan1,Fan2,Fan3, which controls the
   Fan Duty Cycle */
void w83792d_pwm(struct i2c_client *client, int operation, int ctl_name,
		 int *nrels_mag, long *results)
{
	struct w83792d_data *data = client->data;
	int nr = ctl_name - W83792D_SYSCTL_PWM1;
	u8 pwm_to_reg = 0, pwm_mask;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		w83792d_update_client(client);
		results[0] = data->pwm[nr];
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		pwm_to_reg = SENSORS_LIMIT(results[0], 0, 15);
		data->pwm[nr] = pwm_to_reg;
		pwm_mask = w83792d_read_value(client,W83792D_REG_PWM[nr]) & 0xf0;
		w83792d_write_value(client,W83792D_REG_PWM[nr],pwm_mask|data->pwm[nr]);
	}
}

/* Read/Write PWM/DC mode for Fan1,Fan2,Fan3:
   1->PWM mode, 0->DC mode */
void w83792d_pwm_flag(struct i2c_client *client, int operation, int ctl_name,
		      int *nrels_mag, long *results)
{
	struct w83792d_data *data = client->data;
	int i = 0;
	u8 pwm_flag_mask;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		w83792d_update_client(client);
		for (i=0; i<3; i++) {
			results[i] = data->pwm_flag[i];
		}
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		for (i=0; i<3; i++) {
			if (*nrels_mag < (i+1)) {
				return;
			}
			data->pwm_flag[i] = SENSORS_LIMIT(results[i], 0, 1);
			pwm_flag_mask = w83792d_read_value(client,
						W83792D_REG_PWM[i]) & 0x7f;
			w83792d_write_value(client, W83792D_REG_PWM[i],
					((data->pwm_flag[i])<<7)|pwm_flag_mask);
		}
	}
}

/* Read/Write Fan mode into:PWM/DC, Thermal Cruise(SmartFanI), SmartFanII
   0->PWM/DC mode, 1->Thermal Cruise mode, 2/3->SmartFanII mode */
void w83792d_fan_cfg(struct i2c_client *client, int operation,
		     int ctl_name, int *nrels_mag, long *results)
{
	struct w83792d_data *data = client->data;
	u8 temp_cfg1, temp_cfg2, temp_cfg3, temp_cfg4;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		w83792d_update_client(client);
		results[0] = (data->fan_cfg) & 0x03;      /* Fan1's Mode */
		results[1] = ((data->fan_cfg)>>2) & 0x03; /* Fan2's Mode */
		results[2] = ((data->fan_cfg)>>4) & 0x03; /* Fan3's Mode */
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag < 3) {
			return;
		}
		temp_cfg1 = SENSORS_LIMIT(results[0], 0, 3);
		temp_cfg2 = SENSORS_LIMIT(results[1], 0, 3) << 2;
		temp_cfg3 = SENSORS_LIMIT(results[2], 0, 3) << 4;
		temp_cfg4 = w83792d_read_value(client,W83792D_REG_FAN_CFG) & 0xc0;
		data->fan_cfg = ((temp_cfg4|temp_cfg3)|temp_cfg2)|temp_cfg1;
		w83792d_write_value(client,W83792D_REG_FAN_CFG,data->fan_cfg);
	}
}

static int __init sm_w83792d_init(void)
{
	ENTER()

	printk(KERN_INFO "w83792d version %s (%s)\n", LM_VERSION, LM_DATE);

	LEAVE()
	return i2c_add_driver(&w83792d_driver);
}

static void __exit sm_w83792d_exit(void)
{
	ENTER()

	i2c_del_driver(&w83792d_driver);

	LEAVE()
}


MODULE_AUTHOR("Chunhao Huang @ Winbond");
MODULE_DESCRIPTION("W83792AD/D driver for linux-2.4");
MODULE_LICENSE("GPL");

module_init(sm_w83792d_init);
module_exit(sm_w83792d_exit);

