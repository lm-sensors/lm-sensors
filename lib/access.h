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

/* Look up a resource in the intern chip list, and return a pointer to it.
   Do not modify the struct the return value points to! Returns NULL if
   not found. */
const sensors_subfeature *sensors_lookup_feature_nr(const sensors_chip_name *chip,
						      int feature);

/* Check whether the chip name is an 'absolute' name, which can only match
   one chip, or whether it has wildcards. Returns 0 if it is absolute, 1
   if there are wildcards. */
int sensors_chip_name_has_wildcards(const sensors_chip_name *chip);

#endif /* def LIB_SENSORS_ACCESS_H */
