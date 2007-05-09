/*
    sensors.h - Part of lm_sensors, Linux kernel modules for hardware
                monitoring
    Copyright (c) 1998, 1999  Frodo Looijaard <frodol@dds.nl>

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

#ifndef LIB_SENSORS_H
#define LIB_SENSORS_H

/* This file is intended to be included from userland utilities only.
 * It used to be generated from kernel driver code, but the new sysfs
 * standard interface makes these definitions obsolete. For now, we will be
 * keeping the old defintions in this file, until all applications have
 * been upgraded to the new model (dynamic discovery of chip features.)
 */


/* From linux/i2c-proc.h */

/* Sysctl IDs */
#ifdef DEV_HWMON
#define DEV_SENSORS DEV_HWMON
#else				/* ndef DEV_HWMOM */
#define DEV_SENSORS 2		/* The id of the lm_sensors directory within the
				   dev table */
#endif				/* def DEV_HWMON */

/* The maximum length of the prefix */
#define SENSORS_PREFIX_MAX 20

#define SENSORS_CHIPS 1
struct i2c_chips_data {
	int sysctl_id;
	char name[SENSORS_PREFIX_MAX + 13];
};


/* -- SENSORS SYSCTL START -- */

#define ADM1021_SYSCTL_TEMP 1200
#define ADM1021_SYSCTL_REMOTE_TEMP 1201
#define ADM1021_SYSCTL_DIE_CODE 1202
#define ADM1021_SYSCTL_ALARMS 1203

#define ADM1021_ALARM_TEMP_HIGH 0x40
#define ADM1021_ALARM_TEMP_LOW 0x20
#define ADM1021_ALARM_RTEMP_HIGH 0x10
#define ADM1021_ALARM_RTEMP_LOW 0x08
#define ADM1021_ALARM_RTEMP_NA 0x04

/* -- SENSORS SYSCTL END -- */
/* -- SENSORS SYSCTL START -- */

#define ADM1024_SYSCTL_IN0 1000	/* Volts * 100 */
#define ADM1024_SYSCTL_IN1 1001
#define ADM1024_SYSCTL_IN2 1002
#define ADM1024_SYSCTL_IN3 1003
#define ADM1024_SYSCTL_IN4 1004
#define ADM1024_SYSCTL_IN5 1005
#define ADM1024_SYSCTL_FAN1 1101	/* Rotations/min */
#define ADM1024_SYSCTL_FAN2 1102
#define ADM1024_SYSCTL_TEMP 1250	/* Degrees Celsius * 100 */
#define ADM1024_SYSCTL_TEMP1 1290	/* Degrees Celsius */
#define ADM1024_SYSCTL_TEMP2 1295	/* Degrees Celsius */
#define ADM1024_SYSCTL_FAN_DIV 2000	/* 1, 2, 4 or 8 */
#define ADM1024_SYSCTL_ALARMS 2001	/* bitvector */
#define ADM1024_SYSCTL_ANALOG_OUT 2002
#define ADM1024_SYSCTL_VID 2003

#define ADM1024_ALARM_IN0 0x0001
#define ADM1024_ALARM_IN1 0x0002
#define ADM1024_ALARM_IN2 0x0004
#define ADM1024_ALARM_IN3 0x0008
#define ADM1024_ALARM_IN4 0x0100
#define ADM1024_ALARM_IN5 0x0200
#define ADM1024_ALARM_FAN1 0x0040
#define ADM1024_ALARM_FAN2 0x0080
#define ADM1024_ALARM_TEMP 0x0010
#define ADM1024_ALARM_TEMP1 0x0020
#define ADM1024_ALARM_TEMP2 0x0001
#define ADM1024_ALARM_CHAS 0x1000

/* -- SENSORS SYSCTL END -- */
/* -- SENSORS SYSCTL START -- */

#define ADM1025_SYSCTL_IN0     1000 /* Volts * 100 */
#define ADM1025_SYSCTL_IN1     1001
#define ADM1025_SYSCTL_IN2     1002
#define ADM1025_SYSCTL_IN3     1003
#define ADM1025_SYSCTL_IN4     1004
#define ADM1025_SYSCTL_IN5     1005

#define ADM1025_SYSCTL_RTEMP   1250 /* Degrees Celsius * 10 */
#define ADM1025_SYSCTL_TEMP    1251

#define ADM1025_SYSCTL_ALARMS  2001 /* bitvector */
#define ADM1025_SYSCTL_VID     2003 /* Volts * 1000 */
#define ADM1025_SYSCTL_VRM     2004

#define ADM1025_ALARM_IN0     0x0001
#define ADM1025_ALARM_IN1     0x0002
#define ADM1025_ALARM_IN2     0x0004
#define ADM1025_ALARM_IN3     0x0008
#define ADM1025_ALARM_IN4     0x0100
#define ADM1025_ALARM_IN5     0x0200
#define ADM1025_ALARM_RTEMP   0x0020
#define ADM1025_ALARM_TEMP    0x0010
#define ADM1025_ALARM_RFAULT  0x4000

/* -- SENSORS SYSCTL END -- */
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
#define ADM1026_ALARM_UNUSED  (1L << 29)
#define ADM1026_ALARM_CI      (1L << 30)
/* -- SENSORS SYSCTL END -- */
/* -- SENSORS SYSCTL START -- */

#define ADM1031_SYSCTL_TEMP1		1200
#define ADM1031_SYSCTL_TEMP2		1201
#define ADM1031_SYSCTL_TEMP3		1202

#define ADM1031_SYSCTL_FAN1		1210
#define ADM1031_SYSCTL_FAN2		1211

#define ADM1031_SYSCTL_FAN_DIV		1220

#define ADM1031_SYSCTL_ALARMS		1250

#define ADM1031_ALARM_FAN1_MIN		0x0001
#define ADM1031_ALARM_FAN1_FLT		0x0002
#define ADM1031_ALARM_TEMP2_HIGH	0x0004
#define ADM1031_ALARM_TEMP2_LOW		0x0008
#define ADM1031_ALARM_TEMP2_CRIT	0x0010
#define ADM1031_ALARM_TEMP2_DIODE	0x0020
#define ADM1031_ALARM_TEMP1_HIGH	0x0040
#define ADM1031_ALARM_TEMP1_LOW		0x0080
#define ADM1031_ALARM_FAN2_MIN		0x0100
#define ADM1031_ALARM_FAN2_FLT		0x0200
#define ADM1031_ALARM_TEMP3_HIGH	0x0400
#define ADM1031_ALARM_TEMP3_LOW		0x0800
#define ADM1031_ALARM_TEMP3_CRIT	0x1000
#define ADM1031_ALARM_TEMP3_DIODE	0x2000
#define ADM1031_ALARM_TEMP1_CRIT	0x4000
#define ADM1031_ALARM_THERMAL		0x8000


/* -- SENSORS SYSCTL END -- */
/* -- SENSORS SYSCTL START -- */

#define ADM9240_SYSCTL_IN0 1000	/* Volts * 100 */
#define ADM9240_SYSCTL_IN1 1001
#define ADM9240_SYSCTL_IN2 1002
#define ADM9240_SYSCTL_IN3 1003
#define ADM9240_SYSCTL_IN4 1004
#define ADM9240_SYSCTL_IN5 1005
#define ADM9240_SYSCTL_FAN1 1101	/* Rotations/min */
#define ADM9240_SYSCTL_FAN2 1102
#define ADM9240_SYSCTL_TEMP 1250	/* Degrees Celsius * 100 */
#define ADM9240_SYSCTL_FAN_DIV 2000	/* 1, 2, 4 or 8 */
#define ADM9240_SYSCTL_ALARMS 2001	/* bitvector */
#define ADM9240_SYSCTL_ANALOG_OUT 2002
#define ADM9240_SYSCTL_VID 2003

#define ADM9240_ALARM_IN0 0x0001
#define ADM9240_ALARM_IN1 0x0002
#define ADM9240_ALARM_IN2 0x0004
#define ADM9240_ALARM_IN3 0x0008
#define ADM9240_ALARM_IN4 0x0100
#define ADM9240_ALARM_IN5 0x0200
#define ADM9240_ALARM_FAN1 0x0040
#define ADM9240_ALARM_FAN2 0x0080
#define ADM9240_ALARM_TEMP 0x0010
#define ADM9240_ALARM_CHAS 0x1000

