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

/* Publicly accessible library functions */

#define SENSORS_CHIP_NAME_PREFIX_ANY NULL
#define SENSORS_CHIP_NAME_BUS_ISA -1
#define SENSORS_CHIP_NAME_BUS_ANY -2
#define SENSORS_CHIP_NAME_BUS_ANY_I2C -3
#define SENSORS_CHIP_NAME_BUS_DUMMY -4
#define SENSORS_CHIP_NAME_ADDR_ANY -1

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* A chip name is encoded is in this structure */
typedef struct sensors_chip_name {
  char *prefix;
  int bus;
  int addr;
  char *busname;	/* if dummy */
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
extern int sensors_match_chip(sensors_chip_name chip1, 
                              sensors_chip_name chip2);

/* Check whether the chip name is an 'absolute' name, which can only match
   one chip, or whether it has wildcards. Returns 0 if it is absolute, 1
   if there are wildcards. */
extern int sensors_chip_name_has_wildcards(sensors_chip_name chip);

/* These functions return the adapter and algorithm names of a bus number,
   as used within the sensors_chip_name structure. If it could not be found, 
   it returns NULL */
extern const char *sensors_get_adapter_name(int bus_nr);
extern const char *sensors_get_algorithm_name(int bus_nr);

/* Look up the label which belongs to this chip. Note that chip should not
   contain wildcard values! *result is newly allocated (free it yourself).
   This function will return 0 on success, and <0 on failure.  This
   function takes logical mappings into account. */
extern int sensors_get_label(sensors_chip_name name, int feature, 
                             char **result);

/* Looks up whether a feature should be ignored. Returns <0 on failure,
   0 if it should be ignored, 1 if it is valid. This function takes
   logical mappings into account. */
extern int sensors_get_ignored(sensors_chip_name name, int fature);

/* Read the value of a feature of a certain chip. Note that chip should not
   contain wildcard values! This function will return 0 on success, and <0
   on failure.  */
extern int sensors_get_feature(sensors_chip_name name, int feature,
                               double *result);

/* Set the value of a feature of a certain chip. Note that chip should not
   contain wildcard values! This function will return 0 on success, and <0
   on failure. */
extern int sensors_set_feature(sensors_chip_name name, int feature,
                               double value);

/* Execute all set statements for this particular chip. The chip may contain
   wildcards!  This function will return 0 on success, and <0 on failure. */
extern int sensors_do_chip_sets(sensors_chip_name name);

/* Execute all set statements for all detected chips. This is the same as
   calling sensors_do_chip_sets with an all wildcards chip name */
extern int sensors_do_all_sets(void);

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

/* This structure is used when you want to get all features of a specific
   chip. */
typedef struct sensors_feature_data {
  int number;
  const char *name;
  int mapping;
  int unused;
  int mode;
} sensors_feature_data;

/* This returns all features of a specific chip. They are returned in 
   bunches: everything with the same mapping is returned just after each
   other, with the master feature in front (that feature does not map to
   itself, but has SENSORS_NO_MAPPING as mapping field). nr1 and nr2 are
   two internally used variables. Set both to zero to start again at the
   begin of the list. If no more features are found NULL is returned.
   Do not try to change the returned structure; you will corrupt internal
   data structures. */
extern const sensors_feature_data *sensors_get_all_features 
             (sensors_chip_name name, int *nr1,int *nr2);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* def LIB_SENSORS_ERROR_H */
