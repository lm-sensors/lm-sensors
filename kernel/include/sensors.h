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

#ifndef SENSORS_NSENSORS_H
#define SENSORS_NSENSORS_H

#include <linux/i2c-proc.h>

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
#define LM78_SYSCTL_TEMP 1200	/* Degrees Celcius * 10 */
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
#define W83781D_SYSCTL_TEMP1 1200	/* Degrees Celcius * 10 */
#define W83781D_SYSCTL_TEMP2 1201	/* Degrees Celcius * 10 */
#define W83781D_SYSCTL_TEMP3 1202	/* Degrees Celcius * 10 */
#define W83781D_SYSCTL_VID 1300	/* Volts * 100 */
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
#define W83781D_ALARM_FAN1 0x0040
#define W83781D_ALARM_FAN2 0x0080
#define W83781D_ALARM_FAN3 0x0800
#define W83781D_ALARM_TEMP1 0x0010
#define W83781D_ALARM_TEMP23 0x0020	/* 781D only */
#define W83781D_ALARM_TEMP2 0x0020	/* 782D/783S */
#define W83781D_ALARM_TEMP3 0x2000	/* 782D only */
#define W83781D_ALARM_CHAS 0x1000

#define LM75_SYSCTL_TEMP 1200	/* Degrees Celcius * 10 */

#define ADM1021_SYSCTL_TEMP 1200
#define ADM1021_SYSCTL_REMOTE_TEMP 1201
#define ADM1021_SYSCTL_DIE_CODE 1202
#define ADM1021_SYSCTL_ALARMS 1203

#define ADM1021_ALARM_TEMP_HIGH 0x40
#define ADM1021_ALARM_TEMP_LOW 0x20
#define ADM1021_ALARM_RTEMP_HIGH 0x10
#define ADM1021_ALARM_RTEMP_LOW 0x08
#define ADM1021_ALARM_RTEMP_NA 0x04

#define GL518_SYSCTL_VDD  1000	/* Volts * 100 */
#define GL518_SYSCTL_VIN1 1001
#define GL518_SYSCTL_VIN2 1002
#define GL518_SYSCTL_VIN3 1003
#define GL518_SYSCTL_FAN1 1101	/* RPM */
#define GL518_SYSCTL_FAN2 1102
#define GL518_SYSCTL_TEMP 1200	/* Degrees Celcius * 10 */
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

#define GL520_SYSCTL_VDD  1000	/* Volts * 100 */
#define GL520_SYSCTL_VIN1 1001
#define GL520_SYSCTL_VIN2 1002
#define GL520_SYSCTL_VIN3 1003
#define GL520_SYSCTL_VIN4 1004
#define GL520_SYSCTL_FAN1 1101	/* RPM */
#define GL520_SYSCTL_FAN2 1102
#define GL520_SYSCTL_TEMP1 1200	/* Degrees Celcius * 10 */
#define GL520_SYSCTL_TEMP2 1201	/* Degrees Celcius * 10 */
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

#define LM80_SYSCTL_IN0 1000	/* Volts * 100 */
#define LM80_SYSCTL_IN1 1001
#define LM80_SYSCTL_IN2 1002
#define LM80_SYSCTL_IN3 1003
#define LM80_SYSCTL_IN4 1004
#define LM80_SYSCTL_IN5 1005
#define LM80_SYSCTL_IN6 1006
#define LM80_SYSCTL_FAN1 1101	/* Rotations/min */
#define LM80_SYSCTL_FAN2 1102
#define LM80_SYSCTL_TEMP 1250	/* Degrees Celcius * 100 */
#define LM80_SYSCTL_FAN_DIV 2000	/* 1, 2, 4 or 8 */
#define LM80_SYSCTL_ALARMS 2001	/* bitvector */

