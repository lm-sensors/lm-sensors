/*
 * sensord
 *
 * A daemon that periodically logs sensor information to syslog.
 *
 * Copyright (c) 1999-2000 Merlin Hughes <merlin@merlin.org>
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

static int
getLabelAndValid
(sensors_chip_name name, int feature, char **label, int *valid) {
  int err;
  err = sensors_get_label (name, feature, label);
  if (!err)
    err = sensors_get_ignored (name, feature);
  if (err >= 0) {
    *valid = err;
    err = 0;
  }
  return err;
}

static int
readUnknownChip
(const sensors_chip_name *chip) {
  const sensors_feature_data *sensor;
  int index0 = 0, index1 = 0;
  int ret = 0;

  while ((ret == 0) && ((sensor = sensors_get_all_features (*chip, &index0, &index1)) != NULL)) {
    char *label = NULL;
    int valid = 0;
    double value;
    
    if (getLabelAndValid (*chip, sensor->number, &label, &valid)) {
      sensorLog (LOG_ERR, "Error getting sensor label: %s/%s", chip->prefix, sensor->name);
      ret = 20;
    } else if (!valid) {
      /* skip invalid */
    } else if (!(sensor->mode & SENSORS_MODE_R)) {
      sensorLog (LOG_INFO, "%s: %s", sensor->name, label);
    } else if ((ret = sensors_get_feature (*chip, sensor->number, &value))) {
      sensorLog (LOG_ERR, "Error getting sensor data: %s/%s: %s", chip->prefix, sensor->name, sensors_strerror (ret));
      ret = 21;
    } else {
      sensorLog (LOG_INFO, "%s%s: %.2f", (sensor->mapping == SENSORS_NO_MAPPING) ? "" : "-"/*, sensor->name*/, label, value);
    }
    if (label)
      free (label);
  }
  
  return ret;
}

static int
readKnownChip
(const sensors_chip_name *chip, const ChipDescriptor *descriptor) {
  const FeatureDescriptor *features = descriptor->features;
  int alarms = 0, beeps = 0;
  int index0, subindex;
  int ret = 0;
  double tmp;

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
    char *label = NULL;
    int valid = 0;
    
    if (getLabelAndValid (*chip, labelNumber, &label, &valid)) {
      sensorLog (LOG_ERR, "Error getting sensor label: %s/#%d", chip->prefix, labelNumber);
      ret = 22;
    } else if (valid) {
      double values[MAX_DATA];
      int alarm = alarms & feature->alarmMask;
      int beep = beeps & feature->beepMask;

      for (subindex = 0; !ret && (feature->dataNumbers[subindex] >= 0); ++ subindex) {
        if ((ret = sensors_get_feature (*chip, feature->dataNumbers[subindex], values + subindex))) {
          sensorLog (LOG_ERR, "Error getting sensor data: %s/#%d: %s", chip->prefix, feature->dataNumbers[subindex], sensors_strerror (ret));
          ret = 23;
        }
      }
      if (ret == 0) {
        char *formatted = feature->format (values, alarm, beep);
        if (formatted) {
          sensorLog (LOG_INFO, "  %s: %s"/*, sensor->name*/, label, formatted);
        }
      }
    }
    if (label)
      free (label);
  }
  return ret;
}

static void
idChip
(const sensors_chip_name *chip) {
  const char *adapter, *algorithm;

  if (chip->bus == SENSORS_CHIP_NAME_BUS_ISA)
    sensorLog (LOG_INFO, "Chip: %s-isa-%04x", chip->prefix, chip->addr);
  else
    sensorLog (LOG_INFO, "Chip: %s-i2c-%d-%02x", chip->prefix, chip->bus, chip->addr);
  adapter = sensors_get_adapter_name (chip->bus);
  if (adapter)
    sensorLog (LOG_INFO, "Adapter: %s", adapter);
  algorithm = sensors_get_algorithm_name (chip->bus);
  if (algorithm)
    sensorLog (LOG_INFO, "Algorithm: %s", algorithm);
  /* assert adapter || algorithm */
}


static int
setChip
(const sensors_chip_name *chip) {
  int ret;

  if ((ret = sensors_do_chip_sets (*chip))) {
    sensorLog (LOG_ERR, "Error performing chip sets: %s: %s", chip->prefix, sensors_strerror (ret));
    ret = 20;
  }

  return ret;
}

static int
readChip
(const sensors_chip_name *chip) {
  int index0, subindex, chipindex = -1;
  int ret = 0;

  for (index0 = 0; knownChips[index0]; ++ index0)
    for (subindex = 0; knownChips[index0]->names[subindex]; ++ subindex)
      if (!strcmp (chip->prefix, knownChips[index0]->names[subindex]))
        chipindex = index0;
  
  if (chipindex >= 0)
    ret = readKnownChip (chip, knownChips[chipindex]);
  else
    ret = readUnknownChip (chip);

  return ret;
}

int
readChips
(void) {
  const sensors_chip_name *chip;
  int i = 0, j, ret = 0;

  sensorLog (LOG_DEBUG, "sensor scan started");

  while ((ret == 0) && ((chip = sensors_get_detected_chips (&i)) != NULL)) {
    for (j = 0; j < numChipNames; ++ j) {
      if (sensors_match_chip (*chip, chipNames[j])) {
        idChip (chip);
        if (doSet)
          ret = setChip (chip);
        else
          ret = readChip (chip);
        break;
      }
    }
  }

  sensorLog (LOG_DEBUG, "sensor scan finished");

  return ret;
}
