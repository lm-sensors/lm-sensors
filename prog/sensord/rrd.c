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

/*
 * RRD is the Round Robin Database
 * 
 * Get this package from:
 *   http://people.ee.ethz.ch/~oetiker/webtools/rrdtool/
 *
 * For compilation you need the development libraries;
 * for execution you need the runtime libraries; for
 * Web-based graph access you need the binary rrdtool.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <getopt.h>
#include <rrd.h>

#include "sensord.h"
#include "lib/error.h"

#define DO_READ 0
#define DO_SCAN 1
#define DO_SET 2
#define DO_RRD 3

/* one integer */
#define STEP_BUFF 64
/* RRA:AVERAGE:0.5:1:12345 */
#define RRA_BUFF 256
/* weak: max sensors for RRD .. TODO: fix */
#define MAX_RRD_SENSORS 256
/* weak: max raw label length .. TODO: fix */
#define RAW_LABEL_LENGTH 32
/* DS:label:GAUGE:900:U:U | :3000 .. TODO: fix */
#define RRD_BUFF 64

char rrdBuff[MAX_RRD_SENSORS * RRD_BUFF + 1];
static char rrdLabels[MAX_RRD_SENSORS][RAW_LABEL_LENGTH + 1];

#define LOADAVG "loadavg"
#define LOAD_AVERAGE "Load Average"

typedef int (*FeatureFN) (void *data, const char *rawLabel, const char *label, const FeatureDescriptor *feature);

static char
rrdNextChar
(char c) {
  if (c == '9') {
    return 'A';
  } else if (c == 'Z') {
    return 'a';
  } else if (c == 'z') {
    return 0;
  } else {
    return c + 1;
  }
}

static void
rrdCheckLabel
(const char *rawLabel, int index) {
  char *buffer = rrdLabels[index];
  int i, j, okay;
  
  i = 0;
  while ((i < RAW_LABEL_LENGTH) && rawLabel[i]) { /* contrain raw label to [A-Za-z0-9_] */
    char c = rawLabel[i];
    if (((c >= 'A') && (c <= 'Z')) || ((c >= 'a') && (c <= 'z')) || ((c >= '0') && (c <= '9')) || (c == '_')) {
      buffer[i] = c;
    } else {
      buffer[i] = '_';
    }
    ++ i;
  }
  buffer[i] = '\0';

  j = 0;
  okay = (i > 0);
  while (okay && (j < index)) /* locate duplicates */
    okay = strcmp (rrdLabels[j ++], buffer);

  while (!okay) { /* uniquify duplicate labels with _? or _?? */
    if (!buffer[i]) {
      if (i > RAW_LABEL_LENGTH - 3)
        i = RAW_LABEL_LENGTH - 3;
      buffer[i] = '_';
      buffer[i + 1] = '0';
      buffer[i + 2] = '\0';
    } else if (!buffer[i + 2]) {
      if (!(buffer[i + 1] = rrdNextChar (buffer[i + 1]))) {
        buffer[i + 1] = '0';
        buffer[i + 2] = '0';
        buffer[i + 3] = '\0';
      }
    } else {
      if (!(buffer[i + 2] = rrdNextChar (buffer[i + 2]))) {
        buffer[i + 1] = rrdNextChar (buffer[i + 1]);
        buffer[i + 2] = '0';
      }
    }
    j = 0;
    okay = 1;
    while (okay && (j < index))
      okay = strcmp (rrdLabels[j ++], buffer);
  }
}

static int
applyToFeatures
(FeatureFN fn, void *data) {
  const sensors_chip_name *chip;
  int i = 0, j, ret = 0, num = 0;

  while ((ret == 0) && ((chip = sensors_get_detected_chips (&i)) != NULL)) {
    for (j = 0; (ret == 0) && (j < numChipNames); ++ j) {
      if (sensors_match_chip (*chip, chipNames[j])) {
        int index0, subindex, chipindex = -1;
        for (index0 = 0; knownChips[index0]; ++ index0)
          for (subindex = 0; knownChips[index0]->names[subindex]; ++ subindex)
            if (!strcmp (chip->prefix, knownChips[index0]->names[subindex]))
              chipindex = index0;
        if (chipindex >= 0) {
          const ChipDescriptor *descriptor = knownChips[chipindex];
          const FeatureDescriptor *features = descriptor->features;

          for (index0 = 0; (ret == 0) && (num < MAX_RRD_SENSORS) && features[index0].format; ++ index0) {
            const FeatureDescriptor *feature = features + index0;
            int labelNumber = feature->dataNumbers[0];
            const char *rawLabel = NULL;
            char *label = NULL;
            int valid = 0;
            if (getValid (*chip, labelNumber, &valid)) {
              sensorLog (LOG_ERR, "Error getting sensor validity: %s/#%d", chip->prefix, labelNumber);
              ret = -1;
            } else if (getRawLabel (*chip, labelNumber, &rawLabel)) {
              sensorLog (LOG_ERR, "Error getting raw sensor label: %s/#%d", chip->prefix, labelNumber);
              ret = -1;
            } else if (getLabel (*chip, labelNumber, &label)) {
              sensorLog (LOG_ERR, "Error getting sensor label: %s/#%d", chip->prefix, labelNumber);
              ret = -1;
            } else if (valid) {
              rrdCheckLabel (rawLabel, num);
              ret = fn (data, rrdLabels[num], label, feature);
              ++ num;
            }
            if (label)
              free (label);
          }
        }
      }
    }
  }

  return ret;
}

