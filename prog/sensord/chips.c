/*
 * sensord
 *
 * A daemon that logs all sensor information to /var/log/sensors
 * every 5 minutes.
 *
 * Copyright (c) 1999 Merlin Hughes <merlin@merlin.org>
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

#include "lib/sensors.h"
#include "lib/chips.h"

#include "sensord.h"

/* missing: "gl520sm", "thmc50", "adm1022" */

/* I don't support gl518sm-r00 - but neither does sensors */

/* Should the maxilife funky fan stuff be in sensors.conf? */

/* I don't support alarms. */

/** formatters **/

static char buff[4096];

static char *
fmtValu_0
(double values[]) {
  sprintf (buff, "%.0f", values[0]);
  return buff;
}

static char *
fmtTemps_0
(double values[]) {
  sprintf (buff, "%.0f C (limit = %.0f C, hysteresis = %.0f C)", values[0], values[1], values[2]);
  return buff;
}

static char *
fmtTemps_1
(double values[]) {
  sprintf (buff, "%.1f C (limit = %.1f C, hysteresis = %.1f C)", values[0], values[1], values[2]);
  return buff;
}

static char *
fmtVolt_2
(double values[]) {
  sprintf (buff, "%+.2f V", values[0]);
  return buff;
}

static char *
fmtVolts_2
(double values[]) {
  sprintf (buff, "%+.2f V (min = %+.2f V, max = %+.2f V)", values[0], values[1], values[2]);
  return buff;
}

static char *
fmtFans_0
(double values[]) {
  sprintf (buff, "%.0f RPM (min = %.0f RPM, div = %.0f)", values[0], values[1], values[2]);
  return buff;
}

static char *
fmtMHz_2
(double values[]) {
  sprintf (buff, "%.2f MHz (min = %.2f MHz, max = %.2f MHz)", values[0], values[1], values[2]);
  return buff;
}

/** LM75 **/

static const char *lm75_names[] = {
  SENSORS_LM75_PREFIX, NULL
};

static FeatureDescriptor lm75_features[] = {
  { fmtTemps_1, SENSORS_LM75_TEMP,
    { SENSORS_LM75_TEMP, SENSORS_LM75_TEMP_HYST, SENSORS_LM75_TEMP_OVER, -1 } },
  { NULL }
};

/** ADM1021 **/

static const char *adm1021_names[] = {
  SENSORS_ADM1021_PREFIX, NULL
};

static FeatureDescriptor adm1021_features[] = {
  { fmtTemps_0, SENSORS_ADM1021_TEMP,
    { SENSORS_ADM1021_TEMP, SENSORS_ADM1021_TEMP_OVER, SENSORS_ADM1021_TEMP_HYST, -1 } },
  { fmtTemps_0, SENSORS_ADM1021_REMOTE_TEMP,
    { SENSORS_ADM1021_REMOTE_TEMP, SENSORS_ADM1021_REMOTE_TEMP_OVER, SENSORS_ADM1021_REMOTE_TEMP_HYST, -1 } },
  { fmtValu_0, SENSORS_ADM1021_DIE_CODE,
    { SENSORS_ADM1021_DIE_CODE, -1 } },
  { NULL }
};

/** MAX1617 **/

static const char *max1617_names[] = {
  SENSORS_MAX1617_PREFIX, SENSORS_MAX1617A_PREFIX, SENSORS_THMC10_PREFIX, SENSORS_LM84_PREFIX, SENSORS_GL523_PREFIX, NULL
};

static FeatureDescriptor max1617_features[] = {
  { fmtTemps_0, SENSORS_ADM1021_TEMP,
    { SENSORS_ADM1021_TEMP, SENSORS_ADM1021_TEMP_OVER, SENSORS_ADM1021_TEMP_HYST, -1 } },
  { fmtTemps_0, SENSORS_ADM1021_REMOTE_TEMP,
    { SENSORS_ADM1021_REMOTE_TEMP, SENSORS_ADM1021_REMOTE_TEMP_OVER, SENSORS_ADM1021_REMOTE_TEMP_HYST, -1 } },
  { NULL }
};

/** ADM9240 **/

static const char *adm9240_names[] = {
  SENSORS_ADM9240_PREFIX, SENSORS_DS1780_PREFIX, SENSORS_LM81_PREFIX, NULL
};

