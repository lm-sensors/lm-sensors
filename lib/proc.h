/*
    proc.h - Part of libsensors, a Linux library for reading sensor data.
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

#ifndef SENSORS_LIB_PROC_H
#define SENSORS_LIB_PROC_H

/* Read /proc/sys/dev/sensors/chips */
extern int sensors_read_proc_chips(void);

/* Read /proc/bus/i2c */
extern int sensors_read_proc_bus(void);

/* Read a value out of a /proc file */
extern int sensors_read_proc(sensors_chip_name name, int feature, 
                             double *value);

/* Write a value to a /proc file */
extern int sensors_write_proc(sensors_chip_name name, int feature,
                              double value);

#endif
