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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include "lib/sensors.h"

#include "sensord.h"

static char *version = "0.2.0";

static char *sensorsCfgPaths[] = {
  "/etc", "/usr/lib/sensors", "/usr/local/lib/sensors", "/usr/lib", "/usr/local/lib", NULL
};
static char *sensorsCfgFile = "sensors.conf";
static char *sensorsLogFile = "/var/log/sensors";
static int sleepTime = 5 * 60;

static int cfgLoaded = 0;
time_t cfgLastModified;

static FILE *sensorsLog = NULL;

static volatile sig_atomic_t done = 0;
static volatile sig_atomic_t rotate = 0;

static void
signalHandler
(int sig) {
  signal (sig, signalHandler);
  switch (sig) {
    case SIGHUP:
      rotate = 1;
      break;
    case SIGTERM:
      done = 1;
      break;
  }
}

static void
initSignals
(void) {
  /* I should use sigaction but... */
  signal (SIGHUP, signalHandler);
  signal (SIGTERM, signalHandler);
}

static char *
now
(void) {
  time_t now;
  char *str;
  int len;
  static char buff[1024];

  time (&now);
  str = ctime (&now);
  len = strlen (str);
  memcpy (buff, str, len - 1);
  buff[len - 1] = '\0';

  return buff;
}

static int
readUnknownChip
(const sensors_chip_name *chip) {
  const sensors_feature_data *sensor;
  int index0 = 0, index1 = 0;
  int ret = 0;

  while (!done && (ret == 0) && ((sensor = sensors_get_all_features (*chip, &index0, &index1)) != NULL)) {
    char *label = NULL;
    double value;
    
    if (sensors_get_label (*chip, sensor->number, &label)) {
      syslog (LOG_ERR, "Error getting sensor label: %s/%s", chip->prefix, sensor->name);
      ret = 20;
    } else if (!(sensor->mode & SENSORS_MODE_R)) {
      fprintf (sensorsLog, "\t%s: %s\n", sensor->name, label);
    } else if (sensors_get_feature (*chip, sensor->number, &value)) {
      syslog (LOG_ERR, "Error getting sensor data: %s/%s", chip->prefix, sensor->name);
      ret = 21;
    } else {
      fprintf (sensorsLog, "\t%s%s: %s: %.2f\n", (sensor->mapping == SENSORS_NO_MAPPING) ? "" : "-", sensor->name, label, value);
    }
    if (label)
      free (label);
  }
  
  return ret;
}

static int
readKnownChip
(const sensors_chip_name *chip, ChipDescriptor *descriptor) {
  FeatureDescriptor *features = descriptor->features;
  int index, subindex;
  int ret = 0;

  for (index = 0; !done && (ret == 0) && features[index].format; ++ index) {
    FeatureDescriptor *feature = features + index;
    char *label = NULL;
    double values[MAX_DATA];
    
    if (sensors_get_label (*chip, feature->labelNumber, &label)) {
      syslog (LOG_ERR, "Error getting sensor label: %s/#%d", chip->prefix, feature->labelNumber);
      ret = 20;
    }
    for (subindex = 0; !done && (ret == 0) && (feature->dataNumbers[subindex] >= 0); ++ subindex) {
      if (sensors_get_feature (*chip, feature->dataNumbers[subindex], values + subindex)) {
        syslog (LOG_ERR, "Error getting sensor data: %s/#%d", chip->prefix, feature->dataNumbers[subindex]);
        ret = 21;
      }
    }
    if (!done && (ret == 0))
      fprintf (sensorsLog, "\t%s: %s\n"/*, sensor->name*/, label, feature->format (values));
    if (label)
      free (label);
  }
}

static int
readChip
(const sensors_chip_name *chip) {
  const char *adapter, *algorithm;
  int index, subindex, chipindex = -1;
  int ret = 0;

  if (chip->bus == SENSORS_CHIP_NAME_BUS_ISA)
    fprintf (sensorsLog, "%s: Chip: %s-isa-%04x\n", now (), chip->prefix, chip->addr);
  else
    fprintf (sensorsLog, "%s: Chip: %s-i2c-%d-%02x\n", now (), chip->prefix, chip->bus, chip->addr);
  adapter = sensors_get_adapter_name (chip->bus);
  if (adapter)
    fprintf (sensorsLog, "%s: Adapter: %s\n", now (), adapter);
  algorithm = sensors_get_algorithm_name (chip->bus);
  if (algorithm)
    fprintf (sensorsLog, "%s: Algorithm: %s\n", now (), algorithm);
  /* assert adapter || algorithm */

  for (index = 0; knownChips[index].names; ++ index)
    for (subindex = 0; knownChips[index].names[subindex]; ++ subindex)
      if (!strcmp (chip->prefix, knownChips[index].names[subindex]))
        chipindex = index;
  
  if (chipindex >= 0)
    ret = readKnownChip (chip, knownChips + chipindex);
  else
    ret = readUnknownChip (chip);

  return ret;
}