static FeatureDescriptor adm9240_features[] = {
  { fmtVolts_2, SENSORS_ADM9240_IN0,
    { SENSORS_ADM9240_IN0, SENSORS_ADM9240_IN0_MIN, SENSORS_ADM9240_IN0_MAX, -1 } },
  { fmtVolts_2, SENSORS_ADM9240_IN1,
    { SENSORS_ADM9240_IN1, SENSORS_ADM9240_IN1_MIN, SENSORS_ADM9240_IN1_MAX, -1 } },
  { fmtVolts_2, SENSORS_ADM9240_IN2,
    { SENSORS_ADM9240_IN2, SENSORS_ADM9240_IN2_MIN, SENSORS_ADM9240_IN2_MAX, -1 } },
  { fmtVolts_2, SENSORS_ADM9240_IN3,
    { SENSORS_ADM9240_IN3, SENSORS_ADM9240_IN3_MIN, SENSORS_ADM9240_IN3_MAX, -1 } },
  { fmtVolts_2, SENSORS_ADM9240_IN4,
    { SENSORS_ADM9240_IN4, SENSORS_ADM9240_IN4_MIN, SENSORS_ADM9240_IN4_MAX, -1 } },
  { fmtVolts_2, SENSORS_ADM9240_IN5,
    { SENSORS_ADM9240_IN5, SENSORS_ADM9240_IN5_MIN, SENSORS_ADM9240_IN5_MAX, -1 } },
  { fmtFans_0, SENSORS_ADM9240_FAN1,
    { SENSORS_ADM9240_FAN1, SENSORS_ADM9240_FAN1_MIN, SENSORS_ADM9240_FAN1_DIV, -1 } },
  { fmtFans_0, SENSORS_ADM9240_FAN2,
    { SENSORS_ADM9240_FAN2, SENSORS_ADM9240_FAN2_MIN, SENSORS_ADM9240_FAN2_DIV, -1 } },
  { fmtTemps_0, SENSORS_ADM9240_TEMP,
    { SENSORS_ADM9240_TEMP, SENSORS_ADM9240_TEMP_OVER, SENSORS_ADM9240_TEMP_HYST, -1 } },
  { fmtVolt_2, SENSORS_ADM9240_VID,
    { SENSORS_ADM9240_VID, -1 } },
  { NULL }
};

/** SIS5595 **/

static const char *sis5595_names[] = {
  SENSORS_SIS5595_PREFIX, NULL
};

static FeatureDescriptor sis5595_features[] = {
  { fmtVolts_2, SENSORS_SIS5595_IN0,
    { SENSORS_SIS5595_IN0, SENSORS_SIS5595_IN0_MIN, SENSORS_SIS5595_IN0_MAX, -1 } },
  { fmtVolts_2, SENSORS_SIS5595_IN1,
    { SENSORS_SIS5595_IN1, SENSORS_SIS5595_IN1_MIN, SENSORS_SIS5595_IN1_MAX, -1 } },
  { fmtVolts_2, SENSORS_SIS5595_IN2,
    { SENSORS_SIS5595_IN2, SENSORS_SIS5595_IN2_MIN, SENSORS_SIS5595_IN2_MAX, -1 } },
  { fmtVolts_2, SENSORS_SIS5595_IN3,
    { SENSORS_SIS5595_IN3, SENSORS_SIS5595_IN3_MIN, SENSORS_SIS5595_IN3_MAX, -1 } },
  { fmtFans_0, SENSORS_SIS5595_FAN1,
    { SENSORS_SIS5595_FAN1, SENSORS_SIS5595_FAN1_MIN, SENSORS_SIS5595_FAN1_DIV, -1 } },
  { fmtFans_0, SENSORS_SIS5595_FAN2,
    { SENSORS_SIS5595_FAN2, SENSORS_SIS5595_FAN2_MIN, SENSORS_SIS5595_FAN2_DIV, -1 } },
  { fmtTemps_0, SENSORS_SIS5595_TEMP,
    { SENSORS_SIS5595_TEMP, SENSORS_SIS5595_TEMP_OVER, SENSORS_SIS5595_TEMP_HYST, -1 } },
  { NULL }
};

/** LM78 **/

static const char *lm78_names[] = {
  SENSORS_LM78_PREFIX, SENSORS_LM78J_PREFIX, SENSORS_LM79_PREFIX, /*"sis5595",*/ NULL
};

