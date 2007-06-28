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

static int getsysname(const sensors_chip_feature *feature, char *sysname,
	int *sysmag, char *altsysname);

/* This reads a feature from a sysfs file.
   Sysfs uses a one-value-per file system...
*/
int sensors_read_proc(sensors_chip_name name, int feature, double *value)
{
	const sensors_chip_feature *the_feature;
	int mag;
	char n[NAME_MAX], altn[NAME_MAX];
	FILE *f;

	if (!(the_feature = sensors_lookup_feature_nr(&name, feature)))
		return -SENSORS_ERR_NO_ENTRY;

	strcpy(n, name.busname);
	strcat(n, "/");
	strcpy(altn, n);
	/* use rindex to append sysname to n */
	getsysname(the_feature, rindex(n, '\0'), &mag, rindex(altn, '\0'));
	if ((f = fopen(n, "r")) != NULL
	 || (f = fopen(altn, "r")) != NULL) {
		int res = fscanf(f, "%lf", value);
		fclose(f);
		if (res != 1)
			return -SENSORS_ERR_PROC;
		for (; mag > 0; mag --)
			*value /= 10.0;
	} else
		return -SENSORS_ERR_PROC;

	return 0;
}
  
int sensors_write_proc(sensors_chip_name name, int feature, double value)
{
	const sensors_chip_feature *the_feature;
	int mag;
	char n[NAME_MAX], altn[NAME_MAX];
	FILE *f;
 
	if (!(the_feature = sensors_lookup_feature_nr(&name, feature)))
		return -SENSORS_ERR_NO_ENTRY;

	strcpy(n, name.busname);
	strcat(n, "/");
	strcpy(altn, n);
	/* use rindex to append sysname to n */
	getsysname(the_feature, rindex(n, '\0'), &mag, rindex(altn, '\0'));
	if ((f = fopen(n, "w")) != NULL
	 || (f = fopen(altn, "w")) != NULL) {
		for (; mag > 0; mag --)
			value *= 10.0;
		fprintf(f, "%d", (int) value);
		fclose(f);
	} else
		return -SENSORS_ERR_PROC;

	return 0;
}

#define CURRMAG 3
#define FANMAG 0
#define INMAG 3
#define TEMPMAG 3

/* The following are used in getsysname() below */
struct match {
	const char * name, * sysname;
	const int sysmag;
	const char * altsysname;
};

static const struct match matches[] = {
	{ "beeps", "beep_mask", 0 },
	{ "pwm", "pwm1", 0, "fan1_pwm" },
	{ "vid", "cpu0_vid", INMAG, "in0_ref" },
	{ "remote_temp", "temp2_input", TEMPMAG },
	{ "remote_temp_hyst", "temp2_max_hyst", TEMPMAG },
	{ "remote_temp_low", "temp2_min", TEMPMAG },
	{ "remote_temp_over", "temp2_max", TEMPMAG },
	{ "temp", "temp1_input", TEMPMAG },
	{ "temp_hyst", "temp1_max_hyst", TEMPMAG },
	{ "temp_low", "temp1_min", TEMPMAG },
	{ "temp_over", "temp1_max", TEMPMAG },
	{ "temp_high", "temp1_max", TEMPMAG },
	{ "temp_crit", "temp1_crit", TEMPMAG },
	{ NULL, NULL }
};

