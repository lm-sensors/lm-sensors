/*
 * sensord
 *
 * A daemon that periodically logs sensor information to syslog.
 *
 * Copyright (c) 1999-2002 Merlin Hughes <merlin@merlin.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sensord.h"

/* TODO: Temp in C/F */

/** formatters **/

static char buff[4096];

static const char *
fmtExtra
(int alarm, int beep) {
  if (alarm)
    sprintf (buff + strlen (buff), " [ALARM]");
  if (beep)
    sprintf (buff + strlen (buff), " (beep)");
  return buff;
}

static const char *
fmtTemps_1
(const double values[], int alarm, int beep) {
  sprintf (buff, "%.1f C (limit = %.1f C, hysteresis = %.1f C)", values[0], values[1], values[2]);
  return fmtExtra (alarm, beep);
}

static const char *
fmtTemps_minmax_1
(const double values[], int alarm, int beep) {
 sprintf (buff, "%.1f C (min = %.1f C, max = %.1f C)", values[0], values[1], values[2]);
 return fmtExtra (alarm, beep);
}

static const char *
fmtTemp_only
(const double values[], int alarm, int beep) {
  sprintf (buff, "%.1f C", values[0]);
  return fmtExtra (alarm, beep);
}

static const char *
fmtVolt_2
(const double values[], int alarm, int beep) {
  sprintf (buff, "%+.2f V", values[0]);
  return fmtExtra (alarm, beep);
}

static const char *
fmtVolt_3
(const double values[], int alarm, int beep) {
  sprintf (buff, "%+.3f V", values[0]);
  return fmtExtra (alarm, beep);
}

static const char *
fmtVolts_2
(const double values[], int alarm, int beep) {
  sprintf (buff, "%+.2f V (min = %+.2f V, max = %+.2f V)", values[0], values[1], values[2]);
  return fmtExtra (alarm, beep);
}

static const char *
fmtFans_0
(const double values[], int alarm, int beep) {
  sprintf (buff, "%.0f RPM (min = %.0f RPM, div = %.0f)", values[0], values[1], values[2]);
  return fmtExtra (alarm, beep);
}

static const char *
fmtFans_nodiv_0
(const double values[], int alarm, int beep) {
  sprintf (buff, "%.0f RPM (min = %.0f RPM)", values[0], values[1]);
  return fmtExtra (alarm, beep);
}

static const char *
fmtFan_only
(const double values[], int alarm, int beep) {
  sprintf (buff, "%.0f RPM", values[0]);
  return fmtExtra (alarm, beep);
}

static const char *
fmtSoundAlarm
(const double values[], int alarm, int beep) {
  sprintf (buff, "Sound alarm %s", (values[0] < 0.5) ? "disabled" : "enabled");
  return fmtExtra (alarm, beep);
}

static const char *
rrdF0
(const double values[]) {
  sprintf (buff, "%.0f", values[0]);
  return buff;
}

static const char *
rrdF1
(const double values[]) {
  sprintf (buff, "%.1f", values[0]);
  return buff;
}

static const char *
rrdF2
(const double values[]) {
  sprintf (buff, "%.2f", values[0]);
  return buff;
}

static const char *
rrdF3
(const double values[]) {
  sprintf (buff, "%.3f", values[0]);
  return buff;
}

static void getAvailableFeatures (const sensors_chip_name *name,
                                  const sensors_feature_data *feature,
                                  int i, short *has_features,
                                  int *feature_nrs, int size,
                                  int first_val)
{
  const sensors_feature_data *iter;

  while ((iter = sensors_get_all_features (name, &i)) &&
         iter->mapping == feature->number) {
    int index0;

    index0 = iter->type - first_val - 1;
    if (index0 < 0 || index0 >= size)
      /* New feature in libsensors? Ignore. */
      continue;

    has_features[index0] = 1;
    feature_nrs[index0] = iter->number;
  }
}