/* -- SENSORS SYSCTL END -- */
/* -- SENSORS SYSCTL START -- */

#define ASB100_SYSCTL_IN0	1000	/* Volts * 100 */
#define ASB100_SYSCTL_IN1	1001
#define ASB100_SYSCTL_IN2	1002
#define ASB100_SYSCTL_IN3	1003
#define ASB100_SYSCTL_IN4	1004
#define ASB100_SYSCTL_IN5	1005
#define ASB100_SYSCTL_IN6	1006

#define ASB100_SYSCTL_FAN1	1101	/* Rotations/min */
#define ASB100_SYSCTL_FAN2	1102
#define ASB100_SYSCTL_FAN3	1103

#define ASB100_SYSCTL_TEMP1	1200	/* Degrees Celsius * 10 */
#define ASB100_SYSCTL_TEMP2	1201
#define ASB100_SYSCTL_TEMP3	1202
#define ASB100_SYSCTL_TEMP4	1203

#define ASB100_SYSCTL_VID	1300	/* Volts * 1000 */
#define ASB100_SYSCTL_VRM	1301

#define ASB100_SYSCTL_PWM1	1401	/* 0-255 => 0-100% duty cycle */

#define ASB100_SYSCTL_FAN_DIV	2000	/* 1, 2, 4 or 8 */
#define ASB100_SYSCTL_ALARMS	2001	/* bitvector */

#define ASB100_ALARM_IN0	0x0001	/* ? */
#define ASB100_ALARM_IN1	0x0002	/* ? */
#define ASB100_ALARM_IN2	0x0004
#define ASB100_ALARM_IN3	0x0008
#define ASB100_ALARM_TEMP1	0x0010
#define ASB100_ALARM_TEMP2	0x0020
#define ASB100_ALARM_FAN1	0x0040
#define ASB100_ALARM_FAN2	0x0080
#define ASB100_ALARM_IN4	0x0100
#define ASB100_ALARM_IN5	0x0200	/* ? */
#define ASB100_ALARM_IN6	0x0400	/* ? */
#define ASB100_ALARM_FAN3	0x0800
#define ASB100_ALARM_CHAS	0x1000
#define ASB100_ALARM_TEMP3	0x2000

#define ASB100_ALARM_IN7	0x10000 /* ? */
#define ASB100_ALARM_IN8	0x20000	/* ? */

/* -- SENSORS SYSCTL END -- */
/* -- SENSORS SYSCTL START -- */
#define BMC_SYSCTL_IN1 1000
#define BMC_SYSCTL_TEMP1 1100
#define BMC_SYSCTL_CURR1 1200
#define BMC_SYSCTL_FAN1 1300
#define BMC_SYSCTL_ALARMS 5000

/* -- SENSORS SYSCTL END -- */
/* -- SENSORS SYSCTL START -- */
#define BT869_SYSCTL_STATUS 1000
#define BT869_SYSCTL_NTSC   1001
#define BT869_SYSCTL_HALF   1002
#define BT869_SYSCTL_RES    1003
#define BT869_SYSCTL_COLORBARS    1004
#define BT869_SYSCTL_DEPTH  1005
#define BT869_SYSCTL_SVIDEO 1006

/* -- SENSORS SYSCTL END -- */
/* -- SENSORS SYSCTL START -- */

#define DDCMON_SYSCTL_ID 1010
#define DDCMON_SYSCTL_SIZE 1011
#define DDCMON_SYSCTL_SYNC 1012
#define DDCMON_SYSCTL_TIMINGS 1013
#define DDCMON_SYSCTL_SERIAL 1014
#define DDCMON_SYSCTL_TIME 1015
#define DDCMON_SYSCTL_EDID 1016
#define DDCMON_SYSCTL_GAMMA 1017
#define DDCMON_SYSCTL_DPMS 1018
#define DDCMON_SYSCTL_TIMING1 1021
#define DDCMON_SYSCTL_TIMING2 1022
#define DDCMON_SYSCTL_TIMING3 1023
#define DDCMON_SYSCTL_TIMING4 1024
#define DDCMON_SYSCTL_TIMING5 1025
#define DDCMON_SYSCTL_TIMING6 1026
#define DDCMON_SYSCTL_TIMING7 1027
#define DDCMON_SYSCTL_TIMING8 1028
#define DDCMON_SYSCTL_MAXCLOCK 1029

/* -- SENSORS SYSCTL END -- */
/* -- SENSORS SYSCTL START -- */
#define DS1621_SYSCTL_TEMP 1200	/* Degrees Celsius * 10 */
#define DS1621_SYSCTL_ALARMS 2001	/* bitvector */
#define DS1621_ALARM_TEMP_HIGH 0x40
#define DS1621_ALARM_TEMP_LOW 0x20
#define DS1621_SYSCTL_ENABLE 2002
#define DS1621_SYSCTL_CONTINUOUS 2003
#define DS1621_SYSCTL_POLARITY 2004

/* -- SENSORS SYSCTL END -- */
/* -- SENSORS SYSCTL START -- */

#define EEPROM_SYSCTL1 1000
#define EEPROM_SYSCTL2 1001
#define EEPROM_SYSCTL3 1002
#define EEPROM_SYSCTL4 1003
#define EEPROM_SYSCTL5 1004
#define EEPROM_SYSCTL6 1005
#define EEPROM_SYSCTL7 1006
#define EEPROM_SYSCTL8 1007
#define EEPROM_SYSCTL9 1008
#define EEPROM_SYSCTL10 1009
#define EEPROM_SYSCTL11 1010
#define EEPROM_SYSCTL12 1011
#define EEPROM_SYSCTL13 1012
#define EEPROM_SYSCTL14 1013
#define EEPROM_SYSCTL15 1014
#define EEPROM_SYSCTL16 1015

/* -- SENSORS SYSCTL END -- */
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
/* -- SENSORS SYSCTL START -- */
#define FSCHER_SYSCTL_VOLT0    1000       /* 12 volt supply */
#define FSCHER_SYSCTL_VOLT1    1001       /* 5 volt supply */
#define FSCHER_SYSCTL_VOLT2    1002       /* batterie voltage */
#define FSCHER_SYSCTL_FAN0     1101       /* state, ripple, actual value
                                             fan 0 */
#define FSCHER_SYSCTL_FAN1     1102       /* state, ripple, actual value
                                             fan 1 */
#define FSCHER_SYSCTL_FAN2     1103       /* state, ripple, actual value
                                             fan 2 */
#define FSCHER_SYSCTL_TEMP0    1201       /* state and value of sensor 0,
                                             cpu die */
#define FSCHER_SYSCTL_TEMP1    1202       /* state and value of sensor 1,
                                             motherboard */
#define FSCHER_SYSCTL_TEMP2    1203       /* state and value of sensor 2,
                                             chassis */
#define FSCHER_SYSCTL_PWM0     1301       /* min fan 0 */
#define FSCHER_SYSCTL_PWM1     1302       /* min fan 1 */
#define FSCHER_SYSCTL_PWM2     1303       /* min fan 2 */
#define FSCHER_SYSCTL_REV      2000       /* revision */
#define FSCHER_SYSCTL_EVENT    2001       /* global event status */
#define FSCHER_SYSCTL_CONTROL  2002       /* global control byte */
#define FSCHER_SYSCTL_WDOG     2003       /* watch dog preset, state and
                                             control */
