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

#define version "0.5.0"

#include "lib/sensors.h"

extern void sensorLog (int priority, const char *fmt, ...);

/* from args.c */

extern int isDaemon;
extern const char *sensorsCfgFile;
extern const char *pidFile;
extern int scanTime;
extern int logTime;
extern int syslogFacility;
extern int doScan;
extern int doSet;
extern int debug;
extern sensors_chip_name chipNames[];
extern int numChipNames;

extern int parseArgs (int argc, char **argv);
extern int parseChips (int argc, char **argv);

/* from lib.c */

extern int initLib (void);
extern int loadLib (void);
extern int reloadLib (void);
extern int unloadLib (void);

/* from sense.c */

extern int readChips (void);
extern int scanChips (void);
extern int setChips (void);

/* from chips.c */

#define MAX_DATA 5

typedef const char * (*FormatterFN) (const double values[], int alarm, int beep);

typedef struct {
  FormatterFN format;
  int alarmMask;
  int beepMask;
  const int dataNumbers[MAX_DATA + 1]; /* First entry is used for the label */
} FeatureDescriptor;

typedef struct {
  const char * const *names;
  const FeatureDescriptor *features;
  int alarmNumber;
  int beepNumber;
} ChipDescriptor;

extern const ChipDescriptor * const knownChips[];
