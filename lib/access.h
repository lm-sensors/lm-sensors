/*
    access.h - Part of libsensors, a Linux library for reading sensor data.
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

#ifndef LIB_SENSORS_ACCESS_H
#define LIB_SENSORS_ACCESS_H

typedef struct sensors_chip_feature {
  int number;
  const char *name;
  int logical_mapping;
  int compute_mapping;
  int mode;
  int sysctl;
  int offset;
  int scaling;
} sensors_chip_feature;

typedef struct sensors_chip_features {
  const char *prefix;
  struct sensors_chip_feature *feature;
} sensors_chip_features;

#define SENSORS_NO_RW 0
#define SENSORS_R 1
#define SENSORS_W 2
#define SENSORS_RW 3

#define SENSORS_NO_MAPPING -1

extern sensors_chip_features sensors_chip_features_list[];

#endif /* def LIB_SENSORS_ACCESS_H */
