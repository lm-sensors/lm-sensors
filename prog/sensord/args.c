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
#include <getopt.h>
#include <syslog.h>

#include "sensord.h"
#include "lib/error.h"

#define MAX_CHIP_NAMES 32

int isDaemon = 0;
const char *sensorsCfgFile = "sensors.conf";
const char *pidFile = "/var/run/sensord.pid";
const char *rrdFile = NULL;
const char *cgiDir = NULL;
int scanTime = 60;
int logTime = 30 * 60;
int rrdTime = 5 * 60;
int rrdNoAverage = 0;
int syslogFacility = LOG_LOCAL4;
int doScan = 0;
int doSet = 0;
int doCGI = 0;
int doLoad = 0;
int debug = 0;
sensors_chip_name chipNames[MAX_CHIP_NAMES];
int numChipNames = 0;

static int
parseTime
(char *arg) {
  char *end;
  int value = strtoul (arg, &end, 10);
  if ((end > arg) && (*end == 's')) {
    ++ end;
  } else if ((end > arg) && (*end == 'm')) {
    value *= 60;
    ++ end;
  } else if ((end > arg) && (*end == 'h')) {
    value *= 60 * 60;
    ++ end;
  }
  if ((end == arg) || *end) {
    fprintf (stderr, "Error parsing time value `%s'.\n", arg);
    return -1;
  }
  return value;
}

static struct {
  const char *name;
  int id;
} facilities[] = {
  { "local0", LOG_LOCAL0 }, { "local1", LOG_LOCAL1 },
  { "local2", LOG_LOCAL2 }, { "local3", LOG_LOCAL3 },
  { "local4", LOG_LOCAL4 }, { "local5", LOG_LOCAL5 },
  { "local6", LOG_LOCAL6 }, { "local7", LOG_LOCAL7 },
  { "daemon", LOG_DAEMON }, { "user", LOG_USER },
  { NULL }
};

static int
parseFacility
(char *arg) {
  int i = 0;
  while (facilities[i].name && strcasecmp (arg, facilities[i].name))
    ++ i;
  if (!facilities[i].name) {
    fprintf (stderr, "Error parsing facility value `%s'.\n", arg);
    return -1;
  }
  return facilities[i].id;
}

static const char *daemonSyntax =
  "  -i, --interval <time>     -- interval between scanning alarms (default 60s)\n"
  "  -l, --log-interval <time> -- interval between logging sensors (default 30m)\n"
  "  -t, --rrd-interval <time> -- interval between updating RRD file (default 5m)\n"
  "  -T, --rrd-no-average      -- switch RRD in non-average mode\n"
  "  -r, --rrd-file <file>     -- RRD file (default <none>)\n"
  "  -c, --config-file <file>  -- configuration file (default sensors.conf)\n"
  "  -p, --pid-file <file>     -- PID file (default /var/run/sensord.pid)\n"
  "  -f, --syslog-facility <f> -- syslog facility to use (default local4)\n"
  "  -g, --rrd-cgi <img-dir>   -- output an RRD CGI script and exit\n"
  "  -a, --load-average        -- include load average in RRD file\n"
  "  -d, --debug               -- display some debug information\n"
  "  -v, --version             -- display version and exit\n"
  "  -h, --help                -- display help and exit\n"
  "\n"
  "Specify a value of 0 for any interval to disable that operation;\n"
  "for example, specify --log-interval 0 to only scan for alarms."
  "\n"
  "If no path is specified, a list of directories is examined for the config file;\n"
  "specify the filename `-' to read the config file from stdin.\n"
  "\n"
  "If no chips are specified, all chip info will be printed.\n"
  "\n"
  "If unspecified, no RRD (round robin database) is used. If specified and the\n"
  "file does not exist, it will be created. For RRD updates to be successful,\n"
  "the RRD file configuration must EXACTLY match the sensors that are used. If\n"
  "your configuration changes, delete the old RRD file and restart sensord.\n";

static const char *appSyntax =
  "  -a, --alarm-scan          -- only scan for alarms\n"
  "  -s, --set                 -- execute set statements too (root only)\n"
  "  -r, --rrd-file <file>     -- only update RRD file\n"
  "  -c, --config-file <file>  -- configuration file (default sensors.conf)\n"
  "  -d, --debug               -- display some debug information\n"
  "  -v, --version             -- display version and exit\n"
  "  -h, --help                -- display help and exit\n"
  "\n"
  "If no path is specified, a list of directories is examined for the config file;\n"
  "specify the filename `-' to read the config file from stdin.\n"
  "\n"
  "If no chips are specified, all chip info will be printed.\n";

static const char *daemonShortOptions = "i:l:t:Tf:r:c:p:advhg:";