struct ds {
  int num;
  const char **argv;
};

static int
rrdGetSensors_DS
(void *_data, const char *rawLabel, const char *label, const FeatureDescriptor *feature) {
  if (!feature || feature->rrd) {
    struct ds *data = (struct ds *) _data;
    char *ptr = rrdBuff + data->num * RRD_BUFF;
    const char *min, *max;
    data->argv[data->num ++] = ptr;
    switch (feature ? feature->type : DataType_other) { /* arbitrary sanity limits */
      case DataType_voltage:
        min="-25";
        max="25";
        break;
      case DataType_rpm:
        min = "0";
        max = "12000";
        break;
      case DataType_temperature:
        min = "0";
        max = "250";
        break;
      case DataType_mhz:
        min = "0";
        max = "U";
        break;
      default:
        min = max = "U";
        break;
    }
    sprintf (ptr, "DS:%s:GAUGE:%d:%s:%s", rawLabel, /* number of seconds downtime during which average be used instead of unknown */ 5 * rrdTime, min, max);
  }
  return 0;
}

static int
rrdGetSensors
(const char **argv) {
  int ret = 0;
  struct ds data = { 0, argv};
  ret = applyToFeatures (rrdGetSensors_DS, &data);
  if (!ret && doLoad)
    ret = rrdGetSensors_DS (&data, LOADAVG, LOAD_AVERAGE, NULL);
  return ret ? -1 : data.num;
}

int
rrdInit
(void) {
  int ret = 0;
  struct stat tmp;
  
  sensorLog (LOG_DEBUG, "sensor RRD init"); 
  if (stat (rrdFile, &tmp)) {
    if (errno == ENOENT) {
      char stepBuff[STEP_BUFF], rraBuff[RRA_BUFF];
      int argc = 4, num;
      const char *argv[6 + MAX_RRD_SENSORS] = {
        "sensord", rrdFile, "-s", stepBuff
      };
      
      sensorLog (LOG_INFO, "creating round robin database");
      num = rrdGetSensors (argv + argc);
      if (num == 0) {
        sensorLog (LOG_ERR, "Error creating RRD: %s: %s", rrdFile, "No sensors detected");
        ret = 2;
      } else if (num < 0) {
        ret = -num;
      } else {
        sprintf (stepBuff, "%d", rrdTime);
        sprintf (rraBuff, "RRA:%s:%f:%d:%d", rrdNoAverage?"LAST":"AVERAGE", 0.5 /* fraction of non-unknown samples needed per entry */, 1 /* samples per entry */, 7 * 24 * 60 * 60 / rrdTime /* 1 week */);
        argc += num;
        argv[argc ++] = rraBuff;
        argv[argc] = NULL;
        optind = 1;
        opterr = 0;
        optopt = '?';
        optarg = NULL;
        if ((ret = rrd_create (argc, (char **) /* WEAK */ argv))) {
          sensorLog (LOG_ERR, "Error creating RRD file: %s: %s", rrdFile, rrd_get_error ());
        }
      }
    } else {
      sensorLog (LOG_ERR, "Error stat()ing RRD: %s: %s", rrdFile, strerror (errno));
      ret = 1;
    }
  }
  sensorLog (LOG_DEBUG, "sensor RRD inited"); 
  
  return ret;
}

#define RRDCGI "/usr/bin/rrdcgi"
#define WWWDIR "/sensord"

struct gr {
  DataType type;
  char *h2;
  char *image;
  char *title;
  char *axisTitle;
  char *axisDefn;
  char *options;
  int loadAvg;
};

static int
rrdCGI_DEF
(void *_data, const char *rawLabel, const char *label, const FeatureDescriptor *feature) {
  struct gr *data = (struct gr *) _data;
  if (!feature || (feature->rrd && (feature->type == data->type)))
    printf ("\n\tDEF:%s=%s:%s:AVERAGE", rawLabel, rrdFile, rawLabel);
  return 0;
}

static int
rrdCGI_LINE
(void *_data, const char *rawLabel, const char *label, const FeatureDescriptor *feature) {
  struct gr *data = (struct gr *) _data;
  if (!feature || (feature->rrd && (feature->type == data->type)))
    printf ("\n\tLINE2:%s#%.6x:\"%s\"", rawLabel, (int) random () & 0xffffff, label);
  return 0;
}