/*
	Returns the sysfs name and magnitude for a given feature.
	First looks for a sysfs name and magnitude in the feature structure.
	These should be added in chips.c for all non-standard feature names.
	If that fails, converts common /proc feature names
	to their sysfs equivalent, and uses common sysfs magnitude.
	Common magnitudes are #defined above.
	Common conversions are as follows:
		fan%d_min -> fan%d_min (for magnitude)
		fan%d_state -> fan%d_status
		fan%d -> fan_input%d
		pwm%d -> fan%d_pwm
		pwm%d_enable -> fan%d_pwm_enable
		in%d_max -> in%d_max (for magnitude)
		in%d_min -> in%d_min (for magnitude)
		in%d -> in%d_input
		vin%d_max -> in%d_max
		vin%d_min -> in%d_min
		vin%d -> in_input%d
		temp%d_over -> temp%d_max
		temp%d_hyst -> temp%d_max_hyst
		temp%d_max -> temp%d_max (for magnitude)
		temp%d_high -> temp%d_max
		temp%d_min -> temp%d_min (for magnitude)
		temp%d_low -> temp%d_min
		temp%d_state -> temp%d_status
		temp%d -> temp%d_input
		sensor%d -> temp%d_type
	AND all conversions listed in the matches[] structure above.

	If that fails, returns old /proc feature name and magnitude.

	References: doc/developers/proc in the lm_sensors package;
	            Documentation/i2c/sysfs_interface in the kernel
*/
static int getsysname(const sensors_chip_feature *feature, char *sysname,
	int *sysmag, char *altsysname)
{
	const char * name = feature->data.name;
	char last;
	char check; /* used to verify end of string */
	int num;
	const struct match *m;

/* default to a non-existent alternate name (should rarely be tried) */
	strcpy(altsysname, "_");

/* use override in feature structure if present */
	if(feature->sysname != NULL) {
		strcpy(sysname, feature->sysname);
		if(feature->sysscaling)
			*sysmag = feature->sysscaling;
		else
			*sysmag = feature->scaling;
		if(feature->altsysname != NULL)
			strcpy(altsysname, feature->altsysname);
		return 0;
	}

/* check for constant mappings */
	for(m = matches; m->name != NULL; m++) {
		if(!strcmp(m->name, name)) {
			strcpy(sysname, m->sysname);
			if (m->altsysname != NULL)
				strcpy(altsysname, m->altsysname);
			*sysmag = m->sysmag;
			return 0;
		}
	}

/* convert common /proc names to common sysfs names */
	if(sscanf(name, "fan%d_mi%c%c", &num, &last, &check) == 2 && last == 'n') {
		strcpy(sysname, name);
		*sysmag = FANMAG;
		return 0;
	}
	if(sscanf(name, "fan%d_stat%c%c", &num, &last, &check) == 2 && last == 'e') {
		sprintf(sysname, "fan%d_status", num);
		*sysmag = 0;
		return 0;
	}
	if(sscanf(name, "fan%d%c", &num, &check) == 1) {
		sprintf(sysname, "fan%d_input", num);
		*sysmag = FANMAG;
		return 0;
	}
	if(sscanf(name, "pwm%d%c", &num, &check) == 1) {
		strcpy(sysname, name);
		*sysmag = 0;
		sprintf(altsysname, "fan%d_pwm", num);
		return 0;
	}
	if(sscanf(name, "pwm%d_enabl%c%c", &num, &last, &check) == 2 && last == 'e') {
		strcpy(sysname, name);
		*sysmag = 0;
		sprintf(altsysname, "fan%d_pwm_enable", num);
		return 0;
	}

	if((sscanf(name, "in%d_mi%c%c", &num, &last, &check) == 2 && last == 'n')
	|| (sscanf(name, "in%d_ma%c%c", &num, &last, &check) == 2 && last == 'x')) {
		strcpy(sysname, name);
		*sysmag = INMAG;
		return 0;
	}
	if((sscanf(name, "in%d%c", &num, &check) == 1)
	|| (sscanf(name, "vin%d%c", &num, &check) == 1)) {
		sprintf(sysname, "in%d_input", num);
		*sysmag = INMAG;
		return 0;
	}
	if(sscanf(name, "vin%d_mi%c%c", &num, &last, &check) == 2 && last == 'n') {
		sprintf(sysname, "in%d_min", num);
		*sysmag = INMAG;
		return 0;
	}
	if(sscanf(name, "vin%d_ma%c%c", &num, &last, &check) == 2 && last == 'x') {
		sprintf(sysname, "in%d_max", num);
		*sysmag = INMAG;
		return 0;
	}

	if(sscanf(name, "temp%d_hys%c%c", &num, &last, &check) == 2 && last == 't') {
		sprintf(sysname, "temp%d_max_hyst", num);
		*sysmag = TEMPMAG;
		return 0;
	}
	if((sscanf(name, "temp%d_ove%c%c", &num, &last, &check) == 2 && last == 'r')
	|| (sscanf(name, "temp%d_ma%c%c", &num, &last, &check) == 2 && last == 'x')
	|| (sscanf(name, "temp%d_hig%c%c", &num, &last, &check) == 2 && last == 'h')) {
		sprintf(sysname, "temp%d_max", num);
		*sysmag = TEMPMAG;
		return 0;
	}
	if((sscanf(name, "temp%d_mi%c%c", &num, &last, &check) == 2 && last == 'n')
	|| (sscanf(name, "temp%d_lo%c%c", &num, &last, &check) == 2 && last == 'w')) {
		sprintf(sysname, "temp%d_min", num);
		*sysmag = TEMPMAG;
		return 0;
	}
	if(sscanf(name, "temp%d_cri%c%c", &num, &last, &check) == 2 && last == 't') {
		sprintf(sysname, "temp%d_crit", num);
		*sysmag = TEMPMAG;
		return 0;
	}
	if(sscanf(name, "temp%d_stat%c%c", &num, &last, &check) == 2 && last == 'e') {
		sprintf(sysname, "temp%d_status", num);
		*sysmag = 0;
		return 0;
	}
	if(sscanf(name, "temp%d%c", &num, &check) == 1) {
		sprintf(sysname, "temp%d_input", num);
		*sysmag = TEMPMAG;
		return 0;
	}
	if(sscanf(name, "sensor%d%c", &num, &check) == 1) {
		sprintf(sysname, "temp%d_type", num);
		*sysmag = 0;
		return 0;
	}

/* bmcsensors only, not yet in kernel */
/*
	if((sscanf(name, "curr%d_mi%c%c", &num, &last, &check) == 2 && last == 'n')
	|| (sscanf(name, "curr%d_ma%c%c", &num, &last, &check) == 2 && last == 'x')) {
		strcpy(sysname, name);
		*sysmag = CURRMAG;
		return 0;
	}
	if(sscanf(name, "curr%d%c", &num, &check) == 1) {
		sprintf(sysname, "curr%d_input", num);
		*sysmag = CURRMAG;
		return 0;
	}
*/

/* give up, use old name (probably won't work though...) */
/* known to be the same:
	"alarms", "beep_enable", "vrm", "fan%d_div"
*/
	strcpy(sysname, name);
	*sysmag = feature->scaling;
	return 0;
}
