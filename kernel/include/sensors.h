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


/* A structure containing detect information.
   Force variables overrule all other variables; they force a detection on
   that place. If a specific chip is given, the module blindly assumes this
   chip type is present; if a general force (kind == 0) is given, the module
   will still try to figure out what type of chip is present. This is useful
   if for some reasons the detect for SMBus or ISA address space filled
   fails.
   probe: insmod parameter. Initialize this list with SENSORS_I2C_END values.
     A list of pairs. The first value is a bus number (SENSORS_ISA_BUS for
     the ISA bus, -1 for any I2C bus), the second is the address. 
   kind: The kind of chip. 0 equals any chip.
*/
struct sensors_force_data {
  unsigned short *force;
  unsigned short kind;
};

/* A structure containing the detect information.
   normal_i2c: filled in by the module writer. Terminated by SENSORS_I2C_END.
     A list of I2C addresses which should normally be examined.
   normal_i2c_range: filled in by the module writer. Terminated by 
     SENSORS_I2C_END
     A list of pairs of I2C addresses, each pair being an inclusive range of
     addresses which should normally be examined.
   normal_isa: filled in by the module writer. Terminated by SENSORS_ISA_END.
     A list of ISA addresses which should normally be examined.
   normal_isa_range: filled in by the module writer. Terminated by 
     SENSORS_ISA_END
     A list of triples. The first two elements are ISA addresses, being an
     range of addresses which should normally be examined. The third is the
     modulo parameter: only addresses which are 0 module this value relative
     to the first address of the range are actually considered.
   probe: insmod parameter. Initialize this list with SENSORS_I2C_END values.
     A list of pairs. The first value is a bus number (SENSORS_ISA_BUS for
     the ISA bus, -1 for any I2C bus), the second is the address. These
     addresses are also probed, as if they were in the 'normal' list.
   probe_range: insmod parameter. Initialize this list with SENSORS_I2C_END 
     values.
     A list of triples. The first value is a bus number (SENSORS_ISA_BUS for
     the ISA bus, -1 for any I2C bus), the second and third are addresses. 
     These form an inclusive range of addresses that are also probed, as
     if they were in the 'normal' list.
   ignore: insmod parameter. Initialize this list with SENSORS_I2C_END values.
     A list of pairs. The first value is a bus number (SENSORS_ISA_BUS for
     the ISA bus, -1 for any I2C bus), the second is the I2C address. These
     addresses are never probed. This parameter overrules 'normal' and 
     'probe', but not the 'force' lists.
   ignore_range: insmod parameter. Initialize this list with SENSORS_I2C_END 
      values.
     A list of triples. The first value is a bus number (SENSORS_ISA_BUS for
     the ISA bus, -1 for any I2C bus), the second and third are addresses. 
     These form an inclusive range of I2C addresses that are never probed.
     This parameter overrules 'normal' and 'probe', but not the 'force' lists.
   force_data: insmod parameters. A list, ending with an element of which
     the force field is NULL.
*/
struct sensors_address_data {
  unsigned short *normal_i2c;
  unsigned short *normal_i2c_range;
  unsigned int *normal_isa;
  unsigned int *normal_isa_range;
  unsigned short *probe;
  unsigned short *probe_range;
  unsigned short *ignore;
  unsigned short *ignore_range;
  struct sensors_force_data *forces;
};

/* Internal numbers to terminate lists */
#define SENSORS_I2C_END 0xfffe
#define SENSORS_ISA_END 0xfffefffe

/* The numbers to use to set an ISA or I2C bus address */
#define SENSORS_ISA_BUS 9191
#define SENSORS_ANY_I2C_BUS 0xffff

/* The length of the option lists */
#define SENSORS_MAX_OPTS 48