/* -- SENSORS SYSCTL END -- */
/* -- SENSORS SYSCTL START -- */
#define FSCPOS_SYSCTL_VOLT0    1000       /* 12 volt supply */
#define FSCPOS_SYSCTL_VOLT1    1001       /* 5 volt supply */
#define FSCPOS_SYSCTL_VOLT2    1002       /* batterie voltage*/
#define FSCPOS_SYSCTL_FAN0     1101       /* state, min, ripple, actual value fan 0 */
#define FSCPOS_SYSCTL_FAN1     1102       /* state, min, ripple, actual value fan 1 */
#define FSCPOS_SYSCTL_FAN2     1103       /* state, min, ripple, actual value fan 2 */
#define FSCPOS_SYSCTL_TEMP0    1201       /* state and value of sensor 0, cpu die */
#define FSCPOS_SYSCTL_TEMP1    1202       /* state and value of sensor 1, motherboard */
#define FSCPOS_SYSCTL_TEMP2    1203       /* state and value of sensor 2, chassis */
#define FSCPOS_SYSCTL_REV     2000        /* Revision */
#define FSCPOS_SYSCTL_EVENT   2001        /* global event status */
#define FSCPOS_SYSCTL_CONTROL 2002        /* global control byte */
#define FSCPOS_SYSCTL_WDOG     2003       /* state, min, ripple, actual value fan 2 */
/* -- SENSORS SYSCTL END -- */
/* -- SENSORS SYSCTL START -- */
#define FSCSCY_SYSCTL_VOLT0    1000       /* 12 volt supply */
#define FSCSCY_SYSCTL_VOLT1    1001       /* 5 volt supply */
#define FSCSCY_SYSCTL_VOLT2    1002       /* batterie voltage*/
#define FSCSCY_SYSCTL_FAN0     1101       /* state, min, ripple, actual value fan 0 */
#define FSCSCY_SYSCTL_FAN1     1102       /* state, min, ripple, actual value fan 1 */
#define FSCSCY_SYSCTL_FAN2     1103       /* state, min, ripple, actual value fan 2 */
#define FSCSCY_SYSCTL_FAN3     1104       /* state, min, ripple, actual value fan 3 */
#define FSCSCY_SYSCTL_FAN4     1105       /* state, min, ripple, actual value fan 4 */
#define FSCSCY_SYSCTL_FAN5     1106       /* state, min, ripple, actual value fan 5 */
#define FSCSCY_SYSCTL_TEMP0    1201       /* state and value of sensor 0, cpu die */
#define FSCSCY_SYSCTL_TEMP1    1202       /* state and value of sensor 1, motherboard */
#define FSCSCY_SYSCTL_TEMP2    1203       /* state and value of sensor 2, chassis */
#define FSCSCY_SYSCTL_TEMP3    1204       /* state and value of sensor 3, chassis */
#define FSCSCY_SYSCTL_REV     2000        /* Revision */
#define FSCSCY_SYSCTL_EVENT   2001        /* global event status */
#define FSCSCY_SYSCTL_CONTROL 2002        /* global control byte */
#define FSCSCY_SYSCTL_WDOG     2003       /* state, min, ripple, actual value fan 2 */
#define FSCSCY_SYSCTL_PCILOAD  2004       /* PCILoad value */
#define FSCSCY_SYSCTL_INTRUSION 2005      /* state, control for intrusion sensor */

/* -- SENSORS SYSCTL END -- */
/* -- SENSORS SYSCTL START -- */

#define GL518_SYSCTL_VDD  1000	/* Volts * 100 */
#define GL518_SYSCTL_VIN1 1001
#define GL518_SYSCTL_VIN2 1002
#define GL518_SYSCTL_VIN3 1003
#define GL518_SYSCTL_FAN1 1101	/* RPM */
#define GL518_SYSCTL_FAN2 1102
#define GL518_SYSCTL_TEMP 1200	/* Degrees Celsius * 10 */
#define GL518_SYSCTL_FAN_DIV 2000	/* 1, 2, 4 or 8 */
#define GL518_SYSCTL_ALARMS 2001	/* bitvector */
#define GL518_SYSCTL_BEEP 2002	/* bitvector */
#define GL518_SYSCTL_FAN1OFF 2003
#define GL518_SYSCTL_ITERATE 2004

#define GL518_ALARM_VDD 0x01
#define GL518_ALARM_VIN1 0x02
#define GL518_ALARM_VIN2 0x04
#define GL518_ALARM_VIN3 0x08
#define GL518_ALARM_TEMP 0x10
#define GL518_ALARM_FAN1 0x20
#define GL518_ALARM_FAN2 0x40

/* -- SENSORS SYSCTL END -- */
/* -- SENSORS SYSCTL START -- */

#define GL520_SYSCTL_VDD  1000	/* Volts * 100 */
#define GL520_SYSCTL_VIN1 1001
#define GL520_SYSCTL_VIN2 1002
#define GL520_SYSCTL_VIN3 1003
#define GL520_SYSCTL_VIN4 1004
#define GL520_SYSCTL_FAN1 1101	/* RPM */
#define GL520_SYSCTL_FAN2 1102
#define GL520_SYSCTL_TEMP1 1200	/* Degrees Celsius * 10 */
#define GL520_SYSCTL_TEMP2 1201	/* Degrees Celsius * 10 */
#define GL520_SYSCTL_VID 1300
#define GL520_SYSCTL_FAN_DIV 2000	/* 1, 2, 4 or 8 */
#define GL520_SYSCTL_ALARMS 2001	/* bitvector */
#define GL520_SYSCTL_BEEP 2002	/* bitvector */
#define GL520_SYSCTL_FAN1OFF 2003
#define GL520_SYSCTL_CONFIG 2004

#define GL520_ALARM_VDD 0x01
#define GL520_ALARM_VIN1 0x02
#define GL520_ALARM_VIN2 0x04
#define GL520_ALARM_VIN3 0x08
#define GL520_ALARM_TEMP1 0x10
#define GL520_ALARM_FAN1 0x20
#define GL520_ALARM_FAN2 0x40
#define GL520_ALARM_TEMP2 0x80
#define GL520_ALARM_VIN4 0x80

/* -- SENSORS SYSCTL END -- */
/* -- SENSORS SYSCTL START -- */
#define IT87_SYSCTL_IN0 1000    /* Volts * 100 */
#define IT87_SYSCTL_IN1 1001
#define IT87_SYSCTL_IN2 1002
#define IT87_SYSCTL_IN3 1003
#define IT87_SYSCTL_IN4 1004
#define IT87_SYSCTL_IN5 1005
#define IT87_SYSCTL_IN6 1006
#define IT87_SYSCTL_IN7 1007
#define IT87_SYSCTL_IN8 1008
#define IT87_SYSCTL_FAN1 1101   /* Rotations/min */
#define IT87_SYSCTL_FAN2 1102
#define IT87_SYSCTL_FAN3 1103
#define IT87_SYSCTL_TEMP1 1200  /* Degrees Celsius * 10 */
#define IT87_SYSCTL_TEMP2 1201  /* Degrees Celsius * 10 */
#define IT87_SYSCTL_TEMP3 1202  /* Degrees Celsius * 10 */
#define IT87_SYSCTL_VID 1300    /* Volts * 100 */
#define IT87_SYSCTL_FAN_DIV 2000        /* 1, 2, 4 or 8 */
#define IT87_SYSCTL_ALARMS 2004    /* bitvector */

#define IT87_SYSCTL_PWM1 1401
#define IT87_SYSCTL_PWM2 1402
#define IT87_SYSCTL_PWM3 1403
#define IT87_SYSCTL_FAN_CTL  1501
#define IT87_SYSCTL_FAN_ON_OFF  1502
#define IT87_SYSCTL_SENS1 1601	/* 1, 2, or Beta (3000-5000) */
#define IT87_SYSCTL_SENS2 1602
#define IT87_SYSCTL_SENS3 1603

#define IT87_ALARM_IN0 0x000100
#define IT87_ALARM_IN1 0x000200
#define IT87_ALARM_IN2 0x000400
#define IT87_ALARM_IN3 0x000800
#define IT87_ALARM_IN4 0x001000
#define IT87_ALARM_IN5 0x002000
#define IT87_ALARM_IN6 0x004000
#define IT87_ALARM_IN7 0x008000
#define IT87_ALARM_FAN1 0x0001
#define IT87_ALARM_FAN2 0x0002
#define IT87_ALARM_FAN3 0x0004
#define IT87_ALARM_TEMP1 0x00010000
#define IT87_ALARM_TEMP2 0x00020000
#define IT87_ALARM_TEMP3 0x00040000

/* -- SENSORS SYSCTL END -- */
/* -- SENSORS SYSCTL START -- */

#define LM63_SYSCTL_TEMP1		1200
#define LM63_SYSCTL_TEMP2		1201
#define LM63_SYSCTL_TEMP2_TCRIT		1205
#define LM63_SYSCTL_TEMP2_TCRIT_HYST	1208
#define LM63_SYSCTL_ALARMS		1210
#define LM63_SYSCTL_FAN1		1220
#define LM63_SYSCTL_PWM1		1230

#define LM63_ALARM_LOCAL_HIGH		0x40
#define LM63_ALARM_REMOTE_HIGH		0x10
#define LM63_ALARM_REMOTE_LOW		0x08
#define LM63_ALARM_REMOTE_CRIT		0x02
#define LM63_ALARM_REMOTE_OPEN		0x04
#define LM63_ALARM_FAN_LOW		0x01

