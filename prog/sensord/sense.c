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
getValid
(sensors_chip_name name, int feature, int *valid) {
  int err;
  err = sensors_get_ignored (name, feature);
  if (err >= 0) {
    *valid = err;
    err = 0;
  }
  return err;
}

int
getLabel
(sensors_chip_name name, int feature, char **label) {
  int err;
  err = sensors_get_label (name, feature, label);
  return err;
}

int
getRawLabel
(sensors_chip_name name, int feature, const char **label) {
  const sensors_feature_data *rawFeature;
  int nr1 = 0, nr2 = 0, err = 0;
  do {
    rawFeature = sensors_get_all_features (name, &nr1, &nr2);
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
  if (chip->bus == SENSORS_CHIP_NAME_BUS_ISA)
    sprintf (buffer, "%s-isa-%04x", chip->prefix, chip->addr);
  else
    sprintf (buffer, "%s-i2c-%d-%02x", chip->prefix, chip->bus, chip->addr);
  return buffer;
}

static int
idChip
(const sensors_chip_name *chip) {
  const char *adapter, *algorithm;

  sensorLog (LOG_INFO, "Chip: %s", chipName (chip));
  adapter = sensors_get_adapter_name (chip->bus);
  if (adapter)
    sensorLog (LOG_INFO, "Adapter: %s", adapter);
  algorithm = sensors_get_algorithm_name (chip->bus);
  if (algorithm)
    sensorLog (LOG_INFO, "Algorithm: %s", algorithm);
  /* assert adapter || algorithm */
  
  return 0;
}

static int
readUnknownChip
(const sensors_chip_name *chip) {
  const sensors_feature_data *sensor;
  int index0 = 0, index1 = 0;
  int ret = 0;

  ret = idChip (chip);

  while ((ret == 0) && ((sensor = sensors_get_all_features (*chip, &index0, &index1)) != NULL)) {
    char *label = NULL;
    int valid = 0;
    double value;
    
    if (getValid (*chip, sensor->number, &valid)) {
      sensorLog (LOG_ERR, "Error getting sensor validity: %s/%s", chip->prefix, sensor->name);
      ret = 20;
    } else if (getLabel (*chip, sensor->number, &label)) {
      sensorLog (LOG_ERR, "Error getting sensor label: %s/%s", chip->prefix, sensor->name);
      ret = 21;
    } else if (!valid) {
      /* skip invalid */
    } else if (!(sensor->mode & SENSORS_MODE_R)) {
      sensorLog (LOG_INFO, "%s: %s", sensor->name, label);
    } else if ((ret = sensors_get_feature (*chip, sensor->number, &value))) {
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
    if ((ret = sensors_get_feature (*chip, descriptor->alarmNumber, &tmp))) {
      sensorLog (LOG_ERR, "Error getting sensor data: %s/#%d: %s", chip->prefix, descriptor->alarmNumber, sensors_strerror (ret));
      ret = 20;
    } else {
      alarms = (int) (tmp + 0.5);
    }
  }
  if (!ret && descriptor->beepNumber) {
    if ((ret = sensors_get_feature (*chip, descriptor->beepNumber, &tmp))) {
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
    int valid = 0;

    if ((action == DO_SCAN) && !alarm) {
      continue;
    } else if (getValid (*chip, labelNumber, &valid)) {
      sensorLog (LOG_ERR, "Error getting sensor validity: %s/#%d", chip->prefix, labelNumber);
      ret = 22;
    } else if (getLabel (*chip, labelNumber, &label)) {
      sensorLog (LOG_ERR, "Error getting sensor label: %s/#%d", chip->prefix, labelNumber);
      ret = 22;
    } else if (valid) {
      double values[MAX_DATA];

      for (subindex = 0; !ret && (feature->dataNumbers[subindex] >= 0); ++ subindex) {
        if ((ret = sensors_get_feature (*chip, feature->dataNumbers[subindex], values + subindex))) {
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
  } else if ((ret = sensors_do_chip_sets (*chip))) {
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
    int index0, subindex, chipindex = -1;
    for (index0 = 0; knownChips[index0]; ++ index0)
      for (subindex = 0; knownChips[index0]->names[subindex]; ++ subindex)
        if (!strcmp (chip->prefix, knownChips[index0]->names[subindex]))
          chipindex = index0;
    if (chipindex >= 0)
      ret = doKnownChip (chip, knownChips[chipindex], action);
    else if (action == DO_READ)
      ret = readUnknownChip (chip);
  }
  return ret;
}

static int
doChips
(int action) {
  const sensors_chip_name *chip;
  int i = 0, j, ret = 0;

  while ((ret == 0) && ((chip = sensors_get_detected_chips (&i)) != NULL)) {
    for (j = 0; (ret == 0) && (j < numChipNames); ++ j) {
      if (sensors_match_chip (*chip, chipNames[j])) {
        ret = doChip (chip, action);
      }
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
