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
extern int sensors_register_entry(struct i2c_client *client ,
		                  const char *prefix,
                                  ctl_table *ctl_template, 
       			          struct module *controlling_mod);

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

/* This is ugly. We need to evaluate SENSORS_MAX_OPTS before it is 
   stringified */
#define SENSORS_MODPARM_AUX1(x) "1-" #x "h"
#define SENSORS_MODPARM_AUX(x) SENSORS_MODPARM_AUX1(x)
#define SENSORS_MODPARM SENSORS_MODPARM_AUX(SENSORS_MAX_OPTS)

/* SENSORS_MODULE_PARM creates a module parameter, and puts it in the
   module header */
#define SENSORS_MODULE_PARM(var,desc) \
  static unsigned short var[SENSORS_MAX_OPTS] = SENSORS_DEFAULTS; \
  MODULE_PARM(var,SENSORS_MODPARM); \
  MODULE_PARM_DESC(var,desc)

/* SENSORS_MODULE_PARM creates a 'force_*' module parameter, and puts it in
   the module header */
#define SENSORS_MODULE_PARM_FORCE(name) \
  SENSORS_MODULE_PARM(force_ ## name, \
                      "List of adapter,address pairs which are unquestionably" \
                      " assumed to contain a `" # name "' chip")
                         

/* This defines several insmod variables, and the addr_data structure */
#define SENSORS_INSMOD \
  SENSORS_MODULE_PARM(probe, \
                      "List of adapter,address pairs to scan additionally"); \
  SENSORS_MODULE_PARM(probe_range, \
                      "List of adapter,start-addr,end-addr triples to scan " \
                      "additionally"); \
  SENSORS_MODULE_PARM(ignore, \
                      "List of adapter,address pairs not to scan"); \
  SENSORS_MODULE_PARM(ignore_range, \
                      "List of adapter,start-addr,end-addr triples not to " \
                      "scan"); \
  static struct sensors_address_data addr_data = \
                                       {normal_i2c, normal_i2c_range, \
                                        normal_isa, normal_isa_range, \
                                        probe, probe_range, \
                                        ignore, ignore_range, \
                                        forces}

/* The following functions create an enum with the chip names as elements. 
   The first element of the enum is any_chip. These are the only macros
   a module will want to use. */

#define SENSORS_INSMOD_0 \
  enum chips { any_chip }; \
  SENSORS_MODULE_PARM(force, \
                      "List of adapter,address pairs to boldly assume " \
                      "to be present"); \
  static struct sensors_force_data forces[] = {{force,any_chip},{NULL}}; \
  SENSORS_INSMOD

#define SENSORS_INSMOD_1(chip1) \
  enum chips { any_chip, chip1 }; \
  SENSORS_MODULE_PARM(force, \
                      "List of adapter,address pairs to boldly assume " \
                      "to be present"); \
  SENSORS_MODULE_PARM_FORCE(chip1); \
  static struct sensors_force_data forces[] = {{force,any_chip},\
                                                 {force_ ## chip1,chip1}, \
                                                 {NULL}}; \
  SENSORS_INSMOD

#define SENSORS_INSMOD_2(chip1,chip2) \
  enum chips { any_chip, chip1, chip2 }; \
  SENSORS_MODULE_PARM(force, \
                      "List of adapter,address pairs to boldly assume " \
                      "to be present"); \
  SENSORS_MODULE_PARM_FORCE(chip1); \
  SENSORS_MODULE_PARM_FORCE(chip2); \
  static struct sensors_force_data forces[] = {{force,any_chip}, \
                                                 {force_ ## chip1,chip1}, \
                                                 {force_ ## chip2,chip2}, \
                                                 {NULL}}; \
  SENSORS_INSMOD

#define SENSORS_INSMOD_3(chip1,chip2,chip3) \
  enum chips { any_chip, chip1, chip2, chip3 }; \
  SENSORS_MODULE_PARM(force, \
                      "List of adapter,address pairs to boldly assume " \
                      "to be present"); \
  SENSORS_MODULE_PARM_FORCE(chip1); \
  SENSORS_MODULE_PARM_FORCE(chip2); \
  SENSORS_MODULE_PARM_FORCE(chip3); \
  static struct sensors_force_data forces[] = {{force,any_chip}, \
                                                 {force_ ## chip1,chip1}, \
                                                 {force_ ## chip2,chip2}, \
                                                 {force_ ## chip3,chip3}, \
                                                 {NULL}}; \
  SENSORS_INSMOD

#define SENSORS_INSMOD_4(chip1,chip2,chip3,chip4) \
  enum chips { any_chip, chip1, chip2, chip3, chip4 }; \
  SENSORS_MODULE_PARM(force, \
                      "List of adapter,address pairs to boldly assume " \
                      "to be present"); \
  SENSORS_MODULE_PARM_FORCE(chip1); \
  SENSORS_MODULE_PARM_FORCE(chip2); \
  SENSORS_MODULE_PARM_FORCE(chip3); \
  SENSORS_MODULE_PARM_FORCE(chip4); \
  static struct sensors_force_data forces[] = {{force,any_chip}, \
                                                 {force_ ## chip1,chip1}, \
                                                 {force_ ## chip2,chip2}, \
                                                 {force_ ## chip3,chip3}, \
                                                 {force_ ## chip4,chip4}, \
                                                 {NULL}}; \
  SENSORS_INSMOD

#define SENSORS_INSMOD_5(chip1,chip2,chip3,chip4,chip5) \
  enum chips { any_chip, chip1, chip2, chip3, chip4, chip5 }; \
  SENSORS_MODULE_PARM(force, \
                      "List of adapter,address pairs to boldly assume " \
                      "to be present"); \
  SENSORS_MODULE_PARM_FORCE(chip1); \
  SENSORS_MODULE_PARM_FORCE(chip2); \
  SENSORS_MODULE_PARM_FORCE(chip3); \
  SENSORS_MODULE_PARM_FORCE(chip4); \
  SENSORS_MODULE_PARM_FORCE(chip5); \
  static struct sensors_force_data forces[] = {{force,any_chip}, \
                                                 {force_ ## chip1,chip1}, \
                                                 {force_ ## chip2,chip2}, \
                                                 {force_ ## chip3,chip3}, \
                                                 {force_ ## chip4,chip4}, \
                                                 {force_ ## chip5,chip5}, \
                                                 {NULL}}; \
  SENSORS_INSMOD

#define SENSORS_INSMOD_6(chip1,chip2,chip3,chip4,chip5,chip6) \
  enum chips { any_chip, chip1, chip2, chip3, chip4, chip5, chip6 }; \
  SENSORS_MODULE_PARM(force, \
                      "List of adapter,address pairs to boldly assume " \
                      "to be present"); \
  SENSORS_MODULE_PARM_FORCE(chip1); \
  SENSORS_MODULE_PARM_FORCE(chip2); \
  SENSORS_MODULE_PARM_FORCE(chip3); \
  SENSORS_MODULE_PARM_FORCE(chip4); \
  SENSORS_MODULE_PARM_FORCE(chip5); \
  SENSORS_MODULE_PARM_FORCE(chip6); \
  static struct sensors_force_data forces[] = {{force,any_chip}, \
                                                 {force_ ## chip1,chip1}, \
                                                 {force_ ## chip2,chip2}, \
                                                 {force_ ## chip3,chip3}, \
                                                 {force_ ## chip4,chip4}, \
                                                 {force_ ## chip5,chip5}, \
                                                 {force_ ## chip6,chip6}, \
                                                 {NULL}}; \
  SENSORS_INSMOD

typedef int sensors_found_addr_proc (struct i2c_adapter *adapter, 
                                     int addr, unsigned short flags,
                                     int kind);

/* Detect function. It itterates over all possible addresses itself. For
   SMBus addresses, it will only call found_proc if some client is connected
   to the SMBus (unless a 'force' matched); for ISA detections, this is not
   done. */
extern int sensors_detect(struct i2c_adapter *adapter,
                          struct sensors_address_data *address_data,
                          sensors_found_addr_proc *found_proc);


/* This macro is used to scale user-input to sensible values in almost all
   chip drivers. */
extern inline int SENSORS_LIMIT(long value, long low, long high)
{
  if (value < low)
    return low;
  else if (value > high)
    return high;
  else
    return value;
}

#endif /* def __KERNEL__ */


/* The maximum length of the prefix */
#define SENSORS_PREFIX_MAX 20

/* IDs --   Use DRIVERIDs 1000-1999 for sensors. 
   Other drivers define the id in linux/i2c.h     */
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
#define I2C_DRIVERID_BT869 1013
#define I2C_DRIVERID_MAXILIFE 1014
#define I2C_DRIVERID_MATORB 1015
#define I2C_DRIVERID_GL520 1016
#define I2C_DRIVERID_THMC50 1017

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
#define W83781D_SYSCTL_SENS1 1501   /* 1, 2, or Beta (3000-5000) */
#define W83781D_SYSCTL_SENS2 1502
#define W83781D_SYSCTL_SENS3 1503
#define W83781D_SYSCTL_RT1   1601   /* 32-entry table */
#define W83781D_SYSCTL_RT2   1602   /* 32-entry table */
#define W83781D_SYSCTL_RT3   1603   /* 32-entry table */
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
#define W83781D_ALARM_TEMP23 0x0020  /* 781D only */
#define W83781D_ALARM_TEMP2 0x0020   /* 782D/783S */
#define W83781D_ALARM_TEMP3 0x2000   /* 782D only */
#define W83781D_ALARM_CHAS 0x1000

#define LM75_SYSCTL_TEMP 1200 /* Degrees Celcius * 10 */

#define ADM1021_SYSCTL_TEMP 1200 
#define ADM1021_SYSCTL_REMOTE_TEMP 1201
#define ADM1021_SYSCTL_DIE_CODE 1202 
#define ADM1021_SYSCTL_ALARMS 1203 

#define ADM1021_ALARM_TEMP_HIGH 0x40
#define ADM1021_ALARM_TEMP_LOW 0x20
#define ADM1021_ALARM_RTEMP_HIGH 0x10
#define ADM1021_ALARM_RTEMP_LOW 0x08
#define ADM1021_ALARM_RTEMP_NA 0x04

#define GL518_SYSCTL_VDD  1000     /* Volts * 100 */
#define GL518_SYSCTL_VIN1 1001
#define GL518_SYSCTL_VIN2 1002
#define GL518_SYSCTL_VIN3 1003
#define GL518_SYSCTL_FAN1 1101     /* RPM */
#define GL518_SYSCTL_FAN2 1102
#define GL518_SYSCTL_TEMP 1200     /* Degrees Celcius * 10 */
#define GL518_SYSCTL_FAN_DIV 2000  /* 1, 2, 4 or 8 */
#define GL518_SYSCTL_ALARMS 2001   /* bitvector */
#define GL518_SYSCTL_BEEP 2002     /* bitvector */
#define GL518_SYSCTL_FAN1OFF 2003
#define GL518_SYSCTL_ITERATE 2004

#define GL518_ALARM_VDD 0x01
#define GL518_ALARM_VIN1 0x02
#define GL518_ALARM_VIN2 0x04
#define GL518_ALARM_VIN3 0x08
#define GL518_ALARM_TEMP 0x10
#define GL518_ALARM_FAN1 0x20
#define GL518_ALARM_FAN2 0x40

#define GL520_SYSCTL_VDD  1000     /* Volts * 100 */
#define GL520_SYSCTL_VIN1 1001
#define GL520_SYSCTL_VIN2 1002
#define GL520_SYSCTL_VIN3 1003
#define GL520_SYSCTL_VIN4 1004
#define GL520_SYSCTL_FAN1 1101     /* RPM */
#define GL520_SYSCTL_FAN2 1102
#define GL520_SYSCTL_TEMP1 1200     /* Degrees Celcius * 10 */
#define GL520_SYSCTL_TEMP2 1201     /* Degrees Celcius * 10 */
#define GL520_SYSCTL_VID 1300    
#define GL520_SYSCTL_FAN_DIV 2000  /* 1, 2, 4 or 8 */
#define GL520_SYSCTL_ALARMS 2001   /* bitvector */
#define GL520_SYSCTL_BEEP 2002     /* bitvector */
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

#define MAXI_SYSCTL_FAN1   1101    /* Rotations/min */
#define MAXI_SYSCTL_FAN2   1102    /* Rotations/min */
#define MAXI_SYSCTL_FAN3   1103    /* Rotations/min */
#define MAXI_SYSCTL_TEMP1  1201    /* Degrees Celcius */
#define MAXI_SYSCTL_TEMP2  1202    /* Degrees Celcius */
#define MAXI_SYSCTL_TEMP3  1203    /* Degrees Celcius */
#define MAXI_SYSCTL_TEMP4  1204    /* Degrees Celcius */
#define MAXI_SYSCTL_TEMP5  1205    /* Degrees Celcius */
#define MAXI_SYSCTL_PLL    1301    /* MHz */
#define MAXI_SYSCTL_VID1   1401    /* Volts / 6.337 */
#define MAXI_SYSCTL_VID2   1402    /* Volts */
#define MAXI_SYSCTL_VID3   1403    /* Volts */
#define MAXI_SYSCTL_VID4   1404    /* Volts */
#define MAXI_SYSCTL_ALARMS 2001    /* Bitvector (see below) */

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

#define SIS5595_SYSCTL_IN0 1000  /* Volts * 100 */
#define SIS5595_SYSCTL_IN1 1001
#define SIS5595_SYSCTL_IN2 1002
#define SIS5595_SYSCTL_IN3 1003
#define SIS5595_SYSCTL_FAN1 1101 /* Rotations/min */
#define SIS5595_SYSCTL_FAN2 1102
#define SIS5595_SYSCTL_TEMP 1200 /* Degrees Celcius * 10 */
#define SIS5595_SYSCTL_FAN_DIV 2000 /* 1, 2, 4 or 8 */
#define SIS5595_SYSCTL_ALARMS 2001 /* bitvector */

#define SIS5595_ALARM_IN0 0x01
#define SIS5595_ALARM_IN1 0x02
#define SIS5595_ALARM_IN2 0x04
#define SIS5595_ALARM_IN3 0x08
#define SIS5595_ALARM_TEMP 0x10
#define SIS5595_ALARM_BTI 0x20
#define SIS5595_ALARM_FAN1 0x40
#define SIS5595_ALARM_FAN2 0x80

#define ICSPLL_SYSCTL1 1000

#define BT869_SYSCTL_STATUS 1000
#define BT869_SYSCTL_NTSC   1001
#define BT869_SYSCTL_HALF   1002
#define BT869_SYSCTL_RES    1003
#define BT869_SYSCTL_COLORBARS    1004
#define BT869_SYSCTL_DEPTH  1005

#define MATORB_SYSCTL_DISP 1000

#define THMC50_SYSCTL_TEMP 1200 /* Degrees Celcius */
#define THMC50_SYSCTL_REMOTE_TEMP 1201 /* Degrees Celcius */
#define THMC50_SYSCTL_INTER 1202
#define THMC50_SYSCTL_INTER_MASK 1203
#define THMC50_SYSCTL_DIE_CODE 1204
#define THMC50_SYSCTL_ANALOG_OUT 1205


#endif /* def SENSORS_SENSORS_H */
