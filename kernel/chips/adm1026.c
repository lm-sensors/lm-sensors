/*
    adm1026.c - Part of lm_sensors, Linux kernel modules for hardware
             monitoring
    Copyright (c) 2002, 2003  Philip Pokorny <ppokorny@penguincomputing.com>

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

    2003-03-13   Initial development
    2003-05-07   First Release.  Includes GPIO fixup and full
                 functionality.
    2003-05-18   Minor fixups and tweaks.
                 Print GPIO config after fixup.
                 Adjust fan MIN if DIV changes.
    2003-05-21   Fix printing of FAN/GPIO config
                 Fix silly bug in fan_div logic
                 Fix fan_min handling so that 0xff is 0 is 0xff
    2003-05-25   Fix more silly typos...
    2003-06-11   Change FAN_xx_REG macros to use different scaling
                 Most (all?) drivers assume two pulses per rev fans
                 and the old scaling was producing double the RPM's
                 Thanks to Jerome Hsiao @ Arima for pointing this out.
    2004-01-27   Remove use of temporary ID.
                 Define addresses as a range.
*/

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/i2c-proc.h>
#include <linux/init.h>
#include "version.h"
#include "sensors_vid.h"

#ifndef I2C_DRIVERID_ADM1026
#define I2C_DRIVERID_ADM1026	1048
#endif

