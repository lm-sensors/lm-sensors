/*
    chips.c - Part of sensors, a user-space program for hardware monitoring
    Copyright (c) 1998-2003 Frodo Looijaard <frodol@dds.nl>
                            and Mark D. Studebaker <mdsxyz123@yahoo.com>
    Copyright (c) 2003-2006 The lm_sensors team

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
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "main.h"
#include "chips.h"
#include "lib/sensors.h"

static inline float deg_ctof(float cel)
{
	return cel * (9.0F / 5.0F) + 32.0F;
}

#define HYST 0
#define MINMAX 1
#define MAXONLY 2
#define CRIT 3
#define SINGLE 4
#define HYSTONLY 5

/* symbolic constants for minmax defined above */
static void print_temp_info(float n_cur, float n_over, float n_hyst,
			    int minmax)
{
	/* note: deg_ctof() will preserve HUGEVAL */
	if (fahrenheit) {
		n_cur = deg_ctof(n_cur);
		n_over = deg_ctof(n_over);
		n_hyst = deg_ctof(n_hyst);
	}

	if (n_cur != HUGE_VAL)
		printf("%+6.1f%s  ", n_cur, degstr);
	else
		printf("   FAULT  ");

	if (minmax == MINMAX)
		printf("(low  = %+5.1f%s, high = %+5.1f%s)  ",
		       n_hyst, degstr,
		       n_over, degstr);
	else if (minmax == MAXONLY)
		printf("(high = %+5.1f%s)                  ",
		       n_over, degstr);
	else if (minmax == CRIT)
		printf("(high = %+5.1f%s, crit = %+5.1f%s)  ",
		       n_over, degstr,
		       n_hyst, degstr);
	else if (minmax == HYST)
		printf("(high = %+5.1f%s, hyst = %+5.1f%s)  ",
		       n_over, degstr,
		       n_hyst, degstr);
	else if (minmax == HYSTONLY)
		printf("(hyst = %+5.1f%s)                  ",
		       n_over, degstr);
	else if (minmax != SINGLE)
		printf("Unknown temperature mode!");
}

static void print_label(const char *label, int space)
{
	int len = strlen(label)+1;
	printf("%s:%*s", label, space - len, "");
}

static void print_vid_info(const sensors_chip_name *name, int f_vid,
			   int label_size)
{
	char *label;
	double vid;

	if ((label = sensors_get_label(name, f_vid))
	 && !sensors_get_value(name, f_vid, &vid)) {
		print_label(label, label_size);
		printf("%+6.3f V\n", vid);
	}
	free(label);
}

void print_chip_raw(const sensors_chip_name *name)
{
	int a;
	const sensors_feature_data *data;
	char *label;
	double val;

	a = 0;
	while ((data = sensors_get_all_features(name, &a))) {
		if (!(label = sensors_get_label(name, data->number))) {
			printf("ERROR: Can't get feature `%s' data!\n",
			       data->name);
			continue;
		}
		if (data->mode & SENSORS_MODE_R) {
			if (sensors_get_value(name, data->number, &val)) {
				printf("ERROR: Can't get feature `%s' data!\n",
				       data->name);
				continue;
			}
			if (data->mapping != SENSORS_NO_MAPPING)
				printf("  %s: %.2f (%s)\n", label, val,
				       data->name);
			else
				printf("%s: %.2f (%s)\n", label, val,
				       data->name);
		} else
			printf("(%s)\n", label);
		free(label);
	}
}

static int get_feature_value(const sensors_chip_name *name,
			     const sensors_feature_data *feature,
			     double *val)
{
	return sensors_get_value(name, feature->number, val);
}

static void sensors_get_available_features(const sensors_chip_name *name,
					   const sensors_feature_data *feature,
					   int i, short *has_features,
					   double *feature_vals, int size,
					   int first_val)
{
	const sensors_feature_data *iter;

	while ((iter = sensors_get_all_features(name, &i)) &&
	       iter->mapping == feature->number) {
		int indx;

		indx = iter->type - first_val - 1;
		if (indx < 0 || indx >= size) {
			printf("ERROR: Bug in sensors: index out of bound");
			return;
		}

		if (get_feature_value(name, iter, &feature_vals[indx]))
			printf("ERROR: Can't get %s data!\n", iter->name);

		has_features[indx] = 1;
	}
}