/* Default fill of many variables */
#define SENSORS_DEFAULTS {SENSORS_I2C_END, SENSORS_I2C_END, SENSORS_I2C_END, \
                          SENSORS_I2C_END, SENSORS_I2C_END, SENSORS_I2C_END, \
                          SENSORS_I2C_END, SENSORS_I2C_END, SENSORS_I2C_END, \
                          SENSORS_I2C_END, SENSORS_I2C_END, SENSORS_I2C_END, \
                          SENSORS_I2C_END, SENSORS_I2C_END, SENSORS_I2C_END, \
                          SENSORS_I2C_END, SENSORS_I2C_END, SENSORS_I2C_END, \
                          SENSORS_I2C_END, SENSORS_I2C_END, SENSORS_I2C_END, \
                          SENSORS_I2C_END, SENSORS_I2C_END, SENSORS_I2C_END, \
                          SENSORS_I2C_END, SENSORS_I2C_END, SENSORS_I2C_END, \
                          SENSORS_I2C_END, SENSORS_I2C_END, SENSORS_I2C_END, \
                          SENSORS_I2C_END, SENSORS_I2C_END, SENSORS_I2C_END, \
                          SENSORS_I2C_END, SENSORS_I2C_END, SENSORS_I2C_END, \
                          SENSORS_I2C_END, SENSORS_I2C_END, SENSORS_I2C_END, \
                          SENSORS_I2C_END, SENSORS_I2C_END, SENSORS_I2C_END, \
                          SENSORS_I2C_END, SENSORS_I2C_END, SENSORS_I2C_END, \
                          SENSORS_I2C_END, SENSORS_I2C_END, SENSORS_I2C_END}

#define SENSORS_MODPARM_AUX1(x) "1-" #x "h"
#define SENSORS_MODPARM_AUX(x) SENSORS_MODPARM_AUX1(x)
#define SENSORS_MODPARM SENSORS_MODPARM_AUX(SENSORS_MAX_OPTS)

#define SENSORS_CONCAT(x,y) x ## y
#define MODULE_PARM1(x,y) MODULE_PARM(x,y)

/* This defines several insmod variables, and the addr_data structure */
#define SENSORS_INSMOD \
  MODULE_PARM(probe,SENSORS_MODPARM); \
  static unsigned short probe[SENSORS_MAX_OPTS] = SENSORS_DEFAULTS; \
  MODULE_PARM(probe_range,SENSORS_MODPARM); \
  static unsigned short probe_range[SENSORS_MAX_OPTS] = SENSORS_DEFAULTS; \
  MODULE_PARM(ignore,SENSORS_MODPARM); \
  static unsigned short ignore[SENSORS_MAX_OPTS] = SENSORS_DEFAULTS; \
  MODULE_PARM(ignore_range,SENSORS_MODPARM); \
  static unsigned short ignore_range [SENSORS_MAX_OPTS]  = SENSORS_DEFAULTS; \
  static struct sensors_address_data addr_data = \
                                       {normal_i2c, normal_i2c_range, \
                                        normal_isa, normal_isa_range, \
                                        probe, probe_range, \
                                        ignore, ignore_range, \
                                        forces}

/* The following functions assume the existence of an enum with the chip
   names as elements. The first element of the enum should be any_chip */

#define SENSORS_INSMOD_0 \
  enum chips { any_chip }; \
  MODULE_PARM(force,SENSORS_MODPARM); \
  static unsigned short force[SENSORS_MAX_OPTS] = SENSORS_DEFAULTS; \
  static struct sensors_force_data forces[] = {{force,any_chip},{NULL}}; \
  SENSORS_INSMOD

