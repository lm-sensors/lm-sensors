/*
    sensors.h - Part of libsensors, a Linux library for reading sensor data.
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

#ifndef LIB_SENSORS_SENSORS_H
#define LIB_SENSORS_SENSORS_H

#include <stdio.h>
#include <limits.h>

/* Publicly accessible library functions */

#define SENSORS_CHIP_NAME_PREFIX_ANY NULL
#define SENSORS_CHIP_NAME_BUS_ISA -1
#define SENSORS_CHIP_NAME_BUS_ANY -2
#define SENSORS_CHIP_NAME_BUS_ANY_I2C -3
#define SENSORS_CHIP_NAME_BUS_PCI -5
#define SENSORS_CHIP_NAME_ADDR_ANY -1

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern const char *libsensors_version;

/* A chip name is encoded is in this structure */
typedef struct sensors_chip_name {
  char *prefix;
  int bus;
  int addr;
  char *busname;
} sensors_chip_name;

/* (Re)load the configuration file and the detected chips list. If this 
    returns a value unequal to zero, you are in trouble; you can not
    assume anything will be initialized properly. */
extern int sensors_init(FILE *input);

/* Clean-up function: You can't access anything after
   this, until the next sensors_init() call! */
extern void sensors_cleanup(void);

/* Parse a chip name to the internal representation. Return 0 on succes, <0
   on error. */
extern int sensors_parse_chip_name(const char *orig_name,
                                   sensors_chip_name *res);

/* Compare two chips name descriptions, to see whether they could match.
   Return 0 if it does not match, return 1 if it does match. */
extern int sensors_match_chip(const sensors_chip_name *chip1,
                              const sensors_chip_name *chip2);

/* This function returns the adapter name of a bus number,
   as used within the sensors_chip_name structure. If it could not be found, 
   it returns NULL */
extern const char *sensors_get_adapter_name(int bus_nr);

/* Look up the label which belongs to this chip. Note that chip should not
   contain wildcard values! *result is newly allocated (free it yourself).
   This function will return 0 on success, and <0 on failure.  This
   function takes logical mappings into account. */
extern int sensors_get_label(const sensors_chip_name *name, int feature,
                             char **result);

/* Read the value of a feature of a certain chip. Note that chip should not
   contain wildcard values! This function will return 0 on success, and <0
   on failure.  */
extern int sensors_get_feature(const sensors_chip_name *name, int feature,
                               double *result);

/* Set the value of a feature of a certain chip. Note that chip should not
   contain wildcard values! This function will return 0 on success, and <0
   on failure. */
extern int sensors_set_feature(const sensors_chip_name *name, int feature,
                               double value);

/* Execute all set statements for this particular chip. The chip may contain
   wildcards!  This function will return 0 on success, and <0 on failure. */
extern int sensors_do_chip_sets(const sensors_chip_name *name);

/* This function returns all detected chips, one by one. To start at the
   beginning of the list, use 0 for nr; NULL is returned if we are
   at the end of the list. Do not try to change these chip names, as 
   they point to internal structures! Do not use nr for anything else. */
extern const sensors_chip_name *sensors_get_detected_chips(int *nr);


/* These defines are used in the mode field of sensors_feature_data */
#define SENSORS_MODE_NO_RW 0
#define SENSORS_MODE_R 1
#define SENSORS_MODE_W 2
#define SENSORS_MODE_RW 3

/* This define is used in the mapping field of sensors_feature_data if no
   mapping is available */
#define SENSORS_NO_MAPPING -1

/* This enum contains some "magic" used by sensors_read_dynamic_chip() from
   lib/sysfs.c. All the sensor types (in, fan, temp, vid) are a multiple of
   0x100 apart, and sensor features which should not have a compute_mapping to
   the _input feature start at 0x?10. */
typedef enum sensors_feature_type {
  SENSORS_FEATURE_IN = 0x000,
  SENSORS_FEATURE_IN_MIN,
  SENSORS_FEATURE_IN_MAX,
  SENSORS_FEATURE_IN_ALARM = 0x010,
  SENSORS_FEATURE_IN_MIN_ALARM,
  SENSORS_FEATURE_IN_MAX_ALARM,
  
  SENSORS_FEATURE_FAN = 0x100,
  SENSORS_FEATURE_FAN_MIN,
  SENSORS_FEATURE_FAN_ALARM = 0x110,
  SENSORS_FEATURE_FAN_FAULT,
  SENSORS_FEATURE_FAN_DIV,
  
  SENSORS_FEATURE_TEMP = 0x200,
  SENSORS_FEATURE_TEMP_MAX,
  SENSORS_FEATURE_TEMP_MAX_HYST,
  SENSORS_FEATURE_TEMP_MIN,
  SENSORS_FEATURE_TEMP_CRIT,
  SENSORS_FEATURE_TEMP_CRIT_HYST,
  SENSORS_FEATURE_TEMP_ALARM = 0x210,
  SENSORS_FEATURE_TEMP_MAX_ALARM,
  SENSORS_FEATURE_TEMP_MIN_ALARM,
  SENSORS_FEATURE_TEMP_CRIT_ALARM,
  SENSORS_FEATURE_TEMP_FAULT,
  SENSORS_FEATURE_TEMP_SENS,
  
  SENSORS_FEATURE_VID = 0x300,
  
  SENSORS_FEATURE_UNKNOWN = INT_MAX,
  
  /* special the largest number of subfeatures used, iow the 
     highest ## from all the 0x?## above + 1*/
  SENSORS_FEATURE_MAX_SUB_FEATURES = 22
} sensors_feature_type;

/* This structure is used when you want to get all features of a specific
   chip. */
typedef struct sensors_feature_data {
  int number;
  char *name;
  sensors_feature_type type;
  int mapping;
  int compute_mapping;
  int mode;
} sensors_feature_data;

/* This returns all features of a specific chip. They are returned in
   bunches: everything with the same mapping is returned just after each
   other, with the master feature in front (that feature does not map to
   itself, but has SENSORS_NO_MAPPING as mapping field). nr is
   an internally used variable. Set it to zero to start again at the
   begin of the list. If no more features are found NULL is returned.
   Do not try to change the returned structure; you will corrupt internal
   data structures. */
extern const sensors_feature_data *sensors_get_all_features
             (const sensors_chip_name *name, int *nr);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* def LIB_SENSORS_ERROR_H */