static int sensors_get_label_size(const sensors_chip_name *name)
{
	int i;
	const sensors_feature_data *iter;
	char *label;
	unsigned int max_size = 11;	/* 11 as minumum label width */

	i = 0;
	while ((iter = sensors_get_all_features(name, &i))) {
		if (iter->mapping != SENSORS_NO_MAPPING)
			continue;
		if ((label = sensors_get_label(name, iter->number)) &&
		    strlen(label) > max_size)
			max_size = strlen(label);
		free(label);
	}
	return max_size + 1;
}

#define TEMP_FEATURE(x)		has_features[x - SENSORS_FEATURE_TEMP - 1]
#define TEMP_FEATURE_VAL(x)	feature_vals[x - SENSORS_FEATURE_TEMP - 1]
static void print_generic_chip_temp(const sensors_chip_name *name,
				    const sensors_feature_data *feature,
				    int i, int label_size)
{
	double val, max, min;
	char *label;
	int type;
	const int size = SENSORS_FEATURE_TEMP_SENS - SENSORS_FEATURE_TEMP;
	short has_features[SENSORS_FEATURE_TEMP_SENS - SENSORS_FEATURE_TEMP] = { 0, };
	double feature_vals[SENSORS_FEATURE_TEMP_SENS - SENSORS_FEATURE_TEMP] = { 0.0, };

	if (!(label = sensors_get_label(name, feature->number))) {
		printf("ERROR: Can't get temperature label!\n");
		return;
	}

	if (get_feature_value(name, feature, &val)) {
		printf("ERROR: Can't get %s data!\n", label);
		free(label);
		return;
	}

	sensors_get_available_features(name, feature, i, has_features,
				       feature_vals, size,
				       SENSORS_FEATURE_TEMP);

	if (TEMP_FEATURE(SENSORS_FEATURE_TEMP_MAX)) {
		max = TEMP_FEATURE_VAL(SENSORS_FEATURE_TEMP_MAX);

		if (TEMP_FEATURE(SENSORS_FEATURE_TEMP_MIN)) {
			min = TEMP_FEATURE_VAL(SENSORS_FEATURE_TEMP_MIN);
			type = MINMAX;
		} else if (TEMP_FEATURE(SENSORS_FEATURE_TEMP_MAX_HYST)) {
			min = TEMP_FEATURE_VAL(SENSORS_FEATURE_TEMP_MAX_HYST);
			type = HYST;
		} else if (TEMP_FEATURE(SENSORS_FEATURE_TEMP_CRIT)) {
			min = TEMP_FEATURE_VAL(SENSORS_FEATURE_TEMP_CRIT);
			type = CRIT;
		} else {
			min = 0;
			type = MAXONLY;
		}
	} else {
		min = max = 0;
		type = SINGLE;
	}

	if (TEMP_FEATURE(SENSORS_FEATURE_TEMP_FAULT) &&
	    TEMP_FEATURE_VAL(SENSORS_FEATURE_TEMP_FAULT))
		val = HUGE_VAL;

	print_label(label, label_size);
	free(label);

	print_temp_info(val, max, min, type);

	/* ALARM features */
	if ((TEMP_FEATURE(SENSORS_FEATURE_TEMP_ALARM) &&
	     TEMP_FEATURE_VAL(SENSORS_FEATURE_TEMP_ALARM) > 0.5)
	 || (type == MINMAX &&
	     TEMP_FEATURE(SENSORS_FEATURE_TEMP_MIN_ALARM) &&
	     TEMP_FEATURE_VAL(SENSORS_FEATURE_TEMP_MIN_ALARM) > 0.5)
	 || (type == MINMAX &&
	     TEMP_FEATURE(SENSORS_FEATURE_TEMP_MAX_ALARM) &&
	     TEMP_FEATURE_VAL(SENSORS_FEATURE_TEMP_MAX_ALARM) > 0.5)
	 || (type == CRIT &&
	     TEMP_FEATURE(SENSORS_FEATURE_TEMP_CRIT_ALARM) &&
	     TEMP_FEATURE_VAL(SENSORS_FEATURE_TEMP_CRIT_ALARM) > 0.5)) {
		printf("ALARM  ");
	}

	if (type != CRIT && TEMP_FEATURE(SENSORS_FEATURE_TEMP_CRIT)) {
		if (fahrenheit) {
			TEMP_FEATURE_VAL(SENSORS_FEATURE_TEMP_CRIT) = deg_ctof(
				TEMP_FEATURE_VAL(SENSORS_FEATURE_TEMP_CRIT));
			TEMP_FEATURE_VAL(SENSORS_FEATURE_TEMP_CRIT_HYST) = deg_ctof(
				TEMP_FEATURE_VAL(SENSORS_FEATURE_TEMP_CRIT_HYST));
		}

		if (TEMP_FEATURE(SENSORS_FEATURE_TEMP_CRIT_HYST))
			printf("\n%*s(crit = %+5.1f%s, hyst = %+5.1f%s)  ",
			       label_size + 10, "",
			       TEMP_FEATURE_VAL(SENSORS_FEATURE_TEMP_CRIT),
			       degstr,
			       TEMP_FEATURE_VAL(SENSORS_FEATURE_TEMP_CRIT_HYST),
			       degstr);
		else
			printf("\n%*s(crit = %+5.1f%s)  ",
			       label_size + 10, "",
			       TEMP_FEATURE_VAL(SENSORS_FEATURE_TEMP_CRIT),
			       degstr);

		if (TEMP_FEATURE(SENSORS_FEATURE_TEMP_CRIT_ALARM) &&
		    TEMP_FEATURE_VAL(SENSORS_FEATURE_TEMP_CRIT_ALARM)) {
			printf("ALARM  ");
		}
	}

	/* print out temperature sensor info */
	if (TEMP_FEATURE(SENSORS_FEATURE_TEMP_SENS)) {
		int sens = (int)TEMP_FEATURE_VAL(SENSORS_FEATURE_TEMP_SENS);

		/* older kernels / drivers sometimes report a beta value for
		   thermistors */
		if (sens > 1000)
			sens = 4;

		printf("sensor = %s", sens == 0 ? "disabled" :
		       sens == 1 ? "diode" :
		       sens == 2 ? "transistor" :
		       sens == 3 ? "thermal diode" :
		       sens == 4 ? "thermistor" :
		       sens == 5 ? "AMD AMDSI" :
		       sens == 6 ? "Intel PECI" : "unknown");
	}
	printf("\n");
}

