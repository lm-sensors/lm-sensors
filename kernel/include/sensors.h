/*
    sensors.h - Part of lm_sensors, Linux kernel modules for hardware
                monitoring
    Copyright (c) 1998  Frodo Looijaard <frodol@dds.nl>

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

/* Important note: */                                           /* TBD */
/* Lines like these, with the 'TBD' remark (To Be Deleted) */   /* TBD */
/* WILL BE DELETED when this file is installed. */              /* TBD */
/* This allows us to get rid of the ugly LM_SENSORS define */   /* TBD */


#ifndef SENSORS_SENSORS_H
#define SENSORS_SENSORS_H

#ifdef __KERNEL__

/* Next two must be included before sysctl.h can be included, in 2.0 kernels */
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/sysctl.h>

/* The type of callback functions used in sensors_{proc,sysctl}_real */
typedef void (*sensors_real_callback) (struct i2c_client *client,
                                       int operation, int ctl_name,
                                       int *nrels_mag, long *results);

/* Values for the operation field in the above function type */
#define SENSORS_PROC_REAL_INFO 1
#define SENSORS_PROC_REAL_READ 2
#define SENSORS_PROC_REAL_WRITE 3

/* These funcion reads or writes a 'real' value (encoded by the combination
   of an integer and a magnitude, the last is the power of ten the value
   should be divided with) to a /proc/sys directory. To use these functions,
   you must (before registering the ctl_table) set the extra2 field to the
   client, and the extra1 field to a function of the form:
      void func(struct i2c_client *client, int operation, int ctl_name,
                int *nrels_mag, long *results)
   This last function can be called for three values of operation. If
   operation equals SENSORS_PROC_REAL_INFO, the magnitude should be returned
   in nrels_mag. If operation equals SENSORS_PROC_REAL_READ, values should
   be read into results. nrels_mag should return the number of elements
   read; the maximum number is put in it on entry. Finally, if operation
   equals SENSORS_PROC_REAL_WRITE, the values in results should be
   written to the chip. nrels_mag contains on entry the number of elements
   found.
   In all cases, client points to the client we wish to interact with,
   and ctl_name is the SYSCTL id of the file we are accessing. */
extern int sensors_sysctl_real (ctl_table *table, int *name, int nlen,
                                void *oldval, size_t *oldlenp, void *newval,
                                size_t newlen, void **context);
extern int sensors_proc_real(ctl_table *ctl, int write, struct file * filp,
                             void *buffer, size_t *lenp);



/* These rather complex functions must be called when you want to add or
   delete an entry in /proc/sys/dev/sensors/chips (not yet implemented). It
   also creates a new directory within /proc/sys/dev/sensors/.
   ctl_template should be a template of the newly created directory. It is
   copied in memory. The extra2 field of each file is set to point to client.
   If any driver wants subdirectories within the newly created directory,
   these functions must be updated! */
extern int sensors_register_entry(struct i2c_client *client,
                                  const char *prefix, ctl_table *ctl_template);
extern void sensors_deregister_entry(int id);

#endif /* def __KERNEL__ */


/* The maximum length of the prefix */
#define SENSORS_PREFIX_MAX 20

/* Driver IDs */
#define I2C_DRIVERID_I2CDEV 1000
#define I2C_DRIVERID_I2CPROC 1001
#define I2C_DRIVERID_LM78 1002
#define I2C_DRIVERID_LM75 1003
#define I2C_DRIVERID_GL518 1004
#define I2C_DRIVERID_EEPROM 1005
#define I2C_DRIVERID_W83781D 1006
#define I2C_DRIVERID_LM80 1007
#define I2C_DRIVERID_ADM1021 1008
#define I2C_DRIVERID_ADM9240 1009
#define I2C_DRIVERID_LTC1710 1010
#define I2C_DRIVERID_SIS5595 1011

/* Sysctl IDs */
#ifdef DEV_HWMON
#define DEV_SENSORS DEV_HWMON
#else /* ndef DEV_HWMOM */
#define DEV_SENSORS 2  /* The id of the lm_sensors directory within the
                          dev table */
#endif /* def DEV_HWMON */

#define SENSORS_CHIPS 1
struct sensors_chips_data {
  int sysctl_id;
  char name[SENSORS_PREFIX_MAX + 13];
};

#define LM78_SYSCTL_IN0 1000  /* Volts * 100 */
#define LM78_SYSCTL_IN1 1001
#define LM78_SYSCTL_IN2 1002
#define LM78_SYSCTL_IN3 1003
#define LM78_SYSCTL_IN4 1004
#define LM78_SYSCTL_IN5 1005
#define LM78_SYSCTL_IN6 1006
#define LM78_SYSCTL_FAN1 1101 /* Rotations/min */
#define LM78_SYSCTL_FAN2 1102
#define LM78_SYSCTL_FAN3 1103
#define LM78_SYSCTL_TEMP 1200 /* Degrees Celcius * 10 */
#define LM78_SYSCTL_VID 1300 /* Volts * 100 */
#define LM78_SYSCTL_FAN_DIV 2000 /* 1, 2, 4 or 8 */
#define LM78_SYSCTL_ALARMS 2001 /* bitvector */

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

