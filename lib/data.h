/*
    data.h - Part of libsensors, a Linux library for reading sensor data.
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

#ifndef LIB_SENSORS_DATA_H
#define LIB_SENSORS_DATA_H

#include "sensors.h"

/* This header file contains all kinds of data structures which are used
   for the representation of the config file data and the /proc/...
   data. */

/* Kinds of expression operators recognized */
typedef enum sensors_operation { 
  sensors_add, sensors_sub, sensors_multiply, sensors_divide, 
  sensors_negate, sensors_exp, sensors_log } sensors_operation;

/* An expression can have several forms */
typedef enum sensors_expr_kind {
  sensors_kind_val, sensors_kind_source, sensors_kind_var, 
  sensors_kind_sub } sensors_expr_kind;

/* An expression. It is either a floating point value, a variable name,
   an operation on subexpressions, or the special value 'sub' } */
struct sensors_expr;

typedef struct sensors_subexpr {
  sensors_operation op;
  struct sensors_expr *sub1;
  struct sensors_expr *sub2;
} sensors_subexpr;

typedef struct sensors_expr {
  sensors_expr_kind kind;
  union {
    double val;
    char *var;
    sensors_subexpr subexpr;
  } data;
} sensors_expr;

/* Config file label declaration: a feature name, combined with the label
   value */
typedef struct sensors_label {
  char *name;
  char *value;
  int lineno;
} sensors_label;

/* Config file set declaration: a feature name, combined with an expression */
typedef struct sensors_set {
  char *name;
  sensors_expr *value;
  int lineno;
} sensors_set;

/* Config file compute declaration: a feature name, combined with two 
   expressions */
typedef struct sensors_compute {
  char *name;
  sensors_expr *from_proc;
  sensors_expr *to_proc;
  int lineno;
} sensors_compute;

/* Config file ignore declaration: a feature name */
typedef struct sensors_ignore {
  char *name;
  int lineno;
} sensors_ignore;

/* A list of chip names, used to represent a config file chips declaration */
typedef struct sensors_chip_name_list {
  sensors_chip_name *fits;
  int fits_count;
  int fits_max;
} sensors_chip_name_list;

/* A config file chip block */
typedef struct sensors_chip {
  sensors_chip_name_list chips;
  sensors_label *labels;
  int labels_count;
  int labels_max;
  sensors_set *sets;
  int sets_count;
  int sets_max;
  sensors_compute *computes;
  int computes_count;
  int computes_max;
  sensors_ignore *ignores;
  int ignores_count;
  int ignores_max;
  int lineno;
} sensors_chip;

/* Config file bus declaration: the i2c bus number, combined with adapter
   and algorithm names */
typedef struct sensors_bus {
  int number;
  char *adapter;
  char *algorithm;
  int lineno;
} sensors_bus;

/* /proc/sys/dev/sensors/chips line representation */
typedef struct sensors_proc_chips_entry {
  int sysctl;
  sensors_chip_name name;
} sensors_proc_chips_entry;

/* Internal data about a single chip feature.
   name is the string name used to refer to this feature (both in config
     files and through user functions);
   number is the internal feature number, used in many functions to refer
     to this feature
   logical_mapping is either SENSORS_NO_MAPPING if this is feature is the
     main element of category; or it is the number of a feature with which
     this feature is logically grouped (a group could be fan, fan_max and
     fan_div)
   compute_mapping is like logical_mapping, only it refers to another
     feature whose compute line will be inherited (a group could be fan and
     fan_max, but not fan_div)
   mode is SENSORS_MODE_NO_RW, SENSORS_MODE_R, SENSORS_MODE_W or
     SENSORS_MODE_RW, for unaccessible, readable, writable, and both readable
     and writable.
   sysctl is the SYSCTL id of the file the value can be found in.
   offset is the (byte) offset of the place this feature can be found.
   scaling is the number of decimal points to scale by.
     This scaling is performed on the raw sysctl value, NOT the value
     seen in /proc. Therefore the scaling value must be the same as
     the value returned in nrels_mag by the SENSORS_PROC_REAL_INFO
     operation in the chip drivers.
     Divide the read value by 10**scaling to get the real value.
     Scaling can be positive or negative but negative values aren't
     very useful because the driver can scale that direction itself. */
typedef struct sensors_chip_feature {
  int number;
  const char *name;
  int logical_mapping;
  int compute_mapping;
  int mode;
  int sysctl;
  int offset;
  int scaling;
  const char *sysname;
  int sysscaling;
  const char *altsysname;
} sensors_chip_feature;

/* Internal data about all features of a type of chip */
typedef struct sensors_chip_features {
  const char *prefix;
  struct sensors_chip_feature *feature;
} sensors_chip_features;

extern sensors_chip *sensors_config_chips;
extern int sensors_config_chips_count;
extern int sensors_config_chips_max;

extern sensors_bus *sensors_config_busses;
extern int sensors_config_busses_count;
extern int sensors_config_busses_max;

extern sensors_proc_chips_entry *sensors_proc_chips;
extern int sensors_proc_chips_count;
extern int sensors_proc_chips_max;

extern sensors_bus *sensors_proc_bus;
extern int sensors_proc_bus_count;
extern int sensors_proc_bus_max;

extern sensors_chip_features sensors_chip_features_list[];

#endif /* def LIB_SENSORS_DATA_H */
