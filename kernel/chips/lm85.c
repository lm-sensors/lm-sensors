/*
    lm85.c - Part of lm_sensors, Linux kernel modules for hardware
             monitoring
    Copyright (c) 1998, 1999  Frodo Looijaard <frodol@dds.nl> 
    Copyright (c) 2002, 2003  Philip Pokorny <ppokorny@penguincomputing.com>
    Copyright (c) 2003        Margit Schubert-While <margitsw@t-online.de>

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

    CHANGELOG

    2002-11-13   First patch for LM85 functionality
    2002-11-18   LM85 functionality mostly done
    2002-12-02   Adding ADM1027 functionality
    2002-12-06   Adding ADT7463 functionality
    2003-01-09   Code cleanup.
                 Save reserved bits in case they are implemented
                    in a future chip.  (Solve problem with lockups
                    on ADM1027 due to chip initialization)
                 Added chip initialization bypass option
    2003-02-12   Add THERM asserted counts for ADT7463
                 Added #ifdef so we can compile against 2.6.5
                    without updating i2c-ids.h
    2003-02-17   Prepare for switch to 2.7.0 development
                 Implement tmin_control for ADT7463
                 Expose THERM asserted counts to /proc
                 Code cleanup
    2003-02-19   Working with Margit and LM_SENSORS developers
    2003-02-23   Removed chip initialization entirely
                 Scale voltages in driver at Margit's request
                 Change PWM from 0-100% to 0-255 per LM sensors standard
    2003-02-27   Documentation and code cleanups
                 Added this CHANGELOG
                 Print additional precision for temperatures and voltages
                 Many thanks to Margit Schubert-While and Brandt xxxxxx
                    for help testing this version
    2003-02-28   More diagnostic messages regarding BIOS setup
    2003-03-01   Added Interrupt mask register support.
    2003-03-08   Fixed problem with pseudo 16-bit registers
                 Cleaned up some compiler warnings.
                 Fixed problem with Operating Point and THERM counting
    2003-03-21   Initial support for EMC6D100 and EMC6D101 chips
    2003-06-30   Add support for EMC6D100 extra voltage inputs.
*/

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/i2c-proc.h>
#include <linux/init.h>
#include "version.h"
#include "sensors_vid.h"

#ifndef I2C_DRIVERID_LM85
#define I2C_DRIVERID_LM85  1039
#endif

