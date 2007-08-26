/*
    chips.h - Part of sensors, a user-space program for hardware monitoring
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

#ifndef PROG_SENSORS_CHIPS_H
#define PROG_SENSORS_CHIPS_H

#include "lib/sensors.h"

extern void print_chip_raw(const sensors_chip_name *name);

/* some functions used by chips_generic.c */
#define HYST 0
#define MINMAX 1
#define MAXONLY 2
#define CRIT 3
#define SINGLE 4
#define HYSTONLY 5
void print_temp_info(float n_cur, float n_over, float n_hyst,
		     int minmax, int curprec, int limitprec);

void print_vid_info(const sensors_chip_name *name, int f_vid, int label_size);

void print_label(const char *label, int space);

#endif /* def PROG_SENSORS_CHIPS_H */