static FeatureDescriptor lm78_features[] = {
  { fmtVolts_2, SENSORS_LM78_IN0,
    { SENSORS_LM78_IN0, SENSORS_LM78_IN0_MIN, SENSORS_LM78_IN0_MAX, -1 } },
  { fmtVolts_2, SENSORS_LM78_IN1,
    { SENSORS_LM78_IN1, SENSORS_LM78_IN1_MIN, SENSORS_LM78_IN1_MAX, -1 } },
  { fmtVolts_2, SENSORS_LM78_IN2,
    { SENSORS_LM78_IN2, SENSORS_LM78_IN2_MIN, SENSORS_LM78_IN2_MAX, -1 } },
  { fmtVolts_2, SENSORS_LM78_IN3,
    { SENSORS_LM78_IN3, SENSORS_LM78_IN3_MIN, SENSORS_LM78_IN3_MAX, -1 } },
  { fmtVolts_2, SENSORS_LM78_IN4,
    { SENSORS_LM78_IN4, SENSORS_LM78_IN4_MIN, SENSORS_LM78_IN4_MAX, -1 } },
  { fmtVolts_2, SENSORS_LM78_IN5,
    { SENSORS_LM78_IN5, SENSORS_LM78_IN5_MIN, SENSORS_LM78_IN5_MAX, -1 } },
  { fmtVolts_2, SENSORS_LM78_IN6,
    { SENSORS_LM78_IN6, SENSORS_LM78_IN6_MIN, SENSORS_LM78_IN6_MAX, -1 } },
  { fmtFans_0, SENSORS_LM78_FAN1,
    { SENSORS_LM78_FAN1, SENSORS_LM78_FAN1_MIN, SENSORS_LM78_FAN1_DIV, -1 } },
  { fmtFans_0, SENSORS_LM78_FAN2,
    { SENSORS_LM78_FAN2, SENSORS_LM78_FAN2_MIN, SENSORS_LM78_FAN2_DIV, -1 } },
  { fmtFans_0, SENSORS_LM78_FAN3,
    { SENSORS_LM78_FAN3, SENSORS_LM78_FAN3_MIN, SENSORS_LM78_FAN3_DIV, -1 } },
  { fmtTemps_0, SENSORS_LM78_TEMP,
    { SENSORS_LM78_TEMP, SENSORS_LM78_TEMP_OVER, SENSORS_LM78_TEMP_HYST, -1 } },
  { fmtVolt_2, SENSORS_LM78_VID,
    { SENSORS_LM78_VID, -1 } },
  { NULL }
};

/** GL518 **/

static const char *gl518_names[] = {
  SENSORS_GL518_PREFIX, NULL
};

static FeatureDescriptor gl518_features[] = {
  { fmtVolts_2, SENSORS_GL518_VDD,
    { SENSORS_GL518_VDD, SENSORS_GL518_VDD_MIN, SENSORS_GL518_VDD_MAX, -1 } },
  { fmtVolts_2, SENSORS_GL518_VIN1,
    { SENSORS_GL518_VIN1, SENSORS_GL518_VIN1_MIN, SENSORS_GL518_VIN1_MAX, -1 } },
  { fmtVolts_2, SENSORS_GL518_VIN2,
    { SENSORS_GL518_VIN2, SENSORS_GL518_VIN2_MIN, SENSORS_GL518_VIN2_MAX, -1 } },
  { fmtVolts_2, SENSORS_GL518_VIN3,
    { SENSORS_GL518_VIN3, SENSORS_GL518_VIN3_MIN, SENSORS_GL518_VIN3_MAX, -1 } },
  { fmtFans_0, SENSORS_GL518_FAN1,
    { SENSORS_GL518_FAN1, SENSORS_GL518_FAN1_MIN, SENSORS_GL518_FAN1_DIV, -1 } },
  { fmtFans_0, SENSORS_GL518_FAN2,
    { SENSORS_GL518_FAN2, SENSORS_GL518_FAN2_MIN, SENSORS_GL518_FAN2_DIV, -1 } },
  { fmtTemps_0, SENSORS_GL518_TEMP,
    { SENSORS_GL518_TEMP, SENSORS_GL518_TEMP_OVER, SENSORS_GL518_TEMP_HYST, -1 } },
  { NULL }
};

/** LM80 **/

static char *
fmtTemps_LM80
(double values[]) {
  sprintf (buff, "%.0f C (hot limit = %.0f C, hot hysteresis = %.0f C, os limit = %.0f C, os hysteresis = %.0f C)", values[0], values[1], values[2], values[3], values[4]);
  return buff;
}