#define IN_FEATURE(x)		has_features[x - SENSORS_FEATURE_IN - 1]
#define IN_FEATURE_VAL(x)	feature_vals[x - SENSORS_FEATURE_IN - 1]
static void print_generic_chip_in(const sensors_chip_name *name,
				  const sensors_feature_data *feature,
				  int i, int label_size)
{
	const int size = SENSORS_FEATURE_IN_MAX_ALARM - SENSORS_FEATURE_IN;
	short has_features[SENSORS_FEATURE_IN_MAX_ALARM - SENSORS_FEATURE_IN] = { 0, };
	double feature_vals[SENSORS_FEATURE_IN_MAX_ALARM - SENSORS_FEATURE_IN] = { 0.0, };
	double val, alarm_max, alarm_min;
	char *label;

	if (!(label = sensors_get_label(name, feature->number))) {
		printf("ERROR: Can't get in label!\n");
		return;
	}

	if (get_feature_value(name, feature, &val)) {
		printf("ERROR: Can't get %s data!\n", label);
		free(label);
		return;
	}

	sensors_get_available_features(name, feature, i, has_features,
				       feature_vals, size, SENSORS_FEATURE_IN);

	print_label(label, label_size);
	free(label);
	printf("%+6.2f V", val);

	if (IN_FEATURE(SENSORS_FEATURE_IN_MIN) &&
	    IN_FEATURE(SENSORS_FEATURE_IN_MAX))
		printf("  (min = %+6.2f V, max = %+6.2f V)",
		       IN_FEATURE_VAL(SENSORS_FEATURE_IN_MIN),
		       IN_FEATURE_VAL(SENSORS_FEATURE_IN_MAX));
	else if (IN_FEATURE(SENSORS_FEATURE_IN_MIN))
		printf("  (min = %+6.2f V)",
		       IN_FEATURE_VAL(SENSORS_FEATURE_IN_MIN));
	else if (IN_FEATURE(SENSORS_FEATURE_IN_MAX))
		printf("  (max = %+6.2f V)",
		       IN_FEATURE_VAL(SENSORS_FEATURE_IN_MAX));

	if (IN_FEATURE(SENSORS_FEATURE_IN_MAX_ALARM) ||
	    IN_FEATURE(SENSORS_FEATURE_IN_MIN_ALARM)) {
		alarm_max = IN_FEATURE_VAL(SENSORS_FEATURE_IN_MAX_ALARM);
		alarm_min = IN_FEATURE_VAL(SENSORS_FEATURE_IN_MIN_ALARM);

		if (alarm_min || alarm_max) {
			printf(" ALARM (");

			if (alarm_min)
				printf("MIN");
			if (alarm_max)
				printf("%sMAX", (alarm_min) ? ", " : "");

			printf(")");
		}
	} else if (IN_FEATURE(SENSORS_FEATURE_IN_ALARM)) {
		printf("   %s",
		IN_FEATURE_VAL(SENSORS_FEATURE_IN_ALARM) ? "ALARM" : "");
	}

	printf("\n");
}

