/*
    sensors.h - A Linux module for reading sensor data.
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

#ifndef SENSORS_SENSORS_H
#define SENSORS_SENSORS_H

#include <linux/sysctl.h>

#ifdef __KERNEL__
extern int sensors_register_entry(struct i2c_client *client,
                                  const char *prefix, ctl_table *ctl_template);
extern void sensors_deregister_entry(int id);
extern void sensors_parse_reals(int *nrels, void *buffer, int bufsize,
                                long *results, int magnitude);
extern void sensors_write_reals(int nrels,void *buffer,int *bufsize,
                                long *results, int magnitude);

#endif /* def __KERNEL__ */


/* Driver IDs */
#define I2C_DRIVERID_I2CPROC 1001
#define I2C_DRIVERID_LM78 1002

/* Sysctl IDs */
#ifdef DEV_HWMON
#define DEV_SENSORS DEV_HWMON
#else /* ndef DEV_HWMOM */
#define DEV_SENSORS 2  /* The id of the lm_sensors directory within the
                          dev table */
#endif /* def DEV_HWMON */

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

#endif /* def SENSORS_SENSORS_H */