static int
readChips
(void) {
  const sensors_chip_name *chip;
  int index = 0;
  int ret = 0;

  fprintf (sensorsLog, "%s: Sense.\n", now ());
  fflush (sensorsLog);

  while (!done && (ret == 0) && ((chip = sensors_get_detected_chips (&index)) != NULL)) {
    ret = readChip (chip);
  }

  fprintf (sensorsLog, "%s: Done.\n", now ());
  fflush (sensorsLog);

  return ret;
}

static int
rotateLog
(void) {
  int ret = 0;

  if (sensorsLog != NULL) {
    fprintf (sensorsLog, "%s: Rotate.\n", now ());
    fclose (sensorsLog);
  }
  if (!(sensorsLog = fopen (sensorsLogFile, "a"))) {
    syslog (LOG_ERR, "Error opening sensors log: %s", sensorsLogFile);
    ret = 1;
  } else {
    fprintf (sensorsLog, "%s: Started.\n", now ());
    fflush (sensorsLog);
  }
  rotate = 0;

  return ret;
}

char cfgPath[4096] = "";

static int
initSensors
(void) {
  struct stat stats;
  FILE *cfg = NULL;
  int ret = 0;

  if (!cfgPath[0]) {
    if (sensorsCfgFile[0] == '/') {
      strcpy (cfgPath, sensorsCfgFile);
    } else {
      int index;
      for (index = 0; sensorsCfgPaths[index]; ++ index) {
        sprintf (cfgPath, "%s/%s", sensorsCfgPaths[index], sensorsCfgFile);
        if (stat (cfgPath, &stats) == 0)
          break;
      }
      if (!sensorsCfgPaths[index]) {
        syslog (LOG_ERR, "Error locating sensors configuration: %s", sensorsCfgFile);
        return 9;
      }
    }
  }
    
  if (stat (cfgPath, &stats) < 0) {
    syslog (LOG_ERR, "Error stating sensors configuration: %s", cfgPath);
    ret = 10;
  } else if (!cfgLoaded || (difftime (stats.st_mtime, cfgLastModified) > 0.0)) {
    if (!(cfg = fopen (cfgPath, "r"))) {
      syslog (LOG_ERR, "Error opening sensors configuration: %s", cfgPath);
      ret = 11;
    } else if (sensors_init (cfg)) {
      syslog (LOG_ERR, "Error loading sensors configuration: %s", cfgPath);
      ret = 11;
    } else {
      cfgLastModified = stats.st_mtime;
      cfgLoaded = 1;
    }
    if (cfg)
      fclose (cfg);
  }

  return ret;
}

static int
sensord
(void) {
  int ret = 0;

  openlog ("sensord", 0, LOG_DAEMON);
  
  initSignals ();

  while (!done && (ret == 0)) {
    if (rotate || (sensorsLog == NULL))
      ret = rotateLog ();
    if (ret == 0)
      ret = initSensors ();
    if (ret == 0)
      ret = readChips ();
    if (!done && !rotate && (ret == 0))
      sleep (sleepTime);
  }

  if (cfgLoaded)
    sensors_cleanup ();

  if (sensorsLog) {
    fprintf (sensorsLog, "%s: %s.\n", now (), ret ? "Failed" : "Stopped");
    fclose (sensorsLog);
  }

  closelog ();

  return ret;
}

static void
syntaxError
(void) {
  printf ("Syntax: sensord [options]\n"
          "\t-i <interval>     -- seconds between samples (default 300)\n"
          "\t-l <log-file>     -- log file (default /var/log/sensors)\n"
          "\t-c <config-file>  -- configuration file (default /etc/sensors.conf)\n"
          "\t-v                -- display version and exit\n"
          "\t-h                -- display help and exit\n");
  exit (EXIT_FAILURE);
}

static void
parseArgs
(int argc, char **argv) {
  int i;

  for (i = 1; i < argc; ++ i) {
    if (!strcmp (argv[i], "-i") && (i < argc - 1)) {
      sleepTime = atoi (argv[++ i]);
    } else if (!strcmp (argv[i], "-l") && (i < argc - 1)) {
      sensorsLogFile = argv[++ i];
    } else if (!strcmp (argv[i], "-c") && (i < argc - 1)) {
      sensorsCfgFile = argv[++ i];
    } else if (!strcmp (argv[i], "-v")) {
      printf ("sensord version %s\n", version);
      exit (EXIT_SUCCESS);
    } else {
      syntaxError ();
    }
  }
}

static void
daemonize
(void) {
  int pid;

  if ((pid = fork ()) == -1) {
    fprintf (stderr, "Error forking daemon\n");
    exit (EXIT_FAILURE);
  } else if (pid != 0) {
    exit (EXIT_SUCCESS);
  }

  if (chdir ("/") < 0) {
    perror ("chdir()");
    exit (EXIT_FAILURE);
  }

  if (setsid () < 0) {
    perror ("setsid()");
    exit (EXIT_FAILURE);
  }

  close (STDIN_FILENO);
  close (STDOUT_FILENO);
  close (STDERR_FILENO);
}

int
main
(int argc, char **argv) {
  parseArgs (argc, argv);
  daemonize ();
  return sensord ();
}