#define FAN_FEATURE(x)		has_features[x - SENSORS_FEATURE_FAN - 1]
#define FAN_FEATURE_VAL(x)	feature_vals[x - SENSORS_FEATURE_FAN - 1]
static void print_generic_chip_fan(const sensors_chip_name *name,
				   const sensors_feature_data *feature,
				   int i, int label_size)
{
	char *label;
	const int size = SENSORS_FEATURE_FAN_DIV - SENSORS_FEATURE_FAN;
	short has_features[SENSORS_FEATURE_FAN_DIV - SENSORS_FEATURE_FAN] = { 0, };
	double feature_vals[SENSORS_FEATURE_FAN_DIV - SENSORS_FEATURE_FAN] = { 0.0, };
	double val;

	if (!(label = sensors_get_label(name, feature->number))) {
		printf("ERROR: Can't get fan label!\n");
		return;
	}

	if (get_feature_value(name, feature, &val)) {
		printf("ERROR: Can't get %s data!\n", label);
		free(label);
		return;
	}

	print_label(label, label_size);
	free(label);

	if (FAN_FEATURE(SENSORS_FEATURE_FAN_FAULT) &&
	    FAN_FEATURE_VAL(SENSORS_FEATURE_FAN_FAULT))
		printf("   FAULT");
	else
		printf("%4.0f RPM", val);

	sensors_get_available_features(name, feature, i, has_features,
				       feature_vals, size, SENSORS_FEATURE_FAN);

	if (FAN_FEATURE(SENSORS_FEATURE_FAN_MIN) &&
	    FAN_FEATURE(SENSORS_FEATURE_FAN_DIV))
		printf("  (min = %4.0f RPM, div = %1.0f)",
		       FAN_FEATURE_VAL(SENSORS_FEATURE_FAN_MIN),
		       FAN_FEATURE_VAL(SENSORS_FEATURE_FAN_DIV));
	else if (FAN_FEATURE(SENSORS_FEATURE_FAN_MIN))
		printf("  (min = %4.0f RPM)",
		       FAN_FEATURE_VAL(SENSORS_FEATURE_FAN_MIN));
	else if (FAN_FEATURE(SENSORS_FEATURE_FAN_DIV))
		printf("  (div = %1.0f)",
		       FAN_FEATURE_VAL(SENSORS_FEATURE_FAN_DIV));

	if (FAN_FEATURE(SENSORS_FEATURE_FAN_ALARM) &&
	    FAN_FEATURE_VAL(SENSORS_FEATURE_FAN_ALARM)) {
		printf("  ALARM");
	}

	printf("\n");
}

void print_generic_chip(const sensors_chip_name *name)
{
	const sensors_feature_data *feature;
	int i, label_size;

	label_size = sensors_get_label_size(name);

	i = 0;
	while ((feature = sensors_get_all_features(name, &i))) {
		if (feature->mapping != SENSORS_NO_MAPPING)
			continue;

		switch (feature->type) {
		case SENSORS_FEATURE_TEMP:
			print_generic_chip_temp(name, feature, i, label_size);
			break;
		case SENSORS_FEATURE_IN:
			print_generic_chip_in(name, feature, i, label_size);
			break;
		case SENSORS_FEATURE_FAN:
			print_generic_chip_fan(name, feature, i, label_size);
			break;
		case SENSORS_FEATURE_VID:
			print_vid_info(name, feature->number, label_size);
			break;
		default:
			continue;
		}
	}
}
