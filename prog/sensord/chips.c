/*
 * sensord
 *
 * A daemon that periodically logs sensor information to syslog.
 *
 * Copyright (c) 1999-2001 Merlin Hughes <merlin@merlin.org>
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

#include "sensord.h"
#include "lib/chips.h"
#include "kernel/include/sensors.h"

/* N.B: missing from sensors (and this) but in lib/chips.h: "gl520sm", "thmc50", "adm1022" */

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
fmtValu_0
(const double values[], int alarm, int beep) {
  sprintf (buff, "%.0f", values[0]);
  return fmtExtra (alarm, beep);
}

static const char *
fmtTemps_0
(const double values[], int alarm, int beep) {
  sprintf (buff, "%.0f C (limit = %.0f C, hysteresis = %.0f C)", values[0], values[1], values[2]);
  return fmtExtra (alarm, beep);
}

static const char *
fmtTemps_1
(const double values[], int alarm, int beep) {
  sprintf (buff, "%.1f C (limit = %.1f C, hysteresis = %.1f C)", values[0], values[1], values[2]);
  return fmtExtra (alarm, beep);
}

static const char *
fmtVolt_2
(const double values[], int alarm, int beep) {
  sprintf (buff, "%+.2f V", values[0]);
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
fmtMHz_2
(const double values[], int alarm, int beep) {
  sprintf (buff, "%.2f MHz (min = %.2f MHz, max = %.2f MHz)", values[0], values[1], values[2]);
  return fmtExtra (alarm, beep);
}

static const char *
fmtChassisIntrusionDetection
(const double values[], int alarm, int beep) {
  sprintf (buff, "Chassis intrusion detection");
  return fmtExtra (alarm, beep);
}

static const char *
fmtBoardTemperatureInput
(const double values[], int alarm, int beep) {
  sprintf (buff, "Board temperature input"); /* N.B: "(usually LM75 chips)" */
  return fmtExtra (alarm, beep);
}

static const char *
fmtSoundAlarm
(const double values[], int alarm, int beep) {
  sprintf (buff, "Sound alarm %s", (values[0] < 0.5) ? "disabled" : "enabled");
  return fmtExtra (alarm, beep);
}

static const char *
fmtInt
(const double values[], int alarm, int beep) {
  sprintf (buff, "%d", (int) values[0]);
  return fmtExtra (alarm, beep);
}

static const char *
fmtIntCrossInt
(const double values[], int alarm, int beep) {
  sprintf (buff, "%dx%d", (int) values[0], (int) values[1]);
  return fmtExtra (alarm, beep);
}

static const char *
fmtIntDashInt
(const double values[], int alarm, int beep) {
  sprintf (buff, "%d-%d", (int) values[0], (int) values[1]);
  return fmtExtra (alarm, beep);
}

/** LM75 **/

static const char *lm75_names[] = {
  SENSORS_LM75_PREFIX, NULL
};

static const FeatureDescriptor lm75_features[] = {
  { fmtTemps_1, 0, 0,
    { SENSORS_LM75_TEMP, SENSORS_LM75_TEMP_HYST, SENSORS_LM75_TEMP_OVER, -1 } },
  { NULL }
};

static const ChipDescriptor lm75_chip = {
  lm75_names, lm75_features, 0, 0
};

/** ADM1021 **/

static const char *adm1021_names[] = {
  SENSORS_ADM1021_PREFIX, NULL
};

static const FeatureDescriptor adm1021_features[] = {
  { fmtTemps_0, ADM1021_ALARM_TEMP_HIGH | ADM1021_ALARM_TEMP_LOW, 0, /* TODO!! */
    { SENSORS_ADM1021_TEMP, SENSORS_ADM1021_TEMP_OVER, SENSORS_ADM1021_TEMP_HYST, -1 } },
  { fmtTemps_0, ADM1021_ALARM_RTEMP_HIGH | ADM1021_ALARM_RTEMP_LOW | ADM1021_ALARM_RTEMP_NA, 0, /* TODO!! */
    { SENSORS_ADM1021_REMOTE_TEMP, SENSORS_ADM1021_REMOTE_TEMP_OVER, SENSORS_ADM1021_REMOTE_TEMP_HYST, -1 } },
  { fmtValu_0, 0, 0,
    { SENSORS_ADM1021_DIE_CODE, -1 } },
  { NULL }
};

static const ChipDescriptor adm1021_chip = {
  adm1021_names, adm1021_features, SENSORS_ADM1021_ALARMS, 0
};

/** MAX1617 **/

static const char *max1617_names[] = {
  SENSORS_MAX1617_PREFIX, SENSORS_MAX1617A_PREFIX, SENSORS_THMC10_PREFIX, SENSORS_LM84_PREFIX, SENSORS_GL523_PREFIX, NULL
};

static const FeatureDescriptor max1617_features[] = {
  { fmtTemps_0, ADM1021_ALARM_TEMP_HIGH | ADM1021_ALARM_TEMP_LOW, 0, /* TODO!! */
    { SENSORS_MAX1617_TEMP, SENSORS_MAX1617_TEMP_OVER, SENSORS_MAX1617_TEMP_HYST, -1 } },
  { fmtTemps_0, ADM1021_ALARM_RTEMP_HIGH | ADM1021_ALARM_RTEMP_LOW | ADM1021_ALARM_RTEMP_NA, 0, /* TODO!! */
    { SENSORS_MAX1617_REMOTE_TEMP, SENSORS_MAX1617_REMOTE_TEMP_OVER, SENSORS_MAX1617_REMOTE_TEMP_HYST, -1 } },
  { NULL }
};

static const ChipDescriptor max1617_chip = {
  max1617_names, max1617_features, SENSORS_MAX1617_ALARMS, 0
};

/** ADM9240 **/

static const char *adm9240_names[] = {
  SENSORS_ADM9240_PREFIX, SENSORS_DS1780_PREFIX, SENSORS_LM81_PREFIX, NULL
};

static const FeatureDescriptor adm9240_features[] = {
  { fmtVolts_2, ADM9240_ALARM_IN0, 0,
    { SENSORS_ADM9240_IN0, SENSORS_ADM9240_IN0_MIN, SENSORS_ADM9240_IN0_MAX, -1 } },
  { fmtVolts_2, ADM9240_ALARM_IN1, 0,
    { SENSORS_ADM9240_IN1, SENSORS_ADM9240_IN1_MIN, SENSORS_ADM9240_IN1_MAX, -1 } },
  { fmtVolts_2, ADM9240_ALARM_IN2, 0,
    { SENSORS_ADM9240_IN2, SENSORS_ADM9240_IN2_MIN, SENSORS_ADM9240_IN2_MAX, -1 } },
  { fmtVolts_2, ADM9240_ALARM_IN3, 0,
    { SENSORS_ADM9240_IN3, SENSORS_ADM9240_IN3_MIN, SENSORS_ADM9240_IN3_MAX, -1 } },
  { fmtVolts_2, ADM9240_ALARM_IN4, 0,
    { SENSORS_ADM9240_IN4, SENSORS_ADM9240_IN4_MIN, SENSORS_ADM9240_IN4_MAX, -1 } },
  { fmtVolts_2, ADM9240_ALARM_IN5, 0,
    { SENSORS_ADM9240_IN5, SENSORS_ADM9240_IN5_MIN, SENSORS_ADM9240_IN5_MAX, -1 } },
  { fmtFans_0, ADM9240_ALARM_FAN1, 0,
    { SENSORS_ADM9240_FAN1, SENSORS_ADM9240_FAN1_MIN, SENSORS_ADM9240_FAN1_DIV, -1 } },
  { fmtFans_0, ADM9240_ALARM_FAN2, 0,
    { SENSORS_ADM9240_FAN2, SENSORS_ADM9240_FAN2_MIN, SENSORS_ADM9240_FAN2_DIV, -1 } },
  { fmtTemps_0, ADM9240_ALARM_TEMP, 0,
    { SENSORS_ADM9240_TEMP, SENSORS_ADM9240_TEMP_OVER, SENSORS_ADM9240_TEMP_HYST, -1 } },
  { fmtVolt_2, 0, 0,
    { SENSORS_ADM9240_VID, -1 } },
  { fmtChassisIntrusionDetection, ADM9240_ALARM_CHAS, 0,
    { SENSORS_ADM9240_ALARMS, -1 } },
  { NULL }
};

static const ChipDescriptor adm9240_chip = {
  adm9240_names, adm9240_features, SENSORS_ADM9240_ALARMS, 0
};

/** LM78 **/

static const char *lm78_names[] = {
  SENSORS_LM78_PREFIX, SENSORS_LM78J_PREFIX, SENSORS_LM79_PREFIX, NULL
};

static const FeatureDescriptor lm78_features[] = {
  { fmtVolts_2, LM78_ALARM_IN0, 0,
    { SENSORS_LM78_IN0, SENSORS_LM78_IN0_MIN, SENSORS_LM78_IN0_MAX, -1 } },
  { fmtVolts_2, LM78_ALARM_IN1, 0,
    { SENSORS_LM78_IN1, SENSORS_LM78_IN1_MIN, SENSORS_LM78_IN1_MAX, -1 } },
  { fmtVolts_2, LM78_ALARM_IN2, 0,
    { SENSORS_LM78_IN2, SENSORS_LM78_IN2_MIN, SENSORS_LM78_IN2_MAX, -1 } },
  { fmtVolts_2, LM78_ALARM_IN3, 0,
    { SENSORS_LM78_IN3, SENSORS_LM78_IN3_MIN, SENSORS_LM78_IN3_MAX, -1 } },
  { fmtVolts_2, LM78_ALARM_IN4, 0,
    { SENSORS_LM78_IN4, SENSORS_LM78_IN4_MIN, SENSORS_LM78_IN4_MAX, -1 } },
  { fmtVolts_2, LM78_ALARM_IN5, 0,
    { SENSORS_LM78_IN5, SENSORS_LM78_IN5_MIN, SENSORS_LM78_IN5_MAX, -1 } },
  { fmtVolts_2, LM78_ALARM_IN6, 0,
    { SENSORS_LM78_IN6, SENSORS_LM78_IN6_MIN, SENSORS_LM78_IN6_MAX, -1 } },
  { fmtFans_0, LM78_ALARM_FAN1, 0,
    { SENSORS_LM78_FAN1, SENSORS_LM78_FAN1_MIN, SENSORS_LM78_FAN1_DIV, -1 } },
  { fmtFans_0, LM78_ALARM_FAN2, 0,
    { SENSORS_LM78_FAN2, SENSORS_LM78_FAN2_MIN, SENSORS_LM78_FAN2_DIV, -1 } },
  { fmtFans_0, LM78_ALARM_FAN3, 0,
    { SENSORS_LM78_FAN3, SENSORS_LM78_FAN3_MIN, SENSORS_LM78_FAN3_DIV, -1 } },
  { fmtTemps_0, LM78_ALARM_TEMP, 0,
    { SENSORS_LM78_TEMP, SENSORS_LM78_TEMP_OVER, SENSORS_LM78_TEMP_HYST, -1 } },
  { fmtBoardTemperatureInput, LM78_ALARM_BTI, 0,
    { SENSORS_LM78_ALARMS, -1 } },
  { fmtChassisIntrusionDetection, LM78_ALARM_CHAS, 0,
    { SENSORS_LM78_ALARMS, -1 } },
  { fmtVolt_2, 0, 0,
    { SENSORS_LM78_VID, -1 } },
  { NULL }
};

static const ChipDescriptor lm78_chip = {
  lm78_names, lm78_features, SENSORS_LM78_ALARMS, 0
};

/** SIS5595 **/

static const char *sis5595_names[] = {
  SENSORS_SIS5595_PREFIX, NULL
};

static const FeatureDescriptor sis5595_features[] = {
  { fmtVolts_2, SIS5595_ALARM_IN0, 0,
    { SENSORS_SIS5595_IN0, SENSORS_SIS5595_IN0_MIN, SENSORS_SIS5595_IN0_MAX, -1 } },
  { fmtVolts_2, SIS5595_ALARM_IN1, 0,
    { SENSORS_SIS5595_IN1, SENSORS_SIS5595_IN1_MIN, SENSORS_SIS5595_IN1_MAX, -1 } },
  { fmtVolts_2, SIS5595_ALARM_IN2, 0,
    { SENSORS_SIS5595_IN2, SENSORS_SIS5595_IN2_MIN, SENSORS_SIS5595_IN2_MAX, -1 } },
  { fmtVolts_2, SIS5595_ALARM_IN3, 0,
    { SENSORS_SIS5595_IN3, SENSORS_SIS5595_IN3_MIN, SENSORS_SIS5595_IN3_MAX, -1 } },
  { fmtFans_0, SIS5595_ALARM_FAN1, 0,
    { SENSORS_SIS5595_FAN1, SENSORS_SIS5595_FAN1_MIN, SENSORS_SIS5595_FAN1_DIV, -1 } },
  { fmtFans_0, SIS5595_ALARM_FAN2, 0,
    { SENSORS_SIS5595_FAN2, SENSORS_SIS5595_FAN2_MIN, SENSORS_SIS5595_FAN2_DIV, -1 } },
  { fmtTemps_0, SIS5595_ALARM_TEMP, 0,
    { SENSORS_SIS5595_TEMP, SENSORS_SIS5595_TEMP_OVER, SENSORS_SIS5595_TEMP_HYST, -1 } },
  { fmtBoardTemperatureInput, SIS5595_ALARM_BTI, 0,
    { SENSORS_SIS5595_ALARMS, -1 } },
  { NULL }
};

static const ChipDescriptor sis5595_chip = {
  sis5595_names, sis5595_features, SENSORS_SIS5595_ALARMS, 0
};

/** VIA686A **/

static const char *via686a_names[] = {
  SENSORS_VIA686A_PREFIX, NULL
};

static const FeatureDescriptor via686a_features[] = {
  { fmtVolts_2, VIA686A_ALARM_IN0, 0,
    { SENSORS_VIA686A_IN0, SENSORS_VIA686A_IN0_MIN, SENSORS_VIA686A_IN0_MAX, -1 } },
  { fmtVolts_2, VIA686A_ALARM_IN1, 0,
    { SENSORS_VIA686A_IN1, SENSORS_VIA686A_IN1_MIN, SENSORS_VIA686A_IN1_MAX, -1 } },
  { fmtVolts_2, VIA686A_ALARM_IN2, 0,
    { SENSORS_VIA686A_IN2, SENSORS_VIA686A_IN2_MIN, SENSORS_VIA686A_IN2_MAX, -1 } },
  { fmtVolts_2, VIA686A_ALARM_IN3, 0,
    { SENSORS_VIA686A_IN3, SENSORS_VIA686A_IN3_MIN, SENSORS_VIA686A_IN3_MAX, -1 } },
  { fmtVolts_2, VIA686A_ALARM_IN4, 0,
    { SENSORS_VIA686A_IN4, SENSORS_VIA686A_IN4_MIN, SENSORS_VIA686A_IN4_MAX, -1 } },
  { fmtFans_0, VIA686A_ALARM_FAN1, 0,
    { SENSORS_VIA686A_FAN1, SENSORS_VIA686A_FAN1_MIN, SENSORS_VIA686A_FAN1_DIV, -1 } },
  { fmtFans_0, VIA686A_ALARM_FAN2, 0,
    { SENSORS_VIA686A_FAN2, SENSORS_VIA686A_FAN2_MIN, SENSORS_VIA686A_FAN2_DIV, -1 } },
  { fmtTemps_1, VIA686A_ALARM_TEMP, 0,
    { SENSORS_VIA686A_TEMP, SENSORS_VIA686A_TEMP_OVER, SENSORS_VIA686A_TEMP_HYST, -1 } },
  { fmtTemps_1, VIA686A_ALARM_TEMP2, 0,
    { SENSORS_VIA686A_TEMP2, SENSORS_VIA686A_TEMP2_OVER, SENSORS_VIA686A_TEMP2_HYST, -1 } },
  { fmtTemps_1, VIA686A_ALARM_TEMP3, 0,
    { SENSORS_VIA686A_TEMP3, SENSORS_VIA686A_TEMP3_OVER, SENSORS_VIA686A_TEMP3_HYST, -1 } },
  { NULL }
};

static const ChipDescriptor via686a_chip = {
  via686a_names, via686a_features, SENSORS_VIA686A_ALARMS, 0
};

/** LM80 **/

static const char *
fmtTemps_LM80
(const double values[], int alarm, int beep) {
  sprintf (buff, "%.2f C (hot limit = %.0f C, hot hysteresis = %.0f C, os limit = %.0f C, os hysteresis = %.0f C)", values[0], values[1], values[2], values[3], values[4]);
  return fmtExtra (alarm, beep);
}

static const char *lm80_names[] = {
  SENSORS_LM80_PREFIX, NULL
};

static const FeatureDescriptor lm80_features[] = {
  { fmtVolts_2, LM80_ALARM_IN0, 0,
    { SENSORS_LM80_IN0, SENSORS_LM80_IN0_MIN, SENSORS_LM80_IN0_MAX, -1 } },
  { fmtVolts_2, LM80_ALARM_IN1, 0,
    { SENSORS_LM80_IN1, SENSORS_LM80_IN1_MIN, SENSORS_LM80_IN1_MAX, -1 } },
  { fmtVolts_2, LM80_ALARM_IN2, 0,
    { SENSORS_LM80_IN2, SENSORS_LM80_IN2_MIN, SENSORS_LM80_IN2_MAX, -1 } },
  { fmtVolts_2, LM80_ALARM_IN3, 0,
    { SENSORS_LM80_IN3, SENSORS_LM80_IN3_MIN, SENSORS_LM80_IN3_MAX, -1 } },
  { fmtVolts_2, LM80_ALARM_IN4, 0,
    { SENSORS_LM80_IN4, SENSORS_LM80_IN4_MIN, SENSORS_LM80_IN4_MAX, -1 } },
  { fmtVolts_2, LM80_ALARM_IN5, 0,
    { SENSORS_LM80_IN5, SENSORS_LM80_IN5_MIN, SENSORS_LM80_IN5_MAX, -1 } },
  { fmtVolts_2, LM80_ALARM_IN6, 0,
    { SENSORS_LM80_IN6, SENSORS_LM80_IN6_MIN, SENSORS_LM80_IN6_MAX, -1 } },
  { fmtFans_0, LM80_ALARM_FAN1, 0,
    { SENSORS_LM80_FAN1, SENSORS_LM80_FAN1_MIN, SENSORS_LM80_FAN1_DIV, -1 } },
  { fmtFans_0, LM80_ALARM_FAN2, 0,
    { SENSORS_LM80_FAN2, SENSORS_LM80_FAN2_MIN, SENSORS_LM80_FAN2_DIV, -1 } },
  { fmtTemps_LM80, LM80_ALARM_TEMP_HOT, 0,
    { SENSORS_LM80_TEMP, SENSORS_LM80_TEMP_HOT_MAX, SENSORS_LM80_TEMP_HOT_HYST, SENSORS_LM80_TEMP_OS_MAX, SENSORS_LM80_TEMP_OS_HYST, -1 } },
  { fmtBoardTemperatureInput, LM80_ALARM_BTI, 0,
    { SENSORS_LM80_ALARMS, -1 } },
  { fmtChassisIntrusionDetection, LM80_ALARM_CHAS, 0,
    { SENSORS_LM80_ALARMS, -1 } },
  { NULL }
};

static const ChipDescriptor lm80_chip = {
  lm80_names, lm80_features, SENSORS_LM80_ALARMS, 0
};

/** GL518 **/

/* N.B: sensors supports a "gl518sm-r00" but it is not picked up in main.c...
static const char *
fmtVolts_GL518_R00
(const double values[], int alarm, int beep) {
  if (values[0] == 0.0)
    sprintf (buff, "n/a (min = %+.2f V, max = %+.2f V)", values[1], values[2]);
  else
    sprintf (buff, "%+.2f V (min = %+.2f V, max = %+.2f V)", values[0], values[1], values[2]);
  return fmtExtra (alarm, beep);
}
*/

static const char *gl518_names[] = {
  SENSORS_GL518_PREFIX, NULL
};

static const FeatureDescriptor gl518_features[] = {
  { fmtVolts_2, GL518_ALARM_VDD, GL518_ALARM_VDD,
    { SENSORS_GL518_VDD, SENSORS_GL518_VDD_MIN, SENSORS_GL518_VDD_MAX, -1 } },
  { fmtVolts_2, GL518_ALARM_VIN1, GL518_ALARM_VIN1,
    { SENSORS_GL518_VIN1, SENSORS_GL518_VIN1_MIN, SENSORS_GL518_VIN1_MAX, -1 } },
  { fmtVolts_2, GL518_ALARM_VIN2, GL518_ALARM_VIN2,
    { SENSORS_GL518_VIN2, SENSORS_GL518_VIN2_MIN, SENSORS_GL518_VIN2_MAX, -1 } },
  { fmtVolts_2, GL518_ALARM_VIN3, GL518_ALARM_VIN3,
    { SENSORS_GL518_VIN3, SENSORS_GL518_VIN3_MIN, SENSORS_GL518_VIN3_MAX, -1 } },
  { fmtFans_0, GL518_ALARM_FAN1, GL518_ALARM_FAN1,
    { SENSORS_GL518_FAN1, SENSORS_GL518_FAN1_MIN, SENSORS_GL518_FAN1_DIV, -1 } },
  { fmtFans_0, GL518_ALARM_FAN2, GL518_ALARM_FAN2,
    { SENSORS_GL518_FAN2, SENSORS_GL518_FAN2_MIN, SENSORS_GL518_FAN2_DIV, -1 } },
  { fmtTemps_0, GL518_ALARM_TEMP, GL518_ALARM_TEMP,
    { SENSORS_GL518_TEMP, SENSORS_GL518_TEMP_OVER, SENSORS_GL518_TEMP_HYST, -1 } },
  { fmtSoundAlarm, 0, 0,
    { SENSORS_GL518_BEEP_ENABLE, -1 } },
  { NULL }
};

static const ChipDescriptor gl518_chip = {
  gl518_names, gl518_features, SENSORS_GL518_ALARMS, SENSORS_GL518_BEEPS
};

/** ADM1025 **/

static const char *adm1025_names[] = {
  SENSORS_ADM1025_PREFIX, NULL
};

static const FeatureDescriptor adm1025_features[] = {
  { fmtVolts_2, ADM1025_ALARM_IN0, 0,
    { SENSORS_ADM1025_IN0, SENSORS_ADM1025_IN0_MIN, SENSORS_ADM1025_IN0_MAX, -1 } },
  { fmtVolts_2, ADM1025_ALARM_IN1, 0,
    { SENSORS_ADM1025_IN1, SENSORS_ADM1025_IN1_MIN, SENSORS_ADM1025_IN1_MAX, -1 } },
  { fmtVolts_2, ADM1025_ALARM_IN2, 0,
    { SENSORS_ADM1025_IN2, SENSORS_ADM1025_IN2_MIN, SENSORS_ADM1025_IN2_MAX, -1 } },
  { fmtVolts_2, ADM1025_ALARM_IN3, 0,
    { SENSORS_ADM1025_IN3, SENSORS_ADM1025_IN3_MIN, SENSORS_ADM1025_IN3_MAX, -1 } },
  { fmtVolts_2, ADM1025_ALARM_IN4, 0,
    { SENSORS_ADM1025_IN4, SENSORS_ADM1025_IN4_MIN, SENSORS_ADM1025_IN4_MAX, -1 } },
  { fmtVolts_2, ADM1025_ALARM_IN5, 0,
    { SENSORS_ADM1025_IN5, SENSORS_ADM1025_IN5_MIN, SENSORS_ADM1025_IN5_MAX, -1 } },
  { fmtTemps_1, ADM1025_ALARM_TEMP, 0,
    { SENSORS_ADM1025_TEMP1, SENSORS_ADM1025_TEMP1_OVER, SENSORS_ADM1025_TEMP1_HYST, -1 } },
  { NULL }
};

static const ChipDescriptor adm1025_chip = {
  adm1025_names, adm1025_features, SENSORS_ADM1025_ALARMS, 0
};

/** W83781D **/

static const char *w83781d_names[] = {
  SENSORS_W83781D_PREFIX, SENSORS_AS99127F_PREFIX, NULL
};

/* TODO: flags inverted for as99127f */
static const FeatureDescriptor w83781d_features[] = {
  { fmtVolts_2, W83781D_ALARM_IN0, W83781D_ALARM_IN0,
    { SENSORS_W83781D_IN0, SENSORS_W83781D_IN0_MIN, SENSORS_W83781D_IN0_MAX, -1 } },
  { fmtVolts_2, W83781D_ALARM_IN1, W83781D_ALARM_IN1,
    { SENSORS_W83781D_IN1, SENSORS_W83781D_IN1_MIN, SENSORS_W83781D_IN1_MAX, -1 } },
  { fmtVolts_2, W83781D_ALARM_IN2, W83781D_ALARM_IN2,
    { SENSORS_W83781D_IN2, SENSORS_W83781D_IN2_MIN, SENSORS_W83781D_IN2_MAX, -1 } },
  { fmtVolts_2, W83781D_ALARM_IN3, W83781D_ALARM_IN3,
    { SENSORS_W83781D_IN3, SENSORS_W83781D_IN3_MIN, SENSORS_W83781D_IN3_MAX, -1 } },
  { fmtVolts_2, W83781D_ALARM_IN4, W83781D_ALARM_IN4,
    { SENSORS_W83781D_IN4, SENSORS_W83781D_IN4_MIN, SENSORS_W83781D_IN4_MAX, -1 } },
  { fmtVolts_2, W83781D_ALARM_IN5, W83781D_ALARM_IN5,
    { SENSORS_W83781D_IN5, SENSORS_W83781D_IN5_MIN, SENSORS_W83781D_IN5_MAX, -1 } },
  { fmtVolts_2, W83781D_ALARM_IN6, W83781D_ALARM_IN6,
    { SENSORS_W83781D_IN6, SENSORS_W83781D_IN6_MIN, SENSORS_W83781D_IN6_MAX, -1 } },
  { fmtFans_0, W83781D_ALARM_FAN1, W83781D_ALARM_FAN1,
    { SENSORS_W83781D_FAN1, SENSORS_W83781D_FAN1_MIN, SENSORS_W83781D_FAN1_DIV, -1 } },
  { fmtFans_0, W83781D_ALARM_FAN2, W83781D_ALARM_FAN2,
    { SENSORS_W83781D_FAN2, SENSORS_W83781D_FAN2_MIN, SENSORS_W83781D_FAN2_DIV, -1 } },
  { fmtFans_0, W83781D_ALARM_FAN3, W83781D_ALARM_FAN3,
    { SENSORS_W83781D_FAN3, SENSORS_W83781D_FAN3_MIN, SENSORS_W83781D_FAN3_DIV, -1 } },
  { fmtTemps_0, W83781D_ALARM_TEMP1, W83781D_ALARM_TEMP1,
    { SENSORS_W83781D_TEMP1, SENSORS_W83781D_TEMP1_OVER, SENSORS_W83781D_TEMP1_HYST, -1 } },
  { fmtTemps_1, W83781D_ALARM_TEMP2, W83781D_ALARM_TEMP2,
    { SENSORS_W83781D_TEMP2, SENSORS_W83781D_TEMP2_OVER, SENSORS_W83781D_TEMP2_HYST, -1 } },
  { fmtTemps_1, W83781D_ALARM_TEMP3, W83781D_ALARM_TEMP3,
    { SENSORS_W83781D_TEMP3, SENSORS_W83781D_TEMP3_OVER, SENSORS_W83781D_TEMP3_HYST, -1 } },
  { fmtVolt_2, 0, 0,
    { SENSORS_W83781D_VID, -1 } },
  { fmtChassisIntrusionDetection, W83781D_ALARM_CHAS, W83781D_ALARM_CHAS,
    { SENSORS_W83781D_ALARMS, -1 } },
  { fmtSoundAlarm, 0, 0,
    { SENSORS_W83781D_BEEP_ENABLE, -1 } },
  { NULL }
};

static const ChipDescriptor w83781d_chip = {
  w83781d_names, w83781d_features, SENSORS_W83781D_ALARMS, SENSORS_W83781D_BEEPS
};

/** W83782D **/

static const char *
fmtTemps_W8378x_0
(const double values[], int alarm, int beep) {
  int sensorID = (int) values[3];
  const char *sensor = (sensorID == 1) ? "PII/Celeron diode" :
    (sensorID == 2) ? "3904 transistor" : "thermistor";
   /* Is this still right? */
  sprintf (buff, "%.0f C (limit = %.0f C, hysteresis = %.0f C, sensor = %s)",
           values[0], values[1], values[2], sensor);
  return fmtExtra (alarm, beep);
}

static const char *
fmtTemps_W8378x_1
(const double values[], int alarm, int beep) {
  int sensorID = (int) values[3];
  const char *sensor = (sensorID == 1) ? "PII/Celeron diode" :
    (sensorID == 2) ? "3904 transistor" : "thermistor";
   /* Is this still right? */
  sprintf (buff, "%.1f C (limit = %.1f C, hysteresis = %.1f C, sensor = %s)",
           values[0], values[1], values[2], sensor);
  return fmtExtra (alarm, beep);
}

static const char *w83782d_names[] = {
  SENSORS_W83782D_PREFIX, SENSORS_W83627HF_PREFIX, NULL
};

static const FeatureDescriptor w83782d_features[] = {
  { fmtVolts_2, W83781D_ALARM_IN0, W83781D_ALARM_IN0,
    { SENSORS_W83782D_IN0, SENSORS_W83782D_IN0_MIN, SENSORS_W83782D_IN0_MAX, -1 } },
  { fmtVolts_2, W83781D_ALARM_IN1, W83781D_ALARM_IN1,
    { SENSORS_W83782D_IN1, SENSORS_W83782D_IN1_MIN, SENSORS_W83782D_IN1_MAX, -1 } },
  { fmtVolts_2, W83781D_ALARM_IN2, W83781D_ALARM_IN2,
    { SENSORS_W83782D_IN2, SENSORS_W83782D_IN2_MIN, SENSORS_W83782D_IN2_MAX, -1 } },
  { fmtVolts_2, W83781D_ALARM_IN3, W83781D_ALARM_IN3,
    { SENSORS_W83782D_IN3, SENSORS_W83782D_IN3_MIN, SENSORS_W83782D_IN3_MAX, -1 } },
  { fmtVolts_2, W83781D_ALARM_IN4, W83781D_ALARM_IN4,
    { SENSORS_W83782D_IN4, SENSORS_W83782D_IN4_MIN, SENSORS_W83782D_IN4_MAX, -1 } },
  { fmtVolts_2, W83781D_ALARM_IN5, W83781D_ALARM_IN5,
    { SENSORS_W83782D_IN5, SENSORS_W83782D_IN5_MIN, SENSORS_W83782D_IN5_MAX, -1 } },
  { fmtVolts_2, W83781D_ALARM_IN6, W83781D_ALARM_IN6,
    { SENSORS_W83782D_IN6, SENSORS_W83782D_IN6_MIN, SENSORS_W83782D_IN6_MAX, -1 } },
  { fmtVolts_2, W83782D_ALARM_IN7, W83782D_ALARM_IN7,
    { SENSORS_W83782D_IN7, SENSORS_W83782D_IN7_MIN, SENSORS_W83782D_IN7_MAX, -1 } },
  { fmtVolts_2, W83782D_ALARM_IN8, W83782D_ALARM_IN8,
    { SENSORS_W83782D_IN8, SENSORS_W83782D_IN8_MIN, SENSORS_W83782D_IN8_MAX, -1 } },
  { fmtFans_0, W83781D_ALARM_FAN1, W83781D_ALARM_FAN1,
    { SENSORS_W83782D_FAN1, SENSORS_W83782D_FAN1_MIN, SENSORS_W83782D_FAN1_DIV, -1 } },
  { fmtFans_0, W83781D_ALARM_FAN2, W83781D_ALARM_FAN2,
    { SENSORS_W83782D_FAN2, SENSORS_W83782D_FAN2_MIN, SENSORS_W83782D_FAN2_DIV, -1 } },
  { fmtFans_0, W83781D_ALARM_FAN3, W83781D_ALARM_FAN3,
    { SENSORS_W83782D_FAN3, SENSORS_W83782D_FAN3_MIN, SENSORS_W83782D_FAN3_DIV, -1 } },
  { fmtTemps_W8378x_0, W83781D_ALARM_TEMP1, W83781D_ALARM_TEMP1,
    { SENSORS_W83782D_TEMP1, SENSORS_W83782D_TEMP1_OVER, SENSORS_W83782D_TEMP1_HYST, SENSORS_W83782D_SENS1, -1 } },
  { fmtTemps_W8378x_1, W83781D_ALARM_TEMP2, W83781D_ALARM_TEMP2,
    { SENSORS_W83782D_TEMP2, SENSORS_W83782D_TEMP2_OVER, SENSORS_W83782D_TEMP2_HYST, SENSORS_W83782D_SENS2, -1 } },
  { fmtTemps_W8378x_1, W83781D_ALARM_TEMP3, W83781D_ALARM_TEMP3,
    { SENSORS_W83782D_TEMP3, SENSORS_W83782D_TEMP3_OVER, SENSORS_W83782D_TEMP3_HYST, SENSORS_W83782D_SENS3, -1 } },
  { fmtVolt_2, 0, 0,
    { SENSORS_W83782D_VID, -1 } },
  { fmtChassisIntrusionDetection, W83781D_ALARM_CHAS, W83781D_ALARM_CHAS,
    { SENSORS_W83781D_ALARMS, -1 } },
  { fmtSoundAlarm, 0, 0,
    { SENSORS_W83781D_BEEP_ENABLE, -1 } },
  { NULL }
};

static const ChipDescriptor w83782d_chip = {
  w83782d_names, w83782d_features, SENSORS_W83782D_ALARMS, SENSORS_W83782D_BEEPS
};

/** W83783S **/

static const char *w83783s_names[] = {
  SENSORS_W83783S_PREFIX, NULL
};

static const FeatureDescriptor w83783s_features[] = {
  { fmtVolts_2, W83781D_ALARM_IN0, W83781D_ALARM_IN0,
    { SENSORS_W83783S_IN0, SENSORS_W83783S_IN0_MIN, SENSORS_W83783S_IN0_MAX, -1 } },
  { fmtVolts_2, W83781D_ALARM_IN2, W83781D_ALARM_IN2,
    { SENSORS_W83783S_IN2, SENSORS_W83783S_IN2_MIN, SENSORS_W83783S_IN2_MAX, -1 } },
  { fmtVolts_2, W83781D_ALARM_IN3, W83781D_ALARM_IN3,
    { SENSORS_W83783S_IN3, SENSORS_W83783S_IN3_MIN, SENSORS_W83783S_IN3_MAX, -1 } },
  { fmtVolts_2, W83781D_ALARM_IN4, W83781D_ALARM_IN4,
    { SENSORS_W83783S_IN4, SENSORS_W83783S_IN4_MIN, SENSORS_W83783S_IN4_MAX, -1 } },
  { fmtVolts_2, W83781D_ALARM_IN5, W83781D_ALARM_IN5,
    { SENSORS_W83783S_IN5, SENSORS_W83783S_IN5_MIN, SENSORS_W83783S_IN5_MAX, -1 } },
  { fmtFans_0, W83781D_ALARM_FAN1, W83781D_ALARM_FAN1,
    { SENSORS_W83783S_FAN1, SENSORS_W83783S_FAN1_MIN, SENSORS_W83783S_FAN1_DIV, -1 } },
  { fmtFans_0, W83781D_ALARM_FAN2, W83781D_ALARM_FAN2,
    { SENSORS_W83783S_FAN2, SENSORS_W83783S_FAN2_MIN, SENSORS_W83783S_FAN2_DIV, -1 } },
  { fmtFans_0, W83781D_ALARM_FAN3, W83781D_ALARM_FAN3,
    { SENSORS_W83783S_FAN3, SENSORS_W83783S_FAN3_MIN, SENSORS_W83783S_FAN3_DIV, -1 } },
  { fmtTemps_W8378x_0, W83781D_ALARM_TEMP1, W83781D_ALARM_TEMP1,
    { SENSORS_W83783S_TEMP1, SENSORS_W83783S_TEMP1_OVER, SENSORS_W83783S_TEMP1_HYST, SENSORS_W83783S_SENS1, -1 } },
  { fmtTemps_W8378x_1, W83781D_ALARM_TEMP2, W83781D_ALARM_TEMP2,
    { SENSORS_W83783S_TEMP2, SENSORS_W83783S_TEMP2_OVER, SENSORS_W83783S_TEMP2_HYST, SENSORS_W83783S_SENS2, -1 } },
  { fmtVolt_2, 0, 0,
    { SENSORS_W83783S_VID, -1 } },
  { fmtChassisIntrusionDetection, W83781D_ALARM_CHAS, W83781D_ALARM_CHAS,
    { SENSORS_W83781D_ALARMS, -1 } },
  { fmtSoundAlarm, 0, 0,
    { SENSORS_W83781D_BEEP_ENABLE, -1 } },
  { NULL }
};

static const ChipDescriptor w83783s_chip = {
  w83783s_names, w83783s_features, SENSORS_W83783S_ALARMS, SENSORS_W83783S_BEEPS
};

/** MAXILIFE **/

static const char *
fmtTemps_Maxilife
(const double values[], int alarm, int beep) {
  if (!values[0] && !values[1] && !values[2])
    return NULL;
  return fmtTemps_1 (values, alarm, beep);
}

static const char *
fmtFans_Maxilife
(const double values[], int alarm, int beep) {
  if (!values[0] && !values[1] && !values[2])
    return NULL;
  if (values[0] < 0) {
    sprintf (buff, "Off (min = %.0f RPM, div = %.0f)", values[1], values[2]); 
  } else {
    sprintf (buff, "%.0f RPM (min = %.0f RPM, div = %.0f)", values[0] / values[2], values[1] / values[2], values[2]); 
  }
  return fmtExtra (alarm, beep);
}

static const char *
fmtMHz_Maxilife
(const double values[], int alarm, int beep) {
  if (!values[0] && !values[1] && !values[2])
    return NULL;
  return fmtMHz_2 (values, alarm, beep);
}

static const char *
fmtVolts_Maxilife
(const double values[], int alarm, int beep) {
  if (!values[0] && !values[1] && !values[2])
    return NULL;
  return fmtVolts_2 (values, alarm, beep);
}

static const char *maxilife_names[] = {
  SENSORS_MAXI_CG_PREFIX, SENSORS_MAXI_CO_PREFIX, SENSORS_MAXI_AS_PREFIX, NULL
};

static const FeatureDescriptor maxilife_features[] = {
  { fmtTemps_Maxilife, 0, 0,
    { SENSORS_MAXI_CG_TEMP1, SENSORS_MAXI_CG_TEMP1_MAX, SENSORS_MAXI_CG_TEMP1_HYST, -1 } },
  { fmtTemps_Maxilife, MAXI_ALARM_TEMP2, 0,
    { SENSORS_MAXI_CG_TEMP2, SENSORS_MAXI_CG_TEMP2_MAX, SENSORS_MAXI_CG_TEMP2_HYST, -1 } },
  { fmtTemps_Maxilife, 0, 0,
    { SENSORS_MAXI_CG_TEMP3, SENSORS_MAXI_CG_TEMP3_MAX, SENSORS_MAXI_CG_TEMP3_HYST, -1 } },
  { fmtTemps_Maxilife, MAXI_ALARM_TEMP4, 0,
    { SENSORS_MAXI_CG_TEMP4, SENSORS_MAXI_CG_TEMP4_MAX, SENSORS_MAXI_CG_TEMP4_HYST, -1 } },
  { fmtTemps_Maxilife, MAXI_ALARM_TEMP5, 0,
    { SENSORS_MAXI_CG_TEMP5, SENSORS_MAXI_CG_TEMP5_MAX, SENSORS_MAXI_CG_TEMP4_HYST, -1 } },
  { fmtFans_Maxilife, MAXI_ALARM_FAN1, 0,
    { SENSORS_MAXI_CG_FAN1, SENSORS_MAXI_CG_FAN1_MIN, SENSORS_MAXI_CG_FAN1_DIV, -1 } },
  { fmtFans_Maxilife, MAXI_ALARM_FAN2, 0,
    { SENSORS_MAXI_CG_FAN2, SENSORS_MAXI_CG_FAN2_MIN, SENSORS_MAXI_CG_FAN2_DIV, -1 } },
  { fmtFans_Maxilife, MAXI_ALARM_FAN3, 0,
    { SENSORS_MAXI_CG_FAN3, SENSORS_MAXI_CG_FAN3_MIN, SENSORS_MAXI_CG_FAN3_DIV, -1 } },
  { fmtMHz_Maxilife, MAXI_ALARM_PLL, 0,
    { SENSORS_MAXI_CG_PLL, SENSORS_MAXI_CG_PLL_MIN, SENSORS_MAXI_CG_PLL_MAX, -1 } },
  { fmtVolts_Maxilife, MAXI_ALARM_VID1, 0,
    { SENSORS_MAXI_CG_VID1, SENSORS_MAXI_CG_VID1_MIN, SENSORS_MAXI_CG_VID1_MAX, -1 } },
  { fmtVolts_Maxilife, MAXI_ALARM_VID2, 0,
    { SENSORS_MAXI_CG_VID2, SENSORS_MAXI_CG_VID2_MIN, SENSORS_MAXI_CG_VID2_MAX, -1 } },
  { fmtVolts_Maxilife, MAXI_ALARM_VID3, 0,
    { SENSORS_MAXI_CG_VID3, SENSORS_MAXI_CG_VID3_MIN, SENSORS_MAXI_CG_VID3_MAX, -1 } },
  { fmtVolts_Maxilife, MAXI_ALARM_VID4, 0,
    { SENSORS_MAXI_CG_VID4, SENSORS_MAXI_CG_VID4_MIN, SENSORS_MAXI_CG_VID4_MAX, -1 } },
  { NULL }
};

static const ChipDescriptor maxilife_chip = {
  maxilife_names, maxilife_features, SENSORS_MAXI_CG_ALARMS, 0
};

/** EEPROM **/

static const char *
fmtType_EEPROM
(const double values[], int alarm, int beep) {
  if ((int) values[0] == 4)
    sprintf (buff, "SDRAM DIMM SPD");
  else
    sprintf (buff, "Invalid"); /* N.B: sensors just returns, aborting further tests; I don't.. */
  return fmtExtra (alarm, beep);
}

static const char *
fmtRowCol_EEPROM
(const double values[], int alarm, int beep) {
  int row = (int) values[0];
  int col = (int) values[1];
  int num = (int) values[2];
  int foo = (row & 0xf) + (col & 0xf) + num - 16;
  if ((foo > 0) && (foo <= 10))
    sprintf (buff, "%d", 1 << foo);
  else
    sprintf (buff, "Invalid");
  return fmtExtra (alarm, beep);
}

static const char *eeprom_names[] = {
  SENSORS_EEPROM_PREFIX, NULL
};

static const FeatureDescriptor eeprom_features[] = {
  { fmtType_EEPROM, 0, 0,
    { SENSORS_EEPROM_TYPE, -1 } },
  { fmtRowCol_EEPROM, 0, 0,
    { SENSORS_EEPROM_ROWADDR, SENSORS_EEPROM_COLADDR, SENSORS_EEPROM_NUMROWS, -1 } },
  { NULL }
};

static const ChipDescriptor eeprom_chip = {
  eeprom_names, eeprom_features, 0, 0
};

/** DDCMON **/

static const char *
fmtID_DDCMON
(const double values[], int alarm, int beep) {
  int value = (int) values[0];
  buff[0] = ((value >> 10) & 0x1f) + '@';
  buff[1] = ((value >> 5) & 0x1f) + '@';
  buff[2] = (value & 0x1f) + '@';
  buff[3] = ((value >> 20) & 0xf) + '0';
  buff[4] = ((value >> 16) & 0xf) + '0';
  buff[5] = ((value >> 28) & 0xf) + '0';
  buff[6] = ((value >> 24) & 0xf) + '0';
  buff[7] = '\0';
  return fmtExtra (alarm, beep);
}

static const char *ddcmon_names[] = {
  SENSORS_DDCMON_PREFIX, NULL
};

static const FeatureDescriptor ddcmon_features[] = {
  { fmtID_DDCMON, 0, 0,
    { SENSORS_DDCMON_ID, -1 } },
  { fmtInt, 0, 0,
    { SENSORS_DDCMON_SERIAL, -1 } },
  { fmtIntCrossInt, 0, 0,
    { SENSORS_DDCMON_VERSIZE, SENSORS_DDCMON_HORSIZE, -1 } },
  { fmtIntDashInt, 0, 0,
    { SENSORS_DDCMON_VERSYNCMIN, SENSORS_DDCMON_VERSYNCMAX, -1 } },
  { fmtIntDashInt, 0, 0,
    { SENSORS_DDCMON_HORSYNCMIN, SENSORS_DDCMON_HORSYNCMAX, -1 } },
  { NULL }
};

static const ChipDescriptor ddcmon_chip = {
  ddcmon_names, ddcmon_features, 0, 0
};

/** ALL **/

const ChipDescriptor * const knownChips[] = {
  &adm1021_chip,
  &adm1025_chip,
  &adm9240_chip,
  &ddcmon_chip,
  &eeprom_chip,
  &gl518_chip,
  &lm75_chip,
  &lm78_chip,
  &lm80_chip,
  &max1617_chip,
  &maxilife_chip,
  &sis5595_chip,
  &via686a_chip,
  &w83781d_chip,
  &w83782d_chip,
  &w83783s_chip,
  NULL
};