#define SENSORS_INSMOD_1(chip1) \
  enum chips { any_chip, chip1 }; \
  MODULE_PARM(force,SENSORS_MODPARM); \
  static unsigned short force[SENSORS_MAX_OPTS] = SENSORS_DEFAULTS; \
  MODULE_PARM1(SENSORS_CONCAT(force_,chip1),SENSORS_MODPARM); \
  static unsigned short force_ ## chip1 [SENSORS_MAX_OPTS] = SENSORS_DEFAULTS; \
  static struct sensors_force_data forces[] = {{force,any_chip},\
                                                 {force_ ## chip1,chip1}, \
                                                 {NULL}}; \
  SENSORS_INSMOD

#define SENSORS_INSMOD_2(chip1,chip2) \
  enum chips { any_chip, chip1, chip2 }; \
  MODULE_PARM(force,SENSORS_MODPARM); \
  static unsigned short force[SENSORS_MAX_OPTS] = SENSORS_DEFAULTS; \
  MODULE_PARM1(SENSORS_CONCAT(force_,chip1),SENSORS_MODPARM); \
  static unsigned short force_ ## chip1 [SENSORS_MAX_OPTS] = SENSORS_DEFAULTS; \
  MODULE_PARM1(SENSORS_CONCAT(force_,chip2),SENSORS_MODPARM); \
  static unsigned short force_ ## chip2 [SENSORS_MAX_OPTS] = SENSORS_DEFAULTS; \
  static struct sensors_force_data forces[] = {{force,any_chip}, \
                                                 {force_ ## chip1,chip2}, \
                                                 {force_ ## chip2,nr2}, \
                                                 {NULL}}; \
  SENSORS_INSMOD

#define SENSORS_INSMOD_3(chip1,chip2,chip3) \
  enum chips { any_chip, chip1, chip2, chip3 }; \
  MODULE_PARM(force,SENSORS_MODPARM); \
  static unsigned short force[SENSORS_MAX_OPTS] = SENSORS_DEFAULTS; \
  MODULE_PARM1(SENSORS_CONCAT(force_,chip1),SENSORS_MODPARM); \
  static unsigned short force_ ## chip1 [SENSORS_MAX_OPTS] = SENSORS_DEFAULTS; \
  MODULE_PARM1(SENSORS_CONCAT(force_,chip2),SENSORS_MODPARM); \
  static unsigned short force_ ## chip2 [SENSORS_MAX_OPTS] = SENSORS_DEFAULTS; \
  MODULE_PARM1(SENSORS_CONCAT(force_,chip3),SENSORS_MODPARM); \
  static unsigned short force_ ## chip3 [SENSORS_MAX_OPTS] = SENSORS_DEFAULTS; \
  static struct sensors_force_data forces[] = {{force,any_chip}, \
                                                 {force_ ## chip1,chip1}, \
                                                 {force_ ## chip2,chip2}, \
                                                 {force_ ## chip3,chip3}, \
                                                 {NULL}}; \
  SENSORS_INSMOD

typedef int sensors_found_addr_proc (struct i2c_adapter *adapter, 
                                     int addr, int kind);

/* Detect function. It itterates over all possible addresses itself. For
   SMBus addresses, it will only call found_proc if some client is connected
   to the SMBus (unless a 'force' matched); for ISA detections, this is not
   done. */
extern int sensors_detect(struct i2c_adapter *adapter,
                          struct sensors_address_data *address_data,
                          sensors_found_addr_proc *found_proc);

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
#define I2C_DRIVERID_ICSPLL 1012

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
#define W83781D_SYSCTL_IN7 1007
#define W83781D_SYSCTL_IN8 1008
#define W83781D_SYSCTL_FAN1 1101 /* Rotations/min */
#define W83781D_SYSCTL_FAN2 1102
#define W83781D_SYSCTL_FAN3 1103
#define W83781D_SYSCTL_TEMP1 1200 /* Degrees Celcius * 10 */
#define W83781D_SYSCTL_TEMP2 1201 /* Degrees Celcius * 10 */
#define W83781D_SYSCTL_TEMP3 1202 /* Degrees Celcius * 10 */
#define W83781D_SYSCTL_VID 1300 /* Volts * 100 */
#define W83781D_SYSCTL_PWM1 1401
#define W83781D_SYSCTL_PWM2 1402
#define W83781D_SYSCTL_PWM3 1403
#define W83781D_SYSCTL_PWM4 1404
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
#define W83782D_ALARM_IN7 0x10000
#define W83782D_ALARM_IN8 0x20000
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

#define ICSPLL_SYSCTL1 1000
#endif /* def SENSORS_SENSORS_H */