#define IN_FEATURE(x)      has_features[x - SENSORS_FEATURE_IN - 1]
#define IN_FEATURE_NR(x)   feature_nrs[x - SENSORS_FEATURE_IN - 1]
static void fillChipVoltage (FeatureDescriptor *voltage,
                             const sensors_chip_name *name,
                             const sensors_feature_data *feature, int i)
{
  const int size = SENSORS_FEATURE_IN_MAX_ALARM - SENSORS_FEATURE_IN;
  short has_features[SENSORS_FEATURE_IN_MAX_ALARM - SENSORS_FEATURE_IN] = { 0, };
  int feature_nrs[SENSORS_FEATURE_IN_MAX_ALARM - SENSORS_FEATURE_IN];
  int pos = 0;

  voltage->rrd = rrdF2;
  voltage->type = DataType_voltage;
  voltage->dataNumbers[pos++] = feature->number;

  getAvailableFeatures (name, feature, i, has_features,
                        feature_nrs, size, SENSORS_FEATURE_IN);

  if (IN_FEATURE(SENSORS_FEATURE_IN_MIN) &&
      IN_FEATURE(SENSORS_FEATURE_IN_MAX)) {
    voltage->format = fmtVolts_2;
    voltage->dataNumbers[pos++] = IN_FEATURE_NR(SENSORS_FEATURE_IN_MIN);
    voltage->dataNumbers[pos++] = IN_FEATURE_NR(SENSORS_FEATURE_IN_MAX);
  } else {
    voltage->format = fmtVolt_2;
  }
  
  /* terminate the list */
  voltage->dataNumbers[pos] = -1;
}

#define TEMP_FEATURE(x)      has_features[x - SENSORS_FEATURE_TEMP - 1]
#define TEMP_FEATURE_NR(x)   feature_nrs[x - SENSORS_FEATURE_TEMP - 1]
static void fillChipTemperature (FeatureDescriptor *temperature,
                                 const sensors_chip_name *name,
                                 const sensors_feature_data *feature, int i)
{
  const int size = SENSORS_FEATURE_TEMP_TYPE - SENSORS_FEATURE_TEMP;
  short has_features[SENSORS_FEATURE_TEMP_TYPE - SENSORS_FEATURE_TEMP] = { 0, };
  int feature_nrs[SENSORS_FEATURE_TEMP_TYPE - SENSORS_FEATURE_TEMP];
  int pos = 0;

  temperature->rrd = rrdF1;
  temperature->type = DataType_temperature;
  temperature->dataNumbers[pos++] = feature->number;

  getAvailableFeatures (name, feature, i, has_features,
                        feature_nrs, size, SENSORS_FEATURE_TEMP);

  if (TEMP_FEATURE(SENSORS_FEATURE_TEMP_MIN) &&
      TEMP_FEATURE(SENSORS_FEATURE_TEMP_MAX)) {
    temperature->format = fmtTemps_minmax_1;
    temperature->dataNumbers[pos++] = TEMP_FEATURE_NR(SENSORS_FEATURE_TEMP_MIN);
    temperature->dataNumbers[pos++] = TEMP_FEATURE_NR(SENSORS_FEATURE_TEMP_MAX);
  } else if (TEMP_FEATURE(SENSORS_FEATURE_TEMP_MAX) &&
             TEMP_FEATURE(SENSORS_FEATURE_TEMP_MAX_HYST)) {
    temperature->format = fmtTemps_1;
    temperature->dataNumbers[pos++] = TEMP_FEATURE_NR(SENSORS_FEATURE_TEMP_MAX);
    temperature->dataNumbers[pos++] = TEMP_FEATURE_NR(SENSORS_FEATURE_TEMP_MAX_HYST);
  } else {
    temperature->format = fmtTemp_only;
  }
  
  /* terminate the list */
  temperature->dataNumbers[pos] = -1;
}

