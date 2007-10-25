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

static int
loadConfig
(const char *cfgPath, int reload) {
  struct stat stats;
  FILE *cfg = NULL;
  int ret = 0;

  if (cfgPath && !strcmp (cfgPath, "-")) {
    if (!reload) {
      if ((ret = sensors_init (stdin))) {
        sensorLog (LOG_ERR, "Error loading sensors configuration file <stdin>: %s",
                   sensors_strerror (ret));
        ret = 12;
      }
    }
  } else if (cfgPath && stat (cfgPath, &stats) < 0) {
    sensorLog (LOG_ERR, "Error stating sensors configuration file: %s", cfgPath);
    ret = 10;
  } else {
    if (reload) {
      sensorLog (LOG_INFO, "configuration reloading");
      sensors_cleanup ();
    }
    if (cfgPath && !(cfg = fopen (cfgPath, "r"))) {
      sensorLog (LOG_ERR, "Error opening sensors configuration file: %s", cfgPath);
      ret = 11;
    } else if ((ret = sensors_init (cfg))) {
      sensorLog (LOG_ERR, "Error loading sensors configuration file %s: %s",
                 cfgPath ? cfgPath : "(default)", sensors_strerror (ret));
      ret = 11;
    }
    if (cfg)
      fclose (cfg);
  }

  return ret;
}

int
loadLib
(const char *cfgPath) {
  int ret;
  ret = loadConfig (cfgPath, 0);
  if (!ret)
    ret = initKnownChips ();
  return ret;
}

int
reloadLib
(const char *cfgPath) {
  int ret;
  freeKnownChips ();
  ret = loadConfig (cfgPath, 1);
  if (!ret)
    ret = initKnownChips ();
  return ret;
}

int
unloadLib
(void) {
  freeKnownChips ();
  sensors_cleanup ();
  return 0;  
}