#define W83781D_SYSCTL_IN0 1000  /* Volts * 100 */
#define W83781D_SYSCTL_IN1 1001
#define W83781D_SYSCTL_IN2 1002
#define W83781D_SYSCTL_IN3 1003
#define W83781D_SYSCTL_IN4 1004
#define W83781D_SYSCTL_IN5 1005
#define W83781D_SYSCTL_IN6 1006
#define W83781D_SYSCTL_FAN1 1101 /* Rotations/min */
#define W83781D_SYSCTL_FAN2 1102
#define W83781D_SYSCTL_FAN3 1103
#define W83781D_SYSCTL_TEMP1 1200 /* Degrees Celcius * 10 */
#define W83781D_SYSCTL_TEMP2 1201 /* Degrees Celcius * 10 */
#define W83781D_SYSCTL_TEMP3 1202 /* Degrees Celcius * 10 */
#define W83781D_SYSCTL_VID 1300 /* Volts * 100 */
#define W83781D_SYSCTL_FAN_DIV 2000 /* 1, 2, 4 or 8 */
#define W83781D_SYSCTL_ALARMS 2001 /* bitvector */
#define W83781D_SYSCTL_BEEP 2002 /* bitvector */

#define W83781D_ALARM_IN0 0x0001
#define W83781D_ALARM_IN1 0x0002
#define W83781D_ALARM_IN2 0x0004
#define W83781D_ALARM_IN3 0x0008
#define W83781D_ALARM_IN4 0x0100
#define W83781D_ALARM_IN5 0x0200
#define W83781D_ALARM_IN6 0x0400
#define W83781D_ALARM_FAN1 0x0040
#define W83781D_ALARM_FAN2 0x0080
#define W83781D_ALARM_FAN3 0x0800
#define W83781D_ALARM_TEMP1 0x0010
#define W83781D_ALARM_TEMP23 0x0020
#define W83781D_ALARM_CHAS 0x1000

#define LM75_SYSCTL_TEMP 1200 /* Degrees Celcius * 10 */

#define ADM1021_SYSCTL_TEMP 1200 
#define ADM1021_SYSCTL_REMOTE_TEMP 1201
#define ADM1021_SYSCTL_DIE_CODE 1202 
#define ADM1021_SYSCTL_STATUS 1203 

#define GL518_SYSCTL_VDD  1000     /* Volts * 100 */
#define GL518_SYSCTL_VIN1 1001
#define GL518_SYSCTL_VIN2 1002
#define GL518_SYSCTL_VIN3 1003
#define GL518_SYSCTL_FAN1 1101     /* RPM */
#define GL518_SYSCTL_FAN2 1102
#define GL518_SYSCTL_TEMP 1200     /* Degrees Celcius * 10 */
#define GL518_SYSCTL_VID 1300    
#define GL518_SYSCTL_FAN_DIV 2000  /* 1, 2, 4 or 8 */
#define GL518_SYSCTL_ALARMS 2001   /* bitvector */
#define GL518_SYSCTL_BEEP 2002     /* bitvector */

#define GL518_ALARM_VDD 0x01
#define GL518_ALARM_VIN1 0x02
#define GL518_ALARM_VIN2 0x04
#define GL518_ALARM_VIN3 0x08
#define GL518_ALARM_TEMP 0x10
#define GL518_ALARM_FAN1 0x20
#define GL518_ALARM_FAN2 0x40

#define EEPROM_SYSCTL1 1000
#define EEPROM_SYSCTL2 1001
#define EEPROM_SYSCTL3 1002
#define EEPROM_SYSCTL4 1003
#define EEPROM_SYSCTL5 1004
#define EEPROM_SYSCTL6 1005
#define EEPROM_SYSCTL7 1006
#define EEPROM_SYSCTL8 1007

#define LM80_SYSCTL_IN0 1000  /* Volts * 100 */
#define LM80_SYSCTL_IN1 1001
#define LM80_SYSCTL_IN2 1002
#define LM80_SYSCTL_IN3 1003
#define LM80_SYSCTL_IN4 1004
#define LM80_SYSCTL_IN5 1005
#define LM80_SYSCTL_IN6 1006
#define LM80_SYSCTL_FAN1 1101 /* Rotations/min */
#define LM80_SYSCTL_FAN2 1102
#define LM80_SYSCTL_TEMP 1250 /* Degrees Celcius * 100 */
#define LM80_SYSCTL_FAN_DIV 2000 /* 1, 2, 4 or 8 */
#define LM80_SYSCTL_ALARMS 2001 /* bitvector */

#define ADM9240_SYSCTL_IN0 1000  /* Volts * 100 */
#define ADM9240_SYSCTL_IN1 1001
#define ADM9240_SYSCTL_IN2 1002
#define ADM9240_SYSCTL_IN3 1003
#define ADM9240_SYSCTL_IN4 1004
#define ADM9240_SYSCTL_IN5 1005
#define ADM9240_SYSCTL_FAN1 1101 /* Rotations/min */
#define ADM9240_SYSCTL_FAN2 1102
#define ADM9240_SYSCTL_TEMP 1250 /* Degrees Celcius * 100 */
#define ADM9240_SYSCTL_FAN_DIV 2000 /* 1, 2, 4 or 8 */
#define ADM9240_SYSCTL_ALARMS 2001 /* bitvector */
#define ADM9240_SYSCTL_ANALOG_OUT 2002
#define ADM9240_SYSCTL_VID 2003

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
#endif /* def SENSORS_SENSORS_H */
