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

/** ALL **/

const ChipDescriptor * const knownChips[] = {
  NULL
};
