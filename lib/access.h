/*
    access.h - Part of libsensors, a Linux library for reading sensor data.
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

#ifndef LIB_SENSORS_ACCESS_H
#define LIB_SENSORS_ACCESS_H

#include "sensors.h"
#include "data.h"

/* Returns, one by one, a pointer to all sensor_chip structs of the
   config file which match with the given chip name. Last should be
   the value returned by the last call, or NULL if this is the first
   call. Returns NULL if no more matches are found. Do not modify
   the struct the return value points to!
   Note that this visits the list of chips from last to first. Usually,
   you want the match that was latest in the config file. */
extern sensors_chip *sensors_for_all_config_chips(sensors_chip_name chip_name,
                                                  const sensors_chip *last);

/* Look up a resource in the intern chip list, and return a pointer to it.
   Do not modify the struct the return value points to! Returns NULL if
   not found. */
extern const sensors_chip_feature *sensors_lookup_feature_nr(const char *prefix,
                                                             int feature);

/* Look up a resource in the intern chip list, and return a pointer to it.
   Do not modify the struct the return value points to! Returns NULL if
   not found.*/
extern const sensors_chip_feature *sensors_lookup_feature_name
                                  (const char *prefix, const char *feature);

/* Substitute configuration bus numbers with real-world /proc bus numbers
   in the chips lists */
extern int sensors_substitute_busses(void);


/* Parse an i2c bus name into its components. Returns 0 on succes, a value from
   error.h on failure. */
extern int sensors_parse_i2cbus_name(const char *name, int *res);

/* Evaluate an expression */
extern int sensors_eval_expr(sensors_chip_name chipname, 
                             const sensors_expr *expr,
                             double val, double *result);


#endif /* def LIB_SENSORS_ACCESS_H */