/* -- SENSORS SYSCTL END -- */
/* -- SENSORS SYSCTL START -- */

#define LM75_SYSCTL_TEMP 1200	/* Degrees Celsius * 10 */

/* -- SENSORS SYSCTL END -- */
/* -- SENSORS SYSCTL START -- */
#define LM78_SYSCTL_IN0 1000	/* Volts * 100 */
#define LM78_SYSCTL_IN1 1001
#define LM78_SYSCTL_IN2 1002
#define LM78_SYSCTL_IN3 1003
#define LM78_SYSCTL_IN4 1004
#define LM78_SYSCTL_IN5 1005
#define LM78_SYSCTL_IN6 1006
#define LM78_SYSCTL_FAN1 1101	/* Rotations/min */
#define LM78_SYSCTL_FAN2 1102
#define LM78_SYSCTL_FAN3 1103
#define LM78_SYSCTL_TEMP 1200	/* Degrees Celsius * 10 */
#define LM78_SYSCTL_VID 1300	/* Volts * 100 */
#define LM78_SYSCTL_FAN_DIV 2000	/* 1, 2, 4 or 8 */
#define LM78_SYSCTL_ALARMS 2001	/* bitvector */

#define LM78_ALARM_IN0 0x0001
#define LM78_ALARM_IN1 0x0002
#define LM78_ALARM_IN2 0x0004
#define LM78_ALARM_IN3 0x0008
#define LM78_ALARM_IN4 0x0100
#define LM78_ALARM_IN5 0x0200
#define LM78_ALARM_IN6 0x0400
#define LM78_ALARM_FAN1 0x0040
#define LM78_ALARM_FAN2 0x0080
#define LM78_ALARM_FAN3 0x0800
#define LM78_ALARM_TEMP 0x0010
#define LM78_ALARM_BTI 0x0020
#define LM78_ALARM_CHAS 0x1000
#define LM78_ALARM_FIFO 0x2000
#define LM78_ALARM_SMI_IN 0x4000

/* -- SENSORS SYSCTL END -- */
/* -- SENSORS SYSCTL START -- */

#define LM80_SYSCTL_IN0 1000	/* Volts * 100 */
#define LM80_SYSCTL_IN1 1001
#define LM80_SYSCTL_IN2 1002
#define LM80_SYSCTL_IN3 1003
#define LM80_SYSCTL_IN4 1004
#define LM80_SYSCTL_IN5 1005
#define LM80_SYSCTL_IN6 1006
#define LM80_SYSCTL_FAN1 1101	/* Rotations/min */
#define LM80_SYSCTL_FAN2 1102
#define LM80_SYSCTL_TEMP 1250	/* Degrees Celsius * 100 */
#define LM80_SYSCTL_FAN_DIV 2000	/* 1, 2, 4 or 8 */
#define LM80_SYSCTL_ALARMS 2001	/* bitvector */

#define LM80_ALARM_IN0 0x0001
#define LM80_ALARM_IN1 0x0002
#define LM80_ALARM_IN2 0x0004
#define LM80_ALARM_IN3 0x0008
#define LM80_ALARM_IN4 0x0010
#define LM80_ALARM_IN5 0x0020
#define LM80_ALARM_IN6 0x0040
#define LM80_ALARM_FAN1 0x0400
#define LM80_ALARM_FAN2 0x0800
#define LM80_ALARM_TEMP_HOT 0x0100
#define LM80_ALARM_TEMP_OS 0x2000
#define LM80_ALARM_CHAS 0x1000
#define LM80_ALARM_BTI 0x0200
#define LM80_ALARM_INT_IN 0x0080

/* -- SENSORS SYSCTL END -- */
/* -- SENSORS SYSCTL START -- */

#define LM83_SYSCTL_LOCAL_TEMP    1200
#define LM83_SYSCTL_REMOTE1_TEMP  1201
#define LM83_SYSCTL_REMOTE2_TEMP  1202
#define LM83_SYSCTL_REMOTE3_TEMP  1203
#define LM83_SYSCTL_TCRIT         1208
#define LM83_SYSCTL_ALARMS        1210

#define LM83_ALARM_LOCAL_HIGH     0x0040
#define LM83_ALARM_LOCAL_CRIT     0x0001
#define LM83_ALARM_REMOTE1_HIGH   0x8000
#define LM83_ALARM_REMOTE1_CRIT   0x0100
#define LM83_ALARM_REMOTE1_OPEN   0x2000
#define LM83_ALARM_REMOTE2_HIGH   0x0010
#define LM83_ALARM_REMOTE2_CRIT   0x0002
#define LM83_ALARM_REMOTE2_OPEN   0x0004
#define LM83_ALARM_REMOTE3_HIGH   0x1000
#define LM83_ALARM_REMOTE3_CRIT   0x0200
#define LM83_ALARM_REMOTE3_OPEN   0x0400

/* -- SENSORS SYSCTL END -- */
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
/* -- SENSORS SYSCTL START -- */
#define LM87_SYSCTL_IN0        1000 /* Volts * 100 */
#define LM87_SYSCTL_IN1        1001
#define LM87_SYSCTL_IN2        1002
#define LM87_SYSCTL_IN3        1003
#define LM87_SYSCTL_IN4        1004
#define LM87_SYSCTL_IN5        1005
#define LM87_SYSCTL_AIN1       1006
#define LM87_SYSCTL_AIN2       1007
#define LM87_SYSCTL_FAN1       1102
#define LM87_SYSCTL_FAN2       1103
#define LM87_SYSCTL_TEMP1      1250 /* Degrees Celsius * 10 */
#define LM87_SYSCTL_TEMP2      1251 /* Degrees Celsius * 10 */
#define LM87_SYSCTL_TEMP3      1252 /* Degrees Celsius * 10 */
#define LM87_SYSCTL_FAN_DIV    2000 /* 1, 2, 4 or 8 */
#define LM87_SYSCTL_ALARMS     2001 /* bitvector */
#define LM87_SYSCTL_ANALOG_OUT 2002
#define LM87_SYSCTL_VID        2003
#define LM87_SYSCTL_VRM        2004

#define LM87_ALARM_IN0          0x0001
#define LM87_ALARM_IN1          0x0002
#define LM87_ALARM_IN2          0x0004
#define LM87_ALARM_IN3          0x0008
#define LM87_ALARM_TEMP1        0x0010
#define LM87_ALARM_TEMP2        0x0020
#define LM87_ALARM_TEMP3        0x0020 /* same?? */
#define LM87_ALARM_FAN1         0x0040
#define LM87_ALARM_FAN2         0x0080
#define LM87_ALARM_IN4          0x0100
#define LM87_ALARM_IN5          0x0200
#define LM87_ALARM_RESERVED1    0x0400
#define LM87_ALARM_RESERVED2    0x0800
#define LM87_ALARM_CHAS         0x1000
#define LM87_ALARM_THERM_SIG    0x2000
#define LM87_ALARM_TEMP2_FAULT  0x4000
#define LM87_ALARM_TEMP3_FAULT 0x08000

/* -- SENSORS SYSCTL END -- */
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
/* -- SENSORS SYSCTL START -- */
#define LM92_SYSCTL_ALARMS		2001	/* high, low, critical */
#define LM92_SYSCTL_TEMP		1200	/* high, low, critical, hysteresis, input */

#define LM92_ALARM_TEMP_HIGH	0x01
#define LM92_ALARM_TEMP_LOW		0x02
#define LM92_ALARM_TEMP_CRIT	0x04
#define LM92_TEMP_HIGH			0x08
#define LM92_TEMP_LOW			0x10
#define LM92_TEMP_CRIT			0x20
#define LM92_TEMP_HYST			0x40
#define LM92_TEMP_INPUT			0x80