#define ADM9240_SYSCTL_IN0 1000	/* Volts * 100 */
#define ADM9240_SYSCTL_IN1 1001
#define ADM9240_SYSCTL_IN2 1002
#define ADM9240_SYSCTL_IN3 1003
#define ADM9240_SYSCTL_IN4 1004
#define ADM9240_SYSCTL_IN5 1005
#define ADM9240_SYSCTL_FAN1 1101	/* Rotations/min */
#define ADM9240_SYSCTL_FAN2 1102
#define ADM9240_SYSCTL_TEMP 1250	/* Degrees Celcius * 100 */
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

#define ADM1024_SYSCTL_IN0 1000	/* Volts * 100 */
#define ADM1024_SYSCTL_IN1 1001
#define ADM1024_SYSCTL_IN2 1002
#define ADM1024_SYSCTL_IN3 1003
#define ADM1024_SYSCTL_IN4 1004
#define ADM1024_SYSCTL_IN5 1005
#define ADM1024_SYSCTL_FAN1 1101	/* Rotations/min */
#define ADM1024_SYSCTL_FAN2 1102
#define ADM1024_SYSCTL_TEMP 1250	/* Degrees Celcius * 100 */
#define ADM1024_SYSCTL_TEMP1 1290	/* Degrees Celcius */
#define ADM1024_SYSCTL_TEMP2 1295	/* Degrees Celcius */
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

#define ADM1025_SYSCTL_IN0 1000 /* Volts * 100 */
#define ADM1025_SYSCTL_IN1 1001
#define ADM1025_SYSCTL_IN2 1002
#define ADM1025_SYSCTL_IN3 1003
#define ADM1025_SYSCTL_IN4 1004
#define ADM1025_SYSCTL_IN5 1005
#define ADM1025_SYSCTL_RTEMP 1251
#define ADM1025_SYSCTL_TEMP 1250        /* Degrees Celcius * 100 */
#define ADM1025_SYSCTL_ALARMS 2001      /* bitvector */
#define ADM1025_SYSCTL_ANALOG_OUT 2002
#define ADM1025_SYSCTL_VID 2003

#define ADM1025_ALARM_IN0 0x0001
#define ADM1025_ALARM_IN1 0x0002
#define ADM1025_ALARM_IN2 0x0004
#define ADM1025_ALARM_IN3 0x0008
#define ADM1025_ALARM_IN4 0x0100
#define ADM1025_ALARM_IN5 0x0200
#define ADM1025_ALARM_RTEMP 0x0020
#define ADM1025_ALARM_TEMP 0x0010

#define LTC1710_SYSCTL_SWITCH_1 1000
#define LTC1710_SYSCTL_SWITCH_2 1001

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

#define MAXI_SYSCTL_FAN1   1101	/* Rotations/min */
#define MAXI_SYSCTL_FAN2   1102	/* Rotations/min */
#define MAXI_SYSCTL_FAN3   1103	/* Rotations/min */
#define MAXI_SYSCTL_FAN4   1104	/* Rotations/min */
#define MAXI_SYSCTL_TEMP1  1201	/* Degrees Celcius */
#define MAXI_SYSCTL_TEMP2  1202	/* Degrees Celcius */
#define MAXI_SYSCTL_TEMP3  1203	/* Degrees Celcius */
#define MAXI_SYSCTL_TEMP4  1204	/* Degrees Celcius */
#define MAXI_SYSCTL_TEMP5  1205	/* Degrees Celcius */
#define MAXI_SYSCTL_TEMP6  1206	/* Degrees Celcius */
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

#define SIS5595_SYSCTL_IN0 1000	/* Volts * 100 */
#define SIS5595_SYSCTL_IN1 1001
#define SIS5595_SYSCTL_IN2 1002
#define SIS5595_SYSCTL_IN3 1003
#define SIS5595_SYSCTL_IN4 1004
#define SIS5595_SYSCTL_FAN1 1101	/* Rotations/min */
#define SIS5595_SYSCTL_FAN2 1102
#define SIS5595_SYSCTL_TEMP 1200	/* Degrees Celcius * 10 */
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

