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
#include "lib/chips.h"
#include "kernel/include/sensors.h"

/* NB:
 *
 * The following chips from prog/sensors are not (yet) supported:
 *
 * lm87 mtp008 fscpos fscscy pcf8591 vt1211 smsc47m1 lm92 adm1024 lm83
 */

/* NB: missing from sensors (and this) but in lib/chips.h:
 * "gl520sm", "thmc50", "adm1022" */

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
fmtTemps_1_0
(const double values[], int alarm, int beep) {
  sprintf (buff, "%.1f C (limit = %.0f C, hysteresis = %.0f C)", values[0], values[1], values[2]);
  return fmtExtra (alarm, beep);
}

static const char *
fmtTemps_1
(const double values[], int alarm, int beep) {
  sprintf (buff, "%.1f C (limit = %.1f C, hysteresis = %.1f C)", values[0], values[1], values[2]);
  return fmtExtra (alarm, beep);
}

static const char *
fmtTemps_minmax_0
(const double values[], int alarm, int beep) {
 sprintf (buff, "%.0f C (min = %.0f C, max = %.0f C)", values[0], values[1], values[2]);
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

/** DS1621 */

static const char *
fmtTemps_DS1621
(const double values[], int alarm, int beep) {
  sprintf (buff, "%.2f C (min = %.1f C, max = %.1f C)", values[0], values[1], values[2]);
  if (alarm)
    sprintf (buff + strlen (buff), " [ALARM(%s)]", (alarm == DS1621_ALARM_TEMP_LOW) ? "LOW" : (alarm == DS1621_ALARM_TEMP_HIGH) ? "HIGH" : "LOW,HIGH");
  return buff;
}

static const char *ds1621_names[] = {
  SENSORS_DS1621_PREFIX, NULL
};

static const FeatureDescriptor ds1621_features[] = {
  { fmtTemps_DS1621, rrdF2, DataType_temperature, DS1621_ALARM_TEMP_LOW | DS1621_ALARM_TEMP_HIGH, 0,
    { SENSORS_DS1621_TEMP, SENSORS_DS1621_TEMP_HYST, SENSORS_DS1621_TEMP_OVER, -1 } }, /* hyst=min, over=max */
  { NULL }
};

static const ChipDescriptor ds1621_chip = {
  ds1621_names, ds1621_features, SENSORS_DS1621_ALARMS, 0
};

/** LM75 **/

static const char *lm75_names[] = {
  SENSORS_LM75_PREFIX, NULL
};

static const FeatureDescriptor lm75_features[] = {
  { fmtTemps_1, rrdF1, DataType_temperature, 0, 0,
    { SENSORS_LM75_TEMP, SENSORS_LM75_TEMP_OVER, SENSORS_LM75_TEMP_HYST, -1 } },
  { NULL }
};

static const ChipDescriptor lm75_chip = {
  lm75_names, lm75_features, 0, 0
};

/** ADM1021 **/

static const char *
fmtTemps_ADM1021_0
(const double values[], int alarm, int beep) {
  sprintf (buff, "%.0f C (min = %.0f C, max = %.0f C)", values[0], values[1], values[2]);
  if (alarm) {
    int low = alarm & ADM1021_ALARM_TEMP_LOW, high = alarm & ADM1021_ALARM_TEMP_HIGH;
    sprintf (buff + strlen (buff), " [ALARM(%s%s%s)]", low ? "LOW" : "", (low && high) ? "," : "", high ? "HIGH" : "");
  }
  return buff;
}

static const char *
fmtTemps_ADM1021_1
(const double values[], int alarm, int beep) {
  sprintf (buff, "%.0f C (min = %.0f C, max = %.0f C)", values[0], values[1], values[2]);
  if (alarm) {
    int na = alarm & ADM1021_ALARM_RTEMP_NA, low = alarm & ADM1021_ALARM_RTEMP_LOW,
      high = alarm & ADM1021_ALARM_RTEMP_HIGH;
    sprintf (buff + strlen (buff), " [ALARM(%s%s%s%s%s)]", na ? "N/A" : "", (na && (low || high)) ? "," : "", low ? "LOW" : "", (low && high) ? "," : "", high ? "HIGH" : "");
  }
  return buff;
}

static const char *adm1021_names[] = {
  SENSORS_ADM1021_PREFIX, SENSORS_ADM1023_PREFIX, NULL
}; /* N.B: Assume bs sensors 1023 is =~ 1021 */

static const FeatureDescriptor adm1021_features[] = {
  { fmtTemps_ADM1021_0, rrdF0, DataType_temperature, ADM1021_ALARM_TEMP_HIGH | ADM1021_ALARM_TEMP_LOW, 0,
    { SENSORS_ADM1021_TEMP, SENSORS_ADM1021_TEMP_HYST, SENSORS_ADM1021_TEMP_OVER, -1 } }, /* hyst=min, over=max */
  { fmtTemps_ADM1021_1, rrdF0, DataType_temperature, ADM1021_ALARM_RTEMP_HIGH | ADM1021_ALARM_RTEMP_LOW | ADM1021_ALARM_RTEMP_NA, 0,
    { SENSORS_ADM1021_REMOTE_TEMP, SENSORS_ADM1021_REMOTE_TEMP_HYST, SENSORS_ADM1021_REMOTE_TEMP_OVER, -1 } }, /* hyst=min, over=max */
  { fmtValu_0, NULL, DataType_other, 0, 0,
    { SENSORS_ADM1021_DIE_CODE, -1 } },
  { NULL }
};

static const ChipDescriptor adm1021_chip = {
  adm1021_names, adm1021_features, SENSORS_ADM1021_ALARMS, 0
};

/** MAX1617 **/

static const char *max1617_names[] = {
  SENSORS_MAX1617_PREFIX, SENSORS_MAX1617A_PREFIX, SENSORS_THMC10_PREFIX, SENSORS_LM84_PREFIX, SENSORS_GL523_PREFIX, NULL
}; /* N.B: Assume vs sensors these have no die code */

static const FeatureDescriptor max1617_features[] = {
  { fmtTemps_ADM1021_0, rrdF0, DataType_temperature, ADM1021_ALARM_TEMP_HIGH | ADM1021_ALARM_TEMP_LOW, 0,
    { SENSORS_MAX1617_TEMP, SENSORS_MAX1617_TEMP_HYST, SENSORS_MAX1617_TEMP_OVER, -1 } }, /* hyst=min, over=max */
  { fmtTemps_ADM1021_1, rrdF0, DataType_temperature, ADM1021_ALARM_RTEMP_HIGH | ADM1021_ALARM_RTEMP_LOW | ADM1021_ALARM_RTEMP_NA, 0,
    { SENSORS_MAX1617_REMOTE_TEMP, SENSORS_MAX1617_REMOTE_TEMP_HYST, SENSORS_MAX1617_REMOTE_TEMP_OVER, -1 } }, /* hyst=min, over=max */
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
  { fmtVolts_2, rrdF2, DataType_voltage, ADM9240_ALARM_IN0, 0,
    { SENSORS_ADM9240_IN0, SENSORS_ADM9240_IN0_MIN, SENSORS_ADM9240_IN0_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, ADM9240_ALARM_IN1, 0,
    { SENSORS_ADM9240_IN1, SENSORS_ADM9240_IN1_MIN, SENSORS_ADM9240_IN1_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, ADM9240_ALARM_IN2, 0,
    { SENSORS_ADM9240_IN2, SENSORS_ADM9240_IN2_MIN, SENSORS_ADM9240_IN2_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, ADM9240_ALARM_IN3, 0,
    { SENSORS_ADM9240_IN3, SENSORS_ADM9240_IN3_MIN, SENSORS_ADM9240_IN3_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, ADM9240_ALARM_IN4, 0,
    { SENSORS_ADM9240_IN4, SENSORS_ADM9240_IN4_MIN, SENSORS_ADM9240_IN4_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, ADM9240_ALARM_IN5, 0,
    { SENSORS_ADM9240_IN5, SENSORS_ADM9240_IN5_MIN, SENSORS_ADM9240_IN5_MAX, -1 } },
  { fmtFans_0, rrdF0, DataType_rpm, ADM9240_ALARM_FAN1, 0,
    { SENSORS_ADM9240_FAN1, SENSORS_ADM9240_FAN1_MIN, SENSORS_ADM9240_FAN1_DIV, -1 } },
  { fmtFans_0, rrdF0, DataType_rpm, ADM9240_ALARM_FAN2, 0,
    { SENSORS_ADM9240_FAN2, SENSORS_ADM9240_FAN2_MIN, SENSORS_ADM9240_FAN2_DIV, -1 } },
  { fmtTemps_1_0, rrdF1, DataType_temperature, ADM9240_ALARM_TEMP, 0,
    { SENSORS_ADM9240_TEMP, SENSORS_ADM9240_TEMP_OVER, SENSORS_ADM9240_TEMP_HYST, -1 } },
  { fmtVolt_2, rrdF2, DataType_voltage, 0, 0,
    { SENSORS_ADM9240_VID, -1 } },
  { fmtChassisIntrusionDetection, NULL, DataType_other, ADM9240_ALARM_CHAS, 0,
    { SENSORS_ADM9240_ALARMS, -1 } },
  { NULL }
};

static const ChipDescriptor adm9240_chip = {
  adm9240_names, adm9240_features, SENSORS_ADM9240_ALARMS, 0
};

/** SIS5595 **/

static const char *sis5595_names[] = {
  SENSORS_SIS5595_PREFIX, NULL
};

static const FeatureDescriptor sis5595_features[] = {
  { fmtVolts_2, rrdF2, DataType_voltage, SIS5595_ALARM_IN0, 0,
    { SENSORS_SIS5595_IN0, SENSORS_SIS5595_IN0_MIN, SENSORS_SIS5595_IN0_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, SIS5595_ALARM_IN1, 0,
    { SENSORS_SIS5595_IN1, SENSORS_SIS5595_IN1_MIN, SENSORS_SIS5595_IN1_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, SIS5595_ALARM_IN2, 0,
    { SENSORS_SIS5595_IN2, SENSORS_SIS5595_IN2_MIN, SENSORS_SIS5595_IN2_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, SIS5595_ALARM_IN3, 0,
    { SENSORS_SIS5595_IN3, SENSORS_SIS5595_IN3_MIN, SENSORS_SIS5595_IN3_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, SIS5595_ALARM_IN4, 0,
    { SENSORS_SIS5595_IN4, SENSORS_SIS5595_IN4_MIN, SENSORS_SIS5595_IN4_MAX, -1 } },
  { fmtFans_0, rrdF0, DataType_rpm, SIS5595_ALARM_FAN1, 0,
    { SENSORS_SIS5595_FAN1, SENSORS_SIS5595_FAN1_MIN, SENSORS_SIS5595_FAN1_DIV, -1 } },
  { fmtFans_0, rrdF0, DataType_rpm, SIS5595_ALARM_FAN2, 0,
    { SENSORS_SIS5595_FAN2, SENSORS_SIS5595_FAN2_MIN, SENSORS_SIS5595_FAN2_DIV, -1 } },
  { fmtTemps_0, rrdF0, DataType_temperature, SIS5595_ALARM_TEMP, 0,
    { SENSORS_SIS5595_TEMP, SENSORS_SIS5595_TEMP_OVER, SENSORS_SIS5595_TEMP_HYST, -1 } },
  { fmtBoardTemperatureInput, NULL, DataType_other, SIS5595_ALARM_BTI, 0,
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
  { fmtVolts_2, rrdF2, DataType_voltage, VIA686A_ALARM_IN0, 0,
    { SENSORS_VIA686A_IN0, SENSORS_VIA686A_IN0_MIN, SENSORS_VIA686A_IN0_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, VIA686A_ALARM_IN1, 0,
    { SENSORS_VIA686A_IN1, SENSORS_VIA686A_IN1_MIN, SENSORS_VIA686A_IN1_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, VIA686A_ALARM_IN2, 0,
    { SENSORS_VIA686A_IN2, SENSORS_VIA686A_IN2_MIN, SENSORS_VIA686A_IN2_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, VIA686A_ALARM_IN3, 0,
    { SENSORS_VIA686A_IN3, SENSORS_VIA686A_IN3_MIN, SENSORS_VIA686A_IN3_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, VIA686A_ALARM_IN4, 0,
    { SENSORS_VIA686A_IN4, SENSORS_VIA686A_IN4_MIN, SENSORS_VIA686A_IN4_MAX, -1 } },
  { fmtFans_0, rrdF0, DataType_rpm, VIA686A_ALARM_FAN1, 0,
    { SENSORS_VIA686A_FAN1, SENSORS_VIA686A_FAN1_MIN, SENSORS_VIA686A_FAN1_DIV, -1 } },
  { fmtFans_0, rrdF0, DataType_rpm, VIA686A_ALARM_FAN2, 0,
    { SENSORS_VIA686A_FAN2, SENSORS_VIA686A_FAN2_MIN, SENSORS_VIA686A_FAN2_DIV, -1 } },
  { fmtTemps_1_0, rrdF1, DataType_temperature, VIA686A_ALARM_TEMP, 0,
    { SENSORS_VIA686A_TEMP, SENSORS_VIA686A_TEMP_OVER, SENSORS_VIA686A_TEMP_HYST, -1 } },
  { fmtTemps_1_0, rrdF1, DataType_temperature, VIA686A_ALARM_TEMP2, 0,
    { SENSORS_VIA686A_TEMP2, SENSORS_VIA686A_TEMP2_OVER, SENSORS_VIA686A_TEMP2_HYST, -1 } },
  { fmtTemps_1_0, rrdF1, DataType_temperature, VIA686A_ALARM_TEMP3, 0,
    { SENSORS_VIA686A_TEMP3, SENSORS_VIA686A_TEMP3_OVER, SENSORS_VIA686A_TEMP3_HYST, -1 } },
  { NULL }
};

static const ChipDescriptor via686a_chip = {
  via686a_names, via686a_features, SENSORS_VIA686A_ALARMS, 0
};

/** LM78 **/

static const char *lm78_names[] = {
  SENSORS_LM78_PREFIX, SENSORS_LM78J_PREFIX, SENSORS_LM79_PREFIX, NULL
};

static const FeatureDescriptor lm78_features[] = {
  { fmtVolts_2, rrdF2, DataType_voltage, LM78_ALARM_IN0, 0,
    { SENSORS_LM78_IN0, SENSORS_LM78_IN0_MIN, SENSORS_LM78_IN0_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, LM78_ALARM_IN1, 0,
    { SENSORS_LM78_IN1, SENSORS_LM78_IN1_MIN, SENSORS_LM78_IN1_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, LM78_ALARM_IN2, 0,
    { SENSORS_LM78_IN2, SENSORS_LM78_IN2_MIN, SENSORS_LM78_IN2_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, LM78_ALARM_IN3, 0,
    { SENSORS_LM78_IN3, SENSORS_LM78_IN3_MIN, SENSORS_LM78_IN3_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, LM78_ALARM_IN4, 0,
    { SENSORS_LM78_IN4, SENSORS_LM78_IN4_MIN, SENSORS_LM78_IN4_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, LM78_ALARM_IN5, 0,
    { SENSORS_LM78_IN5, SENSORS_LM78_IN5_MIN, SENSORS_LM78_IN5_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, LM78_ALARM_IN6, 0,
    { SENSORS_LM78_IN6, SENSORS_LM78_IN6_MIN, SENSORS_LM78_IN6_MAX, -1 } },
  { fmtFans_0, rrdF0, DataType_rpm, LM78_ALARM_FAN1, 0,
    { SENSORS_LM78_FAN1, SENSORS_LM78_FAN1_MIN, SENSORS_LM78_FAN1_DIV, -1 } },
  { fmtFans_0, rrdF0, DataType_rpm, LM78_ALARM_FAN2, 0,
    { SENSORS_LM78_FAN2, SENSORS_LM78_FAN2_MIN, SENSORS_LM78_FAN2_DIV, -1 } },
  { fmtFans_0, rrdF0, DataType_rpm, LM78_ALARM_FAN3, 0,
    { SENSORS_LM78_FAN3, SENSORS_LM78_FAN3_MIN, SENSORS_LM78_FAN3_DIV, -1 } },
  { fmtTemps_1_0, rrdF1, DataType_rpm, LM78_ALARM_TEMP, 0,
    { SENSORS_LM78_TEMP, SENSORS_LM78_TEMP_OVER, SENSORS_LM78_TEMP_HYST, -1 } },
  { fmtVolt_2, rrdF2, DataType_voltage, 0, 0,
    { SENSORS_LM78_VID, -1 } },
  { fmtBoardTemperatureInput, NULL, DataType_other, LM78_ALARM_BTI, 0,
    { SENSORS_LM78_ALARMS, -1 } },
  { fmtChassisIntrusionDetection, NULL, DataType_other, LM78_ALARM_CHAS, 0,
    { SENSORS_LM78_ALARMS, -1 } },
  { NULL }
};

static const ChipDescriptor lm78_chip = {
  lm78_names, lm78_features, SENSORS_LM78_ALARMS, 0
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
  { fmtVolts_2, rrdF2, DataType_voltage, GL518_ALARM_VDD, GL518_ALARM_VDD,
    { SENSORS_GL518_VDD, SENSORS_GL518_VDD_MIN, SENSORS_GL518_VDD_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, GL518_ALARM_VIN1, GL518_ALARM_VIN1,
    { SENSORS_GL518_VIN1, SENSORS_GL518_VIN1_MIN, SENSORS_GL518_VIN1_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, GL518_ALARM_VIN2, GL518_ALARM_VIN2,
    { SENSORS_GL518_VIN2, SENSORS_GL518_VIN2_MIN, SENSORS_GL518_VIN2_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, GL518_ALARM_VIN3, GL518_ALARM_VIN3,
    { SENSORS_GL518_VIN3, SENSORS_GL518_VIN3_MIN, SENSORS_GL518_VIN3_MAX, -1 } },
  { fmtFans_0, rrdF0, DataType_rpm, GL518_ALARM_FAN1, GL518_ALARM_FAN1,
    { SENSORS_GL518_FAN1, SENSORS_GL518_FAN1_MIN, SENSORS_GL518_FAN1_DIV, -1 } },
  { fmtFans_0, rrdF0, DataType_rpm, GL518_ALARM_FAN2, GL518_ALARM_FAN2,
    { SENSORS_GL518_FAN2, SENSORS_GL518_FAN2_MIN, SENSORS_GL518_FAN2_DIV, -1 } },
  { fmtTemps_1_0, rrdF1, DataType_temperature, GL518_ALARM_TEMP, GL518_ALARM_TEMP,
    { SENSORS_GL518_TEMP, SENSORS_GL518_TEMP_OVER, SENSORS_GL518_TEMP_HYST, -1 } },
  { fmtSoundAlarm, NULL, DataType_other, 0, 0,
    { SENSORS_GL518_BEEP_ENABLE, -1 } },
  { NULL }
};

static const ChipDescriptor gl518_chip = {
  gl518_names, gl518_features, SENSORS_GL518_ALARMS, SENSORS_GL518_BEEPS
};

/** ADM1025 **/

static const char *
fmtTemps_ADM1025
(const double values[], int alarm, int beep) {
  sprintf (buff, "%.1f C (min = %.0f C, max = %.0f C)", values[0], values[1], values[2]);
  return fmtExtra (alarm, beep);
}

static const char *adm1025_names[] = {
  SENSORS_ADM1025_PREFIX, NULL
};

static const FeatureDescriptor adm1025_features[] = {
  { fmtVolts_2, rrdF2, DataType_voltage, ADM1025_ALARM_IN0, 0,
    { SENSORS_ADM1025_IN0, SENSORS_ADM1025_IN0_MIN, SENSORS_ADM1025_IN0_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, ADM1025_ALARM_IN1, 0,
    { SENSORS_ADM1025_IN1, SENSORS_ADM1025_IN1_MIN, SENSORS_ADM1025_IN1_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, ADM1025_ALARM_IN2, 0,
    { SENSORS_ADM1025_IN2, SENSORS_ADM1025_IN2_MIN, SENSORS_ADM1025_IN2_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, ADM1025_ALARM_IN3, 0,
    { SENSORS_ADM1025_IN3, SENSORS_ADM1025_IN3_MIN, SENSORS_ADM1025_IN3_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, ADM1025_ALARM_IN4, 0,
    { SENSORS_ADM1025_IN4, SENSORS_ADM1025_IN4_MIN, SENSORS_ADM1025_IN4_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, ADM1025_ALARM_IN5, 0,
    { SENSORS_ADM1025_IN5, SENSORS_ADM1025_IN5_MIN, SENSORS_ADM1025_IN5_MAX, -1 } },
  { fmtTemps_ADM1025, rrdF1, DataType_temperature, ADM1025_ALARM_TEMP, 0,
    { SENSORS_ADM1025_TEMP1, SENSORS_ADM1025_TEMP1_LOW, SENSORS_ADM1025_TEMP1_HIGH, -1 } },
  { fmtTemps_ADM1025, rrdF1, DataType_temperature, ADM1025_ALARM_RTEMP, 0,
    { SENSORS_ADM1025_TEMP2, SENSORS_ADM1025_TEMP2_LOW, SENSORS_ADM1025_TEMP2_HIGH, -1 } },
  { NULL }
};

static const ChipDescriptor adm1025_chip = {
  adm1025_names, adm1025_features, SENSORS_ADM1025_ALARMS, 0
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
  { fmtVolts_2, rrdF2, DataType_voltage, LM80_ALARM_IN0, 0,
    { SENSORS_LM80_IN0, SENSORS_LM80_IN0_MIN, SENSORS_LM80_IN0_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, LM80_ALARM_IN1, 0,
    { SENSORS_LM80_IN1, SENSORS_LM80_IN1_MIN, SENSORS_LM80_IN1_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, LM80_ALARM_IN2, 0,
    { SENSORS_LM80_IN2, SENSORS_LM80_IN2_MIN, SENSORS_LM80_IN2_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, LM80_ALARM_IN3, 0,
    { SENSORS_LM80_IN3, SENSORS_LM80_IN3_MIN, SENSORS_LM80_IN3_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, LM80_ALARM_IN4, 0,
    { SENSORS_LM80_IN4, SENSORS_LM80_IN4_MIN, SENSORS_LM80_IN4_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, LM80_ALARM_IN5, 0,
    { SENSORS_LM80_IN5, SENSORS_LM80_IN5_MIN, SENSORS_LM80_IN5_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, LM80_ALARM_IN6, 0,
    { SENSORS_LM80_IN6, SENSORS_LM80_IN6_MIN, SENSORS_LM80_IN6_MAX, -1 } },
  { fmtFans_0, rrdF0, DataType_rpm, LM80_ALARM_FAN1, 0,
    { SENSORS_LM80_FAN1, SENSORS_LM80_FAN1_MIN, SENSORS_LM80_FAN1_DIV, -1 } },
  { fmtFans_0, rrdF0, DataType_rpm, LM80_ALARM_FAN2, 0,
    { SENSORS_LM80_FAN2, SENSORS_LM80_FAN2_MIN, SENSORS_LM80_FAN2_DIV, -1 } },
  { fmtTemps_LM80, rrdF2, DataType_temperature, LM80_ALARM_TEMP_HOT, 0,
    { SENSORS_LM80_TEMP, SENSORS_LM80_TEMP_HOT_MAX, SENSORS_LM80_TEMP_HOT_HYST, SENSORS_LM80_TEMP_OS_MAX, SENSORS_LM80_TEMP_OS_HYST, -1 } },
  { fmtBoardTemperatureInput, NULL, DataType_other, LM80_ALARM_BTI, 0,
    { SENSORS_LM80_ALARMS, -1 } },
  { fmtChassisIntrusionDetection, NULL, DataType_other, LM80_ALARM_CHAS, 0,
    { SENSORS_LM80_ALARMS, -1 } },
  { NULL }
};

static const ChipDescriptor lm80_chip = {
  lm80_names, lm80_features, SENSORS_LM80_ALARMS, 0
};

/** IT87 (thanks to Mike Black) **/

static const char *it87_names[] = {
  SENSORS_IT87_PREFIX, SENSORS_IT8712_PREFIX, NULL
};

static const FeatureDescriptor it87_features[] = {
  { fmtVolts_2, rrdF2, DataType_voltage, IT87_ALARM_IN0, 0,
    { SENSORS_IT87_IN0, SENSORS_IT87_IN0_MIN, SENSORS_IT87_IN0_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, IT87_ALARM_IN1, 0,
    { SENSORS_IT87_IN1, SENSORS_IT87_IN1_MIN, SENSORS_IT87_IN1_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, IT87_ALARM_IN2, 0,
    { SENSORS_IT87_IN2, SENSORS_IT87_IN2_MIN, SENSORS_IT87_IN2_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, IT87_ALARM_IN3, 0,
    { SENSORS_IT87_IN3, SENSORS_IT87_IN3_MIN, SENSORS_IT87_IN3_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, IT87_ALARM_IN4, 0,
    { SENSORS_IT87_IN4, SENSORS_IT87_IN4_MIN, SENSORS_IT87_IN4_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, IT87_ALARM_IN5, 0,
    { SENSORS_IT87_IN5, SENSORS_IT87_IN5_MIN, SENSORS_IT87_IN5_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, IT87_ALARM_IN6, 0,
    { SENSORS_IT87_IN6, SENSORS_IT87_IN6_MIN, SENSORS_IT87_IN6_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, IT87_ALARM_IN7, 0,
    { SENSORS_IT87_IN7, SENSORS_IT87_IN7_MIN, SENSORS_IT87_IN7_MAX, -1 } },
  { fmtFans_0, rrdF0, DataType_rpm, IT87_ALARM_FAN1, 0,
    { SENSORS_IT87_FAN1, SENSORS_IT87_FAN1_MIN, SENSORS_IT87_FAN1_DIV, -1 } },
  { fmtFans_0, rrdF0, DataType_rpm, IT87_ALARM_FAN2, 0,
    { SENSORS_IT87_FAN2, SENSORS_IT87_FAN2_MIN, SENSORS_IT87_FAN2_DIV, -1 } },
  { fmtFans_0, rrdF0, DataType_rpm, IT87_ALARM_FAN3, 0,
    { SENSORS_IT87_FAN3, SENSORS_IT87_FAN3_MIN, SENSORS_IT87_FAN3_DIV, -1 } },
  { fmtTemps_minmax_0, rrdF1, DataType_temperature, IT87_ALARM_TEMP1, 0,
    { SENSORS_IT87_TEMP1, SENSORS_IT87_TEMP1_LOW, SENSORS_IT87_TEMP1_HIGH, -1 } },
  { fmtTemps_minmax_0, rrdF1, DataType_temperature, IT87_ALARM_TEMP2, 0,
    { SENSORS_IT87_TEMP2, SENSORS_IT87_TEMP2_LOW, SENSORS_IT87_TEMP2_HIGH, -1 } },
  { fmtTemps_minmax_0, rrdF1, DataType_temperature, IT87_ALARM_TEMP3, 0,
    { SENSORS_IT87_TEMP3, SENSORS_IT87_TEMP3_LOW, SENSORS_IT87_TEMP3_HIGH, -1 } },
  { NULL }
};

static const ChipDescriptor it87_chip = {
  it87_names, it87_features, SENSORS_IT87_ALARMS, 0
};

/** W83781D **/

static const char *
fmtTemps_W83781D
(const double values[], int alarm, int beep) {
  if (values[2] == 127) {
    sprintf (buff, "%.0f C (limit = %.0f C)",
             values[0], values[1]);
  } else {
    sprintf (buff, "%.0f C (limit = %.0f C, hysteresis = %.0f C)",
             values[0], values[1], values[2]);
  }
  return fmtExtra (alarm, beep);
}

static const char *w83781d_names[] = {
  SENSORS_W83781D_PREFIX, NULL
};

static const FeatureDescriptor w83781d_features[] = {
  { fmtVolts_2, rrdF2, DataType_voltage, W83781D_ALARM_IN0, W83781D_ALARM_IN0,
    { SENSORS_W83781D_IN0, SENSORS_W83781D_IN0_MIN, SENSORS_W83781D_IN0_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, W83781D_ALARM_IN1, W83781D_ALARM_IN1,
    { SENSORS_W83781D_IN1, SENSORS_W83781D_IN1_MIN, SENSORS_W83781D_IN1_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, W83781D_ALARM_IN2, W83781D_ALARM_IN2,
    { SENSORS_W83781D_IN2, SENSORS_W83781D_IN2_MIN, SENSORS_W83781D_IN2_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, W83781D_ALARM_IN3, W83781D_ALARM_IN3,
    { SENSORS_W83781D_IN3, SENSORS_W83781D_IN3_MIN, SENSORS_W83781D_IN3_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, W83781D_ALARM_IN4, W83781D_ALARM_IN4,
    { SENSORS_W83781D_IN4, SENSORS_W83781D_IN4_MIN, SENSORS_W83781D_IN4_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, W83781D_ALARM_IN5, W83781D_ALARM_IN5,
    { SENSORS_W83781D_IN5, SENSORS_W83781D_IN5_MIN, SENSORS_W83781D_IN5_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, W83781D_ALARM_IN6, W83781D_ALARM_IN6,
    { SENSORS_W83781D_IN6, SENSORS_W83781D_IN6_MIN, SENSORS_W83781D_IN6_MAX, -1 } },
  { fmtFans_0, rrdF0, DataType_rpm, W83781D_ALARM_FAN1, W83781D_ALARM_FAN1,
    { SENSORS_W83781D_FAN1, SENSORS_W83781D_FAN1_MIN, SENSORS_W83781D_FAN1_DIV, -1 } },
  { fmtFans_0, rrdF0, DataType_rpm, W83781D_ALARM_FAN2, W83781D_ALARM_FAN2,
    { SENSORS_W83781D_FAN2, SENSORS_W83781D_FAN2_MIN, SENSORS_W83781D_FAN2_DIV, -1 } },
  { fmtFans_0, rrdF0, DataType_rpm, W83781D_ALARM_FAN3, W83781D_ALARM_FAN3,
    { SENSORS_W83781D_FAN3, SENSORS_W83781D_FAN3_MIN, SENSORS_W83781D_FAN3_DIV, -1 } },
  { fmtTemps_W83781D, rrdF0, DataType_temperature, W83781D_ALARM_TEMP1, W83781D_ALARM_TEMP1,
    { SENSORS_W83781D_TEMP1, SENSORS_W83781D_TEMP1_OVER, SENSORS_W83781D_TEMP1_HYST, -1 } },
  { fmtTemps_1_0, rrdF1, DataType_temperature, W83781D_ALARM_TEMP23, W83781D_ALARM_TEMP23,
    { SENSORS_W83781D_TEMP2, SENSORS_W83781D_TEMP2_OVER, SENSORS_W83781D_TEMP2_HYST, -1 } },
  { fmtTemps_1_0, rrdF1, DataType_temperature, W83781D_ALARM_TEMP23, W83781D_ALARM_TEMP23,
    { SENSORS_W83781D_TEMP3, SENSORS_W83781D_TEMP3_OVER, SENSORS_W83781D_TEMP3_HYST, -1 } },
  { fmtVolt_3, rrdF3, DataType_voltage, 0, 0,
    { SENSORS_W83781D_VID, -1 } },
  { fmtChassisIntrusionDetection, NULL, DataType_other, W83781D_ALARM_CHAS, W83781D_ALARM_CHAS,
    { SENSORS_W83781D_ALARMS, -1 } },
  { fmtSoundAlarm, NULL, DataType_other, 0, 0,
    { SENSORS_W83781D_BEEP_ENABLE, -1 } },
  { NULL }
};

static const ChipDescriptor w83781d_chip = {
  w83781d_names, w83781d_features, SENSORS_W83781D_ALARMS, SENSORS_W83781D_BEEPS
};

/** AS99127F **/

static const char *as99127f_names[] = {
  SENSORS_AS99127F_PREFIX, NULL
};

static const FeatureDescriptor as99127f_features[] = {
  { fmtVolts_2, rrdF2, DataType_voltage, W83781D_ALARM_IN0, W83781D_ALARM_IN0,
    { SENSORS_W83781D_IN0, SENSORS_W83781D_IN0_MIN, SENSORS_W83781D_IN0_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, W83781D_ALARM_IN1, W83781D_ALARM_IN1,
    { SENSORS_W83781D_IN1, SENSORS_W83781D_IN1_MIN, SENSORS_W83781D_IN1_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, W83781D_ALARM_IN2, W83781D_ALARM_IN2,
    { SENSORS_W83781D_IN2, SENSORS_W83781D_IN2_MIN, SENSORS_W83781D_IN2_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, W83781D_ALARM_IN3, W83781D_ALARM_IN3,
    { SENSORS_W83781D_IN3, SENSORS_W83781D_IN3_MIN, SENSORS_W83781D_IN3_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, W83781D_ALARM_IN4, W83781D_ALARM_IN4,
    { SENSORS_W83781D_IN4, SENSORS_W83781D_IN4_MIN, SENSORS_W83781D_IN4_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, W83781D_ALARM_IN5, W83781D_ALARM_IN5,
    { SENSORS_W83781D_IN5, SENSORS_W83781D_IN5_MIN, SENSORS_W83781D_IN5_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, W83781D_ALARM_IN6, W83781D_ALARM_IN6,
    { SENSORS_W83781D_IN6, SENSORS_W83781D_IN6_MIN, SENSORS_W83781D_IN6_MAX, -1 } },
  { fmtFans_0, rrdF0, DataType_rpm, W83781D_ALARM_FAN1, W83781D_ALARM_FAN1,
    { SENSORS_W83781D_FAN1, SENSORS_W83781D_FAN1_MIN, SENSORS_W83781D_FAN1_DIV, -1 } },
  { fmtFans_0, rrdF0, DataType_rpm, W83781D_ALARM_FAN2, W83781D_ALARM_FAN2,
    { SENSORS_W83781D_FAN2, SENSORS_W83781D_FAN2_MIN, SENSORS_W83781D_FAN2_DIV, -1 } },
  { fmtFans_0, rrdF0, DataType_rpm, W83781D_ALARM_FAN3, W83781D_ALARM_FAN3,
    { SENSORS_W83781D_FAN3, SENSORS_W83781D_FAN3_MIN, SENSORS_W83781D_FAN3_DIV, -1 } },
  { fmtTemps_W83781D, rrdF0, DataType_temperature, W83781D_ALARM_TEMP1, W83781D_ALARM_TEMP1,
    { SENSORS_W83781D_TEMP1, SENSORS_W83781D_TEMP1_OVER, SENSORS_W83781D_TEMP1_HYST, -1 } },
  { fmtTemps_1_0, rrdF1, DataType_temperature, W83781D_ALARM_TEMP2, W83781D_ALARM_TEMP2,
    { SENSORS_W83781D_TEMP2, SENSORS_W83781D_TEMP2_OVER, SENSORS_W83781D_TEMP2_HYST, -1 } },
  { fmtTemps_1_0, rrdF1, DataType_temperature, W83781D_ALARM_TEMP3, W83781D_ALARM_TEMP3,
    { SENSORS_W83781D_TEMP3, SENSORS_W83781D_TEMP3_OVER, SENSORS_W83781D_TEMP3_HYST, -1 } },
  { fmtVolt_3, rrdF3, DataType_voltage, 0, 0,
    { SENSORS_W83781D_VID, -1 } },
  { fmtChassisIntrusionDetection, NULL, DataType_other, W83781D_ALARM_CHAS, W83781D_ALARM_CHAS,
    { SENSORS_W83781D_ALARMS, -1 } },
  { fmtSoundAlarm, NULL, DataType_other, 0, 0,
    { SENSORS_W83781D_BEEP_ENABLE, -1 } },
  { NULL }
};

static const ChipDescriptor as99127f_chip = {
  as99127f_names, as99127f_features, SENSORS_W83781D_ALARMS, SENSORS_W83781D_BEEPS
};

/** W83782D **/

static const char *
fmtTemps_W8378x_0
(const double values[], int alarm, int beep) {
  int sensorID = (int) values[3];
  const char *sensor = (sensorID == 1) ? "PII/Celeron diode" :
    (sensorID == 2) ? "3904 transistor" : "thermistor";
  if (values[2] == 127) {
    sprintf (buff, "%.0f C (limit = %.0f C, sensors = %s)",
             values[0], values[1], sensor);
  } else {
    sprintf (buff, "%.0f C (limit = %.0f C, hysteresis = %.0f C, sensors = %s)",
             values[0], values[1], values[2], sensor);
  }
  return fmtExtra (alarm, beep);
}

static const char *
fmtTemps_W8378x_1
(const double values[], int alarm, int beep) {
  int sensorID = (int) values[3];
  const char *sensor = (sensorID == 1) ? "PII/Celeron diode" :
    (sensorID == 2) ? "3904 transistor" : "thermistor";
  sprintf (buff, "%.1f C (limit = %.0f C, hysteresis = %.0f C, sensor = %s)",
           values[0], values[1], values[2], sensor);
  return fmtExtra (alarm, beep);
}

static const char *w83782d_names[] = {
  SENSORS_W83782D_PREFIX, SENSORS_W83627HF_PREFIX, SENSORS_W83627THF_PREFIX, NULL
};

static const FeatureDescriptor w83782d_features[] = {
  { fmtVolts_2, rrdF2, DataType_voltage, W83781D_ALARM_IN0, W83781D_ALARM_IN0,
    { SENSORS_W83782D_IN0, SENSORS_W83782D_IN0_MIN, SENSORS_W83782D_IN0_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, W83781D_ALARM_IN1, W83781D_ALARM_IN1,
    { SENSORS_W83782D_IN1, SENSORS_W83782D_IN1_MIN, SENSORS_W83782D_IN1_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, W83781D_ALARM_IN2, W83781D_ALARM_IN2,
    { SENSORS_W83782D_IN2, SENSORS_W83782D_IN2_MIN, SENSORS_W83782D_IN2_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, W83781D_ALARM_IN3, W83781D_ALARM_IN3,
    { SENSORS_W83782D_IN3, SENSORS_W83782D_IN3_MIN, SENSORS_W83782D_IN3_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, W83781D_ALARM_IN4, W83781D_ALARM_IN4,
    { SENSORS_W83782D_IN4, SENSORS_W83782D_IN4_MIN, SENSORS_W83782D_IN4_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, W83781D_ALARM_IN5, W83781D_ALARM_IN5,
    { SENSORS_W83782D_IN5, SENSORS_W83782D_IN5_MIN, SENSORS_W83782D_IN5_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, W83781D_ALARM_IN6, W83781D_ALARM_IN6,
    { SENSORS_W83782D_IN6, SENSORS_W83782D_IN6_MIN, SENSORS_W83782D_IN6_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, W83782D_ALARM_IN7, W83782D_ALARM_IN7,
    { SENSORS_W83782D_IN7, SENSORS_W83782D_IN7_MIN, SENSORS_W83782D_IN7_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, W83782D_ALARM_IN8, W83782D_ALARM_IN8,
    { SENSORS_W83782D_IN8, SENSORS_W83782D_IN8_MIN, SENSORS_W83782D_IN8_MAX, -1 } },
  { fmtFans_0, rrdF0, DataType_rpm, W83781D_ALARM_FAN1, W83781D_ALARM_FAN1,
    { SENSORS_W83782D_FAN1, SENSORS_W83782D_FAN1_MIN, SENSORS_W83782D_FAN1_DIV, -1 } },
  { fmtFans_0, rrdF0, DataType_rpm, W83781D_ALARM_FAN2, W83781D_ALARM_FAN2,
    { SENSORS_W83782D_FAN2, SENSORS_W83782D_FAN2_MIN, SENSORS_W83782D_FAN2_DIV, -1 } },
  { fmtFans_0, rrdF0, DataType_rpm, W83781D_ALARM_FAN3, W83781D_ALARM_FAN3,
    { SENSORS_W83782D_FAN3, SENSORS_W83782D_FAN3_MIN, SENSORS_W83782D_FAN3_DIV, -1 } },
  { fmtTemps_W8378x_0, rrdF0, DataType_temperature, W83781D_ALARM_TEMP1, W83781D_ALARM_TEMP1,
    { SENSORS_W83782D_TEMP1, SENSORS_W83782D_TEMP1_OVER, SENSORS_W83782D_TEMP1_HYST, SENSORS_W83782D_SENS1, -1 } },
  { fmtTemps_W8378x_1, rrdF1, DataType_temperature, W83781D_ALARM_TEMP2, W83781D_ALARM_TEMP2,
    { SENSORS_W83782D_TEMP2, SENSORS_W83782D_TEMP2_OVER, SENSORS_W83782D_TEMP2_HYST, SENSORS_W83782D_SENS2, -1 } },
  { fmtTemps_W8378x_1, rrdF1, DataType_temperature, W83781D_ALARM_TEMP3, W83781D_ALARM_TEMP3,
    { SENSORS_W83782D_TEMP3, SENSORS_W83782D_TEMP3_OVER, SENSORS_W83782D_TEMP3_HYST, SENSORS_W83782D_SENS3, -1 } },
  { fmtVolt_3, rrdF3, DataType_voltage, 0, 0,
    { SENSORS_W83782D_VID, -1 } },
  { fmtChassisIntrusionDetection, NULL, DataType_other, W83781D_ALARM_CHAS, W83781D_ALARM_CHAS,
    { SENSORS_W83781D_ALARMS, -1 } },
  { fmtSoundAlarm, NULL, DataType_other, 0, 0,
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
  { fmtVolts_2, rrdF2, DataType_voltage, W83781D_ALARM_IN0, W83781D_ALARM_IN0,
    { SENSORS_W83783S_IN0, SENSORS_W83783S_IN0_MIN, SENSORS_W83783S_IN0_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, W83781D_ALARM_IN2, W83781D_ALARM_IN2,
    { SENSORS_W83783S_IN2, SENSORS_W83783S_IN2_MIN, SENSORS_W83783S_IN2_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, W83781D_ALARM_IN3, W83781D_ALARM_IN3,
    { SENSORS_W83783S_IN3, SENSORS_W83783S_IN3_MIN, SENSORS_W83783S_IN3_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, W83781D_ALARM_IN4, W83781D_ALARM_IN4,
    { SENSORS_W83783S_IN4, SENSORS_W83783S_IN4_MIN, SENSORS_W83783S_IN4_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, W83781D_ALARM_IN5, W83781D_ALARM_IN5,
    { SENSORS_W83783S_IN5, SENSORS_W83783S_IN5_MIN, SENSORS_W83783S_IN5_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, W83781D_ALARM_IN6, W83781D_ALARM_IN6,
    { SENSORS_W83782D_IN6, SENSORS_W83782D_IN6_MIN, SENSORS_W83782D_IN6_MAX, -1 } },
  { fmtFans_0, rrdF0, DataType_rpm, W83781D_ALARM_FAN1, W83781D_ALARM_FAN1,
    { SENSORS_W83783S_FAN1, SENSORS_W83783S_FAN1_MIN, SENSORS_W83783S_FAN1_DIV, -1 } },
  { fmtFans_0, rrdF0, DataType_rpm, W83781D_ALARM_FAN2, W83781D_ALARM_FAN2,
    { SENSORS_W83783S_FAN2, SENSORS_W83783S_FAN2_MIN, SENSORS_W83783S_FAN2_DIV, -1 } },
  { fmtFans_0, rrdF0, DataType_rpm, W83781D_ALARM_FAN3, W83781D_ALARM_FAN3,
    { SENSORS_W83783S_FAN3, SENSORS_W83783S_FAN3_MIN, SENSORS_W83783S_FAN3_DIV, -1 } },
  { fmtTemps_W8378x_0, rrdF0, DataType_temperature, W83781D_ALARM_TEMP1, W83781D_ALARM_TEMP1,
    { SENSORS_W83783S_TEMP1, SENSORS_W83783S_TEMP1_OVER, SENSORS_W83783S_TEMP1_HYST, SENSORS_W83783S_SENS1, -1 } },
  { fmtTemps_W8378x_1, rrdF1, DataType_temperature, W83781D_ALARM_TEMP2, W83781D_ALARM_TEMP2,
    { SENSORS_W83783S_TEMP2, SENSORS_W83783S_TEMP2_OVER, SENSORS_W83783S_TEMP2_HYST, SENSORS_W83783S_SENS2, -1 } },
  { fmtVolt_3, rrdF3, DataType_voltage, 0, 0,
    { SENSORS_W83783S_VID, -1 } },
  { fmtSoundAlarm, NULL, DataType_other, 0, 0,
    { SENSORS_W83781D_BEEP_ENABLE, -1 } },
  { NULL }
};

static const ChipDescriptor w83783s_chip = {
  w83783s_names, w83783s_features, SENSORS_W83783S_ALARMS, SENSORS_W83783S_BEEPS
};

/** W83697HF **/

static const char *w83697hf_names[] = {
  SENSORS_W83697HF_PREFIX, NULL
};

static const FeatureDescriptor w83697hf_features[] = {
  { fmtVolts_2, rrdF2, DataType_voltage, W83781D_ALARM_IN0, W83781D_ALARM_IN0,
    { SENSORS_W83782D_IN0, SENSORS_W83782D_IN0_MIN, SENSORS_W83782D_IN0_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, W83781D_ALARM_IN2, W83781D_ALARM_IN2,
    { SENSORS_W83782D_IN2, SENSORS_W83782D_IN2_MIN, SENSORS_W83782D_IN2_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, W83781D_ALARM_IN3, W83781D_ALARM_IN3,
    { SENSORS_W83782D_IN3, SENSORS_W83782D_IN3_MIN, SENSORS_W83782D_IN3_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, W83781D_ALARM_IN4, W83781D_ALARM_IN4,
    { SENSORS_W83782D_IN4, SENSORS_W83782D_IN4_MIN, SENSORS_W83782D_IN4_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, W83781D_ALARM_IN5, W83781D_ALARM_IN5,
    { SENSORS_W83782D_IN5, SENSORS_W83782D_IN5_MIN, SENSORS_W83782D_IN5_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, W83781D_ALARM_IN6, W83781D_ALARM_IN6,
    { SENSORS_W83782D_IN6, SENSORS_W83782D_IN6_MIN, SENSORS_W83782D_IN6_MAX, -1 } },
  { fmtFans_0, rrdF0, DataType_rpm, W83781D_ALARM_FAN1, W83781D_ALARM_FAN1,
    { SENSORS_W83782D_FAN1, SENSORS_W83782D_FAN1_MIN, SENSORS_W83782D_FAN1_DIV, -1 } },
  { fmtFans_0, rrdF0, DataType_rpm, W83781D_ALARM_FAN2, W83781D_ALARM_FAN2,
    { SENSORS_W83782D_FAN2, SENSORS_W83782D_FAN2_MIN, SENSORS_W83782D_FAN2_DIV, -1 } },
  { fmtTemps_W8378x_0, rrdF0, DataType_temperature, W83781D_ALARM_TEMP1, W83781D_ALARM_TEMP1,
    { SENSORS_W83782D_TEMP1, SENSORS_W83782D_TEMP1_OVER, SENSORS_W83782D_TEMP1_HYST, SENSORS_W83782D_SENS1, -1 } },
  { fmtTemps_W8378x_1, rrdF1, DataType_temperature, W83781D_ALARM_TEMP2, W83781D_ALARM_TEMP2,
    { SENSORS_W83782D_TEMP2, SENSORS_W83782D_TEMP2_OVER, SENSORS_W83782D_TEMP2_HYST, SENSORS_W83782D_SENS2, -1 } },
  { fmtChassisIntrusionDetection, NULL, DataType_other, W83781D_ALARM_CHAS, W83781D_ALARM_CHAS,
    { SENSORS_W83781D_ALARMS, -1 } },
  { fmtSoundAlarm, NULL, DataType_other, 0, 0,
    { SENSORS_W83781D_BEEP_ENABLE, -1 } },
  { NULL }
};

static const ChipDescriptor w83697hf_chip = {
  w83697hf_names, w83697hf_features, SENSORS_W83781D_ALARMS, SENSORS_W83781D_BEEPS
};


/** MAXILIFE **/

static const char *
fmtTemps_Maxilife
(const double values[], int alarm, int beep) {
  if (!values[0] && !values[1] && !values[2])
    return NULL;
  return fmtTemps_1_0 (values, alarm, beep);
}

static const char *
rrdTemps_Maxilife
(const double values[]) {
  if (!values[0] && !values[1] && !values[2])
    return NULL;
  return rrdF1 (values);
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
rrdFans_Maxilife
(const double values[]) {
  if (!values[0] && !values[1] && !values[2])
    return NULL;
  if (values[0] < 0) {
    return NULL;
  } else {
    sprintf (buff, "%.0f", values[0] / values[2]);
    return buff;
  }
}

static const char *
fmtMHz_Maxilife
(const double values[], int alarm, int beep) {
  if (!values[0] && !values[1] && !values[2])
    return NULL;
  return fmtMHz_2 (values, alarm, beep);
}

static const char *
rrdMHz_Maxilife
(const double values[]) {
  if (!values[0] && !values[1] && !values[2])
    return NULL;
  return rrdF2 (values);
}

static const char *
fmtVolts_Maxilife
(const double values[], int alarm, int beep) {
  if (!values[0] && !values[1] && !values[2])
    return NULL;
  return fmtVolts_2 (values, alarm, beep);
}

static const char *
rrdVolts_Maxilife
(const double values[]) {
  if (!values[0] && !values[1] && !values[2])
    return NULL;
  return rrdF2 (values);
}

static const char *maxilife_names[] = {
  SENSORS_MAXI_CG_PREFIX, SENSORS_MAXI_CO_PREFIX, SENSORS_MAXI_AS_PREFIX, SENSORS_MAXI_NBA_PREFIX, NULL
};

static const FeatureDescriptor maxilife_features[] = {
  { fmtTemps_Maxilife, rrdTemps_Maxilife, DataType_temperature, 0, 0,
    { SENSORS_MAXI_CG_TEMP1, SENSORS_MAXI_CG_TEMP1_MAX, SENSORS_MAXI_CG_TEMP1_HYST, -1 } },
  { fmtTemps_Maxilife, rrdTemps_Maxilife, DataType_temperature, MAXI_ALARM_TEMP2, 0,
    { SENSORS_MAXI_CG_TEMP2, SENSORS_MAXI_CG_TEMP2_MAX, SENSORS_MAXI_CG_TEMP2_HYST, -1 } },
  { fmtTemps_Maxilife, rrdTemps_Maxilife, DataType_temperature, 0, 0,
    { SENSORS_MAXI_CG_TEMP3, SENSORS_MAXI_CG_TEMP3_MAX, SENSORS_MAXI_CG_TEMP3_HYST, -1 } },
  { fmtTemps_Maxilife, rrdTemps_Maxilife, DataType_temperature, MAXI_ALARM_TEMP4, 0,
    { SENSORS_MAXI_CG_TEMP4, SENSORS_MAXI_CG_TEMP4_MAX, SENSORS_MAXI_CG_TEMP4_HYST, -1 } },
  { fmtTemps_Maxilife, rrdTemps_Maxilife, DataType_temperature, MAXI_ALARM_TEMP5, 0,
    { SENSORS_MAXI_CG_TEMP5, SENSORS_MAXI_CG_TEMP5_MAX, SENSORS_MAXI_CG_TEMP4_HYST, -1 } },
  { fmtFans_Maxilife, rrdFans_Maxilife, DataType_rpm, MAXI_ALARM_FAN1, 0,
    { SENSORS_MAXI_CG_FAN1, SENSORS_MAXI_CG_FAN1_MIN, SENSORS_MAXI_CG_FAN1_DIV, -1 } },
  { fmtFans_Maxilife, rrdFans_Maxilife, DataType_rpm, MAXI_ALARM_FAN2, 0,
    { SENSORS_MAXI_CG_FAN2, SENSORS_MAXI_CG_FAN2_MIN, SENSORS_MAXI_CG_FAN2_DIV, -1 } },
  { fmtFans_Maxilife, rrdFans_Maxilife, DataType_rpm, MAXI_ALARM_FAN3, 0,
    { SENSORS_MAXI_CG_FAN3, SENSORS_MAXI_CG_FAN3_MIN, SENSORS_MAXI_CG_FAN3_DIV, -1 } },
  { fmtMHz_Maxilife, rrdMHz_Maxilife, DataType_mhz, MAXI_ALARM_PLL, 0,
    { SENSORS_MAXI_CG_PLL, SENSORS_MAXI_CG_PLL_MIN, SENSORS_MAXI_CG_PLL_MAX, -1 } },
  { fmtVolts_Maxilife, rrdVolts_Maxilife, DataType_voltage, MAXI_ALARM_VID1, 0,
    { SENSORS_MAXI_CG_VID1, SENSORS_MAXI_CG_VID1_MIN, SENSORS_MAXI_CG_VID1_MAX, -1 } },
  { fmtVolts_Maxilife, rrdVolts_Maxilife, DataType_voltage, MAXI_ALARM_VID2, 0,
    { SENSORS_MAXI_CG_VID2, SENSORS_MAXI_CG_VID2_MIN, SENSORS_MAXI_CG_VID2_MAX, -1 } },
  { fmtVolts_Maxilife, rrdVolts_Maxilife, DataType_voltage, MAXI_ALARM_VID3, 0,
    { SENSORS_MAXI_CG_VID3, SENSORS_MAXI_CG_VID3_MIN, SENSORS_MAXI_CG_VID3_MAX, -1 } },
  { fmtVolts_Maxilife, rrdVolts_Maxilife, DataType_voltage, MAXI_ALARM_VID4, 0,
    { SENSORS_MAXI_CG_VID4, SENSORS_MAXI_CG_VID4_MIN, SENSORS_MAXI_CG_VID4_MAX, -1 } },
  { NULL }
};

static const ChipDescriptor maxilife_chip = {
  maxilife_names, maxilife_features, SENSORS_MAXI_CG_ALARMS, 0
};

/** ASB100 **/

static const char *asb100_names[] = {
        SENSORS_ASB100_PREFIX, NULL
};

static const FeatureDescriptor asb100_features[] = {
  { fmtVolts_2, rrdF2, DataType_voltage, 0, 0,
    { SENSORS_ASB100_IN0, SENSORS_ASB100_IN0_MIN, SENSORS_ASB100_IN0_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, 0, 0,
    { SENSORS_ASB100_IN1, SENSORS_ASB100_IN1_MIN, SENSORS_ASB100_IN1_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, 0, 0,
    { SENSORS_ASB100_IN2, SENSORS_ASB100_IN2_MIN, SENSORS_ASB100_IN2_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, 0, 0,
    { SENSORS_ASB100_IN3, SENSORS_ASB100_IN3_MIN, SENSORS_ASB100_IN3_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, 0, 0,
    { SENSORS_ASB100_IN4, SENSORS_ASB100_IN4_MIN, SENSORS_ASB100_IN4_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, 0, 0,
    { SENSORS_ASB100_IN5, SENSORS_ASB100_IN5_MIN, SENSORS_ASB100_IN5_MAX, -1 } },
  { fmtVolts_2, rrdF2, DataType_voltage, 0, 0,
    { SENSORS_ASB100_IN6, SENSORS_ASB100_IN6_MIN, SENSORS_ASB100_IN6_MAX, -1 } },

  { fmtFans_0, rrdF0, DataType_rpm, 0, 0,
    { SENSORS_ASB100_FAN1, SENSORS_ASB100_FAN1_MIN, -1 } },
  { fmtFans_0, rrdF0, DataType_rpm, 0, 0,
    { SENSORS_ASB100_FAN2, SENSORS_ASB100_FAN2_MIN, -1 } },
  { fmtFans_0, rrdF0, DataType_rpm, 0, 0,
    { SENSORS_ASB100_FAN3, SENSORS_ASB100_FAN3_MIN, -1 } },

  { fmtTemps_1_0, rrdF1, DataType_temperature, 0, 0,
    { SENSORS_ASB100_TEMP1, SENSORS_ASB100_TEMP1_HYST, SENSORS_ASB100_TEMP1_OVER, -1 } },
  { fmtTemps_1_0, rrdF1, DataType_temperature, 0, 0,
    { SENSORS_ASB100_TEMP2, SENSORS_ASB100_TEMP2_HYST, SENSORS_ASB100_TEMP2_OVER, -1 } },
  { fmtTemps_1_0, rrdF1, DataType_temperature, 0, 0,
    { SENSORS_ASB100_TEMP3, SENSORS_ASB100_TEMP3_HYST, SENSORS_ASB100_TEMP3_OVER, -1 } },
  { fmtTemps_1_0, rrdF1, DataType_temperature, 0, 0,
    { SENSORS_ASB100_TEMP4, SENSORS_ASB100_TEMP4_HYST, SENSORS_ASB100_TEMP4_OVER, -1 } },

  { NULL }
};

static const ChipDescriptor asb100_chip = {
  asb100_names, asb100_features, 0, 0
};

/** EEPROM **/

static const char *
fmtType_EEPROM
(const double values[], int alarm, int beep) {
  if ((int) values[0] == 4)
    sprintf (buff, "SDRAM DIMM SPD");
  else if ((int) values[0] == 7)
    sprintf (buff, "DDR SDRAM DIMM SPD");
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
  int banks = (int) values[3];
  int foo = (row & 0xf) + (col & 0xf) + 17;
  if ((foo > 0) && (foo <= 12) && (num <= 8) && (banks <= 8)) {
    sprintf (buff, "%d", (1 << foo) * num * banks);
  } else {
    sprintf (buff, "Invalid %d %d %d %d", row, col, num, banks);
  }
  return buff;
}

static const char *eeprom_names[] = {
  SENSORS_EEPROM_PREFIX, NULL
};

static const FeatureDescriptor eeprom_features[] = {
  { fmtType_EEPROM, NULL, DataType_other, 0, 0,
    { SENSORS_EEPROM_TYPE, -1 } },
  { fmtRowCol_EEPROM, NULL, DataType_other, 0, 0,
    { SENSORS_EEPROM_ROWADDR, SENSORS_EEPROM_COLADDR, SENSORS_EEPROM_NUMROWS, SENSORS_EEPROM_BANKS, -1 } },
  { NULL }
};

static const ChipDescriptor eeprom_chip = {
  eeprom_names, eeprom_features, 0, 0
};

/** ALL **/

const ChipDescriptor * const knownChips[] = {
  &adm1021_chip,
  &adm1025_chip,
  &adm9240_chip,
  &ds1621_chip,
  &eeprom_chip,
  &gl518_chip,
  &lm75_chip,
  &lm78_chip,
  &lm80_chip,
  &max1617_chip,
  &maxilife_chip,
  &sis5595_chip,
  &via686a_chip,
  &as99127f_chip,
  &w83781d_chip,
  &w83782d_chip,
  &w83783s_chip,
  &w83697hf_chip,
  &it87_chip,
  &asb100_chip,
  NULL
};