/* -- SENSORS SYSCTL END -- */
/* -- SENSORS SYSCTL START -- */
/* volts * 100 */
#define LM93_SYSCTL_IN1				1001
#define LM93_SYSCTL_IN2 			1002
#define LM93_SYSCTL_IN3 			1003
#define LM93_SYSCTL_IN4 			1004
#define LM93_SYSCTL_IN5 			1005
#define LM93_SYSCTL_IN6 			1006
#define LM93_SYSCTL_IN7 			1007
#define LM93_SYSCTL_IN8 			1008
#define LM93_SYSCTL_IN9 			1009
#define LM93_SYSCTL_IN10 			1010
#define LM93_SYSCTL_IN11 			1011
#define LM93_SYSCTL_IN12 			1012
#define LM93_SYSCTL_IN13 			1013
#define LM93_SYSCTL_IN14 			1014
#define LM93_SYSCTL_IN15 			1015
#define LM93_SYSCTL_IN16 			1016

/* degrees Celsius * 10 */
#define LM93_SYSCTL_TEMP1			1101
#define LM93_SYSCTL_TEMP2			1102
#define LM93_SYSCTL_TEMP3			1103

#define LM93_SYSCTL_TEMP1_AUTO_BASE		1111
#define LM93_SYSCTL_TEMP2_AUTO_BASE		1112
#define LM93_SYSCTL_TEMP3_AUTO_BASE		1113

#define LM93_SYSCTL_TEMP1_AUTO_OFFSETS		1121
#define LM93_SYSCTL_TEMP2_AUTO_OFFSETS		1122
#define LM93_SYSCTL_TEMP3_AUTO_OFFSETS		1123

#define LM93_SYSCTL_TEMP1_AUTO_BOOST		1131
#define LM93_SYSCTL_TEMP2_AUTO_BOOST		1132
#define LM93_SYSCTL_TEMP3_AUTO_BOOST		1133

#define LM93_SYSCTL_TEMP1_AUTO_BOOST_HYST	1141
#define LM93_SYSCTL_TEMP2_AUTO_BOOST_HYST	1142
#define LM93_SYSCTL_TEMP3_AUTO_BOOST_HYST	1143

/* 0 => off, 255 => 100% */
#define LM93_SYSCTL_TEMP1_AUTO_PWM_MIN		1151
#define LM93_SYSCTL_TEMP2_AUTO_PWM_MIN		1152
#define LM93_SYSCTL_TEMP3_AUTO_PWM_MIN		1153

/* degrees Celsius * 10 */
#define LM93_SYSCTL_TEMP1_AUTO_OFFSET_HYST	1161
#define LM93_SYSCTL_TEMP2_AUTO_OFFSET_HYST	1162
#define LM93_SYSCTL_TEMP3_AUTO_OFFSET_HYST	1163

/* rotations/minute */
#define LM93_SYSCTL_FAN1			1201
#define LM93_SYSCTL_FAN2			1202
#define LM93_SYSCTL_FAN3			1203
#define LM93_SYSCTL_FAN4			1204

/* 1-2 => enable smart tach mode associated with this pwm #, or disable */
#define LM93_SYSCTL_FAN1_SMART_TACH		1205
#define LM93_SYSCTL_FAN2_SMART_TACH		1206
#define LM93_SYSCTL_FAN3_SMART_TACH		1207
#define LM93_SYSCTL_FAN4_SMART_TACH		1208

/* volts * 1000 */
#define LM93_SYSCTL_VID1			1301
#define LM93_SYSCTL_VID2			1302

/* 0 => off, 255 => 100% */
#define LM93_SYSCTL_PWM1			1401
#define LM93_SYSCTL_PWM2			1402

/* Hz */
#define LM93_SYSCTL_PWM1_FREQ			1411
#define LM93_SYSCTL_PWM2_FREQ			1412

/* bitvector */
#define LM93_SYSCTL_PWM1_AUTO_CHANNELS		1421
#define LM93_SYSCTL_PWM2_AUTO_CHANNELS		1422

/* Hz */
#define LM93_SYSCTL_PWM1_AUTO_SPINUP_MIN	1431
#define LM93_SYSCTL_PWM2_AUTO_SPINUP_MIN	1432

/* seconds */
#define LM93_SYSCTL_PWM1_AUTO_SPINUP_TIME	1441
#define LM93_SYSCTL_PWM2_AUTO_SPINUP_TIME	1442

/* seconds */
#define LM93_SYSCTL_PWM_AUTO_PROCHOT_RAMP	1451
#define LM93_SYSCTL_PWM_AUTO_VRDHOT_RAMP	1452

/* 0 => 0%, 255 => > 99.6% */
#define LM93_SYSCTL_PROCHOT1			1501
#define LM93_SYSCTL_PROCHOT2			1502

/* !0 => enable #PROCHOT logical short */
#define LM93_SYSCTL_PROCHOT_SHORT 		1503

/* 2 boolean enable/disable, 3rd value indicates duty cycle */
#define LM93_SYSCTL_PROCHOT_OVERRIDE		1504

/* 2 values, 0-9 */
#define LM93_SYSCTL_PROCHOT_INTERVAL		1505

/* GPIO input (bitmask) */
#define LM93_SYSCTL_GPIO			1601

/* #VRDHOT input (boolean) */
#define LM93_SYSCTL_VRDHOT1			1701
#define LM93_SYSCTL_VRDHOT2			1702

/* alarms (bitmask) */
#define LM93_SYSCTL_ALARMS			2001

/* alarm bitmask definitions
   The LM93 has nearly 64 bits of error status... I've pared that down to
   what I think is a useful subset in order to fit it into 32 bits.

   Especially note that the #VRD_HOT alarms are missing because we provide
   that information as values in another /proc file.

   If libsensors is extended to support 64 bit values, this could be revisited.
*/
#define LM93_ALARM_IN1		0x00000001
#define LM93_ALARM_IN2		0x00000002
#define LM93_ALARM_IN3		0x00000004
#define LM93_ALARM_IN4		0x00000008
#define LM93_ALARM_IN5		0x00000010
#define LM93_ALARM_IN6		0x00000020
#define LM93_ALARM_IN7		0x00000040
#define LM93_ALARM_IN8		0x00000080
#define LM93_ALARM_IN9		0x00000100
#define LM93_ALARM_IN10		0x00000200
#define LM93_ALARM_IN11		0x00000400
#define LM93_ALARM_IN12		0x00000800
#define LM93_ALARM_IN13		0x00001000
#define LM93_ALARM_IN14		0x00002000
#define LM93_ALARM_IN15		0x00004000
#define LM93_ALARM_IN16		0x00008000
#define LM93_ALARM_FAN1		0x00010000
#define LM93_ALARM_FAN2		0x00020000
#define LM93_ALARM_FAN3		0x00040000
#define LM93_ALARM_FAN4		0x00080000
#define LM93_ALARM_PH1_ERR	0x00100000
#define LM93_ALARM_PH2_ERR	0x00200000
#define LM93_ALARM_SCSI1_ERR	0x00400000
#define LM93_ALARM_SCSI2_ERR	0x00800000
#define LM93_ALARM_DVDDP1_ERR	0x01000000
#define LM93_ALARM_DVDDP2_ERR	0x02000000
#define LM93_ALARM_D1_ERR	0x04000000
#define LM93_ALARM_D2_ERR	0x08000000
#define LM93_ALARM_TEMP1	0x10000000
#define LM93_ALARM_TEMP2	0x20000000
#define LM93_ALARM_TEMP3	0x40000000

/* -- SENSORS SYSCTL END -- */
/* -- SENSORS SYSCTL START -- */

#define LTC1710_SYSCTL_SWITCH_1 1000
#define LTC1710_SYSCTL_SWITCH_2 1001

/* -- SENSORS SYSCTL END -- */
/* -- SENSORS SYSCTL START -- */
#define MATORB_SYSCTL_DISP 1000
/* -- SENSORS SYSCTL END -- */
/* -- SENSORS SYSCTL START -- */

#define MAX1619_SYSCTL_LOCAL_TEMP	1200
#define MAX1619_SYSCTL_REMOTE_TEMP	1201
#define MAX1619_SYSCTL_REMOTE_CRIT	1202
#define MAX1619_SYSCTL_ALARMS		1203

#define MAX1619_ALARM_REMOTE_THIGH	0x10
#define MAX1619_ALARM_REMOTE_TLOW	0x08
#define MAX1619_ALARM_REMOTE_OPEN	0x04
#define MAX1619_ALARM_REMOTE_OVERT	0x02

/* -- SENSORS SYSCTL END -- */
/* -- SENSORS SYSCTL START -- */