#define FAN_FEATURE(x)      has_features[x - SENSORS_FEATURE_FAN - 1]
#define FAN_FEATURE_NR(x)   feature_nrs[x - SENSORS_FEATURE_FAN - 1]
static void fillChipFan (FeatureDescriptor *fan,
                         const sensors_chip_name *name,
                         const sensors_feature_data *feature, int i)
{
  const int size = SENSORS_FEATURE_FAN_DIV - SENSORS_FEATURE_FAN;
  short has_features[SENSORS_FEATURE_FAN_DIV - SENSORS_FEATURE_FAN] = { 0, };
  int feature_nrs[SENSORS_FEATURE_FAN_DIV - SENSORS_FEATURE_FAN];
  int pos = 0;

  fan->rrd = rrdF0;
  fan->type = DataType_rpm;
  fan->dataNumbers[pos++] = feature->number;

  getAvailableFeatures (name, feature, i, has_features,
                        feature_nrs, size, SENSORS_FEATURE_FAN);

  if (FAN_FEATURE(SENSORS_FEATURE_FAN_MIN)) {
    fan->dataNumbers[pos++] = FAN_FEATURE_NR(SENSORS_FEATURE_FAN_MIN);
    if (FAN_FEATURE(SENSORS_FEATURE_FAN_DIV)) {
      fan->format = fmtFans_0;
      fan->dataNumbers[pos++] = FAN_FEATURE_NR(SENSORS_FEATURE_FAN_DIV);
    } else {
      fan->format = fmtFans_nodiv_0;
    }
  } else {
      fan->format = fmtFan_only;
  }
  
  /* terminate the list */
  fan->dataNumbers[pos] = -1;
}

static void fillChipVid (FeatureDescriptor *voltage,
                         const sensors_feature_data *feature)
{
  voltage->format = fmtVolt_3;
  voltage->rrd = rrdF3;
  voltage->type = DataType_voltage;
  voltage->dataNumbers[0] = feature->number;
  voltage->dataNumbers[1] = -1;
}

static void fillChipBeepEnable (FeatureDescriptor *voltage,
                                const sensors_feature_data *feature)
{
  voltage->format = fmtSoundAlarm;
  voltage->rrd = rrdF0;
  voltage->type = DataType_other;
  voltage->dataNumbers[0] = feature->number;
  voltage->dataNumbers[1] = -1;
}

/* Note that alarms and beeps are no longer (or not yet) supported */
ChipDescriptor * generateChipDescriptor (const sensors_chip_name *chip)
{
	int nr, count = 1;
	const sensors_feature_data *sensor;
	ChipDescriptor *descriptor;
	FeatureDescriptor *features;

	/* How many main features do we have? */
	nr = 0;
	while ((sensor = sensors_get_all_features(chip, &nr))) {
		if (sensor->mapping == SENSORS_NO_MAPPING)
			count++;
	}

	/* Allocate the memory we need */
	descriptor = calloc(1, sizeof(ChipDescriptor));
	features = calloc(count, sizeof(FeatureDescriptor));
	if (!descriptor || !features) {
		free(descriptor);
		free(features);
		return NULL;
	}
	descriptor->features = features;

	/* Fill in the data structures */
	count = 0;
	nr = 0;
	while ((sensor = sensors_get_all_features(chip, &nr))) {
		if (sensor->mapping != SENSORS_NO_MAPPING)
			continue;

		switch (sensor->type) {
		case SENSORS_FEATURE_TEMP:
			fillChipTemperature(&features[count], chip, sensor, nr);
			break;
		case SENSORS_FEATURE_IN:
			fillChipVoltage(&features[count], chip, sensor, nr);
			break;
		case SENSORS_FEATURE_FAN:
			fillChipFan(&features[count], chip, sensor, nr);
			break;
		case SENSORS_FEATURE_VID:
			fillChipVid(&features[count], sensor);
			break;
		case SENSORS_FEATURE_BEEP_ENABLE:
			fillChipBeepEnable(&features[count], sensor);
			break;
		default:
			continue;
		}

		count++;
	}

	return descriptor;
}
