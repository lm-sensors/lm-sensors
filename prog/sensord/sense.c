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
#include <syslog.h>

#include "sensord.h"
#include "lib/error.h"

#define DO_READ 0
#define DO_SCAN 1
#define DO_SET 2
#define DO_RRD 3

int
getRawLabel
(const sensors_chip_name *name, int feature, const char **label) {
  const sensors_feature_data *rawFeature;
  int nr = 0, err = 0;
  do {
    rawFeature = sensors_get_all_features (name, &nr);
  } while (rawFeature && (rawFeature->number != feature));
  /* TODO: Ensure labels match RRD construct and are not repeated! */
  if (!rawFeature) {
    err = -1;
  } else {
    *label = rawFeature->name;
  }
  return err;
}

static const char *
chipName
(const sensors_chip_name *chip) {
  static char buffer[256];
  if (sensors_snprintf_chip_name(buffer, 256, chip) < 0)
    return NULL;
  return buffer;
}

static int
idChip
(const sensors_chip_name *chip) {
  const char *adapter;

  sensorLog (LOG_INFO, "Chip: %s", chipName (chip));
  adapter = sensors_get_adapter_name (&chip->bus);
  if (adapter)
    sensorLog (LOG_INFO, "Adapter: %s", adapter);
  
  return 0;
}

static int
readUnknownChip
(const sensors_chip_name *chip) {
  const sensors_feature_data *sensor;
  int index0 = 0;
  int ret = 0;

  ret = idChip (chip);

  while ((ret == 0) && ((sensor = sensors_get_all_features (chip, &index0)) != NULL)) {
    char *label = NULL;
    double value;
    
    if (!(label = sensors_get_label (chip, sensor->number))) {
      sensorLog (LOG_ERR, "Error getting sensor label: %s/%s", chip->prefix, sensor->name);
      ret = 21;
    } else if (!(sensor->flags & SENSORS_MODE_R)) {
      sensorLog (LOG_INFO, "%s: %s", sensor->name, label);
    } else if ((ret = sensors_get_value (chip, sensor->number, &value))) {
      sensorLog (LOG_ERR, "Error getting sensor data: %s/%s: %s", chip->prefix, sensor->name, sensors_strerror (ret));
      ret = 22;
    } else {
      sensorLog (LOG_INFO, "  %s%s: %.2f", (sensor->mapping == SENSORS_NO_MAPPING) ? "" : "-", label, value);
    }
    if (label)
      free (label);
  }
  
  return ret;
}

static int
doKnownChip
(const sensors_chip_name *chip, const ChipDescriptor *descriptor, int action) {
  const FeatureDescriptor *features = descriptor->features;
  int alarms = 0, beeps = 0;
  int index0, subindex;
  int ret = 0;
  double tmp;

  if (action == DO_READ)
    ret = idChip (chip);
  if (!ret && descriptor->alarmNumber) {
    if ((ret = sensors_get_value (chip, descriptor->alarmNumber, &tmp))) {
      sensorLog (LOG_ERR, "Error getting sensor data: %s/#%d: %s", chip->prefix, descriptor->alarmNumber, sensors_strerror (ret));
      ret = 20;
    } else {
      alarms = (int) (tmp + 0.5);
    }
  }
  if (!ret && descriptor->beepNumber) {
    if ((ret = sensors_get_value (chip, descriptor->beepNumber, &tmp))) {
      sensorLog (LOG_ERR, "Error getting sensor data: %s/#%d: %s", chip->prefix, descriptor->beepNumber, sensors_strerror (ret));
      ret = 21;
    } else {
      beeps = (int) (tmp + 0.5);
    }
  }
  for (index0 = 0; (ret == 0) && features[index0].format; ++ index0) {
    const FeatureDescriptor *feature = features + index0;
    int labelNumber = feature->dataNumbers[0];
    int alarm = alarms & feature->alarmMask;
    int beep = beeps & feature->beepMask;
    char *label = NULL;

    if ((action == DO_SCAN) && !alarm) {
      continue;
    } else if (!(label = sensors_get_label (chip, labelNumber))) {
      sensorLog (LOG_ERR, "Error getting sensor label: %s/#%d", chip->prefix, labelNumber);
      ret = 22;
    } else {
      double values[MAX_DATA];

      for (subindex = 0; !ret && (feature->dataNumbers[subindex] >= 0); ++ subindex) {
        if ((ret = sensors_get_value (chip, feature->dataNumbers[subindex], values + subindex))) {
          sensorLog (LOG_ERR, "Error getting sensor data: %s/#%d: %s", chip->prefix, feature->dataNumbers[subindex], sensors_strerror (ret));
          ret = 23;
        }
      }
      if (ret == 0) {
        if (action == DO_RRD) { // arse = "N:"
          if (feature->rrd) {
            const char *rrded = feature->rrd (values);
            strcat (strcat (rrdBuff, ":"), rrded ? rrded : "U");
          }
        } else {
          const char *formatted = feature->format (values, alarm, beep);
          if (formatted) {
            if (action == DO_READ) {
              sensorLog (LOG_INFO, "  %s: %s", label, formatted);
            } else {
              sensorLog (LOG_ALERT, "Sensor alarm: Chip %s: %s: %s", chipName (chip), label, formatted);
            }
          }
        }
      }
    }
    if (label)
      free (label);
  }
  return ret;
}