/* Addresses to scan */
static unsigned short normal_i2c[] = { SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { 0x2c, 0x2e, SENSORS_I2C_END };
static unsigned int normal_isa[] = { SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/* Insmod parameters */
SENSORS_INSMOD_1(adm1026);

static int gpio_input[17]  = { -1, -1, -1, -1, -1, -1, -1, -1, -1,
				-1, -1, -1, -1, -1, -1, -1, -1 };
static int gpio_output[17] = { -1, -1, -1, -1, -1, -1, -1, -1, -1,
				-1, -1, -1, -1, -1, -1, -1, -1 };
static int gpio_inverted[17] = { -1, -1, -1, -1, -1, -1, -1, -1, -1,
				-1, -1, -1, -1, -1, -1, -1, -1 };
static int gpio_normal[17] = { -1, -1, -1, -1, -1, -1, -1, -1, -1,
				-1, -1, -1, -1, -1, -1, -1, -1 };
static int gpio_fan[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
MODULE_PARM(gpio_input,"1-17i");
MODULE_PARM_DESC(gpio_input,"List of GPIO pins (0-16) to program as inputs");
MODULE_PARM(gpio_output,"1-17i");
MODULE_PARM_DESC(gpio_output,"List of GPIO pins (0-16) to program as outputs");
MODULE_PARM(gpio_inverted,"1-17i");
MODULE_PARM_DESC(gpio_inverted,"List of GPIO pins (0-16) to program as inverted");
MODULE_PARM(gpio_normal,"1-17i");
MODULE_PARM_DESC(gpio_normal,"List of GPIO pins (0-16) to program as normal/non-inverted");
MODULE_PARM(gpio_fan,"1-8i");
MODULE_PARM_DESC(gpio_fan,"List of GPIO pins (0-7) to program as fan tachs");

/* Many ADM1026 constants specified below */

/* The ADM1026 registers */
#define ADM1026_REG_CONFIG1  (0x00)
#define CFG1_MONITOR     (0x01)
#define CFG1_INT_ENABLE  (0x02)
#define CFG1_INT_CLEAR   (0x04)
#define CFG1_AIN8_9      (0x08)
#define CFG1_THERM_HOT   (0x10)
#define CFG1_DAC_AFC     (0x20)
#define CFG1_PWM_AFC     (0x40)
#define CFG1_RESET       (0x80)
#define ADM1026_REG_CONFIG2  (0x01)
/* CONFIG2 controls FAN0/GPIO0 through FAN7/GPIO7 */
#define ADM1026_REG_CONFIG3  (0x07)
#define CFG3_GPIO16_ENABLE  (0x01)
#define CFG3_CI_CLEAR  (0x02)
#define CFG3_VREF_250  (0x04)
#define CFG3_GPIO16_DIR  (0x40)
#define CFG3_GPIO16_POL  (0x80)
#define ADM1026_REG_E2CONFIG  (0x13)
#define E2CFG_READ  (0x01)
#define E2CFG_WRITE  (0x02)
#define E2CFG_ERASE  (0x04)
#define E2CFG_ROM  (0x08)
#define E2CFG_CLK_EXT  (0x80)

/* There are 10 general analog inputs and 7 dedicated inputs
 * They are:
 *    0 - 9  =  AIN0 - AIN9
 *       10  =  Vbat
 *       11  =  3.3V Standby
 *       12  =  3.3V Main
 *       13  =  +5V
 *       14  =  Vccp (CPU core voltage)
 *       15  =  +12V
 *       16  =  -12V
 */
static u16 REG_IN[] = {
		0x30, 0x31, 0x32, 0x33, 0x34, 0x35,
		0x36, 0x37, 0x27, 0x29, 0x26, 0x2a,
		0x2b, 0x2c, 0x2d, 0x2e, 0x2f
	};
static u16 REG_IN_MIN[] = {
		0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d,
		0x5e, 0x5f, 0x6d, 0x49, 0x6b, 0x4a,
		0x4b, 0x4c, 0x4d, 0x4e, 0x4f
	};
static u16 REG_IN_MAX[] = {
		0x50, 0x51, 0x52, 0x53, 0x54, 0x55,
		0x56, 0x57, 0x6c, 0x41, 0x6a, 0x42,
		0x43, 0x44, 0x45, 0x46, 0x47
	};
#define ADM1026_REG_IN(nr) (REG_IN[(nr)])
#define ADM1026_REG_IN_MIN(nr) (REG_IN_MIN[(nr)])
#define ADM1026_REG_IN_MAX(nr) (REG_IN_MAX[(nr)])

/* Temperatures are:
 *    0 - Internal
 *    1 - External 1
 *    2 - External 2
 */
static u16 REG_TEMP[] = { 0x1f, 0x28, 0x29 };
static u16 REG_TEMP_MIN[] = { 0x69, 0x48, 0x49 };
static u16 REG_TEMP_MAX[] = { 0x68, 0x40, 0x41 };
static u16 REG_TEMP_TMIN[] = { 0x10, 0x11, 0x12 };
static u16 REG_TEMP_THERM[] = { 0x0d, 0x0e, 0x0f };
static u16 REG_TEMP_OFFSET[] = { 0x1e, 0x6e, 0x6f };
#define ADM1026_REG_TEMP(nr) (REG_TEMP[(nr)])
#define ADM1026_REG_TEMP_MIN(nr) (REG_TEMP_MIN[(nr)])
#define ADM1026_REG_TEMP_MAX(nr) (REG_TEMP_MAX[(nr)])
#define ADM1026_REG_TEMP_TMIN(nr) (REG_TEMP_TMIN[(nr)])
#define ADM1026_REG_TEMP_THERM(nr) (REG_TEMP_THERM[(nr)])
#define ADM1026_REG_TEMP_OFFSET(nr) (REG_TEMP_OFFSET[(nr)])

#define ADM1026_REG_FAN(nr) (0x38 + (nr))
#define ADM1026_REG_FAN_MIN(nr) (0x60 + (nr))
#define ADM1026_REG_FAN_DIV_0_3 (0x02)
#define ADM1026_REG_FAN_DIV_4_7 (0x03)

#define ADM1026_REG_DAC  (0x04)
#define ADM1026_REG_PWM  (0x05)

#define ADM1026_REG_GPIO_CFG_0_3 (0x08)
#define ADM1026_REG_GPIO_CFG_4_7 (0x09)
#define ADM1026_REG_GPIO_CFG_8_11 (0x0a)
#define ADM1026_REG_GPIO_CFG_12_15 (0x0b)
/* CFG_16 in REG_CFG3 */
#define ADM1026_REG_GPIO_STATUS_0_7 (0x24)
#define ADM1026_REG_GPIO_STATUS_8_15 (0x25)
/* STATUS_16 in REG_STATUS4 */
#define ADM1026_REG_GPIO_MASK_0_7 (0x1c)
#define ADM1026_REG_GPIO_MASK_8_15 (0x1d)
/* MASK_16 in REG_MASK4 */

#define ADM1026_REG_COMPANY 0x16
#define ADM1026_REG_VERSTEP 0x17
/* These are the recognized values for the above regs */
#define ADM1026_COMPANY_ANALOG_DEV 0x41
#define ADM1026_VERSTEP_GENERIC 0x40
#define ADM1026_VERSTEP_ADM1026 0x44

#define ADM1026_REG_MASK1 0x18
#define ADM1026_REG_MASK2 0x19
#define ADM1026_REG_MASK3 0x1a
#define ADM1026_REG_MASK4 0x1b

#define ADM1026_REG_STATUS1 0x20
#define ADM1026_REG_STATUS2 0x21
#define ADM1026_REG_STATUS3 0x22
#define ADM1026_REG_STATUS4 0x23

/* Conversions. Rounding and limit checking is only done on the TO_REG 
   variants. Note that you should be a bit careful with which arguments
   these macros are called: arguments may be evaluated more than once.
 */

/* IN are scaled acording to built-in resistors.  These are the
 *   voltages corresponding to 3/4 of full scale (192 or 0xc0)
 *   NOTE: The -12V input needs an additional factor to account
 *      for the Vref pullup resistor.
 *      NEG12_OFFSET = SCALE * Vref / V-192 - Vref
 *                   = 13875 * 2.50 / 1.875 - 2500
 *                   = 16000
 */
#if 1
/* The values in this table are based on Table II, page 15 of the
 *    datasheet.
 */
static int adm1026_scaling[] = {  /* .001 Volts */
		2250, 2250, 2250, 2250, 2250, 2250, 
		1875, 1875, 1875, 1875, 3000, 3330, 
		3330, 4995, 2250, 12000, 13875
	};
#define NEG12_OFFSET  16000
#else
/* The values in this table are based on the resistors in 
 *    Figure 5 on page 16.  But the 3.3V inputs are not in
 *    the figure and the values for the 5V input are wrong.
 *    For 5V, I'm guessing that R2 at 55.2k is right, but
 *    the total resistance should be 1400 or 1449 like the
 *    other inputs.  Using 1449, gives 4.922V at 192.
 */
static int adm1026_scaling[] = {  /* .001 Volts */
		2249, 2249, 2249, 2249, 2249, 2249, 
		1875, 1875, 1875, 1875, 3329, 3329, 
		3329, 4922, 2249, 11969, 13889
	};
#define NEG12_OFFSET  16019
#endif

#define SCALE(val,from,to) (((val)*(to) + ((from)/2))/(from))
#define INS_TO_REG(n,val)  (SENSORS_LIMIT(SCALE(val,adm1026_scaling[n],192),0,255))
#if 0   /* If we have extended A/D bits */
#define INSEXT_FROM_REG(n,val,ext) (SCALE((val)*4 + (ext),192*4,adm1026_scaling[n]))
#define INS_FROM_REG(n,val) (INSEXT_FROM_REG(n,val,0))
#else
#define INS_FROM_REG(n,val) (SCALE(val,192,adm1026_scaling[n]))
#endif

/* FAN speed is measured using 22.5kHz clock and counts for 2 pulses
 *   and we assume a 2 pulse-per-rev fan tach signal
 *      22500 kHz * 60 (sec/min) * 2 (pulse) / 2 (pulse/rev) == 1350000
 */
#define FAN_TO_REG(val,div)  ((val)<=0 ? 0xff : SENSORS_LIMIT(1350000/((val)*(div)),1,254))
#define FAN_FROM_REG(val,div) ((val)==0?-1:(val)==0xff ? 0 : 1350000/((val)*(div)))
#define DIV_FROM_REG(val) (1<<(val))
#define DIV_TO_REG(val) ((val)>=8 ? 3 : (val)>=4 ? 2 : (val)>=2 ? 1 : 0)

/* Temperature is reported in 1 degC increments */
#define TEMP_TO_REG(val) (SENSORS_LIMIT(val,-127,127))
#define TEMP_FROM_REG(val) (val)
#define OFFSET_TO_REG(val) (SENSORS_LIMIT(val,-127,127))
#define OFFSET_FROM_REG(val) (val)

#define PWM_TO_REG(val) (SENSORS_LIMIT(val,0,255))
#define PWM_FROM_REG(val) (val)

/* Analog output is a voltage, but it's used like a PWM
 *   Seems like this should be scaled, but to be consistent
 *   with other drivers, we do it this way.
 */
#define DAC_TO_REG(val) (SENSORS_LIMIT(val,0,255))
#define DAC_FROM_REG(val) (val)

#define ALARMS_FROM_REG(val) (val)

/* Unlike some other drivers we DO NOT set initial limits.  Use
 * the config file to set limits.
 */

/* Typically used with systems using a v9.1 VRM spec ? */
#define ADM1026_INIT_VRM  91
#define ADM1026_INIT_VID  -1

/* Chip sampling rates
 *
 * Some sensors are not updated more frequently than once per second
 *    so it doesn't make sense to read them more often than that.
 *    We cache the results and return the saved data if the driver
 *    is called again before a second has elapsed.
 *
 * Also, there is significant configuration data for this chip
 *    So, we keep the config data up to date in the cache
 *    when it is written and only sample it once every 5 *minutes*
 */
#define ADM1026_DATA_INTERVAL  (1 * HZ)
#define ADM1026_CONFIG_INTERVAL  (5 * 60 * HZ)

/* We allow for multiple chips in a single system.
 *
 * For each registered ADM1026, we need to keep state information
 * at client->data. The adm1026_data structure is dynamically
 * allocated, when a new client structure is allocated. */

struct adm1026_data {
	struct i2c_client client;
	int sysctl_id;

	struct semaphore update_lock;
	int valid;		/* !=0 if following fields are valid */
	unsigned long last_reading;	/* In jiffies */
	unsigned long last_config;	/* In jiffies */

	u8 in[17];		/* Register value */
	u8 in_max[17];		/* Register value */
	u8 in_min[17];		/* Register value */
	s8 temp[3];		/* Register value */
	s8 temp_min[3];		/* Register value */
	s8 temp_max[3];		/* Register value */
	s8 temp_tmin[3];	/* Register value */
	s8 temp_therm[3];	/* Register value */
	s8 temp_offset[3];	/* Register value */
	u8 fan[8];		/* Register value */
	u8 fan_min[8];		/* Register value */
	u8 fan_div[8];		/* Decoded value */
	u8 pwm;			/* Register value */
	u8 analog_out;		/* Register value */
	int vid;		/* Decoded value */
	u8 vrm;			/* VRM version */
	long alarms;		/* Register encoding, combined */
	long alarm_mask;	/* Register encoding, combined */
	long gpio;		/* Register encoding, combined */
	long gpio_mask;		/* Register encoding, combined */
	u8 gpio_config[17];	/* Decoded value */
	u8 config1;		/* Register value */
	u8 config2;		/* Register value */
	u8 config3;		/* Register value */
};

static int adm1026_attach_adapter(struct i2c_adapter *adapter);
static int adm1026_detect(struct i2c_adapter *adapter, int address,
			unsigned short flags, int kind);
static int adm1026_detach_client(struct i2c_client *client);

static int adm1026_read_value(struct i2c_client *client, u8 reg);
static int adm1026_write_value(struct i2c_client *client, u8 reg, int value);
static void adm1026_print_gpio(struct i2c_client *client);
static void adm1026_fixup_gpio(struct i2c_client *client);
static void adm1026_update_client(struct i2c_client *client);
static void adm1026_init_client(struct i2c_client *client);


static void adm1026_in(struct i2c_client *client, int operation, int ctl_name,
			int *nrels_mag, long *results);
static void adm1026_in16(struct i2c_client *client, int operation, int ctl_name,
			int *nrels_mag, long *results);
static void adm1026_fan(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void adm1026_fixup_fan_min(struct i2c_client *client,
			 int fan, int old_div);
static void adm1026_fan_div(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void adm1026_temp(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void adm1026_temp_offset(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void adm1026_temp_tmin(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void adm1026_temp_therm(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void adm1026_vid(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void adm1026_vrm(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void adm1026_alarms(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void adm1026_alarm_mask(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void adm1026_gpio(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void adm1026_gpio_mask(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void adm1026_pwm(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void adm1026_analog_out(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void adm1026_afc(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);

static struct i2c_driver adm1026_driver = {
	.name		= "ADM1026 compatible sensor driver",
	.id		= I2C_DRIVERID_ADM1026,
	.flags		= I2C_DF_NOTIFY,
	.attach_adapter	= &adm1026_attach_adapter,
	.detach_client	= &adm1026_detach_client,
};

/* Unique ID assigned to each ADM1026 detected */
static int adm1026_id = 0;

/* -- SENSORS SYSCTL START -- */
#define ADM1026_SYSCTL_FAN0                 1000
#define ADM1026_SYSCTL_FAN1                 1001
#define ADM1026_SYSCTL_FAN2                 1002
#define ADM1026_SYSCTL_FAN3                 1003
#define ADM1026_SYSCTL_FAN4                 1004
#define ADM1026_SYSCTL_FAN5                 1005
#define ADM1026_SYSCTL_FAN6                 1006
#define ADM1026_SYSCTL_FAN7                 1007
#define ADM1026_SYSCTL_FAN_DIV              1008
#define ADM1026_SYSCTL_GPIO                 1009
#define ADM1026_SYSCTL_GPIO_MASK            1010
#define ADM1026_SYSCTL_ALARMS               1011
#define ADM1026_SYSCTL_ALARM_MASK           1012
#define ADM1026_SYSCTL_IN0                  1013
#define ADM1026_SYSCTL_IN1                  1014
#define ADM1026_SYSCTL_IN2                  1015
#define ADM1026_SYSCTL_IN3                  1016
#define ADM1026_SYSCTL_IN4                  1017
#define ADM1026_SYSCTL_IN5                  1018
#define ADM1026_SYSCTL_IN6                  1019
#define ADM1026_SYSCTL_IN7                  1020
#define ADM1026_SYSCTL_IN8                  1021
#define ADM1026_SYSCTL_IN9                  1022
#define ADM1026_SYSCTL_IN10                 1023
#define ADM1026_SYSCTL_IN11                 1024
#define ADM1026_SYSCTL_IN12                 1025
#define ADM1026_SYSCTL_IN13                 1026
#define ADM1026_SYSCTL_IN14                 1027
#define ADM1026_SYSCTL_IN15                 1028
#define ADM1026_SYSCTL_IN16                 1029
#define ADM1026_SYSCTL_PWM                  1030
#define ADM1026_SYSCTL_ANALOG_OUT           1031
#define ADM1026_SYSCTL_AFC                  1032
#define ADM1026_SYSCTL_TEMP1                1033
#define ADM1026_SYSCTL_TEMP2                1034
#define ADM1026_SYSCTL_TEMP3                1035
#define ADM1026_SYSCTL_TEMP_OFFSET1         1036
#define ADM1026_SYSCTL_TEMP_OFFSET2         1037
#define ADM1026_SYSCTL_TEMP_OFFSET3         1038
#define ADM1026_SYSCTL_TEMP_THERM1          1039
#define ADM1026_SYSCTL_TEMP_THERM2          1040
#define ADM1026_SYSCTL_TEMP_THERM3          1041
#define ADM1026_SYSCTL_TEMP_TMIN1           1042
#define ADM1026_SYSCTL_TEMP_TMIN2           1043
#define ADM1026_SYSCTL_TEMP_TMIN3           1044
#define ADM1026_SYSCTL_VID                  1045
#define ADM1026_SYSCTL_VRM                  1046

#define ADM1026_ALARM_TEMP2   (1L <<  0)
#define ADM1026_ALARM_TEMP3   (1L <<  1)
#define ADM1026_ALARM_IN9     (1L <<  1)
#define ADM1026_ALARM_IN11    (1L <<  2)
#define ADM1026_ALARM_IN12    (1L <<  3)
#define ADM1026_ALARM_IN13    (1L <<  4)
#define ADM1026_ALARM_IN14    (1L <<  5)
#define ADM1026_ALARM_IN15    (1L <<  6)
#define ADM1026_ALARM_IN16    (1L <<  7)
#define ADM1026_ALARM_IN0     (1L <<  8)
#define ADM1026_ALARM_IN1     (1L <<  9)
#define ADM1026_ALARM_IN2     (1L << 10)
#define ADM1026_ALARM_IN3     (1L << 11)
#define ADM1026_ALARM_IN4     (1L << 12)
#define ADM1026_ALARM_IN5     (1L << 13)
#define ADM1026_ALARM_IN6     (1L << 14)
#define ADM1026_ALARM_IN7     (1L << 15)
#define ADM1026_ALARM_FAN0    (1L << 16)
#define ADM1026_ALARM_FAN1    (1L << 17)
#define ADM1026_ALARM_FAN2    (1L << 18)
#define ADM1026_ALARM_FAN3    (1L << 19)
#define ADM1026_ALARM_FAN4    (1L << 20)
#define ADM1026_ALARM_FAN5    (1L << 21)
#define ADM1026_ALARM_FAN6    (1L << 22)
#define ADM1026_ALARM_FAN7    (1L << 23)
#define ADM1026_ALARM_TEMP1   (1L << 24)
#define ADM1026_ALARM_IN10    (1L << 25)
#define ADM1026_ALARM_IN8     (1L << 26)
#define ADM1026_ALARM_THERM   (1L << 27)
#define ADM1026_ALARM_AFC_FAN (1L << 28)
#define ADM1026_ALARM_CI      (1L << 30)
/* -- SENSORS SYSCTL END -- */

/* The /proc/sys entries */
/* These files are created for each detected ADM1026. This is just a template;
 *    The actual list is built from this and additional per-chip
 *    custom lists below.  Note the XXX_LEN macros.  These must be
 *    compile time constants because they will be used to allocate
 *    space for the final template passed to i2c_register_entry.
 *    We depend on the ability of GCC to evaluate expressions at
 *    compile time to turn these expressions into compile time
 *    constants, but this can generate a warning.
 */
static ctl_table adm1026_common[] = {
	{ADM1026_SYSCTL_IN0, "in0", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &adm1026_in},
	{ADM1026_SYSCTL_IN1, "in1", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &adm1026_in},
	{ADM1026_SYSCTL_IN2, "in2", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &adm1026_in},
	{ADM1026_SYSCTL_IN3, "in3", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &adm1026_in},
	{ADM1026_SYSCTL_IN4, "in4", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &adm1026_in},
	{ADM1026_SYSCTL_IN5, "in5", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &adm1026_in},
	{ADM1026_SYSCTL_IN6, "in6", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &adm1026_in},
	{ADM1026_SYSCTL_IN7, "in7", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &adm1026_in},
	{ADM1026_SYSCTL_IN8, "in8", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &adm1026_in},
	{ADM1026_SYSCTL_IN9, "in9", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &adm1026_in},
	{ADM1026_SYSCTL_IN10, "in10", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &adm1026_in},
	{ADM1026_SYSCTL_IN11, "in11", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &adm1026_in},
	{ADM1026_SYSCTL_IN12, "in12", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &adm1026_in},
	{ADM1026_SYSCTL_IN13, "in13", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &adm1026_in},
	{ADM1026_SYSCTL_IN14, "in14", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &adm1026_in},
	{ADM1026_SYSCTL_IN15, "in15", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &adm1026_in},
	{ADM1026_SYSCTL_IN16, "in16", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &adm1026_in16},

	{ADM1026_SYSCTL_FAN0, "fan0", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &adm1026_fan},
	{ADM1026_SYSCTL_FAN1, "fan1", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &adm1026_fan},
	{ADM1026_SYSCTL_FAN2, "fan2", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &adm1026_fan},
	{ADM1026_SYSCTL_FAN3, "fan3", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &adm1026_fan},
	{ADM1026_SYSCTL_FAN4, "fan4", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &adm1026_fan},
	{ADM1026_SYSCTL_FAN5, "fan5", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &adm1026_fan},
	{ADM1026_SYSCTL_FAN6, "fan6", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &adm1026_fan},
	{ADM1026_SYSCTL_FAN7, "fan7", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &adm1026_fan},
	{ADM1026_SYSCTL_FAN_DIV, "fan_div", NULL, 0, 0644, NULL,
		&i2c_proc_real, &i2c_sysctl_real, NULL, &adm1026_fan_div},

	{ADM1026_SYSCTL_TEMP1, "temp1", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &adm1026_temp},
	{ADM1026_SYSCTL_TEMP2, "temp2", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &adm1026_temp},
	{ADM1026_SYSCTL_TEMP3, "temp3", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &adm1026_temp},
	{ADM1026_SYSCTL_TEMP_OFFSET1, "temp1_offset", NULL, 0, 0644, NULL,
		&i2c_proc_real, &i2c_sysctl_real, NULL, &adm1026_temp_offset},
	{ADM1026_SYSCTL_TEMP_OFFSET2, "temp2_offset", NULL, 0, 0644, NULL,
		&i2c_proc_real, &i2c_sysctl_real, NULL, &adm1026_temp_offset},
	{ADM1026_SYSCTL_TEMP_OFFSET3, "temp3_offset", NULL, 0, 0644, NULL,
		&i2c_proc_real, &i2c_sysctl_real, NULL, &adm1026_temp_offset},
	{ADM1026_SYSCTL_TEMP_TMIN1, "temp1_tmin", NULL, 0, 0644, NULL,
		&i2c_proc_real, &i2c_sysctl_real, NULL, &adm1026_temp_tmin},
	{ADM1026_SYSCTL_TEMP_TMIN2, "temp2_tmin", NULL, 0, 0644, NULL,
		&i2c_proc_real, &i2c_sysctl_real, NULL, &adm1026_temp_tmin},
	{ADM1026_SYSCTL_TEMP_TMIN3, "temp3_tmin", NULL, 0, 0644, NULL,
		&i2c_proc_real, &i2c_sysctl_real, NULL, &adm1026_temp_tmin},
	{ADM1026_SYSCTL_TEMP_THERM1, "temp1_therm", NULL, 0, 0644, NULL,
		&i2c_proc_real, &i2c_sysctl_real, NULL, &adm1026_temp_therm},
	{ADM1026_SYSCTL_TEMP_THERM2, "temp2_therm", NULL, 0, 0644, NULL,
		&i2c_proc_real, &i2c_sysctl_real, NULL, &adm1026_temp_therm},
	{ADM1026_SYSCTL_TEMP_THERM3, "temp3_therm", NULL, 0, 0644, NULL,
		&i2c_proc_real, &i2c_sysctl_real, NULL, &adm1026_temp_therm},

	{ADM1026_SYSCTL_VID, "vid", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &adm1026_vid},
	{ADM1026_SYSCTL_VRM, "vrm", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &adm1026_vrm},

	{ADM1026_SYSCTL_ALARMS, "alarms", NULL, 0, 0444, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &adm1026_alarms},
	{ADM1026_SYSCTL_ALARM_MASK, "alarm_mask", NULL, 0, 0644, NULL,
		&i2c_proc_real, &i2c_sysctl_real, NULL, &adm1026_alarm_mask},

	{ADM1026_SYSCTL_GPIO, "gpio", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &adm1026_gpio},
	{ADM1026_SYSCTL_GPIO_MASK, "gpio_mask", NULL, 0, 0644, NULL,
		&i2c_proc_real, &i2c_sysctl_real, NULL, &adm1026_gpio_mask},

	{ADM1026_SYSCTL_PWM, "pwm", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &adm1026_pwm},
	{ADM1026_SYSCTL_ANALOG_OUT, "analog_out", NULL, 0, 0644, NULL,
		&i2c_proc_real, &i2c_sysctl_real, NULL, &adm1026_analog_out},
	{ADM1026_SYSCTL_AFC, "afc", NULL, 0, 0644, NULL, &i2c_proc_real,
		&i2c_sysctl_real, NULL, &adm1026_afc},

	{0}
};
#define CTLTBL_COMMON (sizeof(adm1026_common)/sizeof(adm1026_common[0]))

#define MAX2(a,b) ((a)>(b)?(a):(b))
#define MAX3(a,b,c) ((a)>(b)?MAX2((a),(c)):MAX2((b),(c)))
#define MAX4(a,b,c,d) ((a)>(b)?MAX3((a),(c),(d)):MAX3((b),(c),(d)))

#define CTLTBL_MAX (CTLTBL_COMMON)

/* This function is called when:
     * the module is loaded
     * a new adapter is loaded
 */
static int adm1026_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_detect(adapter, &addr_data, adm1026_detect);
}

/* This function is called by i2c_detect */
static int adm1026_detect(struct i2c_adapter *adapter, int address,
			  unsigned short flags, int kind)
{
	int i;
	int company, verstep ;
	struct i2c_client *new_client;
	struct adm1026_data *data;
	int err = 0;
	const char *type_name = "";
	struct ctl_table template[CTLTBL_MAX] ;
	struct ctl_table * template_next = template ;

	if (i2c_is_isa_adapter(adapter)) {
		/* This chip has no ISA interface */
		goto ERROR0 ;
	}

	if (!i2c_check_functionality(adapter,
					I2C_FUNC_SMBUS_BYTE_DATA)) {
		/* We need to be able to do byte I/O */
		goto ERROR0 ;
	}

	/* OK. For now, we presume we have a valid client. We now create the
	   client structure, even though we cannot fill it completely yet.
	   But it allows us to access adm1026_{read,write}_value. */

	if (!(data = kmalloc(sizeof(struct adm1026_data), GFP_KERNEL))) {
		err = -ENOMEM;
		goto ERROR0;
	}

	new_client = &data->client;
	new_client->addr = address;
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &adm1026_driver;
	new_client->flags = 0;

	/* Now, we do the remaining detection. */

	company = adm1026_read_value(new_client, ADM1026_REG_COMPANY);
	verstep = adm1026_read_value(new_client, ADM1026_REG_VERSTEP);

#ifdef DEBUG
	printk("adm1026: Detecting device at %d,0x%02x with"
		" COMPANY: 0x%02x and VERSTEP: 0x%02x\n",
		i2c_adapter_id(new_client->adapter), new_client->addr,
		company, verstep
	    );
#endif

	/* If auto-detecting, Determine the chip type. */
	if (kind <= 0) {
#ifdef DEBUG
		printk("adm1026: Autodetecting device at %d,0x%02x ...\n",
			i2c_adapter_id(adapter), address );
#endif
		if( company == ADM1026_COMPANY_ANALOG_DEV
		    && verstep == ADM1026_VERSTEP_ADM1026 ) {
			kind = adm1026 ;
		} else if( company == ADM1026_COMPANY_ANALOG_DEV
		    && (verstep & 0xf0) == ADM1026_VERSTEP_GENERIC ) {
			printk("adm1026: Unrecognized stepping 0x%02x"
			    " Defaulting to ADM1026.\n", verstep );
			kind = adm1026 ;
		} else if( (verstep & 0xf0) == ADM1026_VERSTEP_GENERIC ) {
			printk("adm1026: Found version/stepping 0x%02x"
			    " Assuming generic ADM1026.\n", verstep );
			kind = any_chip ;
		} else {
#ifdef DEBUG
			printk("adm1026: Autodetection failed\n");
#endif
			/* Not an ADM1026 ... */
			if( kind == 0 ) {  /* User used force=x,y */
			    printk("adm1026: Generic ADM1026 Version 6 not"
				" found at %d,0x%02x. Try force_adm1026.\n",
				i2c_adapter_id(adapter), address );
			}
			goto ERROR1;
		}
	}

	/* Fill in the chip specific driver values */
	switch (kind) {
	case any_chip :
		type_name = "adm1026";
		strcpy(new_client->name, "Generic ADM1026");
		template_next = template ;  /* None used */
		break ;
	case adm1026 :
		type_name = "adm1026";
		strcpy(new_client->name, "Analog Devices ADM1026");
		template_next = template ;
		break ;
#if 0
	/* Example of another adm1026 "compatible" device */
	case adx1000 :
		type_name = "adx1000";
		strcpy(new_client->name, "Compatible ADX1000");
		memcpy( template, adx_specific, sizeof(adx_specific) );
		template_next = template + CTLTBL_ADX1000 ;
		break ;
#endif
	default :
		printk("adm1026: Internal error, invalid kind (%d)!\n", kind);
		err = -EFAULT ;
		goto ERROR1;
	}

	/* Fill in the remaining client fields */
	new_client->id = adm1026_id++;
	printk("adm1026(%d): Assigning ID %d to %s at %d,0x%02x\n",
		new_client->id, new_client->id, new_client->name,
		i2c_adapter_id(new_client->adapter),
		new_client->addr
	    );

	/* Housekeeping values */
	data->valid = 0;

	/* Set the VRM version */
	data->vrm = ADM1026_INIT_VRM ;
	data->vid = ADM1026_INIT_VID ;

	init_MUTEX(&data->update_lock);

	/* Initialize the ADM1026 chip */
	adm1026_init_client(new_client);

	/* Tell the I2C layer a new client has arrived */
	if ((err = i2c_attach_client(new_client)))
		goto ERROR1;

	/* Finish out the template */
	memcpy(template_next, adm1026_common, sizeof(adm1026_common));

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

static int adm1026_detach_client(struct i2c_client *client)
{
	int err;
	int id ;

	id = client->id;
	i2c_deregister_entry(((struct adm1026_data *)(client->data))->sysctl_id);

	if ((err = i2c_detach_client(client))) {
		printk("adm1026(%d): Client deregistration failed,"
			" client not detached.\n", id );
		return err;
	}

	kfree(client->data);

	return 0;
}

static int adm1026_read_value(struct i2c_client *client, u8 reg)
{
	int res;

	if( reg < 0x80 ) {
		/* "RAM" locations */
		res = i2c_smbus_read_byte_data(client, reg) & 0xff ;
	} else {
		/* EEPROM, do nothing */
		res = 0 ;
	}

	return res ;
}

static int adm1026_write_value(struct i2c_client *client, u8 reg, int value)
{
	int res ;

	if( reg < 0x80 ) {
		/* "RAM" locations */
		res = i2c_smbus_write_byte_data(client, reg, value);
	} else {
		/* EEPROM, do nothing */
		res = 0 ;
	}

	return res ;
}

/* Called when we have found a new ADM1026. */
static void adm1026_init_client(struct i2c_client *client)
{
	int value ;
	int i;
	struct adm1026_data *data = client->data;

#ifdef DEBUG
	printk("adm1026(%d): Initializing device\n", client->id);
#endif

	/* Read chip config */
	data->config1 = adm1026_read_value(client, ADM1026_REG_CONFIG1);
	data->config2 = adm1026_read_value(client, ADM1026_REG_CONFIG2);
	data->config3 = adm1026_read_value(client, ADM1026_REG_CONFIG3);

	/* Inform user of chip config */
#ifdef DEBUG
	printk("adm1026(%d): ADM1026_REG_CONFIG1 is: 0x%02x\n",
		client->id, data->config1 );
#endif
	if( (data->config1 & CFG1_MONITOR) == 0 ) {
		printk("adm1026(%d): Monitoring not currently enabled.\n",
			    client->id );
	}
	if( data->config1 & CFG1_INT_ENABLE ) {
		printk("adm1026(%d): SMBALERT interrupts are enabled.\n",
			    client->id );
	}
	if( data->config1 & CFG1_AIN8_9 ) {
		printk("adm1026(%d): in8 and in9 enabled.  temp3 disabled.\n",
			    client->id );
	} else {
		printk("adm1026(%d): temp3 enabled.  in8 and in9 disabled.\n",
			    client->id );
	}
	if( data->config1 & CFG1_THERM_HOT ) {
		printk("adm1026(%d): Automatic THERM, PWM, and temp limits enabled.\n",
			    client->id );
	}

	if( data->config3 & CFG3_GPIO16_ENABLE ) {
		printk("adm1026(%d): GPIO16 enabled.  THERM pin disabled.\n",
			    client->id );
	} else {
		printk("adm1026(%d): THERM pin enabled.  GPIO16 disabled.\n",
			    client->id );
	}
	if( data->config3 & CFG3_VREF_250 ) {
		printk("adm1026(%d): Vref is 2.50 Volts.\n", client->id );
	} else {
		printk("adm1026(%d): Vref is 1.82 Volts.\n", client->id );
	}

	/* Read and pick apart the existing GPIO configuration */
	value = 0 ;
	for( i = 0 ; i <= 15 ; ++i ) {
		if( (i & 0x03) == 0 ) {
			value = adm1026_read_value(client,
					ADM1026_REG_GPIO_CFG_0_3 + i/4 );
		}
		data->gpio_config[i] = value & 0x03 ;
		value >>= 2 ;
	}
	data->gpio_config[16] = (data->config3 >> 6) & 0x03 ;

	/* ... and then print it */
	adm1026_print_gpio(client);

	/* If the user asks us to reprogram the GPIO config, then
	 *   do it now.  But only if this is the first ADM1026.
	 */
	if( client->id == 0
	    && (gpio_input[0] != -1 || gpio_output[0] != -1
		|| gpio_inverted[0] != -1 || gpio_normal[0] != -1
		|| gpio_fan[0] != -1 ) ) {
		adm1026_fixup_gpio(client);
	}

	/* WE INTENTIONALLY make no changes to the limits,
	 *   offsets, pwms and fans.  If they were
	 *   configured, we don't want to mess with them.
	 *   If they weren't, the default is generally safe
	 *   and will suffice until 'sensors -s' can be run.
	 */

	/* Start monitoring */
	value = adm1026_read_value(client, ADM1026_REG_CONFIG1);

	/* Set MONITOR, clear interrupt acknowledge and s/w reset */
	value = (value | CFG1_MONITOR) & (~CFG1_INT_CLEAR & ~CFG1_RESET) ;
#ifdef DEBUG
	printk("adm1026(%d): Setting CONFIG to: 0x%02x\n", client->id, value );
#endif
	data->config1 = value ;
	adm1026_write_value(client, ADM1026_REG_CONFIG1, value);

}

static void adm1026_print_gpio(struct i2c_client *client)
{
	struct adm1026_data *data = client->data;
	int  i ;

	printk("adm1026(%d): GPIO config is:\nadm1026(%d):",
			    client->id, client->id );
	for( i = 0 ; i <= 7 ; ++i ) {
		if( data->config2 & (1 << i) ) {
			printk( " %sGP%s%d",
				data->gpio_config[i] & 0x02 ? "" : "!",
				data->gpio_config[i] & 0x01 ? "OUT" : "IN",
				i );
		} else {
			printk( " FAN%d", i );
		}
	}
	printk( "\nadm1026(%d):", client->id );
	for( i = 8 ; i <= 15 ; ++i ) {
		printk( " %sGP%s%d",
			data->gpio_config[i] & 0x02 ? "" : "!",
			data->gpio_config[i] & 0x01 ? "OUT" : "IN",
			i );
	}
	if( data->config3 & CFG3_GPIO16_ENABLE ) {
		printk( " %sGP%s16\n",
			data->gpio_config[16] & 0x02 ? "" : "!",
			data->gpio_config[16] & 0x01 ? "OUT" : "IN" );
	} else {
		/* GPIO16 is THERM */
		printk( " THERM\n" );
	}
}

static void adm1026_fixup_gpio(struct i2c_client *client)
{
	struct adm1026_data *data = client->data;
	int  i ;
	int  value ;

	/* Make the changes requested. */
	/* We may need to unlock/stop monitoring or soft-reset the
	 *    chip before we can make changes.  This hasn't been
	 *    tested much.  FIXME
	 */

	/* Make outputs */
	for( i = 0 ; i <= 16 ; ++i ) {
		if( gpio_output[i] >= 0 && gpio_output[i] <= 16 ) {
			data->gpio_config[gpio_output[i]] |= 0x01 ;
		}
		/* if GPIO0-7 is output, it isn't a FAN tach */
		if( gpio_output[i] >= 0 && gpio_output[i] <= 7 ) {
			data->config2 |= 1 << gpio_output[i] ;
		}
	}

	/* Input overrides output */
	for( i = 0 ; i <= 16 ; ++i ) {
		if( gpio_input[i] >= 0 && gpio_input[i] <= 16 ) {
			data->gpio_config[gpio_input[i]] &= ~ 0x01 ;
		}
		/* if GPIO0-7 is input, it isn't a FAN tach */
		if( gpio_input[i] >= 0 && gpio_input[i] <= 7 ) {
			data->config2 |= 1 << gpio_input[i] ;
		}
	}

	/* Inverted  */
	for( i = 0 ; i <= 16 ; ++i ) {
		if( gpio_inverted[i] >= 0 && gpio_inverted[i] <= 16 ) {
			data->gpio_config[gpio_inverted[i]] &= ~ 0x02 ;
		}
	}

	/* Normal overrides inverted  */
	for( i = 0 ; i <= 16 ; ++i ) {
		if( gpio_normal[i] >= 0 && gpio_normal[i] <= 16 ) {
			data->gpio_config[gpio_normal[i]] |= 0x02 ;
		}
	}

	/* Fan overrides input and output */
	for( i = 0 ; i <= 7 ; ++i ) {
		if( gpio_fan[i] >= 0 && gpio_fan[i] <= 7 ) {
			data->config2 &= ~( 1 << gpio_fan[i] );
		}
	}

	/* Write new configs to registers */
	adm1026_write_value(client, ADM1026_REG_CONFIG2, data->config2);
	data->config3 = (data->config3 & 0x3f)
			| ((data->gpio_config[16] & 0x03) << 6) ;
	adm1026_write_value(client, ADM1026_REG_CONFIG3, data->config3);
	for( i = 15, value = 0 ; i >= 0 ; --i ) {
		value <<= 2 ;
		value |= data->gpio_config[i] & 0x03 ;
		if( (i & 0x03) == 0 ) {
			adm1026_write_value(client,
					ADM1026_REG_GPIO_CFG_0_3 + i/4,
					value );
			value = 0 ;
		}
	}

	/* Print the new config */
	adm1026_print_gpio(client);
}

static void adm1026_update_client(struct i2c_client *client)
{
	struct adm1026_data *data = client->data;
	int i;
	long value, alarms, gpio ;

	down(&data->update_lock);

	if (!data->valid
	    || (jiffies - data->last_reading > ADM1026_DATA_INTERVAL )) {
		/* Things that change quickly */

#ifdef DEBUG
		printk("adm1026(%d): Reading sensor values\n", client->id);
#endif
		for (i = 0 ; i <= 16 ; ++i) {
			data->in[i] =
			    adm1026_read_value(client, ADM1026_REG_IN(i));
		}

		for (i = 0 ; i <= 7 ; ++i) {
			data->fan[i] =
			    adm1026_read_value(client, ADM1026_REG_FAN(i));
		}

		for (i = 0 ; i <= 2 ; ++i) {
			/* NOTE: temp[] is s8 and we assume 2's complement
			 *   "conversion" in the assignment   */
			data->temp[i] =
			    adm1026_read_value(client, ADM1026_REG_TEMP(i));
		}

		data->pwm = adm1026_read_value(client, ADM1026_REG_PWM);
		data->analog_out = adm1026_read_value(client, ADM1026_REG_DAC);

		/* GPIO16 is MSbit of alarms, move it to gpio */
		alarms  = adm1026_read_value(client, ADM1026_REG_STATUS4);
		gpio = alarms & 0x80 ? 0x0100 : 0 ;  /* GPIO16 */
		alarms &= 0x7f ;
		alarms <<= 8 ;
		alarms |= adm1026_read_value(client, ADM1026_REG_STATUS3);
		alarms <<= 8 ;
		alarms |= adm1026_read_value(client, ADM1026_REG_STATUS2);
		alarms <<= 8 ;
		alarms |= adm1026_read_value(client, ADM1026_REG_STATUS1);
		data->alarms = alarms ;

		/* Read the GPIO values */
		gpio |= adm1026_read_value(client, ADM1026_REG_GPIO_STATUS_8_15);
		gpio <<= 8 ;
		gpio |= adm1026_read_value(client, ADM1026_REG_GPIO_STATUS_0_7);
		data->gpio = gpio ;

		data->last_reading = jiffies ;
	};  /* last_reading */

	if (!data->valid
	    || (jiffies - data->last_config > ADM1026_CONFIG_INTERVAL) ) {
		/* Things that don't change often */

#ifdef DEBUG
		printk("adm1026(%d): Reading config values\n", client->id);
#endif
		for (i = 0 ; i <= 16 ; ++i) {
			data->in_min[i] =
			    adm1026_read_value(client, ADM1026_REG_IN_MIN(i));
			data->in_max[i] =
			    adm1026_read_value(client, ADM1026_REG_IN_MAX(i));
		}

		value = adm1026_read_value(client, ADM1026_REG_FAN_DIV_0_3)
			| (adm1026_read_value(client, ADM1026_REG_FAN_DIV_4_7) << 8);
		for (i = 0 ; i <= 7 ; ++i) {
			data->fan_min[i] =
			    adm1026_read_value(client, ADM1026_REG_FAN_MIN(i));
			data->fan_div[i] = DIV_FROM_REG(value & 0x03);
			value >>= 2 ;
		}

		for (i = 0; i <= 2; ++i) {
			/* NOTE: temp_xxx[] are s8 and we assume 2's complement
			 *   "conversion" in the assignment   */
			data->temp_min[i] =
			    adm1026_read_value(client, ADM1026_REG_TEMP_MIN(i));
			data->temp_max[i] =
			    adm1026_read_value(client, ADM1026_REG_TEMP_MAX(i));
			data->temp_tmin[i] =
			    adm1026_read_value(client, ADM1026_REG_TEMP_TMIN(i));
			data->temp_therm[i] =
			    adm1026_read_value(client, ADM1026_REG_TEMP_THERM(i));
			data->temp_offset[i] =
			    adm1026_read_value(client, ADM1026_REG_TEMP_OFFSET(i));
		}

		/* Read the STATUS/alarm masks */
		alarms  = adm1026_read_value(client, ADM1026_REG_MASK4);
		gpio    = alarms & 0x80 ? 0x0100 : 0 ;  /* GPIO16 */
		alarms  = (alarms & 0x7f) << 8 ;
		alarms |= adm1026_read_value(client, ADM1026_REG_MASK3);
		alarms <<= 8 ;
		alarms |= adm1026_read_value(client, ADM1026_REG_MASK2);
		alarms <<= 8 ;
		alarms |= adm1026_read_value(client, ADM1026_REG_MASK1);
		data->alarm_mask = alarms ;

		/* Read the GPIO values */
		gpio |= adm1026_read_value(client, ADM1026_REG_GPIO_MASK_8_15);
		gpio <<= 8 ;
		gpio |= adm1026_read_value(client, ADM1026_REG_GPIO_MASK_0_7);
		data->gpio_mask = gpio ;

		/* Read the GPIO config */
		data->config2 = adm1026_read_value(client, ADM1026_REG_CONFIG2);
		data->config3 = adm1026_read_value(client, ADM1026_REG_CONFIG3);
		data->gpio_config[16] = (data->config3 >> 6) & 0x03 ;

		value = 0 ;
		for( i = 0 ; i <= 15 ; ++i ) {
			if( (i & 0x03) == 0 ) {
				value = adm1026_read_value(client,
					    ADM1026_REG_GPIO_CFG_0_3 + i/4 );
			}
			data->gpio_config[i] = value & 0x03 ;
			value >>= 2 ;
		}

		data->last_config = jiffies;
	};  /* last_config */

	/* We don't know where or even _if_ the VID might be on the GPIO
	 *    pins.  But the datasheet gives an example config showing
	 *    GPIO11-15 being used to monitor VID0-4, so we go with that
	 *    but make the vid WRITEABLE so if it's wrong, the user can
	 *    set it in /etc/sensors.conf perhaps using an expression or
	 *    0 to trigger a re-read from the GPIO pins.
	 */
	if( data->vid == ADM1026_INIT_VID ) {
		/* Hasn't been set yet, make a bold assumption */
		printk("adm1026(%d): Setting VID from GPIO11-15.\n",
			    client->id );
		data->vid = (data->gpio >> 11) & 0x1f ;
	}
	
	data->valid = 1;

	up(&data->update_lock);
}


/* The following functions are the call-back functions of the /proc/sys and
   sysctl files.  The appropriate function is referenced in the ctl_table
   extra1 field.

   Each function must return the magnitude (power of 10 to divide the
   data with) if it is called with operation set to SENSORS_PROC_REAL_INFO.
   It must put a maximum of *nrels elements in results reflecting the
   data of this file, and set *nrels to the number it actually put in
   it, if operation is SENSORS_PROC_REAL_READ.  Finally, it must get
   up to *nrels elements from results and write them to the chip, if
   operations is SENSORS_PROC_REAL_WRITE.
 */
void adm1026_in(struct i2c_client *client, int operation, int ctl_name,
	     int *nrels_mag, long *results)
{
	struct adm1026_data *data = client->data;
	int nr = ctl_name - ADM1026_SYSCTL_IN0;

	/* We handle in0 - in15 here.  in16 (-12V) is handled below */
	if (nr < 0 || nr > 15)
		return ;  /* ERROR */

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 3;  /* 1.000 */
	else if (operation == SENSORS_PROC_REAL_READ) {
		adm1026_update_client(client);
		results[0] = INS_FROM_REG(nr,data->in_min[nr]);
		results[1] = INS_FROM_REG(nr,data->in_max[nr]);
		results[2] = INS_FROM_REG(nr,data->in[nr]);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		down(&data->update_lock);
		if (*nrels_mag > 1) {
			data->in_max[nr] = INS_TO_REG(nr,results[1]);
			adm1026_write_value(client, ADM1026_REG_IN_MAX(nr),
					 data->in_max[nr]);
		}
		if (*nrels_mag > 0) {
			data->in_min[nr] = INS_TO_REG(nr,results[0]);
			adm1026_write_value(client, ADM1026_REG_IN_MIN(nr),
					 data->in_min[nr]);
		}
		up(&data->update_lock);
	}
}

void adm1026_in16(struct i2c_client *client, int operation, int ctl_name,
	     int *nrels_mag, long *results)
{
	struct adm1026_data *data = client->data;
	int nr = ctl_name - ADM1026_SYSCTL_IN0;

	/* We handle in16 (-12V) here */
	if (nr != 16)
		return ;  /* ERROR */

	/* Apply offset and swap min/max so that min is 90% of
	 *    target and max is 110% of target.
	 */

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 3;  /* 1.000 */
	else if (operation == SENSORS_PROC_REAL_READ) {
		adm1026_update_client(client);
		results[0] = INS_FROM_REG(nr,data->in_max[nr])-NEG12_OFFSET ;
		results[1] = INS_FROM_REG(nr,data->in_min[nr])-NEG12_OFFSET ;
		results[2] = INS_FROM_REG(nr,data->in[nr])-NEG12_OFFSET ;
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		down(&data->update_lock);
		if (*nrels_mag > 1) {
			data->in_min[nr] = INS_TO_REG(nr,results[1]+NEG12_OFFSET);
			adm1026_write_value(client, ADM1026_REG_IN_MIN(nr),
					 data->in_min[nr]);
		}
		if (*nrels_mag > 0) {
			data->in_max[nr] = INS_TO_REG(nr,results[0]+NEG12_OFFSET);
			adm1026_write_value(client, ADM1026_REG_IN_MAX(nr),
					 data->in_max[nr]);
		}
		up(&data->update_lock);
	}
}

void adm1026_fan(struct i2c_client *client, int operation, int ctl_name,
	      int *nrels_mag, long *results)
{
	struct adm1026_data *data = client->data;
	int nr = ctl_name - ADM1026_SYSCTL_FAN0 ;

	if (nr < 0 || nr > 7)
		return ;  /* ERROR */

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		adm1026_update_client(client);
		results[0] = FAN_FROM_REG(data->fan_min[nr], data->fan_div[nr]);
		results[1] = FAN_FROM_REG(data->fan[nr], data->fan_div[nr]);
		*nrels_mag = 2;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		down(&data->update_lock);
		if (*nrels_mag > 0) {
			data->fan_min[nr] = FAN_TO_REG(results[0],
							data->fan_div[nr]);
			adm1026_write_value(client, ADM1026_REG_FAN_MIN(nr),
					 data->fan_min[nr]);
		}
		up(&data->update_lock);
	}
}

/* Adjust fan_min to account for new fan divisor */
void adm1026_fixup_fan_min(struct i2c_client *client, int fan, int old_div)
{
	struct adm1026_data *data = client->data;
	int  new_div = data->fan_div[fan] ;
	int  new_min;

	/* 0 and 0xff are special.  Don't adjust them */
	if( data->fan_min[fan] == 0 || data->fan_min[fan] == 0xff ) {
		return ;
	}

	new_min = data->fan_min[fan] * old_div / new_div ;
	new_min = SENSORS_LIMIT(new_min, 1, 254);
	data->fan_min[fan] = new_min ;
	adm1026_write_value(client, ADM1026_REG_FAN_MIN(fan), new_min);
}

void adm1026_fan_div(struct i2c_client *client, int operation, int ctl_name,
		int *nrels_mag, long *results)
{
	struct adm1026_data *data = client->data;
	int i ;
	int value, div, old ;

	if (ctl_name != ADM1026_SYSCTL_FAN_DIV)
		return ;  /* ERROR */

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		adm1026_update_client(client);
		for( i = 0 ; i <= 7 ; ++i ) {
			results[i] = data->fan_div[i] ;
		}
		*nrels_mag = 8;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		down(&data->update_lock);
		value = 0 ;
		for( i = 7 ; i >= 0 ; --i ) {
			value <<= 2 ;
			if (*nrels_mag > i) {
				old = data->fan_div[i] ;
				div = DIV_TO_REG(results[i]) ;
				data->fan_div[i] = DIV_FROM_REG(div) ;
				if( data->fan_div[i] != old ) {
					adm1026_fixup_fan_min(client,i,old);
				}
			} else {
				div = DIV_TO_REG(data->fan_div[i]) ;
			}
			value |= div ;
		}
		adm1026_write_value(client, ADM1026_REG_FAN_DIV_0_3,
			value & 0xff);
		adm1026_write_value(client, ADM1026_REG_FAN_DIV_4_7,
			(value >> 8) & 0xff);
		up(&data->update_lock);
	}
}

void adm1026_temp(struct i2c_client *client, int operation, int ctl_name,
	       int *nrels_mag, long *results)
{
	struct adm1026_data *data = client->data;
	int nr = ctl_name - ADM1026_SYSCTL_TEMP1 ;

	if (nr < 0 || nr > 2)
		return ;  /* ERROR */

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		adm1026_update_client(client);
		results[0] = TEMP_FROM_REG(data->temp_min[nr]);
		results[1] = TEMP_FROM_REG(data->temp_max[nr]);
		results[2] = TEMP_FROM_REG(data->temp[nr]);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		down(&data->update_lock);
		if (*nrels_mag > 1) {
			data->temp_max[nr] = TEMP_TO_REG(results[1]);
			adm1026_write_value(client, ADM1026_REG_TEMP_MAX(nr),
					 data->temp_max[nr]);
		}
		if (*nrels_mag > 0) {
			data->temp_min[nr] = TEMP_TO_REG(results[0]);
			adm1026_write_value(client, ADM1026_REG_TEMP_MIN(nr),
					 data->temp_min[nr]);
		}
		up(&data->update_lock);
	}
}

void adm1026_temp_offset(struct i2c_client *client, int operation, int ctl_name,
		int *nrels_mag, long *results)
{
	struct adm1026_data *data = client->data;
	int nr = ctl_name - ADM1026_SYSCTL_TEMP_OFFSET1 ;

	if (nr < 0 || nr > 2)
		return ;  /* ERROR */

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		adm1026_update_client(client);
		results[0] = TEMP_FROM_REG(data->temp_offset[nr]);
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		down(&data->update_lock);
		if (*nrels_mag > 0) {
			data->temp_offset[nr] = TEMP_TO_REG(results[0]);
			adm1026_write_value(client, ADM1026_REG_TEMP_OFFSET(nr),
			    data->temp_offset[nr]);
		}
		up(&data->update_lock);
	}
}

void adm1026_temp_tmin(struct i2c_client *client, int operation, int ctl_name,
		int *nrels_mag, long *results)
{
	struct adm1026_data *data = client->data;
	int nr = ctl_name - ADM1026_SYSCTL_TEMP_TMIN1 ;

	if (nr < 0 || nr > 2)
		return ;  /* ERROR */

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		adm1026_update_client(client);
		results[0] = TEMP_FROM_REG(data->temp_tmin[nr]);
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		down(&data->update_lock);
		if (*nrels_mag > 0) {
			data->temp_tmin[nr] = TEMP_TO_REG(results[0]);
			adm1026_write_value(client, ADM1026_REG_TEMP_TMIN(nr),
			    data->temp_tmin[nr]);
		}
		up(&data->update_lock);
	}
}

void adm1026_temp_therm(struct i2c_client *client, int operation, int ctl_name,
		int *nrels_mag, long *results)
{
	struct adm1026_data *data = client->data;
	int nr = ctl_name - ADM1026_SYSCTL_TEMP_THERM1 ;

	if (nr < 0 || nr > 2)
		return ;  /* ERROR */

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		adm1026_update_client(client);
		results[0] = TEMP_FROM_REG(data->temp_therm[nr]);
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		down(&data->update_lock);
		if (*nrels_mag > 0) {
			data->temp_therm[nr] = TEMP_TO_REG(results[0]);
			adm1026_write_value(client, ADM1026_REG_TEMP_THERM(nr),
			    data->temp_therm[nr]);
		}
		up(&data->update_lock);
	}
}

void adm1026_pwm(struct i2c_client *client, int operation, int ctl_name,
	      int *nrels_mag, long *results)
{
	struct adm1026_data *data = client->data;

	if (ctl_name != ADM1026_SYSCTL_PWM)
		return ;  /* ERROR */

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		adm1026_update_client(client);
		results[0] = PWM_FROM_REG(data->pwm);
		results[1] = 1 ;  /* Always enabled */
		*nrels_mag = 2;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		down(&data->update_lock);
		/* PWM enable is read-only */
		if (*nrels_mag > 0) {
			data->pwm = PWM_TO_REG(results[0]);
			adm1026_write_value(client, ADM1026_REG_PWM,
					 data->pwm);
		}
		up(&data->update_lock);
	}
}

void adm1026_analog_out(struct i2c_client *client, int operation, int ctl_name,
	      int *nrels_mag, long *results)
{
	struct adm1026_data *data = client->data;

	if (ctl_name != ADM1026_SYSCTL_ANALOG_OUT)
		return ;  /* ERROR */

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;  /* 0 - 255 */
	else if (operation == SENSORS_PROC_REAL_READ) {
		adm1026_update_client(client);
		results[0] = DAC_FROM_REG(data->analog_out);
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		down(&data->update_lock);
		if (*nrels_mag > 0) {
			data->analog_out = DAC_TO_REG(results[0]);
			adm1026_write_value(client, ADM1026_REG_DAC,
					 data->analog_out);
		}
		up(&data->update_lock);
	}
}

void adm1026_afc(struct i2c_client *client, int operation, int ctl_name,
	      int *nrels_mag, long *results)
{
	struct adm1026_data *data = client->data;

	if (ctl_name != ADM1026_SYSCTL_AFC)
		return ;  /* ERROR */

	/* PWM auto fan control, DAC auto fan control */

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		adm1026_update_client(client);
		results[0] = (data->config1 & CFG1_PWM_AFC) != 0 ;
		results[1] = (data->config1 & CFG1_DAC_AFC) != 0 ;
		*nrels_mag = 2;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		down(&data->update_lock);
		if (*nrels_mag > 1) {
			data->config1 = (data->config1 & ~CFG1_DAC_AFC)
				| (results[1] ? CFG1_DAC_AFC : 0) ;
		}
		if (*nrels_mag > 0) {
			data->config1 = (data->config1 & ~CFG1_PWM_AFC)
				| (results[0] ? CFG1_PWM_AFC : 0) ;
			adm1026_write_value(client, ADM1026_REG_CONFIG1,
					 data->config1);
		}
		up(&data->update_lock);
	}
}

void adm1026_vid(struct i2c_client *client, int operation, int ctl_name,
	      int *nrels_mag, long *results)
{
	struct adm1026_data *data = client->data;

	if( ctl_name != ADM1026_SYSCTL_VID )
		return ;  /* ERROR */

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 3;
	else if (operation == SENSORS_PROC_REAL_READ) {
		adm1026_update_client(client);
		results[0] = vid_from_reg((data->vid)&0x3f,data->vrm);
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		down(&data->update_lock);
		/* Hmmm... There isn't a VID_TO_REG mapping */
		if (*nrels_mag > 0) {
			if( results[0] >= 0 ) {
				data->vid = results[0] & 0x3f ;
			} else {
				data->vid = ADM1026_INIT_VID ;
			}
		}
		up(&data->update_lock);
	}

}

void adm1026_vrm(struct i2c_client *client, int operation, int ctl_name,
	      int *nrels_mag, long *results)
{
	struct adm1026_data *data = client->data;

	if( ctl_name != ADM1026_SYSCTL_VRM )
		return ;  /* ERROR */

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 1;
	else if (operation == SENSORS_PROC_REAL_READ) {
		results[0] = data->vrm ;
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag > 0) {
			data->vrm = results[0] ;
		}
	}
}

void adm1026_alarms(struct i2c_client *client, int operation, int ctl_name,
		int *nrels_mag, long *results)
{
	struct adm1026_data *data = client->data;

	if( ctl_name != ADM1026_SYSCTL_ALARMS )
		return ;  /* ERROR */

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		adm1026_update_client(client);
		results[0] = data->alarms ;
		*nrels_mag = 1;
	}
	/* FIXME: Perhaps we should implement a write function
	 *   to clear an alarm?
	 */
}

void adm1026_alarm_mask(struct i2c_client *client, int operation,
		int ctl_name, int *nrels_mag, long *results)
{
	struct adm1026_data *data = client->data;
	unsigned long mask ;

	if( ctl_name != ADM1026_SYSCTL_ALARM_MASK )
		return ;  /* ERROR */

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		adm1026_update_client(client);
		results[0] = data->alarm_mask ;
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		down(&data->update_lock);
		if (*nrels_mag > 0) {
			data->alarm_mask = results[0] & 0x7fffffff ;
			mask = data->alarm_mask
				| (data->gpio_mask & 0x10000 ? 0x80000000 : 0) ;
			adm1026_write_value(client, ADM1026_REG_MASK1,
					mask & 0xff);
			mask >>= 8 ;
			adm1026_write_value(client, ADM1026_REG_MASK2,
					mask & 0xff);
			mask >>= 8 ;
			adm1026_write_value(client, ADM1026_REG_MASK3,
					mask & 0xff);
			mask >>= 8 ;
			adm1026_write_value(client, ADM1026_REG_MASK4,
					mask & 0xff);
		}
		up(&data->update_lock);
	}
}

void adm1026_gpio(struct i2c_client *client, int operation, int ctl_name,
		int *nrels_mag, long *results)
{
	struct adm1026_data *data = client->data;
	long gpio ;

	if( ctl_name != ADM1026_SYSCTL_GPIO )
		return ;  /* ERROR */

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		adm1026_update_client(client);
		results[0] = data->gpio ;
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		down(&data->update_lock);
		if (*nrels_mag > 0) {
			data->gpio = results[0] & 0x1ffff ;
			gpio = data->gpio ;
			adm1026_write_value(client,
				ADM1026_REG_GPIO_STATUS_0_7,
				gpio & 0xff );
			gpio >>= 8 ;
			adm1026_write_value(client,
				ADM1026_REG_GPIO_STATUS_8_15,
				gpio & 0xff );
			gpio = ((gpio >> 1) & 0x80)
				| (data->alarms >> 24 & 0x7f);
			adm1026_write_value(client,
				ADM1026_REG_STATUS4,
				gpio & 0xff );
		}
		up(&data->update_lock);
	}
}

void adm1026_gpio_mask(struct i2c_client *client, int operation,
		int ctl_name, int *nrels_mag, long *results)
{
	struct adm1026_data *data = client->data;
	long mask ;

	if( ctl_name != ADM1026_SYSCTL_GPIO_MASK )
		return ;  /* ERROR */

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		adm1026_update_client(client);
		results[0] = data->gpio_mask ;
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		down(&data->update_lock);
		if (*nrels_mag > 0) {
			data->gpio_mask = results[0] & 0x1ffff ;
			mask = data->gpio_mask ;
			adm1026_write_value(client, ADM1026_REG_GPIO_MASK_0_7,
					mask & 0xff);
			mask >>= 8 ;
			adm1026_write_value(client, ADM1026_REG_GPIO_MASK_8_15,
					mask & 0xff);
			mask = ((mask >> 1) & 0x80)
				| (data->alarm_mask >> 24 & 0x7f);
			adm1026_write_value(client, ADM1026_REG_MASK1,
					mask & 0xff);
		}
		up(&data->update_lock);
	}
}

static int __init sm_adm1026_init(void)
{
	printk("adm1026: Version %s (%s)\n", LM_VERSION, LM_DATE);
	return i2c_add_driver(&adm1026_driver);
}

static void __exit sm_adm1026_exit(void)
{
	i2c_del_driver(&adm1026_driver);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Philip Pokorny <ppokorny@penguincomputing.com");
MODULE_DESCRIPTION("ADM1026 driver");

module_init(sm_adm1026_init);
module_exit(sm_adm1026_exit);