/* Addresses to scan */
static unsigned short normal_i2c[] = { 0x2c, 0x2d, 0x2e, SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { SENSORS_I2C_END };
static unsigned int normal_isa[] = { SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/* Insmod parameters */
SENSORS_INSMOD_6(lm85b, lm85c, adm1027, adt7463, emc6d100, emc6d102);

/* Many LM85 constants specified below */

/* The LM85 registers */
#define LM85_REG_IN(nr) (0x20 + (nr))
#define LM85_REG_IN_MIN(nr) (0x44 + (nr) * 2)
#define LM85_REG_IN_MAX(nr) (0x45 + (nr) * 2)

#define LM85_REG_TEMP(nr) (0x25 + (nr))
#define LM85_REG_TEMP_MIN(nr) (0x4e + (nr) * 2)
#define LM85_REG_TEMP_MAX(nr) (0x4f + (nr) * 2)

/* Fan speeds are LSB, MSB (2 bytes) */
#define LM85_REG_FAN(nr) (0x28 + (nr) *2)
#define LM85_REG_FAN_MIN(nr) (0x54 + (nr) *2)

#define LM85_REG_PWM(nr) (0x30 + (nr))

#define ADT7463_REG_OPPOINT(nr) (0x33 + (nr))

#define ADT7463_REG_TMIN_CTL1 0x36
#define ADT7463_REG_TMIN_CTL2 0x37
#define ADT7463_REG_TMIN_CTL  0x0136

#define LM85_REG_DEVICE 0x3d
#define LM85_REG_COMPANY 0x3e
#define LM85_REG_VERSTEP 0x3f
/* These are the recognized values for the above regs */
#define LM85_DEVICE_ADX 0x27
#define LM85_COMPANY_NATIONAL 0x01
#define LM85_COMPANY_ANALOG_DEV 0x41
#define LM85_COMPANY_SMSC 0x5c
#define LM85_VERSTEP_VMASK 0xf0
#define LM85_VERSTEP_SMASK 0x0f
#define LM85_VERSTEP_GENERIC 0x60
#define LM85_VERSTEP_LM85C 0x60
#define LM85_VERSTEP_LM85B 0x62
#define LM85_VERSTEP_ADM1027 0x60
#define LM85_VERSTEP_ADT7463 0x62
#define LM85_VERSTEP_ADT7463C 0x6A
#define LM85_VERSTEP_EMC6D100_A0 0x60
#define LM85_VERSTEP_EMC6D100_A1 0x61
#define LM85_VERSTEP_EMC6D102 0x65

#define LM85_REG_CONFIG 0x40

#define LM85_REG_ALARM1 0x41
#define LM85_REG_ALARM2 0x42
#define LM85_REG_ALARM  0x0141

#define LM85_REG_VID 0x43

/* Automated FAN control */
#define LM85_REG_AFAN_CONFIG(nr) (0x5c + (nr))
#define LM85_REG_AFAN_RANGE(nr) (0x5f + (nr))
#define LM85_REG_AFAN_SPIKE1 0x62
#define LM85_REG_AFAN_SPIKE2 0x63
#define LM85_REG_AFAN_MINPWM(nr) (0x64 + (nr))
#define LM85_REG_AFAN_LIMIT(nr) (0x67 + (nr))
#define LM85_REG_AFAN_CRITICAL(nr) (0x6a + (nr))
#define LM85_REG_AFAN_HYST1 0x6d
#define LM85_REG_AFAN_HYST2 0x6e

#define LM85_REG_TACH_MODE 0x74
#define LM85_REG_SPINUP_CTL 0x75

#define ADM1027_REG_TEMP_OFFSET(nr) (0x70 + (nr))
#define ADM1027_REG_CONFIG2 0x73
#define ADM1027_REG_INTMASK1 0x74
#define ADM1027_REG_INTMASK2 0x75
#define ADM1027_REG_INTMASK  0x0174
#define ADM1027_REG_EXTEND_ADC1 0x76
#define ADM1027_REG_EXTEND_ADC2 0x77
#define ADM1027_REG_EXTEND_ADC  0x0176
#define ADM1027_REG_CONFIG3 0x78
#define ADM1027_REG_FAN_PPR 0x7b

#define ADT7463_REG_THERM 0x79
#define ADT7463_REG_THERM_LIMIT 0x7A
#define ADT7463_REG_CONFIG4 0x7D

#define EMC6D100_REG_SFR  0x7c
#define EMC6D100_REG_ALARM3  0x7d
#define EMC6D100_REG_CONF  0x7f
#define EMC6D100_REG_INT_EN  0x80
/* IN5, IN6 and IN7 */
#define EMC6D100_REG_IN(nr)  (0x70 + ((nr)-5))
#define EMC6D100_REG_IN_MIN(nr) (0x73 + ((nr)-5) * 2)
#define EMC6D100_REG_IN_MAX(nr) (0x74 + ((nr)-5) * 2)

#define EMC6D102_REG_EXTEND_ADC1 0x85
#define EMC6D102_REG_EXTEND_ADC2 0x86
#define EMC6D102_REG_EXTEND_ADC3 0x87
#define EMC6D102_REG_EXTEND_ADC4 0x88

/* Conversions. Rounding and limit checking is only done on the TO_REG 
   variants. Note that you should be a bit careful with which arguments
   these macros are called: arguments may be evaluated more than once.
 */

/* IN are scaled 1.000 == 0xc0, mag = 3 */
#define IN_TO_REG(val)  (SENSORS_LIMIT((((val)*0xc0+500)/1000),0,255))
#define INEXT_FROM_REG(val,ext) (((val)*1000 + (ext)*250 + 96)/0xc0)
#define IN_FROM_REG(val) (INEXT_FROM_REG(val,0))

/* IN are scaled acording to built-in resistors */
static int lm85_scaling[] = {  /* .001 Volts */
		2500, 2250, 3300, 5000, 12000,
		3300, 1500, 1800,	/* EMC6D100 */
	};
#define SCALE(val,from,to) (((val)*(to) + ((from)/2))/(from))
#define INS_TO_REG(n,val)  (SENSORS_LIMIT(SCALE(val,lm85_scaling[n],192),0,255))
#define INSEXT_FROM_REG(n,val,ext) (SCALE((val)*4 + (ext),192*4,lm85_scaling[n]))
#define INS_FROM_REG(n,val) (INSEXT_FROM_REG(n,val,0))

/* FAN speed is measured using 90kHz clock */
#define FAN_TO_REG(val)  ((val)<=0?0xffff:SENSORS_LIMIT(5400000/(val),1,65534))
#define FAN_FROM_REG(val) ((val)==0?-1:(val)==0xffff?0:5400000/(val))

/* Temperature is reported in .01 degC increments */
#define TEMP_TO_REG(val) (SENSORS_LIMIT(((val)+50)/100,-127,127))
#define TEMPEXT_FROM_REG(val,ext) ((val)*100 + (ext)*25)
#define TEMP_FROM_REG(val) (TEMPEXT_FROM_REG(val,0))
#define EXTTEMP_TO_REG(val) (SENSORS_LIMIT((val)/25,-127,127))
#define OPPOINT_TO_REG(val) (SENSORS_LIMIT(val,-127,127))
#define OPPOINT_FROM_REG(val) (val)

#define PWM_TO_REG(val) (SENSORS_LIMIT(val,0,255))
#define PWM_FROM_REG(val) (val)

#define EXT_FROM_REG(val,sensor) (((val)>>(sensor * 2))&0x03)

/* ZONEs have the following parameters:
 *    Limit (low) temp,           1. degC
 *    Hysteresis (below limit),   1. degC (0-15)
 *    Range of speed control,     .1 degC (2-80)
 *    Critical (high) temp,       1. degC
 *
 * FAN PWMs have the following parameters:
 *    Reference Zone,                 1, 2, 3, etc.
 *    Spinup time,                    .05 sec
 *    PWM value at limit/low temp,    1 count
 *    PWM Frequency,                  1. Hz
 *    PWM is Min or OFF below limit,  flag
 *    Invert PWM output,              flag
 *
 * Some chips filter the temp, others the fan.
 *    Filter constant (or disabled)   .1 seconds
 */

/* These are the zone temperature range encodings */
static int lm85_range_map[] = {   /* .1 degC */
		 20,  25,  33,  40,  50,  66,
		 80, 100, 133, 160, 200, 266,
		320, 400, 533, 800
	};
static int RANGE_TO_REG( int range )
{
	int i;

	if( range >= lm85_range_map[15] ) { return 15 ; }
	for( i = 0 ; i < 15 ; ++i )
		if( range <= lm85_range_map[i] )
			break ;
	return( i & 0x0f );
}
#define RANGE_FROM_REG(val) (lm85_range_map[(val)&0x0f])

/* These are the Acoustic Enhancement, or Temperature smoothing encodings
 * NOTE: The enable/disable bit is INCLUDED in these encodings as the
 *       MSB (bit 3, value 8).  If the enable bit is 0, the encoded value
 *       is ignored, or set to 0.
 */
static int lm85_smooth_map[] = {  /* .1 sec */
		350, 176, 118,  70,  44,   30,   16,    8
/*    35.4 *    1/1, 1/2, 1/3, 1/5, 1/8, 1/12, 1/24, 1/48  */
	};
static int SMOOTH_TO_REG( int smooth )
{
	int i;

	if( smooth <= 0 ) { return 0 ; }  /* Disabled */
	for( i = 0 ; i < 7 ; ++i )
		if( smooth >= lm85_smooth_map[i] )
			break ;
	return( (i & 0x07) | 0x08 );
}
#define SMOOTH_FROM_REG(val) ((val)&0x08?lm85_smooth_map[(val)&0x07]:0)

/* These are the fan spinup delay time encodings */
static int lm85_spinup_map[] = {  /* .1 sec */
		0, 1, 2, 4, 7, 10, 20, 40
	};
static int SPINUP_TO_REG( int spinup )
{
	int i;

	if( spinup >= lm85_spinup_map[7] ) { return 7 ; }
	for( i = 0 ; i < 7 ; ++i )
		if( spinup <= lm85_spinup_map[i] )
			break ;
	return( i & 0x07 );
}
#define SPINUP_FROM_REG(val) (lm85_spinup_map[(val)&0x07])

/* These are the PWM frequency encodings */
static int lm85_freq_map[] = { /* .1 Hz */
		100, 150, 230, 300, 380, 470, 620, 980
	};
static int FREQ_TO_REG( int freq )
{
	int i;

	if( freq >= lm85_freq_map[7] ) { return 7 ; }
	for( i = 0 ; i < 7 ; ++i )
		if( freq <= lm85_freq_map[i] )
			break ;
	return( i & 0x07 );
}
#define FREQ_FROM_REG(val) (lm85_freq_map[(val)&0x07])

/* Since we can't use strings, I'm abusing these numbers
 *   to stand in for the following meanings:
 *      1 -- PWM responds to Zone 1
 *      2 -- PWM responds to Zone 2
 *      3 -- PWM responds to Zone 3
 *     23 -- PWM responds to the higher temp of Zone 2 or 3
 *    123 -- PWM responds to highest of Zone 1, 2, or 3
 *      0 -- PWM is always at 0% (ie, off)
 *     -1 -- PWM is always at 100%
 *     -2 -- PWM responds to manual control
 */
static int lm85_zone_map[] = { 1, 2, 3, -1, 0, 23, 123, -2 };
static int ZONE_TO_REG( int zone )
{
	int i;

	for( i = 0 ; i <= 7 ; ++i )
		if( zone == lm85_zone_map[i] )
			break ;
	if( i > 7 )   /* Not found. */
		i = 3;  /* Always 100% */
	return( (i & 0x07)<<5 );
}
#define ZONE_FROM_REG(val) (lm85_zone_map[((val)>>5)&0x07])

#define HYST_TO_REG(val) (SENSORS_LIMIT((-(val)+5)/10,0,15))
#define HYST_FROM_REG(val) (-(val)*10)

#define OFFSET_TO_REG(val) (SENSORS_LIMIT((val)/25,-127,127))
#define OFFSET_FROM_REG(val) ((val)*25)

#define PPR_MASK(fan) (0x03<<(fan *2))
#define PPR_TO_REG(val,fan) (SENSORS_LIMIT((val)-1,0,3)<<(fan *2))
#define PPR_FROM_REG(val,fan) ((((val)>>(fan * 2))&0x03)+1)

/* When converting to REG, we need to fixup the carry-over bit */
#define INTMASK_FROM_REG(val) (val)
#define INTMASK_TO_REG(val) (SENSORS_LIMIT((val)|((val)&0xff00?0x80:0),0,65535))

/* Typically used with Pentium 4 systems v9.1 VRM spec */
#define LM85_INIT_VRM  91

/* Chip sampling rates
 *
 * Some sensors are not updated more frequently than once per second
 *    so it doesn't make sense to read them more often than that.
 *    We cache the results and return the saved data if the driver
 *    is called again before a second has elapsed.
 *
 * Also, there is significant configuration data for this chip
 *    given the automatic PWM fan control that is possible.  There
 *    are about 47 bytes of config data to only 22 bytes of actual
 *    readings.  So, we keep the config data up to date in the cache
 *    when it is written and only sample it once every 5 *minutes*
 */
#define LM85_DATA_INTERVAL  (1 * HZ)
#define LM85_CONFIG_INTERVAL  (5 * 60 * HZ)

/* For each registered LM85, we need to keep some data in memory. That
   data is pointed to by client->data. The structure itself is
   dynamically allocated, when a new lm85 client is allocated. */

/* LM85 can automatically adjust fan speeds based on temperature
 * This structure encapsulates an entire Zone config.  There are
 * three zones (one for each temperature input) on the lm85
 */
struct lm85_zone {
	s8 limit;	/* Low temp limit */
	u8 hyst;	/* Low limit hysteresis. (0-15) */
	u8 range;	/* Temp range, encoded */
	s8 critical;	/* "All fans ON" temp limit */
};

struct lm85_autofan {
	u8 config;	/* Register value */
	u8 freq;	/* PWM frequency, encoded */
	u8 min_pwm;	/* Minimum PWM value, encoded */
	u8 min_off;	/* Min PWM or OFF below "limit", flag */
};

struct lm85_data {
	struct i2c_client client;
	int sysctl_id;
	enum chips type;

	struct semaphore update_lock;
	int valid;		/* !=0 if following fields are valid */
	unsigned long last_reading;	/* In jiffies */
	unsigned long last_config;	/* In jiffies */

	u8 in[8];		/* Register value */
	u8 in_max[8];		/* Register value */
	u8 in_min[8];		/* Register value */
	s8 temp[3];		/* Register value */
	s8 temp_min[3];		/* Register value */
	s8 temp_max[3];		/* Register value */
	s8 temp_offset[3];	/* Register value */
	u16 fan[4];		/* Register value */
	u16 fan_min[4];		/* Register value */
	u8 pwm[3];		/* Register value */
	u8 spinup_ctl;		/* Register encoding, combined */
	u8 tach_mode;		/* Register encoding, combined */
	u16 extend_adc;		/* Register value */
	u8 fan_ppr;		/* Register value */
	u8 smooth[3];		/* Register encoding */
	u8 vid;			/* Register value */
	u8 vrm;			/* VRM version */
	u8 syncpwm3;		/* Saved PWM3 for TACH 2,3,4 config */
	s8 oppoint[3];		/* Register value */
	u16 tmin_ctl;		/* Register value */
	long therm_total;	/* Cummulative therm count */
	long therm_ovfl;	/* Count of therm overflows */
	u8 therm_limit;		/* Register value */
	u32 alarms;		/* Register encoding, combined */
	u32 alarm_mask;		/* Register encoding, combined */
	struct lm85_autofan autofan[3];
	struct lm85_zone zone[3];
};

static int lm85_attach_adapter(struct i2c_adapter *adapter);
static int lm85_detect(struct i2c_adapter *adapter, int address,
			unsigned short flags, int kind);
static int lm85_detach_client(struct i2c_client *client);
static int lm85_read_value(struct i2c_client *client, u16 reg);
static int lm85_write_value(struct i2c_client *client, u16 reg, int value);
static void lm85_update_client(struct i2c_client *client);
static void lm85_init_client(struct i2c_client *client);


static void lm85_in(struct i2c_client *client, int operation, int ctl_name,
			int *nrels_mag, long *results);
static void lm85_fan(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void lm85_temp(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void lm85_vid(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void lm85_vrm(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void lm85_alarms(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void lm85_pwm(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void lm85_zone(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void lm85_pwm_config(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void lm85_pwm_zone(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void lm85_smooth(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);

static void lm85_spinup_ctl(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void lm85_tach_mode(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);

static void adm1027_tach_mode(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void adm1027_temp_offset(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void adm1027_fan_ppr(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void adm1027_alarm_mask(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);

static void adt7463_tmin_ctl(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void adt7463_therm_signal(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);

static void emc6d100_in(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);

static struct i2c_driver lm85_driver = {
	.name		=  "LM85 compatible sensor driver",
	.id		= I2C_DRIVERID_LM85,
	.flags		= I2C_DF_NOTIFY,
	.attach_adapter	= &lm85_attach_adapter,
	.detach_client	= &lm85_detach_client,
};

/* Unique ID assigned to each LM85 detected */
static int lm85_id = 0;

/* -- SENSORS SYSCTL START -- */
/* Common parameters */
#define LM85_SYSCTL_IN0                1000
#define LM85_SYSCTL_IN1                1001
#define LM85_SYSCTL_IN2                1002
#define LM85_SYSCTL_IN3                1003
#define LM85_SYSCTL_IN4                1004
#define LM85_SYSCTL_FAN1               1005
#define LM85_SYSCTL_FAN2               1006
#define LM85_SYSCTL_FAN3               1007
#define LM85_SYSCTL_FAN4               1008
#define LM85_SYSCTL_TEMP1              1009
#define LM85_SYSCTL_TEMP2              1010
#define LM85_SYSCTL_TEMP3              1011
#define LM85_SYSCTL_VID                1012
#define LM85_SYSCTL_ALARMS             1013
#define LM85_SYSCTL_PWM1               1014
#define LM85_SYSCTL_PWM2               1015
#define LM85_SYSCTL_PWM3               1016
#define LM85_SYSCTL_VRM                1017
#define LM85_SYSCTL_PWM_CFG1           1019
#define LM85_SYSCTL_PWM_CFG2           1020
#define LM85_SYSCTL_PWM_CFG3           1021
#define LM85_SYSCTL_PWM_ZONE1          1022
#define LM85_SYSCTL_PWM_ZONE2          1023
#define LM85_SYSCTL_PWM_ZONE3          1024
#define LM85_SYSCTL_ZONE1              1025
#define LM85_SYSCTL_ZONE2              1026
#define LM85_SYSCTL_ZONE3              1027
#define LM85_SYSCTL_SMOOTH1            1028
#define LM85_SYSCTL_SMOOTH2            1029
#define LM85_SYSCTL_SMOOTH3            1030

/* Vendor specific values */
#define LM85_SYSCTL_SPINUP_CTL         1100
#define LM85_SYSCTL_TACH_MODE          1101

/* Analog Devices variant of the LM85 */
#define ADM1027_SYSCTL_TACH_MODE       1200
#define ADM1027_SYSCTL_TEMP_OFFSET1    1201
#define ADM1027_SYSCTL_TEMP_OFFSET2    1202
#define ADM1027_SYSCTL_TEMP_OFFSET3    1203
#define ADM1027_SYSCTL_FAN_PPR         1204
#define ADM1027_SYSCTL_ALARM_MASK      1205

/* Analog Devices variant of the LM85/ADM1027 */
#define ADT7463_SYSCTL_TMIN_CTL1       1300
#define ADT7463_SYSCTL_TMIN_CTL2       1301
#define ADT7463_SYSCTL_TMIN_CTL3       1302
#define ADT7463_SYSCTL_THERM_SIGNAL    1303

/* SMSC variant of the LM85 */
#define EMC6D100_SYSCTL_IN5            1400
#define EMC6D100_SYSCTL_IN6            1401
#define EMC6D100_SYSCTL_IN7            1402

#define LM85_ALARM_IN0          0x0001
#define LM85_ALARM_IN1          0x0002
#define LM85_ALARM_IN2          0x0004
#define LM85_ALARM_IN3          0x0008
#define LM85_ALARM_TEMP1        0x0010
#define LM85_ALARM_TEMP2        0x0020
#define LM85_ALARM_TEMP3        0x0040
#define LM85_ALARM_ALARM2       0x0080
#define LM85_ALARM_IN4          0x0100
#define LM85_ALARM_RESERVED     0x0200
#define LM85_ALARM_FAN1         0x0400
#define LM85_ALARM_FAN2         0x0800
#define LM85_ALARM_FAN3         0x1000
#define LM85_ALARM_FAN4         0x2000
#define LM85_ALARM_TEMP1_FAULT  0x4000
#define LM85_ALARM_TEMP3_FAULT 0x08000
#define LM85_ALARM_IN6         0x10000
#define LM85_ALARM_IN7         0x20000
#define LM85_ALARM_IN5         0x40000
/* -- SENSORS SYSCTL END -- */

/* The /proc/sys entries */
/* These files are created for each detected LM85. This is just a template;
 *    The actual list is built from this and additional per-chip
 *    custom lists below.  Note the XXX_LEN macros.  These must be
 *    compile time constants because they will be used to allocate
 *    space for the final template passed to i2c_register_entry.
 *    We depend on the ability of GCC to evaluate expressions at
 *    compile time to turn these expressions into compile time
 *    constants, but this can generate a warning.
 */
static ctl_table lm85_common[] = {
	{LM85_SYSCTL_IN0, "in0", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &lm85_in},
	{LM85_SYSCTL_IN1, "in1", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &lm85_in},
	{LM85_SYSCTL_IN2, "in2", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &lm85_in},
	{LM85_SYSCTL_IN3, "in3", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &lm85_in},
	{LM85_SYSCTL_IN4, "in4", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &lm85_in},
	{LM85_SYSCTL_FAN1, "fan1", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &lm85_fan},
	{LM85_SYSCTL_FAN2, "fan2", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &lm85_fan},
	{LM85_SYSCTL_FAN3, "fan3", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &lm85_fan},
	{LM85_SYSCTL_FAN4, "fan4", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &lm85_fan},
	{LM85_SYSCTL_TEMP1, "temp1", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &lm85_temp},
	{LM85_SYSCTL_TEMP2, "temp2", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &lm85_temp},
	{LM85_SYSCTL_TEMP3, "temp3", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &lm85_temp},
	{LM85_SYSCTL_VID, "vid", NULL, 0, 0444, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &lm85_vid},
	{LM85_SYSCTL_VRM, "vrm", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &lm85_vrm},
	{LM85_SYSCTL_ALARMS, "alarms", NULL, 0, 0444, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &lm85_alarms},
	{LM85_SYSCTL_PWM1, "pwm1", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &lm85_pwm},
	{LM85_SYSCTL_PWM2, "pwm2", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &lm85_pwm},
	{LM85_SYSCTL_PWM3, "pwm3", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &lm85_pwm},
	{LM85_SYSCTL_PWM_CFG1, "pwm1_cfg", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &lm85_pwm_config},
	{LM85_SYSCTL_PWM_CFG2, "pwm2_cfg", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &lm85_pwm_config},
	{LM85_SYSCTL_PWM_CFG3, "pwm3_cfg", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &lm85_pwm_config},
	{LM85_SYSCTL_PWM_ZONE1, "pwm1_zone", NULL, 0, 0644, NULL,
		&i2c_proc_real, &i2c_sysctl_real, NULL, &lm85_pwm_zone},
	{LM85_SYSCTL_PWM_ZONE2, "pwm2_zone", NULL, 0, 0644, NULL,
		&i2c_proc_real, &i2c_sysctl_real, NULL, &lm85_pwm_zone},
	{LM85_SYSCTL_PWM_ZONE3, "pwm3_zone", NULL, 0, 0644, NULL,
		&i2c_proc_real, &i2c_sysctl_real, NULL, &lm85_pwm_zone},
	{LM85_SYSCTL_ZONE1, "zone1", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &lm85_zone},
	{LM85_SYSCTL_ZONE2, "zone2", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &lm85_zone},
	{LM85_SYSCTL_ZONE3, "zone3", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &lm85_zone},
	{LM85_SYSCTL_SMOOTH1, "smooth1", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &lm85_smooth},
	{LM85_SYSCTL_SMOOTH2, "smooth2", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &lm85_smooth},
	{LM85_SYSCTL_SMOOTH3, "smooth3", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &lm85_smooth},
	{0}
};
#define CTLTBL_COMMON (sizeof(lm85_common)/sizeof(lm85_common[0]))

/* NOTE: tach_mode is a shared name, but implemented with
 *   different functions
 */
static ctl_table lm85_specific[] = {
	{LM85_SYSCTL_SPINUP_CTL, "spinup_ctl", NULL, 0, 0644, NULL,
		&i2c_proc_real, &i2c_sysctl_real, NULL, &lm85_spinup_ctl},
	{LM85_SYSCTL_TACH_MODE, "tach_mode", NULL, 0, 0644, NULL,
		&i2c_proc_real, &i2c_sysctl_real, NULL, &lm85_tach_mode},
/*	{0} The doc generator needs this. */
};
#define CTLTBL_LM85 (sizeof(lm85_specific)/sizeof(lm85_specific[0]))

static ctl_table adm1027_specific[] = {
	{ADM1027_SYSCTL_TACH_MODE, "tach_mode", NULL, 0, 0644, NULL,
		&i2c_proc_real, &i2c_sysctl_real, NULL, &adm1027_tach_mode},
	{ADM1027_SYSCTL_TEMP_OFFSET1, "temp1_offset", NULL, 0, 0644, NULL,
		&i2c_proc_real, &i2c_sysctl_real, NULL, &adm1027_temp_offset},
	{ADM1027_SYSCTL_TEMP_OFFSET2, "temp2_offset", NULL, 0, 0644, NULL,
		&i2c_proc_real, &i2c_sysctl_real, NULL, &adm1027_temp_offset},
	{ADM1027_SYSCTL_TEMP_OFFSET3, "temp3_offset", NULL, 0, 0644, NULL,
		&i2c_proc_real, &i2c_sysctl_real, NULL, &adm1027_temp_offset},
	{ADM1027_SYSCTL_FAN_PPR, "fan_ppr", NULL, 0, 0644, NULL,
		&i2c_proc_real, &i2c_sysctl_real, NULL, &adm1027_fan_ppr},
	{ADM1027_SYSCTL_ALARM_MASK, "alarm_mask", NULL, 0, 0644, NULL,
		&i2c_proc_real, &i2c_sysctl_real, NULL, &adm1027_alarm_mask},
/*	{0} The doc generator needs this. */
};
#define CTLTBL_ADM1027 (sizeof(adm1027_specific)/sizeof(adm1027_specific[0]))

static ctl_table adt7463_specific[] = {
	{ADT7463_SYSCTL_TMIN_CTL1, "tmin_ctl1", NULL, 0, 0644, NULL,
		&i2c_proc_real, &i2c_sysctl_real, NULL, &adt7463_tmin_ctl},
	{ADT7463_SYSCTL_TMIN_CTL2, "tmin_ctl2", NULL, 0, 0644, NULL,
		&i2c_proc_real, &i2c_sysctl_real, NULL, &adt7463_tmin_ctl},
	{ADT7463_SYSCTL_TMIN_CTL3, "tmin_ctl3", NULL, 0, 0644, NULL,
		&i2c_proc_real, &i2c_sysctl_real, NULL, &adt7463_tmin_ctl},
	{ADT7463_SYSCTL_THERM_SIGNAL, "therm_signal", NULL, 0, 0644, NULL,
		&i2c_proc_real, &i2c_sysctl_real, NULL, &adt7463_therm_signal},
/*	{0} The doc generator needs this. */
};
#define CTLTBL_ADT7463 (sizeof(adt7463_specific)/sizeof(adt7463_specific[0]))

static ctl_table emc6d100_specific[] = {
	{EMC6D100_SYSCTL_IN5, "in5", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &emc6d100_in},
	{EMC6D100_SYSCTL_IN6, "in6", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &emc6d100_in},
	{EMC6D100_SYSCTL_IN7, "in7", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &emc6d100_in},
/*	{0} The doc generator needs this. */
};
#define CTLTBL_EMC6D100 (sizeof(emc6d100_specific)/sizeof(emc6d100_specific[0]))


#define MAX2(a,b) ((a)>(b)?(a):(b))
#define MAX3(a,b,c) ((a)>(b)?MAX2((a),(c)):MAX2((b),(c)))
#define MAX4(a,b,c,d) ((a)>(b)?MAX3((a),(c),(d)):MAX3((b),(c),(d)))

#define CTLTBL_MAX (CTLTBL_COMMON + MAX3(CTLTBL_LM85, CTLTBL_ADM1027+CTLTBL_ADT7463, CTLTBL_EMC6D100))

/* This function is called when:
     * lm85_driver is inserted (when this module is loaded), for each
       available adapter
     * when a new adapter is inserted (and lm85_driver is still present) */
static int lm85_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_detect(adapter, &addr_data, lm85_detect);
}

/* This function is called by i2c_detect */
static int lm85_detect(struct i2c_adapter *adapter, int address,
		       unsigned short flags, int kind)
{
	int i;
	int company, verstep ;
	struct i2c_client *new_client;
	struct lm85_data *data;
	int err = 0;
	const char *type_name = "";
	struct ctl_table template[CTLTBL_MAX] ;
	int template_used ;

	if (i2c_is_isa_adapter(adapter)) {
		/* This chip has no ISA interface */
		goto ERROR0 ;
	};

	if (!i2c_check_functionality(adapter,
					I2C_FUNC_SMBUS_BYTE_DATA)) {
		/* We need to be able to do byte I/O */
		goto ERROR0 ;
	};

	/* OK. For now, we presume we have a valid client. We now create the
	   client structure, even though we cannot fill it completely yet.
	   But it allows us to access lm85_{read,write}_value. */

	if (!(data = kmalloc(sizeof(struct lm85_data), GFP_KERNEL))) {
		err = -ENOMEM;
		goto ERROR0;
	}

	new_client = &data->client;
	new_client->addr = address;
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &lm85_driver;
	new_client->flags = 0;

	/* Now, we do the remaining detection. */

	company = lm85_read_value(new_client, LM85_REG_COMPANY);
	verstep = lm85_read_value(new_client, LM85_REG_VERSTEP);

#ifdef DEBUG
	printk("lm85: Detecting device at %d,0x%02x with"
		" COMPANY: 0x%02x and VERSTEP: 0x%02x\n",
		i2c_adapter_id(new_client->adapter), new_client->addr,
		company, verstep
	    );
#endif

	/* If auto-detecting, Determine the chip type. */
	if (kind <= 0) {
#ifdef DEBUG
		printk("lm85: Autodetecting device at %d,0x%02x ...\n",
			i2c_adapter_id(adapter), address );
#endif
		if( company == LM85_COMPANY_NATIONAL
		    && verstep == LM85_VERSTEP_LM85C ) {
			kind = lm85c ;
		} else if( company == LM85_COMPANY_NATIONAL
		    && verstep == LM85_VERSTEP_LM85B ) {
			kind = lm85b ;
		} else if( company == LM85_COMPANY_NATIONAL
		    && (verstep & LM85_VERSTEP_VMASK) == LM85_VERSTEP_GENERIC) {
			printk("lm85: Detected National Semiconductor chip\n");
			printk("lm85: Unrecognized version/stepping 0x%02x"
			    " Defaulting to Generic LM85.\n", verstep );
			kind = any_chip ;
		} else if( company == LM85_COMPANY_ANALOG_DEV
		    && verstep == LM85_VERSTEP_ADM1027 ) {
			kind = adm1027 ;
		} else if( company == LM85_COMPANY_ANALOG_DEV
		    && (verstep == LM85_VERSTEP_ADT7463
			 || verstep == LM85_VERSTEP_ADT7463C) ) {
			kind = adt7463 ;
		} else if( company == LM85_COMPANY_ANALOG_DEV
		    && (verstep & LM85_VERSTEP_VMASK) == LM85_VERSTEP_GENERIC) {
			printk("lm85: Detected Analog Devices chip\n");
			printk("lm85: Unrecognized version/stepping 0x%02x"
			    " Defaulting to Generic LM85.\n", verstep );
			kind = any_chip ;
		} else if( company == LM85_COMPANY_SMSC
		    && verstep == LM85_VERSTEP_EMC6D102) {
			kind = emc6d102;
		} else if( company == LM85_COMPANY_SMSC
		    && (verstep == LM85_VERSTEP_EMC6D100_A0
			 || verstep == LM85_VERSTEP_EMC6D100_A1) ) {
			/* Unfortunately, we can't tell a '100 from a '101
			 *   from the registers.  Since a '101 is a '100
			 *   in a package with fewer pins and therefore no
			 *   3.3V, 1.5V or 1.8V inputs, perhaps if those
			 *   inputs read 0, then it's a '101.
			 */
			kind = emc6d100 ;
		} else if( company == LM85_COMPANY_SMSC
		    && (verstep & LM85_VERSTEP_VMASK) == LM85_VERSTEP_GENERIC) {
			printk("lm85: Detected SMSC chip\n");
			printk("lm85: Unrecognized version/stepping 0x%02x"
			    " Defaulting to Generic LM85.\n", verstep );
			kind = any_chip ;
		} else if( kind == any_chip
		    && (verstep & LM85_VERSTEP_VMASK) == LM85_VERSTEP_GENERIC) {
			printk("lm85: Generic LM85 Version 6 detected\n");
			/* Leave kind as "any_chip" */
		} else {
#ifdef DEBUG
			printk("lm85: Autodetection failed\n");
#endif
			/* Not an LM85 ... */
			if( kind == any_chip ) {  /* User used force=x,y */
			    printk("lm85: Generic LM85 Version 6 not"
				" found at %d,0x%02x. Try force_lm85c.\n",
				i2c_adapter_id(adapter), address );
			}
			goto ERROR1;
		}
	}

	/* Fill in the chip specific driver values */
	switch (kind) {
	case any_chip :
		type_name = "lm85";
		strcpy(new_client->name, "Generic LM85");
		template_used = 0 ;
		break ;
	case lm85b :
		type_name = "lm85b";
		strcpy(new_client->name, "National LM85-B");
		memcpy( template, lm85_specific, sizeof(lm85_specific) );
		template_used = CTLTBL_LM85 ;
		break ;
	case lm85c :
		type_name = "lm85c";
		strcpy(new_client->name, "National LM85-C");
		memcpy( template, lm85_specific, sizeof(lm85_specific) );
		template_used = CTLTBL_LM85 ;
		break ;
	case adm1027 :
		type_name = "adm1027";
		strcpy(new_client->name, "Analog Devices ADM1027");
		memcpy( template, adm1027_specific, sizeof(adm1027_specific) );
		template_used = CTLTBL_ADM1027 ;
		break ;
	case adt7463 :
		type_name = "adt7463";
		strcpy(new_client->name, "Analog Devices ADT7463");
		memcpy( template, adt7463_specific, sizeof(adt7463_specific) );
		template_used = CTLTBL_ADT7463 ;
		memcpy( template+template_used, adm1027_specific, sizeof(adm1027_specific) );
		template_used += CTLTBL_ADM1027 ;
		break ;
	case emc6d100 :
		type_name = "emc6d100";
		strcpy(new_client->name, "SMSC EMC6D100");
		memcpy(template, emc6d100_specific, sizeof(emc6d100_specific));
		template_used = CTLTBL_EMC6D100 ;
		break ;
	case emc6d102 :
		type_name = "emc6d102";
		strcpy(new_client->name, "SMSC EMC6D102");
		template_used = 0;
		break;
	default :
		printk("lm85: Internal error, invalid kind (%d)!\n", kind);
		err = -EFAULT ;
		goto ERROR1;
	}

	/* Fill in the remaining client fields */
	new_client->id = lm85_id++;
	printk("lm85: Assigning ID %d to %s at %d,0x%02x\n",
		new_client->id, new_client->name,
		i2c_adapter_id(new_client->adapter),
		new_client->addr
	    );

	/* Housekeeping values */
	data->type = kind;
	data->valid = 0;

	/* Set the VRM version */
	data->vrm = LM85_INIT_VRM ;

	/* Zero the accumulators */
	data->therm_total = 0;
	data->therm_ovfl = 0;

	init_MUTEX(&data->update_lock);
	data->extend_adc = 0;

	/* Initialize the LM85 chip */
	lm85_init_client(new_client);

	/* Tell the I2C layer a new client has arrived */
	if ((err = i2c_attach_client(new_client)))
		goto ERROR1;

	/* Finish out the template */
	memcpy( template + template_used, lm85_common, sizeof(lm85_common) );

	/* Register a new directory entry with module sensors */
	if ((i = i2c_register_entry(new_client,
					type_name,
					template,
					THIS_MODULE)) < 0) {
		err = i;
		goto ERROR2;
	}
	data->sysctl_id = i;

	return 0;

	/* Error out and cleanup code */
    ERROR2:
	i2c_detach_client(new_client);
    ERROR1:
	kfree(data);
    ERROR0:
	return err;
}

static int lm85_detach_client(struct i2c_client *client)
{
	int err;
	int id ;

	id = client->id;
	i2c_deregister_entry(((struct lm85_data *)(client->data))->sysctl_id);

	if ((err = i2c_detach_client(client))) {
		printk("lm85(%d): Client deregistration failed,"
			" client not detached.\n", id );
		return err;
	}

	kfree(client->data);

	return 0;
}

static int lm85_read_value(struct i2c_client *client, u16 reg)
{
	int res;

	/* What size location is it? */
	switch( reg ) {
	case LM85_REG_FAN(0) :  /* Read WORD data */
	case LM85_REG_FAN(1) :
	case LM85_REG_FAN(2) :
	case LM85_REG_FAN(3) :
	case LM85_REG_FAN_MIN(0) :
	case LM85_REG_FAN_MIN(1) :
	case LM85_REG_FAN_MIN(2) :
	case LM85_REG_FAN_MIN(3) :
	case LM85_REG_ALARM :  /* Read ALARM1 and ALARM2 */
	case ADM1027_REG_INTMASK :  /* Read MASK1 and MASK2 */
	case ADM1027_REG_EXTEND_ADC :  /* Read ADC1 and ADC2 */
		reg &= 0xff ;  /* Pseudo words have address + 0x0100 */
		res = i2c_smbus_read_byte_data(client, reg) & 0xff ;
		res |= (i2c_smbus_read_byte_data(client, reg+1) & 0xff) << 8 ;
		break ;
	case ADT7463_REG_TMIN_CTL :  /* Read WORD MSB, LSB */
		reg &= 0xff ;  /* Pseudo words have address + 0x0100 */
		res = (i2c_smbus_read_byte_data(client, reg) & 0xff) << 8 ;
		res |= i2c_smbus_read_byte_data(client, reg+1) & 0xff ;
		break ;
	default:	/* Read BYTE data */
		res = i2c_smbus_read_byte_data(client, reg & 0xff) & 0xff ;
		break ;
	}

	return res ;
}

static int lm85_write_value(struct i2c_client *client, u16 reg, int value)
{
	int res ;

	switch( reg ) {
	case LM85_REG_FAN(0) :  /* Write WORD data */
	case LM85_REG_FAN(1) :
	case LM85_REG_FAN(2) :
	case LM85_REG_FAN(3) :
	case LM85_REG_FAN_MIN(0) :
	case LM85_REG_FAN_MIN(1) :
	case LM85_REG_FAN_MIN(2) :
	case LM85_REG_FAN_MIN(3) :
	case ADM1027_REG_INTMASK :
	/* NOTE: ALARM and ADC are read only, so not included here */
		reg &= 0xff ;  /* Pseudo words have address + 0x0100 */
		res = i2c_smbus_write_byte_data(client, reg, value & 0xff) ;
		res |= i2c_smbus_write_byte_data(client, reg+1, (value>>8) & 0xff) ;
		break ;
	case ADT7463_REG_TMIN_CTL :  /* Write WORD MSB, LSB */
		reg &= 0xff ;  /* Pseudo words have address + 0x0100 */
		res = i2c_smbus_write_byte_data(client, reg, (value>>8) & 0xff);
		res |= i2c_smbus_write_byte_data(client, reg+1, value & 0xff) ;
		break ;
	default:	/* Write BYTE data */
		res = i2c_smbus_write_byte_data(client, reg & 0xff, value);
		break ;
	}

	return res ;
}

/* Called when we have found a new LM85. */
static void lm85_init_client(struct i2c_client *client)
{
	int value;
	struct lm85_data *data = client->data;

#ifdef DEBUG
	printk("lm85(%d): Initializing device\n", client->id);
#endif

	/* Warn if part was not "READY" */
	value = lm85_read_value(client, LM85_REG_CONFIG);
#ifdef DEBUG
	printk("lm85(%d): LM85_REG_CONFIG is: 0x%02x\n", client->id, value );
#endif
	if( value & 0x02 ) {
		printk("lm85(%d): Client (%d,0x%02x) config is locked.\n",
			    client->id,
			    i2c_adapter_id(client->adapter), client->addr );
	};
	if( ! (value & 0x04) ) {
		printk("lm85(%d): Client (%d,0x%02x) is not ready.\n",
			    client->id,
			    i2c_adapter_id(client->adapter), client->addr );
	};
	if( (data->type == adm1027 || data->type == adt7463)
	    && (value & 0x10)
	) {
		printk("lm85(%d): Client (%d,0x%02x) VxI mode is set.  "
			"Please report this to the lm85 maintainer.\n",
			    client->id,
			    i2c_adapter_id(client->adapter), client->addr );
	};

	/* See if SYNC to PWM3 is set */
	if( data->type == adt7463 
	    && (lm85_read_value(client, LM85_REG_AFAN_SPIKE1) & 0x10)
	) {
		printk("lm85(%d): Sync to PWM3 is set.  Expect PWM3 "
			"to control fans 2, 3, and 4\n",
			client->id );
	};

	/* See if PWM2 is #SMBALERT */
	if( (data->type == adm1027 || data->type == adt7463)
	    && (lm85_read_value(client, ADM1027_REG_CONFIG3) & 0x01)
	) {
		printk("lm85(%d): PWM2 is SMBALERT.  PWM2 not available.\n",
			client->id );
	};

	/* Check if 2.5V and 5V inputs are reconfigured */
	if( data->type == adt7463 ) {
		value = lm85_read_value(client, ADT7463_REG_CONFIG4);
		if( value & 0x01 ) {
			printk("lm85(%d): 2.5V input (in0) is SMBALERT.  "
				"in0 not available.\n", client->id );
		};
		if( value & 0x02 ) {
			printk("lm85(%d): 5V input (in3) is THERM.  "
				"in3 not available.\n", client->id );
		}
	};

	/* FIXME?  Display EMC6D100 config info? */

	/* WE INTENTIONALLY make no changes to the limits,
	 *   offsets, pwms, fans and zones.  If they were
	 *   configured, we don't want to mess with them.
	 *   If they weren't, the default is 100% PWM, no
	 *   control and will suffice until 'sensors -s'
	 *   can be run by the user.
	 */

	/* Start monitoring */
	value = lm85_read_value(client, LM85_REG_CONFIG);
	/* Try to clear LOCK, Set START, save everything else */
	value = ((value & ~ 0x02) | 0x01) & 0xff ;
#ifdef DEBUG
	printk("lm85(%d): Setting CONFIG to: 0x%02x\n", client->id, value );
#endif
	lm85_write_value(client, LM85_REG_CONFIG, value);

}

static void lm85_update_client(struct i2c_client *client)
{
	struct lm85_data *data = client->data;
	int i;

	down(&data->update_lock);

	if (!data->valid
	    || (jiffies - data->last_reading > LM85_DATA_INTERVAL )) {
		/* Things that change quickly */

#ifdef DEBUG
		printk("lm85(%d): Reading sensor values\n", client->id);
#endif
		/* Have to read extended bits first to "freeze" the
		 * more significant bits that are read later.
		 */
		switch( data->type ) {
		case adm1027 :
		case adt7463 :
			data->extend_adc =
			    lm85_read_value(client, ADM1027_REG_EXTEND_ADC);
			break ;
		default :
			break ;
		}

		for (i = 0; i <= 4; ++i) {
			data->in[i] =
			    lm85_read_value(client, LM85_REG_IN(i));
		}

		for (i = 0; i <= 3; ++i) {
			data->fan[i] =
			    lm85_read_value(client, LM85_REG_FAN(i));
		}

		for (i = 0; i <= 2; ++i) {
			data->temp[i] =
			    lm85_read_value(client, LM85_REG_TEMP(i));
		}

		for (i = 0; i <= 2; ++i) {
			data->pwm[i] =
			    lm85_read_value(client, LM85_REG_PWM(i));
		}

		data->alarms = lm85_read_value(client, LM85_REG_ALARM);

		switch( data->type ) {
		case adt7463 :
			/* REG_THERM code duplicated in therm_signal() */
			i = lm85_read_value(client, ADT7463_REG_THERM);
			if( data->therm_total < LONG_MAX - 256 ) {
			    data->therm_total += i ;
			}
			if( i >= 255 ) {
				++data->therm_ovfl ;
			}
			break ;
		case emc6d100 :
			/* Three more voltage sensors */
			for (i = 5; i <= 7; ++i) {
			    data->in[i] =
				lm85_read_value(client, EMC6D100_REG_IN(i));
			}
			/* More alarm bits */
			data->alarms |=
			    lm85_read_value(client, EMC6D100_REG_ALARM3) << 16;

			break ;
		case emc6d102 :
			/* Have to read LSB bits after the MSB ones because
			   the reading of the MSB bits has frozen the
			   LSBs (backward from the ADM1027).
			   We use only two extra bits per channel, and encode
			   them in the same format the ADM1027 uses, to keep the
			   rest of the code unchanged.
			 */
			i = lm85_read_value(client, EMC6D102_REG_EXTEND_ADC1);
			data->extend_adc = (i & 0xcc) << 8; /* temp3, temp1 */
			i = lm85_read_value(client, EMC6D102_REG_EXTEND_ADC2);
			data->extend_adc |= (i & 0x0c) << 10; /* temp2 */
			data->extend_adc |= (i & 0xc0) << 2; /* in4 */
			i = lm85_read_value(client, EMC6D102_REG_EXTEND_ADC3);
			data->extend_adc |= (i & 0x0c) >> 2; /* in0 */
			data->extend_adc |= i & 0xc0; /* in3 */
			i = lm85_read_value(client, EMC6D102_REG_EXTEND_ADC4);
			data->extend_adc |= i & 0x0c; /* in1 */
			data->extend_adc |= (i & 0xc0) >> 2; /* in2 */
			break;
		default : break ; /* no warnings */
		}

		data->last_reading = jiffies ;
	};  /* last_reading */

	if (!data->valid
	    || (jiffies - data->last_config > LM85_CONFIG_INTERVAL) ) {
		/* Things that don't change often */

#ifdef DEBUG
		printk("lm85(%d): Reading config values\n", client->id);
#endif
		for (i = 0; i <= 4; ++i) {
			data->in_min[i] =
			    lm85_read_value(client, LM85_REG_IN_MIN(i));
			data->in_max[i] =
			    lm85_read_value(client, LM85_REG_IN_MAX(i));
		}

		for (i = 0; i <= 3; ++i) {
			data->fan_min[i] =
			    lm85_read_value(client, LM85_REG_FAN_MIN(i));
		}

		for (i = 0; i <= 2; ++i) {
			data->temp_min[i] =
			    lm85_read_value(client, LM85_REG_TEMP_MIN(i));
			data->temp_max[i] =
			    lm85_read_value(client, LM85_REG_TEMP_MAX(i));
		}

		data->vid = lm85_read_value(client, LM85_REG_VID);

		for (i = 0; i <= 2; ++i) {
			int val ;
			data->autofan[i].config =
			    lm85_read_value(client, LM85_REG_AFAN_CONFIG(i));
			val = lm85_read_value(client, LM85_REG_AFAN_RANGE(i));
			data->autofan[i].freq = val & 0x07 ;
			data->zone[i].range = (val >> 4) & 0x0f ;
			data->autofan[i].min_pwm =
			    lm85_read_value(client, LM85_REG_AFAN_MINPWM(i));
			data->zone[i].limit =
			    lm85_read_value(client, LM85_REG_AFAN_LIMIT(i));
			data->zone[i].critical =
			    lm85_read_value(client, LM85_REG_AFAN_CRITICAL(i));
		}

		i = lm85_read_value(client, LM85_REG_AFAN_SPIKE1);
		data->smooth[0] = i & 0x0f ;
		data->syncpwm3 = i & 0x10 ;  /* Save PWM3 config */
		data->autofan[0].min_off = i & 0x20 ;
		data->autofan[1].min_off = i & 0x40 ;
		data->autofan[2].min_off = i & 0x80 ;
		i = lm85_read_value(client, LM85_REG_AFAN_SPIKE2);
		data->smooth[1] = (i>>4) & 0x0f ;
		data->smooth[2] = i & 0x0f ;

		i = lm85_read_value(client, LM85_REG_AFAN_HYST1);
		data->zone[0].hyst = (i>>4) & 0x0f ;
		data->zone[1].hyst = i & 0x0f ;

		i = lm85_read_value(client, LM85_REG_AFAN_HYST2);
		data->zone[2].hyst = (i>>4) & 0x0f ;

		switch( data->type ) {
		case lm85b :
		case lm85c :
			data->tach_mode = lm85_read_value(client,
				LM85_REG_TACH_MODE );
			data->spinup_ctl = lm85_read_value(client,
				LM85_REG_SPINUP_CTL );
			break ;
		case adt7463 :
			for (i = 0; i <= 2; ++i) {
			    data->oppoint[i] = lm85_read_value(client,
				ADT7463_REG_OPPOINT(i) );
			}
			data->tmin_ctl = lm85_read_value(client,
				ADT7463_REG_TMIN_CTL );
			data->therm_limit = lm85_read_value(client,
				ADT7463_REG_THERM_LIMIT );
		/* FALL THROUGH */
		case adm1027 :
			for (i = 0; i <= 2; ++i) {
			    data->temp_offset[i] = lm85_read_value(client,
				ADM1027_REG_TEMP_OFFSET(i) );
			}
			data->tach_mode = lm85_read_value(client,
				ADM1027_REG_CONFIG3 );
			data->fan_ppr = lm85_read_value(client,
				ADM1027_REG_FAN_PPR );
			data->alarm_mask = lm85_read_value(client,
				ADM1027_REG_INTMASK );
			break ;
		case emc6d100 :
			for (i = 5; i <= 7; ++i) {
			    data->in_min[i] =
				lm85_read_value(client, EMC6D100_REG_IN_MIN(i));
			    data->in_max[i] =
				lm85_read_value(client, EMC6D100_REG_IN_MAX(i));
			}
			break ;
		default : break ; /* no warnings */
		}
	
		data->last_config = jiffies;
	};  /* last_config */

	data->valid = 1;

	up(&data->update_lock);
}


/* The next functions are the call-back functions of the /proc/sys and
   sysctl files. Which function is used is defined in the ctl_table in
   the extra1 field.
   Each function must return the magnitude (power of 10 to divide the data
   with) if it is called with operation==SENSORS_PROC_REAL_INFO.  It must
   put a maximum of *nrels elements in results reflecting the data of this
   file, and set *nrels to the number it actually put in it, if operation==
   SENSORS_PROC_REAL_READ.  Finally, it must get upto *nrels elements from
   results and write them to the chip, if operations==SENSORS_PROC_REAL_WRITE.
 */
void lm85_in(struct i2c_client *client, int operation, int ctl_name,
	     int *nrels_mag, long *results)
{
	struct lm85_data *data = client->data;
	int nr = ctl_name - LM85_SYSCTL_IN0;

	if (nr < 0 || nr > 4)
		return ;  /* ERROR */

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 3;  /* 1.000 */
	else if (operation == SENSORS_PROC_REAL_READ) {
		int ext;
		lm85_update_client(client);
		ext = EXT_FROM_REG(data->extend_adc, nr);
		results[0] = INS_FROM_REG(nr,data->in_min[nr]);
		results[1] = INS_FROM_REG(nr,data->in_max[nr]);
		results[2] = INSEXT_FROM_REG(nr,data->in[nr],ext);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		down(&data->update_lock);
		if (*nrels_mag > 1) {
			data->in_max[nr] = INS_TO_REG(nr,results[1]);
			lm85_write_value(client, LM85_REG_IN_MAX(nr),
					 data->in_max[nr]);
		}
		if (*nrels_mag > 0) {
			data->in_min[nr] = INS_TO_REG(nr,results[0]);
			lm85_write_value(client, LM85_REG_IN_MIN(nr),
					 data->in_min[nr]);
		}
		up(&data->update_lock);
	}
}

void lm85_fan(struct i2c_client *client, int operation, int ctl_name,
	      int *nrels_mag, long *results)
{
	struct lm85_data *data = client->data;
	int nr = ctl_name - LM85_SYSCTL_FAN1 ;

	if (nr < 0 || nr > 3)
		return ;  /* ERROR */

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		lm85_update_client(client);
		results[0] = FAN_FROM_REG(data->fan_min[nr]);
		results[1] = FAN_FROM_REG(data->fan[nr]);
		*nrels_mag = 2;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		down(&data->update_lock);
		if (*nrels_mag > 0) {
			data->fan_min[nr] = FAN_TO_REG(results[0]);
			lm85_write_value(client, LM85_REG_FAN_MIN(nr),
					 data->fan_min[nr]);
		}
		up(&data->update_lock);
	}
}


void lm85_temp(struct i2c_client *client, int operation, int ctl_name,
	       int *nrels_mag, long *results)
{
	struct lm85_data *data = client->data;
	int nr = ctl_name - LM85_SYSCTL_TEMP1 ;

	if (nr < 0 || nr > 2)
		return ;  /* ERROR */

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 2;
	else if (operation == SENSORS_PROC_REAL_READ) {
		int ext;
		lm85_update_client(client);

		/* +5 for offset of temp data in ext reg */
		ext = EXT_FROM_REG(data->extend_adc, nr+5);

		results[0] = TEMP_FROM_REG(data->temp_min[nr]);
		results[1] = TEMP_FROM_REG(data->temp_max[nr]);
		results[2] = TEMPEXT_FROM_REG(data->temp[nr],ext);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		down(&data->update_lock);
		if (*nrels_mag > 1) {
			data->temp_max[nr] = TEMP_TO_REG(results[1]);
			lm85_write_value(client, LM85_REG_TEMP_MAX(nr),
					 data->temp_max[nr]);
		}
		if (*nrels_mag > 0) {
			data->temp_min[nr] = TEMP_TO_REG(results[0]);
			lm85_write_value(client, LM85_REG_TEMP_MIN(nr),
					 data->temp_min[nr]);
		}
		up(&data->update_lock);
	}
}

void lm85_pwm(struct i2c_client *client, int operation, int ctl_name,
	      int *nrels_mag, long *results)
{
	struct lm85_data *data = client->data;
	int nr = ctl_name - LM85_SYSCTL_PWM1 ;
	int pwm_zone ;

	if (nr < 0 || nr > 2)
		return ;  /* ERROR */

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		lm85_update_client(client);
		results[0] = PWM_FROM_REG(data->pwm[nr]);
		pwm_zone = ZONE_FROM_REG(data->autofan[nr].config);
		/* PWM "enabled" if not off (0) nor on (-1) */
		results[1] = pwm_zone != 0 && pwm_zone != -1 ;
		*nrels_mag = 2;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		down(&data->update_lock);
		/* PWM enable is read-only */
		if (*nrels_mag > 0) {
			data->pwm[nr] = PWM_TO_REG(results[0]);
			lm85_write_value(client, LM85_REG_PWM(nr),
					 data->pwm[nr]);
		}
		up(&data->update_lock);
	}
}

void lm85_vid(struct i2c_client *client, int operation, int ctl_name,
	      int *nrels_mag, long *results)
{
	struct lm85_data *data = client->data;

	if( ctl_name != LM85_SYSCTL_VID )
		return ;  /* ERROR */

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 3;
	else if (operation == SENSORS_PROC_REAL_READ) {
		lm85_update_client(client);
		results[0] = vid_from_reg((data->vid)&0x3f, data->vrm);
		*nrels_mag = 1;
	}
}

void lm85_vrm(struct i2c_client *client, int operation, int ctl_name,
	      int *nrels_mag, long *results)
{
	struct lm85_data *data = client->data;

	if( ctl_name != LM85_SYSCTL_VRM )
		return ;  /* ERROR */

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 1;
	else if (operation == SENSORS_PROC_REAL_READ) {
		results[0] = data->vrm ;
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		down(&data->update_lock);
		if (*nrels_mag > 0) {
			data->vrm = results[0] ;
		}
		up(&data->update_lock);
	}
}

void lm85_alarms(struct i2c_client *client, int operation, int ctl_name,
		int *nrels_mag, long *results)
{
	struct lm85_data *data = client->data;

	if( ctl_name != LM85_SYSCTL_ALARMS )
		return ;  /* ERROR */

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		lm85_update_client(client);
		results[0] = data->alarms;
		*nrels_mag = 1;
	}
}

void lm85_spinup_ctl(struct i2c_client *client, int operation, int ctl_name,
		  int *nrels_mag, long *results)
{
	struct lm85_data *data = client->data;
	int old;

	if( ctl_name != LM85_SYSCTL_SPINUP_CTL )
		return ;  /* ERROR */

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		lm85_update_client(client);
		results[0] = (data->spinup_ctl & 1) != 0 ;
		results[1] = (data->spinup_ctl & 2) != 0 ;
		results[2] = (data->spinup_ctl & 4) != 0 ;
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		down(&data->update_lock);
		old = data->spinup_ctl ;
		if (*nrels_mag > 2) {
			old = (old & (~4)) | (results[2]?4:0) ;
		}
		if (*nrels_mag > 1) {
			old = (old & (~2)) | (results[1]?2:0) ;
		}
		if (*nrels_mag > 0) {
			old = (old & (~1)) | (results[0]?1:0) ;
			lm85_write_value(client, LM85_REG_SPINUP_CTL, old);
			data->spinup_ctl = old ;
		}
		up(&data->update_lock);
	}
}

void lm85_tach_mode(struct i2c_client *client, int operation, int ctl_name,
		  int *nrels_mag, long *results)
{
	struct lm85_data *data = client->data;
	int old;

	/* Tach Mode 1, Tach Mode 2, Tach Mode 3 & 4 */

	if( ctl_name != LM85_SYSCTL_TACH_MODE )
		return ;  /* ERROR */

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		lm85_update_client(client);
		results[0] = (data->tach_mode & 0x03) ;
		results[1] = (data->tach_mode & 0x0c) >> 2 ;
		results[2] = (data->tach_mode & 0x30) >> 4 ;
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		down(&data->update_lock);
		old = data->tach_mode ;
		if (*nrels_mag > 2) {
			old = (old & (~0x30)) | ((results[2]&3) << 4) ;
		}
		if (*nrels_mag > 1) {
			old = (old & (~0x0c)) | ((results[1]&3) << 2) ;
		}
		if (*nrels_mag > 0) {
			old = (old & (~0x03)) |  (results[0]&3) ;
			lm85_write_value(client, LM85_REG_TACH_MODE, old);
			data->tach_mode = old ;
		}
		up(&data->update_lock);
	}
}

void adm1027_tach_mode(struct i2c_client *client, int operation, int ctl_name,
		  int *nrels_mag, long *results)
{
	struct lm85_data *data = client->data;
	int old;

	/* Tach/DC 1, Tach/DC 2, Tach/DC 3, Tach/DC 4 */

	if( ctl_name != ADM1027_SYSCTL_TACH_MODE )
		return ;  /* ERROR */

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		lm85_update_client(client);
		results[0] = (data->tach_mode & 0x10) != 0 ;
		results[1] = (data->tach_mode & 0x20) != 0 ;
		results[2] = (data->tach_mode & 0x40) != 0 ;
		results[3] = (data->tach_mode & 0x80) != 0 ;
		*nrels_mag = 4;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		down(&data->update_lock);
		old = data->tach_mode ;
		if (*nrels_mag > 3) {
			old = (old & (~0x80)) | (results[3] ? 0x80 : 0) ;
		}
		if (*nrels_mag > 2) {
			old = (old & (~0x40)) | (results[2] ? 0x40 : 0) ;
		}
		if (*nrels_mag > 1) {
			old = (old & (~0x20)) | (results[1] ? 0x20 : 0) ;
		}
		if (*nrels_mag > 0) {
			old = (old & (~0x10)) | (results[0] ? 0x10 : 0) ;

			/* Enable fast measurements if any TACH's are DC */
			old = (old & (~0x08)) | ((old&0xf0) ? 0x08 : 0) ;

			lm85_write_value(client, ADM1027_REG_CONFIG3, old);
			data->tach_mode = old ;
		}
		up(&data->update_lock);
	}
}

void lm85_pwm_config(struct i2c_client *client, int operation, int ctl_name,
		  int *nrels_mag, long *results)
{
	struct lm85_data *data = client->data;
	int nr = ctl_name - LM85_SYSCTL_PWM_CFG1 ;

	/* Spinup, min PWM, PWM Frequency, min below limit, Invert */

	if (nr < 0 || nr > 2)
		return ;  /* ERROR */

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 1;
	else if (operation == SENSORS_PROC_REAL_READ) {
		lm85_update_client(client);

		results[0] = SPINUP_FROM_REG(data->autofan[nr].config);
		results[1] = PWM_FROM_REG(data->autofan[nr].min_pwm)*10;
		results[2] = FREQ_FROM_REG(data->autofan[nr].freq);
		results[3] = data->autofan[nr].min_off ? 10 : 0 ;
		results[4] = (data->autofan[nr].config & 0x10) ? 10 : 0 ;
		*nrels_mag = 5;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		int  old_config ;

		down(&data->update_lock);
		old_config = data->autofan[nr].config ;
		if (*nrels_mag > 4) {
			old_config = (old_config & (~0x10)) | (results[4]?0x10:0) ;
		}
		if (*nrels_mag > 3) {
			data->autofan[nr].min_off = results[3] != 0 ;
			lm85_write_value(client, LM85_REG_AFAN_SPIKE1,
				data->smooth[0]
				| data->syncpwm3
				| (data->autofan[0].min_off ? 0x20 : 0)
				| (data->autofan[1].min_off ? 0x40 : 0)
				| (data->autofan[2].min_off ? 0x80 : 0)
			);
		}
		if (*nrels_mag > 2) {
			data->autofan[nr].freq = FREQ_TO_REG(results[2]) ;
			lm85_write_value(client, LM85_REG_AFAN_RANGE(nr),
			    (data->zone[nr].range << 4)
			    | data->autofan[nr].freq
			);
		}
		if (*nrels_mag > 1) {
			data->autofan[nr].min_pwm = PWM_TO_REG((results[1]+5)/10);
			lm85_write_value(client, LM85_REG_AFAN_MINPWM(nr),
					data->autofan[nr].min_pwm
			);
		}
		if (*nrels_mag > 0) {
			old_config = (old_config & (~0x07)) | SPINUP_TO_REG(results[0]) ;
			lm85_write_value(client, LM85_REG_AFAN_CONFIG(nr), old_config);
			data->autofan[nr].config = old_config ;
		}
		up(&data->update_lock);
	}
}

void lm85_smooth(struct i2c_client *client, int operation, int ctl_name,
		  int *nrels_mag, long *results)
{
	struct lm85_data *data = client->data;
	int nr = ctl_name - LM85_SYSCTL_SMOOTH1 ;

	if (nr < 0 || nr > 2)
		return ;  /* ERROR */

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 1;
	else if (operation == SENSORS_PROC_REAL_READ) {
		lm85_update_client(client);
		results[0] = SMOOTH_FROM_REG(data->smooth[nr]);
		*nrels_mag = 1;

	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		down(&data->update_lock);
		if( *nrels_mag > 0 ) {
			data->smooth[nr] = SMOOTH_TO_REG(results[0]);
		}
		if( nr == 0 ) {
		    lm85_write_value(client, LM85_REG_AFAN_SPIKE1,
			data->smooth[0]
			| data->syncpwm3
			| (data->autofan[0].min_off ? 0x20 : 0)
			| (data->autofan[1].min_off ? 0x40 : 0)
			| (data->autofan[2].min_off ? 0x80 : 0)
		    );
		} else {
		    lm85_write_value(client, LM85_REG_AFAN_SPIKE2,
			(data->smooth[1] << 4) | data->smooth[2]);
		}
		up(&data->update_lock);
	}
}

void lm85_zone(struct i2c_client *client, int operation, int ctl_name,
		  int *nrels_mag, long *results)
{
	struct lm85_data *data = client->data;
	int nr = ctl_name - LM85_SYSCTL_ZONE1 ;

	/* Limit, Hysteresis (neg), Range, Critical */

	if (nr < 0 || nr > 2)
		return ;  /* ERROR */

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 1;
	else if (operation == SENSORS_PROC_REAL_READ) {
		lm85_update_client(client);

		results[0] = TEMP_FROM_REG(data->zone[nr].limit) / 10;
		results[1] = HYST_FROM_REG(data->zone[nr].hyst);
		results[2] = RANGE_FROM_REG(data->zone[nr].range);
		results[3] = TEMP_FROM_REG(data->zone[nr].critical) / 10;
		*nrels_mag = 4;

	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		down(&data->update_lock);
		if (*nrels_mag > 3) {
			data->zone[nr].critical = TEMP_TO_REG(results[3]*10);
			lm85_write_value(client, LM85_REG_AFAN_CRITICAL(nr),
				data->zone[nr].critical );
		}
		if (*nrels_mag > 2) {
			data->zone[nr].range = RANGE_TO_REG(results[2]);
			lm85_write_value(client, LM85_REG_AFAN_RANGE(nr),
			    (data->zone[nr].range << 4)
			    | data->autofan[nr].freq
			);
		}
		if (*nrels_mag > 1) {
			data->zone[nr].hyst = HYST_TO_REG(results[1]);
			if( nr == 0 || nr == 1 ) {
			    lm85_write_value(client, LM85_REG_AFAN_HYST1,
				(data->zone[0].hyst << 4)
				| data->zone[1].hyst
			    );
			} else {
			    lm85_write_value(client, LM85_REG_AFAN_HYST2,
				(data->zone[2].hyst << 4)
			    );
			}
		}
		if (*nrels_mag > 0) {
			data->zone[nr].limit = TEMP_TO_REG(results[0]*10);
			lm85_write_value(client, LM85_REG_AFAN_LIMIT(nr),
			    data->zone[nr].limit
			);
		}
		up(&data->update_lock);
	}
}

void lm85_pwm_zone(struct i2c_client *client, int operation, int ctl_name,
		int *nrels_mag, long *results)
{
	struct lm85_data *data = client->data;
	int nr = ctl_name - LM85_SYSCTL_PWM_ZONE1 ;

	if (nr < 0 || nr > 2)
		return ;  /* ERROR */

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		lm85_update_client(client);
		results[0] = ZONE_FROM_REG(data->autofan[nr].config);
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		down(&data->update_lock);
		if (*nrels_mag > 0) {
			data->autofan[nr].config =
			    (data->autofan[nr].config & (~0xe0))
			    | ZONE_TO_REG(results[0]) ;
			lm85_write_value(client, LM85_REG_AFAN_CONFIG(nr),
			    data->autofan[nr].config);
		}
		up(&data->update_lock);
	}
}

void adm1027_temp_offset(struct i2c_client *client, int operation, int ctl_name,
		int *nrels_mag, long *results)
{
	struct lm85_data *data = client->data;
	int nr = ctl_name - ADM1027_SYSCTL_TEMP_OFFSET1 ;

	if (nr < 0 || nr > 2)
		return ;  /* ERROR */

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 2;
	else if (operation == SENSORS_PROC_REAL_READ) {
		lm85_update_client(client);
		switch( data->type ) {
		case adm1027 :
		default :
			results[0] = TEMP_FROM_REG(data->temp_offset[nr]);
			break ;
		case adt7463 :
			results[0] = TEMPEXT_FROM_REG(0,data->temp_offset[nr]);
			break ;
		}
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		down(&data->update_lock);
		if (*nrels_mag > 0) {
			switch( data->type ) {
			case adm1027 :
			default :
			    data->temp_offset[nr] = TEMP_TO_REG(results[0]);
			    break ;
			case adt7463 :
			    data->temp_offset[nr] = EXTTEMP_TO_REG(results[0]);
			    break ;
			};
			lm85_write_value(client, ADM1027_REG_TEMP_OFFSET(nr),
			    data->temp_offset[nr]);
		}
		up(&data->update_lock);
	}
}

void adm1027_fan_ppr(struct i2c_client *client, int operation, int ctl_name,
		int *nrels_mag, long *results)
{
	struct lm85_data *data = client->data;
	int old ;

	if (ctl_name != ADM1027_SYSCTL_FAN_PPR)
		return ;  /* ERROR */

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		lm85_update_client(client);
		results[0] = PPR_FROM_REG(data->fan_ppr,0);
		results[1] = PPR_FROM_REG(data->fan_ppr,1);
		results[2] = PPR_FROM_REG(data->fan_ppr,2);
		results[3] = PPR_FROM_REG(data->fan_ppr,3);
		*nrels_mag = 4;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		down(&data->update_lock);
		old = data->fan_ppr ;
		if (*nrels_mag > 3) {
			old = (old & ~PPR_MASK(3)) | PPR_TO_REG(results[3],3);
		};
		if (*nrels_mag > 2) {
			old = (old & ~PPR_MASK(2)) | PPR_TO_REG(results[2],2);
		};
		if (*nrels_mag > 1) {
			old = (old & ~PPR_MASK(1)) | PPR_TO_REG(results[1],1);
		};
		if (*nrels_mag > 0) {
			old = (old & ~PPR_MASK(0)) | PPR_TO_REG(results[0],0);
			lm85_write_value(client, ADM1027_REG_FAN_PPR, old);
			data->fan_ppr = old ;
		}
		up(&data->update_lock);
	}
}

void adm1027_alarm_mask(struct i2c_client *client, int operation,
		int ctl_name, int *nrels_mag, long *results)
{
	struct lm85_data *data = client->data;

	if( ctl_name != ADM1027_SYSCTL_ALARM_MASK )
		return ;  /* ERROR */

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		lm85_update_client(client);
		results[0] = INTMASK_FROM_REG(data->alarm_mask);
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		down(&data->update_lock);
		if (*nrels_mag > 0) {
			data->alarm_mask = INTMASK_TO_REG(results[0]);
			lm85_write_value(client, ADM1027_REG_INTMASK,
			    data->alarm_mask);
		}
		up(&data->update_lock);
	}
}

void adt7463_tmin_ctl(struct i2c_client *client, int operation, int ctl_name,
		int *nrels_mag, long *results)
{
	struct lm85_data *data = client->data;
	int nr = ctl_name - ADT7463_SYSCTL_TMIN_CTL1 ;
	u16 old ;

	if (nr < 0 || nr > 2)
		return ;  /* ERROR */

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		old = data->tmin_ctl ;
		results[0] = (old & ( 0x2000 << nr )) != 0 ;
		results[1] = (old >> (nr*3)) & 0x07  ;
		results[2] = (old & ( 0x0400 << nr )) != 0 ;
		results[3] = OPPOINT_FROM_REG(data->oppoint[nr]);
		*nrels_mag = 4;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		down(&data->update_lock);
		old = data->tmin_ctl ;
		if (*nrels_mag > 3) {
			data->oppoint[nr] = OPPOINT_TO_REG(results[3]);
			lm85_write_value(client, ADT7463_REG_OPPOINT(nr),
			    data->oppoint[nr]);
		};
		if (*nrels_mag > 2) {
			if( results[2] ) {
				old |= (0x0400 << nr) ;
			} else {
				old &= ~(0x0400 << nr) ;
			}
		};
		if (*nrels_mag > 1) {
			old &= ~(0x07 << (nr*3)) ;
			old |= (results[1] & 0x07) << (nr*3) ;
		};
		if (*nrels_mag > 0) {
			if( results[0] ) {
				old |= 0x2000 << nr ;
			} else {
				old &= ~(0x2000 << nr) ;
			}
			lm85_write_value(client, ADT7463_REG_TMIN_CTL, old);
			data->tmin_ctl = old ;
		}
		up(&data->update_lock);
	}
}

void adt7463_therm_signal(struct i2c_client *client, int operation,
		int ctl_name, int *nrels_mag, long *results)
{
	struct lm85_data *data = client->data;
	int counts ;

	if (ctl_name != ADT7463_SYSCTL_THERM_SIGNAL)
		return ;  /* ERROR */

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		/* Don't call update_client here because
		 *   ADT7463_REG_THERM has to be read every
		 *   5 seconds to prevent lost counts
		 */
		down(&data->update_lock);
		counts = lm85_read_value(client, ADT7463_REG_THERM) & 0xff;
		if( data->therm_total < LONG_MAX - 256 ) {
		    data->therm_total += counts ;
		}
		if( counts >= 255 ) {
		    ++data->therm_ovfl ;
		}
		up(&data->update_lock);

		results[0] = data->therm_limit ;
		results[1] = data->therm_total ;
		results[2] = data->therm_ovfl ;
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		down(&data->update_lock);
		/* therm_total and therm_ovfl are read only */
		if (*nrels_mag > 0) {
			data->therm_limit = SENSORS_LIMIT(results[0],0,255);
			lm85_write_value(client, ADT7463_REG_THERM_LIMIT,
			    data->therm_limit);
		};
		up(&data->update_lock);
	}
}


void emc6d100_in(struct i2c_client *client, int operation, int ctl_name,
	     int *nrels_mag, long *results)
{
	struct lm85_data *data = client->data;
	int nr = ctl_name - EMC6D100_SYSCTL_IN5 +5;

	if (nr < 5 || nr > 7)
		return ;  /* ERROR */

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 3;  /* 1.000 */
	else if (operation == SENSORS_PROC_REAL_READ) {
		lm85_update_client(client);
		results[0] = INS_FROM_REG(nr,data->in_min[nr]);
		results[1] = INS_FROM_REG(nr,data->in_max[nr]);
		results[2] = INS_FROM_REG(nr,data->in[nr]);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		down(&data->update_lock);
		if (*nrels_mag > 1) {
			data->in_max[nr] = INS_TO_REG(nr,results[1]);
			lm85_write_value(client, EMC6D100_REG_IN_MAX(nr),
					 data->in_max[nr]);
		}
		if (*nrels_mag > 0) {
			data->in_min[nr] = INS_TO_REG(nr,results[0]);
			lm85_write_value(client, EMC6D100_REG_IN_MIN(nr),
					 data->in_min[nr]);
		}
		up(&data->update_lock);
	}
}


static int __init sm_lm85_init(void)
{
	printk("lm85: Version %s (%s)\n", LM_VERSION, LM_DATE);
	return i2c_add_driver(&lm85_driver);
}

static void __exit sm_lm85_exit(void)
{
	i2c_del_driver(&lm85_driver);
}

/* Thanks to Richard Barrington for adding the LM85 to sensors-detect.
 * Thanks to Margit Schubert-While <margitsw@t-online.de> for help with
 *     post 2.7.0 CVS changes
 */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Philip Pokorny <ppokorny@penguincomputing.com");
MODULE_DESCRIPTION("LM85-B, LM85-C driver");

module_init(sm_lm85_init);
module_exit(sm_lm85_exit);