static int
setChip
(const sensors_chip_name *chip) {
  int ret = 0;
  if ((ret = idChip (chip))) {
    sensorLog (LOG_ERR, "Error identifying chip: %s", chip->prefix);
  } else if ((ret = sensors_do_chip_sets (chip))) {
    sensorLog (LOG_ERR, "Error performing chip sets: %s: %s", chip->prefix, sensors_strerror (ret));
    ret = 50;
  } else {
    sensorLog (LOG_INFO, "Set.");
  }
  return ret;
}

static int
doChip
(const sensors_chip_name *chip, int action) {
  int ret = 0;
  if (action == DO_SET) {
    ret = setChip (chip);
  } else {
    ChipDescriptor *descriptor;
    descriptor = generateChipDescriptor (chip);
    if (descriptor) {
      ret = doKnownChip (chip, descriptor, action);
      free (descriptor->features);
      free (descriptor);
    } else if (action == DO_READ)
      ret = readUnknownChip (chip);
  }
  return ret;
}

static int
doChips
(int action) {
  const sensors_chip_name *chip;
  int i = 0, j, ret = 0;

  for (j = 0; (ret == 0) && (j < numChipNames); ++ j) {
    while ((ret == 0) && ((chip = sensors_get_detected_chips (&chipNames[j], &i)) != NULL)) {
      ret = doChip (chip, action);
    }
  }

  return ret;
}

int
readChips
(void) {
  int ret = 0;

  sensorLog (LOG_DEBUG, "sensor read started");
  ret = doChips (DO_READ);
  sensorLog (LOG_DEBUG, "sensor read finished");

  return ret;
}

int
scanChips
(void) {
  int ret = 0;

  sensorLog (LOG_DEBUG, "sensor sweep started"); /* only logged in debug mode */
  ret = doChips (DO_SCAN);
  sensorLog (LOG_DEBUG, "sensor sweep finished");

  return ret;
}

int
setChips
(void) {
  int ret = 0;

  sensorLog (LOG_DEBUG, "sensor set started");
  ret = doChips (DO_SET);
  sensorLog (LOG_DEBUG, "sensor set finished");

  return ret;
}

/* TODO: loadavg entry */

int
rrdChips
(void) {
  int ret = 0;

  strcpy (rrdBuff, "N");

  sensorLog (LOG_DEBUG, "sensor rrd started"); 
  ret = doChips (DO_RRD);
  sensorLog (LOG_DEBUG, "sensor rrd finished");

  return ret;
}