static const char *lm80_names[] = {
  SENSORS_LM80_PREFIX, NULL
};

static FeatureDescriptor lm80_features[] = {
  { fmtVolts_2, SENSORS_LM80_IN0,
    { SENSORS_LM80_IN0, SENSORS_LM80_IN0_MIN, SENSORS_LM80_IN0_MAX, -1 } },
  { fmtVolts_2, SENSORS_LM80_IN1,
    { SENSORS_LM80_IN1, SENSORS_LM80_IN1_MIN, SENSORS_LM80_IN1_MAX, -1 } },
  { fmtVolts_2, SENSORS_LM80_IN2,
    { SENSORS_LM80_IN2, SENSORS_LM80_IN2_MIN, SENSORS_LM80_IN2_MAX, -1 } },
  { fmtVolts_2, SENSORS_LM80_IN3,
    { SENSORS_LM80_IN3, SENSORS_LM80_IN3_MIN, SENSORS_LM80_IN3_MAX, -1 } },
  { fmtVolts_2, SENSORS_LM80_IN4,
    { SENSORS_LM80_IN4, SENSORS_LM80_IN4_MIN, SENSORS_LM80_IN4_MAX, -1 } },
  { fmtVolts_2, SENSORS_LM80_IN5,
    { SENSORS_LM80_IN5, SENSORS_LM80_IN5_MIN, SENSORS_LM80_IN5_MAX, -1 } },
  { fmtVolts_2, SENSORS_LM80_IN6,
    { SENSORS_LM80_IN6, SENSORS_LM80_IN6_MIN, SENSORS_LM80_IN6_MAX, -1 } },
  { fmtFans_0, SENSORS_LM80_FAN1,
    { SENSORS_LM80_FAN1, SENSORS_LM80_FAN1_MIN, SENSORS_LM80_FAN1_DIV, -1 } },
  { fmtFans_0, SENSORS_LM80_FAN2,
    { SENSORS_LM80_FAN2, SENSORS_LM80_FAN2_MIN, SENSORS_LM80_FAN2_DIV, -1 } },
  { fmtTemps_LM80, SENSORS_LM80_TEMP,
    { SENSORS_LM80_TEMP, SENSORS_LM80_TEMP_HOT_MAX, SENSORS_LM80_TEMP_HOT_HYST, SENSORS_LM80_TEMP_OS_MAX, SENSORS_LM80_TEMP_OS_HYST, -1 } },
  { NULL }
};

/** W83781D **/

static const char *w83781d_names[] = {
  SENSORS_W83781D_PREFIX, NULL
};

static FeatureDescriptor w83781d_features[] = {
  { fmtVolts_2, SENSORS_W83781D_IN0,
    { SENSORS_W83781D_IN0, SENSORS_W83781D_IN0_MIN, SENSORS_W83781D_IN0_MAX, -1 } },
  { fmtVolts_2, SENSORS_W83781D_IN1,
    { SENSORS_W83781D_IN1, SENSORS_W83781D_IN1_MIN, SENSORS_W83781D_IN1_MAX, -1 } },
  { fmtVolts_2, SENSORS_W83781D_IN2,
    { SENSORS_W83781D_IN2, SENSORS_W83781D_IN2_MIN, SENSORS_W83781D_IN2_MAX, -1 } },
  { fmtVolts_2, SENSORS_W83781D_IN3,
    { SENSORS_W83781D_IN3, SENSORS_W83781D_IN3_MIN, SENSORS_W83781D_IN3_MAX, -1 } },
  { fmtVolts_2, SENSORS_W83781D_IN4,
    { SENSORS_W83781D_IN4, SENSORS_W83781D_IN4_MIN, SENSORS_W83781D_IN4_MAX, -1 } },
  { fmtVolts_2, SENSORS_W83781D_IN5,
    { SENSORS_W83781D_IN5, SENSORS_W83781D_IN5_MIN, SENSORS_W83781D_IN5_MAX, -1 } },
  { fmtVolts_2, SENSORS_W83781D_IN6,
    { SENSORS_W83781D_IN6, SENSORS_W83781D_IN6_MIN, SENSORS_W83781D_IN6_MAX, -1 } },
  { fmtFans_0, SENSORS_W83781D_FAN1,
    { SENSORS_W83781D_FAN1, SENSORS_W83781D_FAN1_MIN, SENSORS_W83781D_FAN1_DIV, -1 } },
  { fmtFans_0, SENSORS_W83781D_FAN2,
    { SENSORS_W83781D_FAN2, SENSORS_W83781D_FAN2_MIN, SENSORS_W83781D_FAN2_DIV, -1 } },
  { fmtFans_0, SENSORS_W83781D_FAN3,
    { SENSORS_W83781D_FAN3, SENSORS_W83781D_FAN3_MIN, SENSORS_W83781D_FAN3_DIV, -1 } },
  { fmtTemps_1, SENSORS_W83781D_TEMP1,
    { SENSORS_W83781D_TEMP1, SENSORS_W83781D_TEMP1_OVER, SENSORS_W83781D_TEMP1_HYST, -1 } },
  { fmtTemps_1, SENSORS_W83781D_TEMP2,
    { SENSORS_W83781D_TEMP2, SENSORS_W83781D_TEMP2_OVER, SENSORS_W83781D_TEMP2_HYST, -1 } },
  { fmtTemps_1, SENSORS_W83781D_TEMP3,
    { SENSORS_W83781D_TEMP3, SENSORS_W83781D_TEMP3_OVER, SENSORS_W83781D_TEMP3_HYST, -1 } },
  { fmtVolt_2, SENSORS_W83781D_VID,
    { SENSORS_W83781D_VID, -1 } },
  { NULL }
};

