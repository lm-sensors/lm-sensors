/*
    error.c - Part of libsensors, a Linux library for reading sensor data.
    Copyright (c) 1998, 1999  Frodo Looijaard <frodol@dds.nl>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <stdlib.h>
#include <stdio.h>
#include "error.h"

static void sensors_default_parse_error(const char *err, int lineno);
static void sensors_default_fatal_error(const char *proc,const char *err);

void (*sensors_parse_error) (const char *err, int lineno) = 
                                                  sensors_default_parse_error;
void (*sensors_fatal_error) (const char *proc, const char *err) = 
                                                  sensors_default_fatal_error;

static const char *errorlist[] =
 { /* Unknown error         */ "sensors_strerror: Unknown error!",
   /* SENSORS_ERR_WILDCARDS */ "Wildcard found in chip name",
   /* SENSORS_ERR_NO_ENTRY  */ "No such feature known",
   /* SENSORS_ERR_ACCESS    */ "Can't read or write",
   /* SENSORS_ERR_PROC      */ "Can't access procfs/sysfs file",
   /* SENSORS_ERR_DIV_ZERO  */ "Divide by zero",
   /* SENSORS_ERR_CHIP_NAME */ "Can't parse chip name",
   /* SENSORS_ERR_BUS_NAME  */ "Can't parse bus name",
   /* SENSORS_ERR_PARSE     */ "General parse error",
   /* SENSORS_ERR_ACCESS_W  */ "Can't write",
   /* SENSORS_ERR_ACCESS_R  */ "Can't read"
 };

#define ERROR_LIST_LEN (sizeof(errorlist) / sizeof(char *))

const char *sensors_strerror(int errnum)
{
  if (errnum < 0)
    errnum = -errnum;
  if (errnum >= ERROR_LIST_LEN)
    errnum = 0;
  return errorlist[errnum];
} 

void sensors_default_parse_error(const char *err, int lineno)
{
  fprintf(stderr,"Error: Line %d: %s\n",lineno,err);
}

void sensors_default_fatal_error(const char *proc, const char *err)
{
  fprintf(stderr,"Fatal error in `%s': %s\n",proc,err);
  exit(1);
}