static struct gr graphs[] = {
  {
    DataType_temperature,
    "Daily Temperature Summary",
    "daily-temperature",
    "Temperature",
    "Temperature (C)",
    "HOUR:1:HOUR:3:HOUR:3:0:%b %d %H:00",
    "-s -1d -l 0",
    1
  }, {
    DataType_rpm,
    "Daily Fan Speed Summary",
    "daily-rpm",
    "Fan Speed",
    "Speed (RPM)",
    "HOUR:1:HOUR:3:HOUR:3:0:%b %d %H:00",
    "-s -1d -l 0",
    0
  }, {
    DataType_voltage,
    "Daily Voltage Summary",
    "daily-voltage",
    "Power Supply",
    "Voltage (V)",
    "HOUR:1:HOUR:3:HOUR:3:0:%b %d %H:00",
    "-s -1d --alt-autoscale",
    0
  }, {
    DataType_temperature,
    "Weekly Temperature Summary",
    "weekly-temperature",
    "Temperature",
    "Temperature (C)",
    "HOUR:6:DAY:1:DAY:1:86400:%a %b %d",
    "-s -1w -l 0",
    1
  }, {
    DataType_rpm,
    "Weekly Fan Speed Summary",
    "weekly-rpm",
    "Fan Speed",
    "Speed (RPM)",
    "HOUR:6:DAY:1:DAY:1:86400:%a %b %d",
    "-s -1w -l 0",
    0
  }, {
    DataType_voltage,
    "Weekly Voltage Summary",
    "weekly-voltage",
    "Power Supply",
    "Voltage (V)",
    "HOUR:6:DAY:1:DAY:1:86400:%a %b %d",
    "-s -1w --alt-autoscale",
    0
  }, {
    DataType_other
  }  
};

int
rrdUpdate
(void) {
  int ret = rrdChips ();
  if (!ret && doLoad) {
    FILE *loadavg;
    if (!(loadavg = fopen ("/proc/loadavg", "r"))) {
      sensorLog (LOG_ERR, "Error opening `/proc/loadavg': %s", strerror (errno));
      ret = 1;
    } else {
      float value;
      if (fscanf (loadavg, "%f", &value) != 1) {
        sensorLog (LOG_ERR, "Error reading load average");
        ret = 2;
      } else {
        sprintf (rrdBuff + strlen (rrdBuff), ":%f", value);
      }
      fclose (loadavg);
    }
  }
  if (!ret) {
    const char *argv[] = {
      "sensord", rrdFile, rrdBuff, NULL
    };
    optind = 1;
    opterr = 0;
    optopt = '?';
    optarg = NULL;
    if ((ret = rrd_update (3, (char **) /* WEAK */ argv))) {
      sensorLog (LOG_ERR, "Error updating RRD file: %s: %s", rrdFile, rrd_get_error ());
    }
  }
  sensorLog (LOG_DEBUG, "sensor rrd updated"); 
  
  return ret;
}

int
rrdCGI
(void) {
  int ret = 0;
  struct gr *graph = graphs;

  printf ("#!" RRDCGI "\n\n<HTML>\n<HEAD>\n<TITLE>sensord</TITLE>\n</HEAD>\n<BODY>\n<H1>sensord</H1>\n");
  while (graph->type != DataType_other) {
    printf ("<H2>%s</H2>\n", graph->h2);
    printf ("<P>\n<RRD::GRAPH %s/%s.png\n\t--imginfo '<IMG SRC=" WWWDIR "/%%s WIDTH=%%lu HEIGHT=%%lu>'\n\t-a PNG\n\t-h 200 -w 800\n", cgiDir, graph->image);
    printf ("\t--lazy\n\t-v '%s'\n\t-t '%s'\n\t-x '%s'\n\t%s", graph->axisTitle, graph->title, graph->axisDefn, graph->options);
    if (!ret)
      ret = applyToFeatures (rrdCGI_DEF, graph);
    if (!ret && doLoad && graph->loadAvg)
      ret = rrdCGI_DEF (graph, LOADAVG, LOAD_AVERAGE, NULL);
    if (!ret)
      ret = applyToFeatures (rrdCGI_LINE, graph);
    if (!ret && doLoad && graph->loadAvg)
      ret = rrdCGI_LINE (graph, LOADAVG, LOAD_AVERAGE, NULL);
    printf (">\n</P>\n");
    ++ graph;
  }
  printf ("<p>\n<small><b>sensord</b> by <a href=\"mailto:merlin@merlin.org\">Merlin Hughes</a>, all credit to the <a href=\"http://www.lm-sensors.nu/\">lm_sensors</a> crew.</small>\n</p>\n");
  printf ("</BODY>\n</HTML>\n");
  
  return ret;
}
