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

/* Next two must be included before sysctl.h can be included, in 2.0 kernels */
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/sysctl.h>

#ifdef __KERNEL__

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


/* Driver IDs */
#define I2C_DRIVERID_I2CPROC 1001
#define I2C_DRIVERID_LM78 1002
#define I2C_DRIVERID_LM75 1003

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

#define LM75_SYSCTL_TEMP 1200 /* Degrees Celcius * 10 */

#endif /* def SENSORS_SENSORS_H */