/** W83782D **/

static char *
fmtTemps_W8378x
(double values[]) {
  int sensorID = (int) values[3];
  char *sensor = (sensorID == 1) ? "PII/Celeron diode" : (sensorID == 2) ? "3904 transistor" : "thermistor"; /* Is this still right? */
  sprintf (buff, "%.1f C (limit = %.1f C, hysteresis = %.1f C, sensor = %s)", values[0], values[1], values[2], sensor);
  return buff;
}

static const char *w83782d_names[] = {
  SENSORS_W83782D_PREFIX, SENSORS_W83627HF_PREFIX, SENSORS_AS99127F_PREFIX, NULL
};

static FeatureDescriptor w83782d_features[] = {
  { fmtVolts_2, SENSORS_W83782D_IN0,
    { SENSORS_W83782D_IN0, SENSORS_W83782D_IN0_MIN, SENSORS_W83782D_IN0_MAX, -1 } },
  { fmtVolts_2, SENSORS_W83782D_IN1,
    { SENSORS_W83782D_IN1, SENSORS_W83782D_IN1_MIN, SENSORS_W83782D_IN1_MAX, -1 } },
  { fmtVolts_2, SENSORS_W83782D_IN2,
    { SENSORS_W83782D_IN2, SENSORS_W83782D_IN2_MIN, SENSORS_W83782D_IN2_MAX, -1 } },
  { fmtVolts_2, SENSORS_W83782D_IN3,
    { SENSORS_W83782D_IN3, SENSORS_W83782D_IN3_MIN, SENSORS_W83782D_IN3_MAX, -1 } },
  { fmtVolts_2, SENSORS_W83782D_IN4,
    { SENSORS_W83782D_IN4, SENSORS_W83782D_IN4_MIN, SENSORS_W83782D_IN4_MAX, -1 } },
  { fmtVolts_2, SENSORS_W83782D_IN5,
    { SENSORS_W83782D_IN5, SENSORS_W83782D_IN5_MIN, SENSORS_W83782D_IN5_MAX, -1 } },
  { fmtVolts_2, SENSORS_W83782D_IN7,
    { SENSORS_W83782D_IN7, SENSORS_W83782D_IN7_MIN, SENSORS_W83782D_IN7_MAX, -1 } },
  { fmtVolts_2, SENSORS_W83782D_IN8,
    { SENSORS_W83782D_IN8, SENSORS_W83782D_IN8_MIN, SENSORS_W83782D_IN8_MAX, -1 } },
  { fmtFans_0, SENSORS_W83782D_FAN1,
    { SENSORS_W83782D_FAN1, SENSORS_W83782D_FAN1_MIN, SENSORS_W83782D_FAN1_DIV, -1 } },
  { fmtFans_0, SENSORS_W83782D_FAN2,
    { SENSORS_W83782D_FAN2, SENSORS_W83782D_FAN2_MIN, SENSORS_W83782D_FAN2_DIV, -1 } },
  { fmtFans_0, SENSORS_W83782D_FAN3,
    { SENSORS_W83782D_FAN3, SENSORS_W83782D_FAN3_MIN, SENSORS_W83782D_FAN3_DIV, -1 } },
  { fmtTemps_W8378x, SENSORS_W83782D_TEMP1,
    { SENSORS_W83782D_TEMP1, SENSORS_W83782D_TEMP1_OVER, SENSORS_W83782D_TEMP1_HYST, SENSORS_W83782D_SENS1, -1 } },
  { fmtTemps_W8378x, SENSORS_W83782D_TEMP2,
    { SENSORS_W83782D_TEMP2, SENSORS_W83782D_TEMP2_OVER, SENSORS_W83782D_TEMP2_HYST, SENSORS_W83782D_SENS2, -1 } },
  { fmtTemps_W8378x, SENSORS_W83782D_TEMP3,
    { SENSORS_W83782D_TEMP3, SENSORS_W83782D_TEMP3_OVER, SENSORS_W83782D_TEMP3_HYST, SENSORS_W83782D_SENS3, -1 } },
  { fmtVolt_2, SENSORS_W83782D_VID,
    { SENSORS_W83782D_VID, -1 } },
  { NULL }
};

