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

#include "main.h"
#include "chips.h"
#include "lib/sensors.h"

void print_chip_raw(const sensors_chip_name *name)
{
	int a;
	const sensors_feature_data *data;
	char *label;
	double val;

	a = 0;
	while ((data = sensors_get_all_features(name, &a))) {
		if (!(label = sensors_get_label(name, data->number))) {
			printf("ERROR: Can't get feature `%s' label!\n",
			       data->name);
			continue;
		}
		if (data->flags & SENSORS_MODE_R) {
			if (sensors_get_value(name, data->number, &val))
				printf("ERROR: Can't get feature `%s' data!\n",
				       data->name);
			else if (data->mapping != SENSORS_NO_MAPPING)
				printf("  %s: %.2f\n", label, val);
			else
				printf("%s: %.2f (%s)\n", label, val,
				       data->name);
		} else
			printf("(%s)\n", label);
		free(label);
	}
}

static inline double deg_ctof(double cel)
{
	return cel * (9.0F / 5.0F) + 32.0F;
}

static void print_label(const char *label, int space)
{
	int len = strlen(label)+1;
	printf("%s:%*s", label, space - len, "");
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

		if (sensors_get_value(name, iter->number, &feature_vals[indx]))
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

static void print_temp_limits(double limit1, double limit2,
			      const char *name1, const char *name2, int alarm)
{
	if (fahrenheit) {
		limit1 = deg_ctof(limit1);
		limit2 = deg_ctof(limit2);
        }

	if (name2) {
		printf("(%-4s = %+5.1f%s, %-4s = %+5.1f%s)  ",
		       name1, limit1, degstr,
		       name2, limit2, degstr);
	} else if (name1) {
		printf("(%-4s = %+5.1f%s)                  ",
		       name1, limit1, degstr);
	} else {
		printf("                                  ");
	}

	if (alarm)
		printf("ALARM  ");
}

#define TEMP_FEATURE(x)		has_features[x - SENSORS_FEATURE_TEMP - 1]
#define TEMP_FEATURE_VAL(x)	feature_vals[x - SENSORS_FEATURE_TEMP - 1]
static void print_chip_temp(const sensors_chip_name *name,
			    const sensors_feature_data *feature, int i,
			    int label_size)
{
	double val, limit1, limit2;
	const char *s1, *s2;
	int alarm, crit_displayed = 0;
	char *label;
	const int size = SENSORS_FEATURE_TEMP_TYPE - SENSORS_FEATURE_TEMP;
	short has_features[SENSORS_FEATURE_TEMP_TYPE - SENSORS_FEATURE_TEMP] = { 0, };
	double feature_vals[SENSORS_FEATURE_TEMP_TYPE - SENSORS_FEATURE_TEMP] = { 0.0, };

	if (!(label = sensors_get_label(name, feature->number))) {
		printf("ERROR: Can't get temperature label!\n");
		return;
	}

	if (sensors_get_value(name, feature->number, &val)) {
		printf("ERROR: Can't get %s data!\n", label);
		free(label);
		return;
	}

	sensors_get_available_features(name, feature, i, has_features,
				       feature_vals, size,
				       SENSORS_FEATURE_TEMP);

	alarm = TEMP_FEATURE(SENSORS_FEATURE_TEMP_ALARM) &&
		TEMP_FEATURE_VAL(SENSORS_FEATURE_TEMP_ALARM);
	if (TEMP_FEATURE(SENSORS_FEATURE_TEMP_MAX)) {
		if (TEMP_FEATURE(SENSORS_FEATURE_TEMP_MAX_ALARM) &&
		    TEMP_FEATURE_VAL(SENSORS_FEATURE_TEMP_MAX_ALARM))
			alarm |= 1;

     		if (TEMP_FEATURE(SENSORS_FEATURE_TEMP_MIN)) {
			limit1 = TEMP_FEATURE_VAL(SENSORS_FEATURE_TEMP_MIN);
			s1 = "low";
			limit2 = TEMP_FEATURE_VAL(SENSORS_FEATURE_TEMP_MAX);
			s2 = "high";

			if (TEMP_FEATURE(SENSORS_FEATURE_TEMP_MIN_ALARM) &&
			    TEMP_FEATURE_VAL(SENSORS_FEATURE_TEMP_MIN_ALARM))
				alarm |= 1;
		} else {
			limit1 = TEMP_FEATURE_VAL(SENSORS_FEATURE_TEMP_MAX);
			s1 = "high";

			if (TEMP_FEATURE(SENSORS_FEATURE_TEMP_MAX_HYST)) {
				limit2 = TEMP_FEATURE_VAL(SENSORS_FEATURE_TEMP_MAX_HYST);
				s2 = "hyst";
			} else if (TEMP_FEATURE(SENSORS_FEATURE_TEMP_CRIT)) {
				limit2 = TEMP_FEATURE_VAL(SENSORS_FEATURE_TEMP_CRIT);
				s2 = "crit";

				if (TEMP_FEATURE(SENSORS_FEATURE_TEMP_CRIT_ALARM) &&
				    TEMP_FEATURE_VAL(SENSORS_FEATURE_TEMP_CRIT_ALARM))
					alarm |= 1;
				crit_displayed = 1;
			} else {
				limit2 = 0;
				s2 = NULL;
			}
		}
	} else {
		limit1 = limit2 = 0;
		s1 = s2 = NULL;
	}

	print_label(label, label_size);
	free(label);

	if (TEMP_FEATURE(SENSORS_FEATURE_TEMP_FAULT) &&
	    TEMP_FEATURE_VAL(SENSORS_FEATURE_TEMP_FAULT)) {
		printf("   FAULT  ");
	} else {
		if (fahrenheit)
			val = deg_ctof(val);
		printf("%+6.1f%s  ", val, degstr);
	}
	print_temp_limits(limit1, limit2, s1, s2, alarm);

	if (!crit_displayed && TEMP_FEATURE(SENSORS_FEATURE_TEMP_CRIT)) {
		limit1 = TEMP_FEATURE_VAL(SENSORS_FEATURE_TEMP_CRIT);
		s1 = "crit";

		if (TEMP_FEATURE(SENSORS_FEATURE_TEMP_CRIT_HYST)) {
			limit2 = TEMP_FEATURE_VAL(SENSORS_FEATURE_TEMP_CRIT_HYST);
			s2 = "hyst";
		} else {
			limit2 = 0;
			s2 = NULL;
		}

		alarm = TEMP_FEATURE(SENSORS_FEATURE_TEMP_CRIT_ALARM) &&
			TEMP_FEATURE_VAL(SENSORS_FEATURE_TEMP_CRIT_ALARM);

		printf("\n%*s", label_size + 10, "");
		print_temp_limits(limit1, limit2, s1, s2, alarm);
	}

	/* print out temperature sensor info */
	if (TEMP_FEATURE(SENSORS_FEATURE_TEMP_TYPE)) {
		int sens = (int)TEMP_FEATURE_VAL(SENSORS_FEATURE_TEMP_TYPE);

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
static void print_chip_in(const sensors_chip_name *name,
			  const sensors_feature_data *feature, int i,
			  int label_size)
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

	if (sensors_get_value(name, feature->number, &val)) {
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
static void print_chip_fan(const sensors_chip_name *name,
			   const sensors_feature_data *feature, int i,
			   int label_size)
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

	if (sensors_get_value(name, feature->number, &val)) {
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

static void print_chip_vid(const sensors_chip_name *name, int f_vid,
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

void print_chip(const sensors_chip_name *name)
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
			print_chip_temp(name, feature, i, label_size);
			break;
		case SENSORS_FEATURE_IN:
			print_chip_in(name, feature, i, label_size);
			break;
		case SENSORS_FEATURE_FAN:
			print_chip_fan(name, feature, i, label_size);
			break;
		case SENSORS_FEATURE_VID:
			print_chip_vid(name, feature->number, label_size);
			break;
		default:
			continue;
		}
	}
}