#define ICSPLL_SYSCTL1 1000

#define BT869_SYSCTL_STATUS 1000
#define BT869_SYSCTL_NTSC   1001
#define BT869_SYSCTL_HALF   1002
#define BT869_SYSCTL_RES    1003
#define BT869_SYSCTL_COLORBARS    1004
#define BT869_SYSCTL_DEPTH  1005
#define BT869_SYSCTL_SVIDEO 1006

#define MATORB_SYSCTL_DISP 1000

#define THMC50_SYSCTL_TEMP 1200	/* Degrees Celcius */
#define THMC50_SYSCTL_REMOTE_TEMP 1201	/* Degrees Celcius */
#define THMC50_SYSCTL_INTER 1202
#define THMC50_SYSCTL_INTER_MASK 1203
#define THMC50_SYSCTL_DIE_CODE 1204
#define THMC50_SYSCTL_ANALOG_OUT 1205

#define DDCMON_SYSCTL_ID 1010
#define DDCMON_SYSCTL_SIZE 1011
#define DDCMON_SYSCTL_SYNC 1012
#define DDCMON_SYSCTL_TIMINGS 1013
#define DDCMON_SYSCTL_SERIAL 1014

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
#define LM87_SYSCTL_TEMP1  1250 /* Degrees Celcius * 100 */
#define LM87_SYSCTL_TEMP2   1251 /* Degrees Celcius * 100 */
#define LM87_SYSCTL_TEMP3   1252 /* Degrees Celcius * 100 */
#define LM87_SYSCTL_FAN_DIV    2000 /* 1, 2, 4 or 8 */
#define LM87_SYSCTL_ALARMS     2001 /* bitvector */
#define LM87_SYSCTL_ANALOG_OUT 2002
#define LM87_SYSCTL_VID        2003

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

#define PCF8574_SYSCTL_READ     1000
#define PCF8574_SYSCTL_WRITE    1001

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
#define MTP008_SYSCTL_TEMP1	1200	/* Degrees Celcius * 10 */
#define MTP008_SYSCTL_TEMP2	1201	/* Degrees Celcius * 10 */
#define MTP008_SYSCTL_TEMP3	1202	/* Degrees Celcius * 10 */
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

#define DS1621_SYSCTL_TEMP 1200	/* Degrees Celcius * 10 */
#define DS1621_SYSCTL_ALARMS 2001	/* bitvector */
#define DS1621_ALARM_TEMP_HIGH 0x40
#define DS1621_ALARM_TEMP_LOW 0x20
#define DS1621_SYSCTL_ENABLE 2002
#define DS1621_SYSCTL_CONTINUOUS 2003
#define DS1621_SYSCTL_POLARITY 2004

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
#define IT87_SYSCTL_TEMP1 1200  /* Degrees Celcius * 10 */
#define IT87_SYSCTL_TEMP2 1201  /* Degrees Celcius * 10 */
#define IT87_SYSCTL_TEMP3 1202  /* Degrees Celcius * 10 */
#define IT87_SYSCTL_VID 1300    /* Volts * 100 */
#define IT87_SYSCTL_FAN_DIV 2000        /* 1, 2, 4 or 8 */
#define IT87_SYSCTL_ALARMS 2004    /* bitvector */

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

#define PCF8591_SYSCTL_AIN_CONF 1000      /* Analog input configuration */
#define PCF8591_SYSCTL_CH0 1001           /* Input channel 1 */
#define PCF8591_SYSCTL_CH1 1002           /* Input channel 2 */
#define PCF8591_SYSCTL_CH2 1003           /* Input channel 3 */
#define PCF8591_SYSCTL_CH3 1004           /* Input channel 4 */
#define PCF8591_SYSCTL_AOUT_ENABLE 1005   /* Analog output enable flag */
#define PCF8591_SYSCTL_AOUT 1006          /* Analog output */

#endif				/* def SENSORS_SENSORS_H */