/** W83783S **/

static const char *w83783s_names[] = {
  SENSORS_W83783S_PREFIX, NULL
};

static FeatureDescriptor w83783s_features[] = {
  { fmtVolts_2, SENSORS_W83783S_IN0,
    { SENSORS_W83783S_IN0, SENSORS_W83783S_IN0_MIN, SENSORS_W83783S_IN0_MAX, -1 } },
  { fmtVolts_2, SENSORS_W83783S_IN1,
    { SENSORS_W83783S_IN1, SENSORS_W83783S_IN1_MIN, SENSORS_W83783S_IN1_MAX, -1 } },
  { fmtVolts_2, SENSORS_W83783S_IN2,
    { SENSORS_W83783S_IN2, SENSORS_W83783S_IN2_MIN, SENSORS_W83783S_IN2_MAX, -1 } },
  { fmtVolts_2, SENSORS_W83783S_IN3,
    { SENSORS_W83783S_IN3, SENSORS_W83783S_IN3_MIN, SENSORS_W83783S_IN3_MAX, -1 } },
  { fmtVolts_2, SENSORS_W83783S_IN4,
    { SENSORS_W83783S_IN4, SENSORS_W83783S_IN4_MIN, SENSORS_W83783S_IN4_MAX, -1 } },
  { fmtVolts_2, SENSORS_W83783S_IN5,
    { SENSORS_W83783S_IN5, SENSORS_W83783S_IN5_MIN, SENSORS_W83783S_IN5_MAX, -1 } },
  { fmtFans_0, SENSORS_W83783S_FAN1,
    { SENSORS_W83783S_FAN1, SENSORS_W83783S_FAN1_MIN, SENSORS_W83783S_FAN1_DIV, -1 } },
  { fmtFans_0, SENSORS_W83783S_FAN2,
    { SENSORS_W83783S_FAN2, SENSORS_W83783S_FAN2_MIN, SENSORS_W83783S_FAN2_DIV, -1 } },
  { fmtFans_0, SENSORS_W83783S_FAN3,
    { SENSORS_W83783S_FAN3, SENSORS_W83783S_FAN3_MIN, SENSORS_W83783S_FAN3_DIV, -1 } },
  { fmtTemps_W8378x, SENSORS_W83783S_TEMP1,
    { SENSORS_W83783S_TEMP1, SENSORS_W83783S_TEMP1_OVER, SENSORS_W83783S_TEMP1_HYST, SENSORS_W83783S_SENS1, -1 } },
  { fmtTemps_W8378x, SENSORS_W83783S_TEMP2,
    { SENSORS_W83783S_TEMP2, SENSORS_W83783S_TEMP2_OVER, SENSORS_W83783S_TEMP2_HYST, SENSORS_W83783S_SENS2, -1 } },
  { fmtVolt_2, SENSORS_W83783S_VID,
    { SENSORS_W83783S_VID, -1 } },
  { NULL }
};

/** MAXILIFE **/

static char *
fmtFans_Maxilife
(double values[]) {
  if (values[0] < 0) {
    sprintf (buff, "Off (min = %.0f RPM, div = %.0f)", values[1], values[2]); 
  } else {
    sprintf (buff, "%.0f RPM (min = %.0f RPM, div = %.0f)", values[0] / values[2], values[1] / values[2], values[2]); 
  }
  return buff;
}

