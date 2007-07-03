/*
    proc.c - Part of libsensors, a Linux library for reading sensor data.
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

#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <dirent.h>

#include "data.h"
#include "error.h"
#include "access.h"
#include "proc.h"

/* This reads a feature from a sysfs file.
   Sysfs uses a one-value-per file system...
*/
int sensors_read_proc(sensors_chip_name name, int feature, double *value)
{
	const sensors_chip_feature *the_feature;
	int mag;
	char n[NAME_MAX];
	FILE *f;
	int dummy;
	char check;
	const char *suffix = "";

	if (!(the_feature = sensors_lookup_feature_nr(&name, feature)))
		return -SENSORS_ERR_NO_ENTRY;

	/* REVISIT: this is a ugly hack */
	if (sscanf(the_feature->data.name, "in%d%c", &dummy, &check) == 1
	 || sscanf(the_feature->data.name, "fan%d%c", &dummy, &check) == 1
	 || sscanf(the_feature->data.name, "temp%d%c", &dummy, &check) == 1)
		suffix = "_input";

	snprintf(n, NAME_MAX, "%s/%s%s", name.busname, the_feature->data.name,
		 suffix);
	if ((f = fopen(n, "r"))) {
		int res = fscanf(f, "%lf", value);
		fclose(f);
		if (res != 1)
			return -SENSORS_ERR_PROC;
		for (mag = the_feature->scaling; mag > 0; mag --)
			*value /= 10.0;
	} else
		return -SENSORS_ERR_PROC;

	return 0;
}
  
int sensors_write_proc(sensors_chip_name name, int feature, double value)
{
	const sensors_chip_feature *the_feature;
	int mag;
	char n[NAME_MAX];
	FILE *f;
	int dummy;
	char check;
	const char *suffix = "";
 
	if (!(the_feature = sensors_lookup_feature_nr(&name, feature)))
		return -SENSORS_ERR_NO_ENTRY;

	/* REVISIT: this is a ugly hack */
	if (sscanf(the_feature->data.name, "in%d%c", &dummy, &check) == 1
	 || sscanf(the_feature->data.name, "fan%d%c", &dummy, &check) == 1
	 || sscanf(the_feature->data.name, "temp%d%c", &dummy, &check) == 1)
		suffix = "_input";

	snprintf(n, NAME_MAX, "%s/%s%s", name.busname, the_feature->data.name,
		 suffix);
	if ((f = fopen(n, "w"))) {
		for (mag = the_feature->scaling; mag > 0; mag --)
			value *= 10.0;
		fprintf(f, "%d", (int) value);
		fclose(f);
	} else
		return -SENSORS_ERR_PROC;

	return 0;
}