#define MAX6650_SYSCTL_FAN1     1101
#define MAX6650_SYSCTL_FAN2     1102
#define MAX6650_SYSCTL_FAN3     1103
#define MAX6650_SYSCTL_FAN4     1104
#define MAX6650_SYSCTL_SPEED    1105
#define MAX6650_SYSCTL_XDUMP    1106


/* -- SENSORS SYSCTL END -- */
/* -- SENSORS SYSCTL START -- */
#define MAXI_SYSCTL_FAN1   1101	/* Rotations/min */
#define MAXI_SYSCTL_FAN2   1102	/* Rotations/min */
#define MAXI_SYSCTL_FAN3   1103	/* Rotations/min */
#define MAXI_SYSCTL_FAN4   1104	/* Rotations/min */
#define MAXI_SYSCTL_TEMP1  1201	/* Degrees Celsius */
#define MAXI_SYSCTL_TEMP2  1202	/* Degrees Celsius */
#define MAXI_SYSCTL_TEMP3  1203	/* Degrees Celsius */
#define MAXI_SYSCTL_TEMP4  1204	/* Degrees Celsius */
#define MAXI_SYSCTL_TEMP5  1205	/* Degrees Celsius */
#define MAXI_SYSCTL_TEMP6  1206	/* Degrees Celsius */
#define MAXI_SYSCTL_PLL    1301	/* MHz */
#define MAXI_SYSCTL_VID1   1401	/* Volts / 6.337, for nba just Volts */
#define MAXI_SYSCTL_VID2   1402	/* Volts */
#define MAXI_SYSCTL_VID3   1403	/* Volts */
#define MAXI_SYSCTL_VID4   1404	/* Volts */
#define MAXI_SYSCTL_VID5   1405	/* Volts */
#define MAXI_SYSCTL_LCD1   1501	/* Line 1 of LCD */
#define MAXI_SYSCTL_LCD2   1502	/* Line 2 of LCD */
#define MAXI_SYSCTL_LCD3   1503	/* Line 3 of LCD */
#define MAXI_SYSCTL_LCD4   1504	/* Line 4 of LCD */
#define MAXI_SYSCTL_ALARMS 2001	/* Bitvector (see below) */

#define MAXI_ALARM_VID4      0x0001
#define MAXI_ALARM_TEMP2     0x0002
#define MAXI_ALARM_VID1      0x0004
#define MAXI_ALARM_VID2      0x0008
#define MAXI_ALARM_VID3      0x0010
#define MAXI_ALARM_PLL       0x0080
#define MAXI_ALARM_TEMP4     0x0100
#define MAXI_ALARM_TEMP5     0x0200
#define MAXI_ALARM_FAN1      0x1000
#define MAXI_ALARM_FAN2      0x2000
#define MAXI_ALARM_FAN3      0x4000

#define MAXI_ALARM_FAN       0x0100	/* To be used with  MaxiLife'99 */
#define MAXI_ALARM_VID       0x0200	/* The MSB specifies which sensor */
#define MAXI_ALARM_TEMP      0x0400	/* in the alarm group failed, i.e.: */
#define MAXI_ALARM_VADD      0x0800	/* 0x0402 = TEMP2 failed = CPU2 temp */

/* -- SENSORS SYSCTL END -- */
/* -- SENSORS SYSCTL START -- */
#define MIC74_SYSCTL_REG0     1000
#define MIC74_SYSCTL_REG1     1001
#define MIC74_SYSCTL_REG2     1002
#define MIC74_SYSCTL_REG3     1003
#define MIC74_SYSCTL_REG4     1004
#define MIC74_SYSCTL_REG5     1005
#define MIC74_SYSCTL_REG6     1006
/* -- SENSORS SYSCTL END -- */
/* -- SENSORS SYSCTL START -- */
#define MTP008_SYSCTL_IN0	1000	/* Volts * 100 */
#define MTP008_SYSCTL_IN1	1001
#define MTP008_SYSCTL_IN2	1002
#define MTP008_SYSCTL_IN3	1003
#define MTP008_SYSCTL_IN4	1004
#define MTP008_SYSCTL_IN5	1005
#define MTP008_SYSCTL_IN6	1006
#define MTP008_SYSCTL_FAN1	1101	/* Rotations/min */
#define MTP008_SYSCTL_FAN2	1102
#define MTP008_SYSCTL_FAN3	1103
#define MTP008_SYSCTL_TEMP1	1200	/* Degrees Celsius * 10 */
#define MTP008_SYSCTL_TEMP2	1201	/* Degrees Celsius * 10 */
#define MTP008_SYSCTL_TEMP3	1202	/* Degrees Celsius * 10 */
#define MTP008_SYSCTL_VID	1300	/* Volts * 100 */
#define MTP008_SYSCTL_PWM1	1401
#define MTP008_SYSCTL_PWM2	1402
#define MTP008_SYSCTL_PWM3	1403
#define MTP008_SYSCTL_SENS1	1501	/* 1, 2, or Beta (3000-5000) */
#define MTP008_SYSCTL_SENS2	1502
#define MTP008_SYSCTL_SENS3	1503
#define MTP008_SYSCTL_FAN_DIV	2000	/* 1, 2, 4 or 8 */
#define MTP008_SYSCTL_ALARMS	2001	/* bitvector */
#define MTP008_SYSCTL_BEEP	2002	/* bitvector */

#define MTP008_ALARM_IN0	0x0001
#define MTP008_ALARM_IN1	0x0002
#define MTP008_ALARM_IN2	0x0004
#define MTP008_ALARM_IN3	0x0008
#define MTP008_ALARM_IN4	0x0100
#define MTP008_ALARM_IN5	0x0200
#define MTP008_ALARM_IN6	0x0400
#define MTP008_ALARM_FAN1	0x0040
#define MTP008_ALARM_FAN2	0x0080
#define MTP008_ALARM_FAN3	0x0800
#define MTP008_ALARM_TEMP1	0x0010
#define MTP008_ALARM_TEMP2	0x0100
#define MTP008_ALARM_TEMP3	0x0200

/* -- SENSORS SYSCTL END -- */
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

#define PC87365_SYSCTL_TEMP1		3101 /* degrees Celsius */
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
/* -- SENSORS SYSCTL START -- */

#define PCA9540_SYSCTL_CHANNEL		1000

/* -- SENSORS SYSCTL END -- */
/* -- SENSORS SYSCTL START -- */
#define PCF8574_SYSCTL_READ     1000
#define PCF8574_SYSCTL_WRITE    1001

/* -- SENSORS SYSCTL END -- */
/* -- SENSORS SYSCTL START -- */
#define PCF8591_SYSCTL_AIN_CONF 1000      /* Analog input configuration */
#define PCF8591_SYSCTL_CH0 1001           /* Input channel 1 */
#define PCF8591_SYSCTL_CH1 1002           /* Input channel 2 */
#define PCF8591_SYSCTL_CH2 1003           /* Input channel 3 */
#define PCF8591_SYSCTL_CH3 1004           /* Input channel 4 */
#define PCF8591_SYSCTL_AOUT_ENABLE 1005   /* Analog output enable flag */
#define PCF8591_SYSCTL_AOUT 1006          /* Analog output */
/* -- SENSORS SYSCTL END -- */
/* -- SENSORS SYSCTL START -- */

#define SAA1064_SYSCTL_BRIGHT     1000 /* Brightness, 0-7 */
#define SAA1064_SYSCTL_TEST       1001 /* Testmode (on = all digits lit) */
#define SAA1064_SYSCTL_DISP       1005 /* four eight bit values */
#define SAA1064_SYSCTL_REFRESH    1006 /* refresh digits in case of powerloss */

/* -- SENSORS SYSCTL END -- */
/* -- SENSORS SYSCTL START -- */
#define SIS5595_SYSCTL_IN0 1000	/* Volts * 100 */
#define SIS5595_SYSCTL_IN1 1001
#define SIS5595_SYSCTL_IN2 1002
#define SIS5595_SYSCTL_IN3 1003
#define SIS5595_SYSCTL_IN4 1004
#define SIS5595_SYSCTL_FAN1 1101	/* Rotations/min */
#define SIS5595_SYSCTL_FAN2 1102
#define SIS5595_SYSCTL_TEMP 1200	/* Degrees Celsius * 10 */
#define SIS5595_SYSCTL_FAN_DIV 2000	/* 1, 2, 4 or 8 */
#define SIS5595_SYSCTL_ALARMS 2001	/* bitvector */

