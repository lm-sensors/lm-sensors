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
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "sensord.h"

static int logOpened = 0;

static volatile sig_atomic_t done = 0;

#define LOG_BUFFER 4096

#include <stdarg.h>

void
sensorLog
(int priority, const char *fmt, ...) {
  static char buffer[1 + LOG_BUFFER];
  va_list ap;
  va_start (ap, fmt);
  vsnprintf (buffer, LOG_BUFFER, fmt, ap);
  buffer[LOG_BUFFER] = '\0';
  va_end (ap);
  if (debug || (priority < LOG_DEBUG)) {
    if (logOpened) {
      syslog (priority, "%s", buffer);
    } else {
      fprintf (stderr, "%s\n", buffer);
      fflush (stderr);
    }
  }
}

static void
signalHandler
(int sig) {
  signal (sig, signalHandler);
  switch (sig) {
    case SIGTERM:
      done = 1;
      break;
  }
}

static int
sensord
(void) {
  int ret = 0;
  int scanValue = 0, logValue = 0;

  sensorLog (LOG_INFO, "sensord started");

  while (!done && (ret == 0)) {
    if (ret == 0)
      ret = reloadLib ();
    if ((ret == 0) && scanTime) {
      ret = scanChips ();
      if (scanValue <= 0)
        scanValue += scanTime;
    }
    if ((ret == 0) && logTime && (logValue <= 0)) {
      ret = readChips ();
      logValue += logTime;
    }
    if (!done && (ret == 0)) {
      int sleepTime;
      if (!logTime) {
        sleepTime = scanValue;
      } else if (!scanTime) {
        sleepTime = logValue;
      } else {
        sleepTime = (scanValue < logValue) ? scanValue : logValue;
      }
      sleep (sleepTime);
      scanValue -= sleepTime;
      logValue -= sleepTime;
    }
  }

  sensorLog (LOG_INFO, "sensord %s", ret ? "failed" : "stopped");

  return ret;
}

static void
daemonize
(void) {
  int pid;
  struct stat fileStat;
  FILE *file;

  openlog ("sensord", 0, syslogFacility);
  
  logOpened = 1;

  if (chdir ("/") < 0) {
    perror ("chdir()");
    exit (EXIT_FAILURE);
  }

  if (!(stat (pidFile, &fileStat)) &&
      ((!S_ISREG (fileStat.st_mode)) || (fileStat.st_size > 11))) {
    fprintf (stderr, "Error: PID file `%s' already exists and looks suspicious.\n", pidFile);
    exit (EXIT_FAILURE);
  }
 
  if (!(file = fopen (pidFile, "w"))) {
    fprintf (stderr, "fopen(\"%s\"): %s\n", pidFile, strerror (errno));
    exit (EXIT_FAILURE);
  }
  
  /* I should use sigaction but... */
  if (signal (SIGTERM, signalHandler) == SIG_ERR) {
    perror ("signal(SIGTERM)");
    exit (EXIT_FAILURE);
  }

  if ((pid = fork ()) == -1) {
    perror ("fork()");
    exit (EXIT_FAILURE);
  } else if (pid != 0) {
    fprintf (file, "%d\n", pid);
    fclose (file);
    exit (EXIT_SUCCESS);
  }

  if (setsid () < 0) {
    perror ("setsid()");
    exit (EXIT_FAILURE);
  }

  fclose (file);
  close (STDIN_FILENO);
  close (STDOUT_FILENO);
  close (STDERR_FILENO);
}

static void 
undaemonize
(void) {
  unlink (pidFile);
  closelog ();
}

int
main
(int argc, char **argv) {
  int ret;
  
  if (parseArgs (argc, argv) ||
      parseChips (argc, argv))
    exit (EXIT_FAILURE);
  
  if (initLib () ||
      loadLib ())
    exit (EXIT_FAILURE);
  
  if (isDaemon) {
    daemonize ();
    ret = sensord ();
    undaemonize ();
  } else {
    if (doSet)
      ret = setChips ();
    else if (doScan)
      ret = scanChips ();
    else
      ret = readChips ();
  }
  
  if (unloadLib ())
    exit (EXIT_FAILURE);
  
  return ret;
}
