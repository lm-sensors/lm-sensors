/*
    proc.c - Part of libsensors, a Linux library for reading sensor data.
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

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/sysctl.h>
#include <linux/sysctl.h>
#include "kernel/include/sensors.h"
#include "data.h"
#include "error.h"
#include "access.h"
#include "general.h"

/* OK, this proves one thing: if there are too many chips detected, we get in
   trouble. The limit is around 4096/sizeof(struct sensors_chip_data), which
   works out to about 100 entries right now. That seems sensible enough,
   but if we ever get at the point where more chips can be detected, we must
   enlarge buf, and check that sysctl can handle larger buffers. */

#define BUF_LEN 4096

static char buf[BUF_LEN];

sensors_proc_chips_entry *sensors_proc_chips;
int sensors_proc_chips_count;
int sensors_proc_chips_max;

sensors_bus *sensors_proc_bus;
int sensors_proc_bus_count;
int sensors_proc_bus_max;

static int sensors_get_chip_id(sensors_chip_name name);

#define add_proc_chips(el) sensors_add_array_el(el,\
                                       (void **) &sensors_proc_chips,\
                                       &sensors_proc_chips_count,\
                                       &sensors_proc_chips_max,\
                                       sizeof(struct sensors_proc_chips_entry))

#define add_bus(el) sensors_add_array_el(el,\
                                       (void **) &sensors_proc_bus,\
                                       &sensors_proc_bus_count,\
                                       &sensors_proc_bus_max,\
                                       sizeof(struct sensors_bus))

/* This reads /proc/sys/dev/sensors/chips into memory */
int sensors_read_proc_chips(void)
{
  int name[3] = { CTL_DEV, DEV_SENSORS, SENSORS_CHIPS };
  int buflen = BUF_LEN;
  char *bufptr = buf;
  sensors_proc_chips_entry entry;
  int res,lineno;

  if (sysctl(name, 3, bufptr, &buflen, NULL, 0))
    return -SENSORS_ERR_PROC;

  lineno = 1;
  while (buflen >= sizeof(struct i2c_chips_data)) {
    if ((res = 
          sensors_parse_chip_name(((struct i2c_chips_data *) bufptr)->name, 
                                   &entry.name))) {
      sensors_parse_error("Parsing /proc/sys/dev/sensors/chips",lineno);
      return res;
    }
    entry.sysctl = ((struct i2c_chips_data *) bufptr)->sysctl_id;
    add_proc_chips(&entry);
    bufptr += sizeof(struct i2c_chips_data);
    buflen -= sizeof(struct i2c_chips_data);
    lineno++;
  }
  return 0;
}

int sensors_read_proc_bus(void)
{
  FILE *f;
  char line[255];
  char *border;
  sensors_bus entry;
  int lineno;

  f = fopen("/proc/bus/i2c","r");
  if (!f)
    return -SENSORS_ERR_PROC;
  lineno=1;
  while (fgets(line,255,f)) {
    if (strlen(line) > 0)
      line[strlen(line)-1] = '\0';
    if (! (border = rindex(line,'\t')))
      goto ERROR;
    if (! (entry.algorithm = strdup(border+1)))
      goto FAT_ERROR;
    *border='\0';
    if (! (border = rindex(line,'\t')))
      goto ERROR;
    if (! (entry.adapter = strdup(border + 1)))
      goto FAT_ERROR;
    *border='\0';
    if (! (border = rindex(line,'\t')))
      goto ERROR;
    *border='\0';
    if (strncmp(line,"i2c-",4))
      goto ERROR;
    if (sensors_parse_i2cbus_name(line,&entry.number))
      goto ERROR;
    sensors_strip_of_spaces(entry.algorithm);
    sensors_strip_of_spaces(entry.adapter);
    add_bus(&entry);
    lineno++;
  }
  fclose(f);
  return 0;
FAT_ERROR:
  sensors_fatal_error("sensors_read_proc_bus","Allocating entry");
ERROR:
  sensors_parse_error("Parsing /proc/bus/i2c",lineno);
  fclose(f);
  return -SENSORS_ERR_PROC;
}
    

/* This returns the first detected chip which matches the name */
int sensors_get_chip_id(sensors_chip_name name)
{
  int i;
  for (i = 0; i < sensors_proc_chips_count; i++)
    if (sensors_match_chip(name, sensors_proc_chips[i].name))
      return sensors_proc_chips[i].sysctl;
  return -SENSORS_ERR_NO_ENTRY;
}
  
/* This reads a feature /proc file */
int sensors_read_proc(sensors_chip_name name, int feature, double *value)
{
  int sysctl_name[4] = { CTL_DEV, DEV_SENSORS };
  const sensors_chip_feature *the_feature;
  int buflen = BUF_LEN;
  int mag;
  
  if ((sysctl_name[2] = sensors_get_chip_id(name)) < 0)
    return sysctl_name[2];
  if (! (the_feature = sensors_lookup_feature_nr(name.prefix,feature)))
    return -SENSORS_ERR_NO_ENTRY;
  sysctl_name[3] = the_feature->sysctl;
  if (sysctl(sysctl_name, 4, buf, &buflen, NULL, 0))
    return -SENSORS_ERR_PROC;
  *value = *((long *) (buf + the_feature->offset));
  for (mag = the_feature->scaling; mag > 0; mag --)
    *value /= 10.0;
  for (; mag < 0; mag --)
    *value *= 10.0;
  return 0;
}
  
int sensors_write_proc(sensors_chip_name name, int feature, double value)
{
  int sysctl_name[4] = { CTL_DEV, DEV_SENSORS };
  const sensors_chip_feature *the_feature;
  int buflen = BUF_LEN;
  int mag;
 
  if ((sysctl_name[2] = sensors_get_chip_id(name)) < 0)
    return sysctl_name[2];
  if (! (the_feature = sensors_lookup_feature_nr(name.prefix,feature)))
    return -SENSORS_ERR_NO_ENTRY;
  sysctl_name[3] = the_feature->sysctl;
  if (sysctl(sysctl_name, 4, buf, &buflen, NULL, 0))
    return -SENSORS_ERR_PROC;
  for (mag = the_feature->scaling; mag > 0; mag --)
    value *= 10.0;
  for (; mag < 0; mag --)
    value /= 10.0;
  * ((long *) (buf + the_feature->offset)) = (long) value;
  buflen = the_feature->offset + sizeof(long);
  if (sysctl(sysctl_name, 4, NULL, 0, buf, buflen))
    return -SENSORS_ERR_PROC;
  return 0;
}