#define SIS5595_ALARM_IN0 0x01
#define SIS5595_ALARM_IN1 0x02
#define SIS5595_ALARM_IN2 0x04
#define SIS5595_ALARM_IN3 0x08
#define SIS5595_ALARM_BTI 0x20
#define SIS5595_ALARM_FAN1 0x40
#define SIS5595_ALARM_FAN2 0x80
#define SIS5595_ALARM_IN4  0x8000
#define SIS5595_ALARM_TEMP 0x8000

/* -- SENSORS SYSCTL END -- */
/* -- SENSORS SYSCTL START -- */

/* Status Register Bits */
/* * * * * * Alarm Bits * * * * */ 
#define SMARTBATT_OVER_CHARGED_ALARM 0x8000 
#define SMARTBATT_TERMINATE_CHARGE_ALARM 0x4000 
#define SMARTBATT_OVER_TEMP_ALARM 0x1000 
#define SMARTBATT_TERMINATE_DISCHARGE_ALARM 0x0800 
#define SMARTBATT_REMAINING_CAPACITY_ALARM  0x0200 
#define SMARTBATT_REMAINING_TIME_ALARM 0x0100 
/* * * * * * Status Bits * * * * */
#define SMARTBATT_INITIALIZED 0x0080 
#define SMARTBATT_DISCHARGING 0x0040 
#define SMARTBATT_FULLY_CHARGED 0x0020 
#define SMARTBATT_FULLY_DISCHARGED 0x0010 
/* * * * * * Error Bits * * * * */ 
#define SMARTBATT_OK 0x0000 
#define SMARTBATT_BUSY 0x0001 
#define SMARTBATT_RESERVED_COMMAND 0x0002 
#define SMARTBATT_UNSUPPORTED_COMMAND 0x0003 
#define SMARTBATT_ACCESS_DENIED 0x0004 
#define SMARTBATT_OVER_UNDERFLOW 0x0005 
#define SMARTBATT_BAD_SIZE 0x0006 
#define SMARTBATT_UNKNOWN_ERROR 0x0007

#define SMARTBATT_ALARM (SMARTBATT_OVER_CHARGED_ALARM \
		| SMARTBATT_TERMINATE_CHARGE_ALARM | SMARTBATT_OVER_TEMP_ALARM \
		| SMARTBATT_TERMINATE_DISCHARGE_ALARM \
		| SMARTBATT_REMAINING_CAPACITY_ALARM \
		| SMARTBATT_REMAINING_TIME_ALARM)

#define SMARTBATT_STATUS (SMARTBATT_INITIALIZED | SMARTBATT_DISCHARGING \
		| SMARTBATT_FULLY_CHARGED | SMARTBATT_FULLY_DISCHARGED )

#define SMARTBATT_ERROR (SMARTBATT_BUSY | SMARTBATT_RESERVED_COMMAND \
		| SMARTBATT_UNSUPPORTED_COMMAND | SMARTBATT_ACCESS_DENIED \
		| SMARTBATT_OVER_UNDERFLOW | SMARTBATT_BAD_SIZE\
		| SMARTBATT_UNKNOWN_ERROR)


#define SMARTBATT_SYSCTL_I 1001
#define SMARTBATT_SYSCTL_V 1002
#define SMARTBATT_SYSCTL_TEMP 1003
#define SMARTBATT_SYSCTL_TIME 1004
#define SMARTBATT_SYSCTL_ALARMS 1005
#define SMARTBATT_SYSCTL_STATUS 1006
#define SMARTBATT_SYSCTL_ERROR 1007
#define SMARTBATT_SYSCTL_CHARGE 1008

/* -- SENSORS SYSCTL END -- */
/* -- SENSORS SYSCTL START -- */
#define ARP_SYSCTL1 1000
#define ARP_SYSCTL2 1001
#define ARP_SYSCTL3 1002
#define ARP_SYSCTL4 1003
#define ARP_SYSCTL5 1004
#define ARP_SYSCTL6 1005
#define ARP_SYSCTL7 1006
#define ARP_SYSCTL8 1007

/* -- SENSORS SYSCTL END -- */
/* -- SENSORS SYSCTL START -- */
#define SMSC47M1_SYSCTL_FAN1 1101   /* Rotations/min */
#define SMSC47M1_SYSCTL_FAN2 1102
#define SMSC47M1_SYSCTL_FAN3 1103
#define SMSC47M1_SYSCTL_PWM1 1401
#define SMSC47M1_SYSCTL_PWM2 1402
#define SMSC47M1_SYSCTL_PWM3 1403
#define SMSC47M1_SYSCTL_FAN_DIV 2000        /* 1, 2, 4 or 8 */
#define SMSC47M1_SYSCTL_ALARMS 2004    /* bitvector */

#define SMSC47M1_ALARM_FAN1 0x0001
#define SMSC47M1_ALARM_FAN2 0x0002
#define SMSC47M1_ALARM_FAN3 0x0004

/* -- SENSORS SYSCTL END -- */
/* -- SENSORS SYSCTL START -- */

#define THMC50_SYSCTL_TEMP 1200	/* Degrees Celsius */
#define THMC50_SYSCTL_REMOTE_TEMP 1201	/* Degrees Celsius */
#define THMC50_SYSCTL_INTER 1202
#define THMC50_SYSCTL_INTER_MASK 1203
#define THMC50_SYSCTL_DIE_CODE 1204
#define THMC50_SYSCTL_ANALOG_OUT 1205

/* -- SENSORS SYSCTL END -- */
/* -- SENSORS SYSCTL START -- */
#define VIA686A_SYSCTL_IN0 1000
#define VIA686A_SYSCTL_IN1 1001
#define VIA686A_SYSCTL_IN2 1002
#define VIA686A_SYSCTL_IN3 1003
#define VIA686A_SYSCTL_IN4 1004
#define VIA686A_SYSCTL_FAN1 1101
#define VIA686A_SYSCTL_FAN2 1102
#define VIA686A_SYSCTL_TEMP 1200
#define VIA686A_SYSCTL_TEMP2 1201
#define VIA686A_SYSCTL_TEMP3 1202
#define VIA686A_SYSCTL_FAN_DIV 2000
#define VIA686A_SYSCTL_ALARMS 2001

#define VIA686A_ALARM_IN0 0x01
#define VIA686A_ALARM_IN1 0x02
#define VIA686A_ALARM_IN2 0x04
#define VIA686A_ALARM_IN3 0x08
#define VIA686A_ALARM_TEMP 0x10
#define VIA686A_ALARM_FAN1 0x40
#define VIA686A_ALARM_FAN2 0x80
#define VIA686A_ALARM_IN4 0x100
#define VIA686A_ALARM_TEMP2 0x800
#define VIA686A_ALARM_CHAS 0x1000
#define VIA686A_ALARM_TEMP3 0x8000

/* -- SENSORS SYSCTL END -- */
/* -- SENSORS SYSCTL START -- */
#define VT1211_SYSCTL_IN0 1000
#define VT1211_SYSCTL_IN1 1001
#define VT1211_SYSCTL_IN2 1002
#define VT1211_SYSCTL_IN3 1003
#define VT1211_SYSCTL_IN4 1004
#define VT1211_SYSCTL_IN5 1005
#define VT1211_SYSCTL_FAN1 1101
#define VT1211_SYSCTL_FAN2 1102
#define VT1211_SYSCTL_TEMP1 1200
#define VT1211_SYSCTL_TEMP2 1201
#define VT1211_SYSCTL_TEMP3 1202
#define VT1211_SYSCTL_TEMP4 1203
#define VT1211_SYSCTL_TEMP5 1204
#define VT1211_SYSCTL_TEMP6 1205
#define VT1211_SYSCTL_TEMP7 1206
#define VT1211_SYSCTL_VID	1300
#define VT1211_SYSCTL_PWM1	1401
#define VT1211_SYSCTL_PWM2	1402
#define VT1211_SYSCTL_VRM	1600
#define VT1211_SYSCTL_UCH	1700
#define VT1211_SYSCTL_FAN_DIV 2000
#define VT1211_SYSCTL_ALARMS 2001