static const char *maxilife_names[] = {
  SENSORS_MAXI_CG_PREFIX, SENSORS_MAXI_CO_PREFIX, SENSORS_MAXI_AS_PREFIX, NULL
};

static FeatureDescriptor maxilife_features[] = {
  { fmtTemps_1, SENSORS_MAXI_CG_TEMP1,
    { SENSORS_MAXI_CG_TEMP1, SENSORS_MAXI_CG_TEMP1_MAX, SENSORS_MAXI_CG_TEMP1_HYST, -1 } },
  { fmtTemps_1, SENSORS_MAXI_CG_TEMP2,
    { SENSORS_MAXI_CG_TEMP2, SENSORS_MAXI_CG_TEMP2_MAX, SENSORS_MAXI_CG_TEMP2_HYST, -1 } },
  { fmtTemps_1, SENSORS_MAXI_CG_TEMP3,
    { SENSORS_MAXI_CG_TEMP3, SENSORS_MAXI_CG_TEMP3_MAX, SENSORS_MAXI_CG_TEMP3_HYST, -1 } },
  { fmtTemps_1, SENSORS_MAXI_CG_TEMP4,
    { SENSORS_MAXI_CG_TEMP4, SENSORS_MAXI_CG_TEMP4_MAX, SENSORS_MAXI_CG_TEMP4_HYST, -1 } },
  { fmtTemps_1, SENSORS_MAXI_CG_TEMP4,
    { SENSORS_MAXI_CG_TEMP5, SENSORS_MAXI_CG_TEMP5_MAX, SENSORS_MAXI_CG_TEMP4_HYST, -1 } },
  { fmtFans_Maxilife, SENSORS_MAXI_CG_FAN1,
    { SENSORS_MAXI_CG_FAN1, SENSORS_MAXI_CG_FAN1_MIN, SENSORS_MAXI_CG_FAN1_DIV, -1 } },
  { fmtFans_Maxilife, SENSORS_MAXI_CG_FAN2,
    { SENSORS_MAXI_CG_FAN2, SENSORS_MAXI_CG_FAN2_MIN, SENSORS_MAXI_CG_FAN2_DIV, -1 } },
  { fmtFans_Maxilife, SENSORS_MAXI_CG_FAN3,
    { SENSORS_MAXI_CG_FAN3, SENSORS_MAXI_CG_FAN3_MIN, SENSORS_MAXI_CG_FAN3_DIV, -1 } },
  { fmtMHz_2, SENSORS_MAXI_CG_PLL,
    { SENSORS_MAXI_CG_PLL, SENSORS_MAXI_CG_PLL_MIN, SENSORS_MAXI_CG_PLL_MAX, -1 } },
  { fmtVolts_2, SENSORS_MAXI_CG_VID1,
    { SENSORS_MAXI_CG_VID1, SENSORS_MAXI_CG_VID1_MIN, SENSORS_MAXI_CG_VID1_MAX, -1 } },
  { fmtVolts_2, SENSORS_MAXI_CG_VID2,
    { SENSORS_MAXI_CG_VID2, SENSORS_MAXI_CG_VID2_MIN, SENSORS_MAXI_CG_VID2_MAX, -1 } },
  { fmtVolts_2, SENSORS_MAXI_CG_VID3,
    { SENSORS_MAXI_CG_VID3, SENSORS_MAXI_CG_VID3_MIN, SENSORS_MAXI_CG_VID3_MAX, -1 } },
  { fmtVolts_2, SENSORS_MAXI_CG_VID4,
    { SENSORS_MAXI_CG_VID4, SENSORS_MAXI_CG_VID4_MIN, SENSORS_MAXI_CG_VID4_MAX, -1 } },
  { NULL }
};

/** ALL **/

ChipDescriptor knownChips[] = {
  { lm75_names, lm75_features },
  { adm1021_names, adm1021_features },
  { max1617_names, max1617_features },
  { adm9240_names, adm9240_features },
  { lm78_names, lm78_features },
  { sis5595_names, sis5595_features },
  { lm80_names, lm80_features },
  { gl518_names, gl518_features },
  { w83781d_names, w83781d_features },
  { w83782d_names, w83782d_features },
  { w83783s_names, w83783s_features },
  { maxilife_names, maxilife_features },
  { NULL }
};