static const struct option daemonLongOptions[] = {
  { "interval", required_argument, NULL, 'i' },
  { "log-interval", required_argument, NULL, 'l' },
  { "rrd-interval", required_argument, NULL, 't' },
  { "rrd-no-average", no_argument, NULL, 'T' },
  { "syslog-facility", required_argument, NULL, 'f' },
  { "rrd-file", required_argument, NULL, 'r' },
  { "config-file", required_argument, NULL, 'c' },
  { "pid-file", required_argument, NULL, 'p' },
  { "rrd-cgi", required_argument, NULL, 'g' },
  { "load-average", no_argument, NULL, 'a' },
  { "debug", no_argument, NULL, 'd' },
  { "version", no_argument, NULL, 'v' },
  { "help", no_argument, NULL, 'h' },
  { NULL, 0, NULL, 0 }
};

static const char *appShortOptions = "asr:c:dvh";

static const struct option appLongOptions[] = {
  { "alarm-scan", no_argument, NULL, 'a' },
  { "set", no_argument, NULL, 's' },
  { "rrd-file", required_argument, NULL, 'r' },
  { "config-file", required_argument, NULL, 'c' },
  { "debug", no_argument, NULL, 'd' },
  { "version", no_argument, NULL, 'v' },
  { "help", no_argument, NULL, 'h' },
  { NULL, 0, NULL, 0 }
};

int
parseArgs
(int argc, char **argv) {
  int c;
  const char *shortOptions;
  const struct option *longOptions;

  isDaemon = (argv[0][strlen (argv[0]) - 1] == 'd');
  shortOptions = isDaemon ? daemonShortOptions : appShortOptions;
  longOptions = isDaemon ? daemonLongOptions : appLongOptions;

  while ((c = getopt_long (argc, argv, shortOptions, longOptions, NULL)) != EOF) {
    switch(c) {
      case 'i':
        if ((scanTime = parseTime (optarg)) < 0)
          return -1;
        break;
      case 'l':
        if ((logTime = parseTime (optarg)) < 0)
          return -1;
        break;
      case 't':
        if ((rrdTime = parseTime (optarg)) < 0)
          return -1;
        break;
      case 'T':
        rrdNoAverage = 1;
        break;
      case 'f':
        if ((syslogFacility = parseFacility (optarg)) < 0)
          return -1;
        break;
      case 'a':
        if (isDaemon)
          doLoad = 1;
        else
          doScan = 1;
        break;
      case 's':
        doSet = 1;
        break;
      case 'c':
        if ((sensorsCfgFile = strdup (optarg)) == NULL)
          return -1;
        break;
      case 'p':
        if ((pidFile = strdup (optarg)) == NULL)
          return -1;
        break;
      case 'r':
        if ((rrdFile = strdup (optarg)) == NULL)
          return -1;
        break;
      case 'd':
        debug = 1;
        break;
      case 'g':
        doCGI = 1;
        if ((cgiDir = strdup (optarg)) == NULL)
          return -1;
        break;
      case 'v':
        printf ("sensord version %s\n", version);
        exit (EXIT_SUCCESS);
        break;
      case 'h':
        printf ("Syntax: %s {options} {chips}\n%s", argv[0], isDaemon ? daemonSyntax : appSyntax);
        exit (EXIT_SUCCESS);
        break;
      case ':':
      case '?':
        printf ("Try `%s --help' for more information.\n", argv[0]);
        return -1;
        break;
      default:
        fprintf (stderr, "Internal error while parsing options.\n");
        return -1;
        break;
    }
  }

  if (doScan && doSet) {
    fprintf (stderr, "Error: Incompatible --set and --alarm-scan.\n");
    return -1;
  }
  
  if (rrdFile && doSet) {
    fprintf (stderr, "Error: Incompatible --set and --rrd-file.\n");
    return -1;
  }
  
  if (doScan && rrdFile) {
    fprintf (stderr, "Error: Incompatible --rrd-file and --alarm-scan.\n");
    return -1;
  }

  if (doCGI && !rrdFile) {
    fprintf (stderr, "Error: Incompatible --rrd-cgi without --rrd-file.\n");
    return -1;
  }
  
  if (rrdFile && !rrdTime) {
    fprintf (stderr, "Error: Incompatible --rrd-file without --rrd-interval.\n");
    return -1;
  }
  
  if (!logTime && !scanTime && !rrdFile) {
    fprintf (stderr, "Error: No logging, alarm or RRD scanning.\n");
    return -1;
  }

  return 0;
}

int
parseChips
(int argc, char **argv) {
  if (optind == argc) {
    chipNames[0].prefix = SENSORS_CHIP_NAME_PREFIX_ANY;
    chipNames[0].bus = SENSORS_CHIP_NAME_BUS_ANY;
    chipNames[0].addr = SENSORS_CHIP_NAME_ADDR_ANY;
    numChipNames = 1;
  } else {
    int i, n = argc - optind, err;
    if (n > MAX_CHIP_NAMES) {
      fprintf (stderr, "Too many chip names.\n");
      return -1;
    }
    for (i = 0; i < n; ++ i) {
      char *arg = argv[optind + i];
      if ((err = sensors_parse_chip_name (arg, chipNames + i))) {
        fprintf (stderr, "Invalid chip name `%s': %s\n", arg, sensors_strerror (err));
        return -1;
      }
    }
    numChipNames = n;
  }
  return 0;
}