#define VT1211_ALARM_IN1 0x01
#define VT1211_ALARM_IN2 0x02
#define VT1211_ALARM_IN5 0x04
#define VT1211_ALARM_IN3 0x08
#define VT1211_ALARM_TEMP1 0x10
#define VT1211_ALARM_FAN1 0x40
#define VT1211_ALARM_FAN2 0x80
#define VT1211_ALARM_IN4 0x100
#define VT1211_ALARM_TEMP3 0x800
#define VT1211_ALARM_CHAS 0x1000
#define VT1211_ALARM_TEMP2 0x8000
/* duplicates */
#define VT1211_ALARM_IN0 VT1211_ALARM_TEMP3
#define VT1211_ALARM_TEMP4 VT1211_ALARM_IN1
#define VT1211_ALARM_TEMP5 VT1211_ALARM_IN2
#define VT1211_ALARM_TEMP6 VT1211_ALARM_IN3
#define VT1211_ALARM_TEMP7 VT1211_ALARM_IN4

/* -- SENSORS SYSCTL END -- */
/* -- SENSORS SYSCTL START -- */
#define VT8231_SYSCTL_IN0 1000
#define VT8231_SYSCTL_IN1 1001
#define VT8231_SYSCTL_IN2 1002
#define VT8231_SYSCTL_IN3 1003
#define VT8231_SYSCTL_IN4 1004
#define VT8231_SYSCTL_IN5 1005
#define VT8231_SYSCTL_FAN1 1101
#define VT8231_SYSCTL_FAN2 1102
#define VT8231_SYSCTL_TEMP 1200
#define VT8231_SYSCTL_TEMP2 1201
#define VT8231_SYSCTL_TEMP3 1202
#define VT8231_SYSCTL_TEMP4 1203
#define VT8231_SYSCTL_TEMP5 1204
#define VT8231_SYSCTL_TEMP6 1205
#define VT8231_SYSCTL_VID	1300
#define VT8231_SYSCTL_PWM1	1401
#define VT8231_SYSCTL_PWM2	1402
#define VT8231_SYSCTL_VRM	1600
#define VT8231_SYSCTL_UCH	1700
#define VT8231_SYSCTL_FAN_DIV 2000
#define VT8231_SYSCTL_ALARMS 2001

#define VT8231_ALARM_IN1 0x01
#define VT8231_ALARM_IN2 0x02
#define VT8231_ALARM_IN5 0x04
#define VT8231_ALARM_IN3 0x08
#define VT8231_ALARM_TEMP 0x10
#define VT8231_ALARM_FAN1 0x40
#define VT8231_ALARM_FAN2 0x80
#define VT8231_ALARM_IN4 0x100
#define VT8231_ALARM_TEMP2 0x800
#define VT8231_ALARM_CHAS 0x1000
/* duplicates */
#define VT8231_ALARM_IN0 VT8231_ALARM_TEMP2
#define VT8231_ALARM_TEMP3 VT8231_ALARM_IN1
#define VT8231_ALARM_TEMP4 VT8231_ALARM_IN2
#define VT8231_ALARM_TEMP5 VT8231_ALARM_IN3
#define VT8231_ALARM_TEMP6 VT8231_ALARM_IN4

/* -- SENSORS SYSCTL END -- */
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
#define W83781D_SYSCTL_IN9 1009
#define W83781D_SYSCTL_FAN1 1101	/* Rotations/min */
#define W83781D_SYSCTL_FAN2 1102
#define W83781D_SYSCTL_FAN3 1103
#define W83781D_SYSCTL_FAN4 1104
#define W83781D_SYSCTL_FAN5 1105

#define W83781D_SYSCTL_TEMP1 1200	/* Degrees Celsius * 10 */
#define W83781D_SYSCTL_TEMP2 1201	/* Degrees Celsius * 10 */
#define W83781D_SYSCTL_TEMP3 1202	/* Degrees Celsius * 10 */
#define W83781D_SYSCTL_VID 1300		/* Volts * 1000 */
#define W83781D_SYSCTL_VRM 1301
#define W83781D_SYSCTL_PWM1 1401
#define W83781D_SYSCTL_PWM2 1402
#define W83781D_SYSCTL_PWM3 1403
#define W83781D_SYSCTL_PWM4 1404
#define W83781D_SYSCTL_SENS1 1501	/* 1, 2, or Beta (3000-5000) */
#define W83781D_SYSCTL_SENS2 1502
#define W83781D_SYSCTL_SENS3 1503
#define W83781D_SYSCTL_RT1   1601	/* 32-entry table */
#define W83781D_SYSCTL_RT2   1602	/* 32-entry table */
#define W83781D_SYSCTL_RT3   1603	/* 32-entry table */
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
#define W83791D_ALARM_IN7 0x080000	/* 791D only */
#define W83791D_ALARM_IN8 0x100000	/* 791D only */
#define W83791D_ALARM_IN9 0x004000	/* 791D only */
#define W83781D_ALARM_FAN1 0x0040
#define W83781D_ALARM_FAN2 0x0080
#define W83781D_ALARM_FAN3 0x0800
#define W83791D_ALARM_FAN4 0x200000	/* 791D only */
#define W83791D_ALARM_FAN5 0x400000	/* 791D only */
#define W83781D_ALARM_TEMP1 0x0010
#define W83781D_ALARM_TEMP23 0x0020	/* 781D only */
#define W83781D_ALARM_TEMP2 0x0020	/* 782D/783S/791D */
#define W83781D_ALARM_TEMP3 0x2000	/* 782D/791D */
#define W83781D_ALARM_CHAS 0x1000	/* 782D/791D */

#define W83791D_BEEP_IN1 0x002000	/* 791D only */
#define W83791D_BEEP_IN7 0x010000	/* 791D only */
#define W83791D_BEEP_IN8 0x020000	/* 791D only */
#define W83791D_BEEP_TEMP3 0x000002	/* 791D only */

/* -- SENSORS SYSCTL END -- */
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
#define W83792D_SYSCTL_ALARMS 1503

#define W83792D_SYSCTL_THERMAL_CRUISE 1600	/* Smart Fan I: target value */
#define W83792D_SYSCTL_FAN_TOLERANCE 1601	/* Smart Fan I/II: tolerance */
#define W83792D_SYSCTL_SF2_POINTS_FAN1 1602	/* Smart Fan II: Fan1 points */
#define W83792D_SYSCTL_SF2_POINTS_FAN2 1603	/* Smart Fan II: Fan2 points */
#define W83792D_SYSCTL_SF2_POINTS_FAN3 1604	/* Smart Fan II: Fan3 points */
#define W83792D_SYSCTL_SF2_LEVELS_FAN1 1605	/* Smart Fan II: Fan1 levels */
#define W83792D_SYSCTL_SF2_LEVELS_FAN2 1606	/* Smart Fan II: Fan2 levels */
#define W83792D_SYSCTL_SF2_LEVELS_FAN3 1607	/* Smart Fan II: Fan3 levels */

#define W83792D_ALARM_IN0 0x0001
#define W83792D_ALARM_IN1 0x0002
#define W83792D_ALARM_IN2 0x0100
#define W83792D_ALARM_IN3 0x0200
#define W83792D_ALARM_IN4 0x0400
#define W83792D_ALARM_IN5 0x0800
#define W83792D_ALARM_IN6 0x1000
#define W83792D_ALARM_IN7 0x80000
#define W83792D_ALARM_IN8 0x100000
#define W83792D_ALARM_TEMP1 0x0004
#define W83792D_ALARM_TEMP2 0x0008
#define W83792D_ALARM_TEMP3 0x0010
#define W83792D_ALARM_FAN1 0x0020
#define W83792D_ALARM_FAN2 0x0040
#define W83792D_ALARM_FAN3 0x0080
#define W83792D_ALARM_FAN4 0x200000
#define W83792D_ALARM_FAN5 0x400000
#define W83792D_ALARM_FAN6 0x800000
#define W83792D_ALARM_FAN7 0x8000

/* -- SENSORS SYSCTL END -- */
/* -- SENSORS SYSCTL START -- */

#define W83L785TS_SYSCTL_TEMP	1200

/* -- SENSORS SYSCTL END -- */
/* -- SENSORS SYSCTL START -- */

#define XEONTEMP_SYSCTL_REMOTE_TEMP 1201
#define XEONTEMP_SYSCTL_ALARMS 1203

#define XEONTEMP_ALARM_RTEMP_HIGH 0x10
#define XEONTEMP_ALARM_RTEMP_LOW 0x08
#define XEONTEMP_ALARM_RTEMP_NA 0x04

/* -- SENSORS SYSCTL END -- */
#endif
