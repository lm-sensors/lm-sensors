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
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

#include "sensord.h"
#include "lib/error.h"

static const char *sensorsCfgPaths[] = {
  "/etc", "/usr/local/etc", "/usr/lib/sensors", "/usr/local/lib/sensors", "/usr/lib", "/usr/local/lib", NULL
};

#define CFG_PATH_LEN 4096

static char cfgPath[CFG_PATH_LEN + 1];

static time_t cfgLastModified;

int
initLib
(void) {
  cfgPath[CFG_PATH_LEN] = '\0';
  if (!strcmp (sensorsCfgFile, "-")) {
    strncpy (cfgPath, sensorsCfgFile, CFG_PATH_LEN);
  } else if (sensorsCfgFile[0] == '/') {
    strncpy (cfgPath, sensorsCfgFile, CFG_PATH_LEN);
  } else if (strchr (sensorsCfgFile, '/')) {
    char *cwd = getcwd (NULL, 0);
    snprintf (cfgPath, CFG_PATH_LEN, "%s/%s", cwd, sensorsCfgFile);
    free (cwd);
  } else {
    int index0;
    struct stat stats;
    for (index0 = 0; sensorsCfgPaths[index0]; ++ index0) {
      snprintf (cfgPath, CFG_PATH_LEN, "%s/%s", sensorsCfgPaths[index0], sensorsCfgFile);
      if (stat (cfgPath, &stats) == 0)
        break;
    }
    if (!sensorsCfgPaths[index0]) {
      sensorLog (LOG_ERR, "Error locating sensors configuration file: %s", sensorsCfgFile);
      return 9;
    }
  }
  return 0;
}

static int
loadConfig
(int reload) {
  struct stat stats;
  FILE *cfg = NULL;
  int ret = 0;

  if (!strcmp (cfgPath, "-")) {
    if (!reload) {
      if ((ret = sensors_init (stdin))) {
        if (ret == -SENSORS_ERR_PROC)
          sensorLog (LOG_ERR, "Error reading /proc or /sys; modules probably not loaded");
        else
          sensorLog (LOG_ERR, "Error %d loading sensors configuration file: <stdin>", ret);
        ret = 12;
      }
    }
  } else if (stat (cfgPath, &stats) < 0) {
    sensorLog (LOG_ERR, "Error stating sensors configuration file: %s", cfgPath);
    ret = 10;
  } else if (!reload || (difftime (stats.st_mtime, cfgLastModified) > 0.0)) {
    if (reload)
      sensorLog (LOG_INFO, "configuration reloading");
    if (!(cfg = fopen (cfgPath, "r"))) {
      sensorLog (LOG_ERR, "Error opening sensors configuration file: %s", cfgPath);
      ret = 11;
    } else if ((ret = sensors_init (cfg))) {
      if (ret == -SENSORS_ERR_PROC)
        sensorLog (LOG_ERR, "Error reading /proc or /sys; modules probably not loaded");
      else
        sensorLog (LOG_ERR, "Error %d loading sensors configuration file: %s", ret, cfgPath);
      ret = 11;
    } else {
      cfgLastModified = stats.st_mtime;
    }
    if (cfg)
      fclose (cfg);
  }

  return ret;
}

int
loadLib
(void) {
  return loadConfig (0);
}

int
reloadLib
(void) {
  return loadConfig (1);
}

int
unloadLib
(void) {
  sensors_cleanup ();
  return 0;  
}
