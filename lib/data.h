/*
    data.h - Part of libsensors, a Linux library for reading sensor data.
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

#ifndef LIB_SENSORS_DATA_H
#define LIB_SENSORS_DATA_H

#include "sensors.h"

/* This header file contains all kinds of data structures which are used
   for the representation of the config file data and the /proc/.../chips
   data. */

typedef enum sensors_operation { 
  sensors_add, sensors_sub, sensors_multiply, sensors_divide, 
  sensors_negate } sensors_operation;

typedef enum sensors_expr_kind {
  sensors_kind_val, sensors_kind_var, sensors_kind_sub } sensors_expr_kind;

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
    

typedef struct sensors_label {
  char *name;
  char *value;
} sensors_label;

typedef struct sensors_set {
  char *name;
  sensors_expr *value;
} sensors_set;

typedef struct sensors_compute {
  char *name;
  sensors_expr *from_proc;
  sensors_expr *to_proc;
} sensors_compute;

typedef struct sensors_chip_name_list {
  sensors_chip_name *fits;
  int fits_count;
  int fits_max;
} sensors_chip_name_list;

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
} sensors_chip;

typedef enum sensors_bus_type {sensors_i2c, sensors_isa, 
                               sensors_smbus } sensors_bus_type;

typedef struct sensors_bus {
  int number;
  char *adapter;
  char *algorithm;
} sensors_bus;

typedef struct sensors_proc_chips_entry {
  int sysctl;
  sensors_chip_name name;
} sensors_proc_chips_entry;

extern sensors_chip *sensors_config_chips;
extern int sensors_config_chips_count;
extern int sensors_config_chips_max;

extern sensors_bus *sensors_config_busses;
extern int sensors_config_busses_count;
extern int sensors_config_busses_max;

extern sensors_proc_chips_entry *sensors_proc_chips;
extern int sensors_proc_chips_count;
extern int sensors_proc_chips_max;

/* Parse an i2c bus name into its components. Returns 0 on succes, a value from
   error.h on failure. */
extern int sensors_parse_i2cbus_name(const char *name, int *res);

extern int sensors_eval_expr(sensors_expr *expr, double val, double *result);


#endif /* def LIB_SENSORS_DATA_H */
