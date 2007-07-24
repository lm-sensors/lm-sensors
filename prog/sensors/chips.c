/*
    chips.c - Part of sensors, a user-space program for hardware monitoring
    Copyright (c) 1998-2003 Frodo Looijaard <frodol@dds.nl>
                            and Mark D. Studebaker <mdsxyz123@yahoo.com>
    Copyright (c) 2003-2006 The lm_sensors team

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "chips.h"
#include "lib/sensors.h"

extern int fahrenheit;
extern char degstr[5];

static inline float deg_ctof(float cel)
{
   return ( cel * ( 9.0F / 5.0F ) + 32.0F );
}

/* minmax = 0 for limit/hysteresis, 1 for max/min, 2 for max only;
   curprec and limitprec are # of digits after decimal point
   for the current temp and the limits
   note: symbolic constants defined in chips.h */
void print_temp_info(float n_cur, float n_over, float n_hyst,
                     int minmax, int curprec, int limitprec)
{
   /* note: deg_ctof() will preserve HUGEVAL */
   if (fahrenheit) {
      n_cur  = deg_ctof(n_cur);
      n_over = deg_ctof(n_over);
      n_hyst = deg_ctof(n_hyst);
   }

   /* use %* to pass precision as an argument */
   if (n_cur != HUGE_VAL)
      printf("%+6.*f%s  ", curprec, n_cur, degstr);
   else
      printf("   FAULT  ");

   if(minmax == MINMAX)
	printf("(low  = %+5.*f%s, high = %+5.*f%s)  ",
	    limitprec, n_hyst, degstr,
	    limitprec, n_over, degstr);
   else if(minmax == MAXONLY)
	printf("(high = %+5.*f%s)                  ",
	    limitprec, n_over, degstr);
   else if(minmax == CRIT)
	printf("(high = %+5.*f%s, crit = %+5.*f%s)  ",
	    limitprec, n_over, degstr,
	    limitprec, n_hyst, degstr);
   else if(minmax == HYST)
	printf("(high = %+5.*f%s, hyst = %+5.*f%s)  ",
	    limitprec, n_over, degstr,
	    limitprec, n_hyst, degstr);
   else if(minmax == HYSTONLY)
	printf("(hyst = %+5.*f%s)                  ",
	    limitprec, n_over, degstr);
   else if(minmax != SINGLE)
	printf("Unknown temperature mode!");
}

void print_label(const char *label, int space)
{
  int len=strlen(label)+1;
  if (len > space)
    printf("%s:\n%*s", label, space, "");
  else
    printf("%s:%*s", label, space - len, "");
}

int sensors_get_label_and_valid(sensors_chip_name name, int feature, char **label,
                        int *valid)
{
  int err;
  err = sensors_get_label(name,feature,label);
  if (!err)
    err = sensors_get_ignored(name,feature);
  if (err >= 0) {
    *valid = err;
    err = 0;
  }
  return err;
}

void print_vid_info(const sensors_chip_name *name, int f_vid, int label_size)
{
  char *label;
  int valid;
  double vid;

  if (!sensors_get_label_and_valid(*name,f_vid,&label,&valid)
      && !sensors_get_feature(*name,f_vid,&vid) ) {
    if (valid) {
      print_label(label, label_size);
      printf("%+6.3f V\n", vid);
    }
  }
  free(label);
}

void print_unknown_chip(const sensors_chip_name *name)
{
  int a, valid;
  const sensors_feature_data *data;
  char *label;
  double val;
 
  a = 0;
  while((data=sensors_get_all_features(*name, &a))) {
    if (sensors_get_label_and_valid(*name,data->number,&label,&valid)) {
      printf("ERROR: Can't get feature `%s' data!\n",data->name);
      continue;
    }
    if (! valid)
      continue;
    if (data->mode & SENSORS_MODE_R) {
      if(sensors_get_feature(*name,data->number,&val)) {
        printf("ERROR: Can't get feature `%s' data!\n",data->name);
        continue;
      }
      if (data->mapping != SENSORS_NO_MAPPING)
        printf("  %s: %.2f (%s)\n",label,val,data->name);
      else
        printf("%s: %.2f (%s)\n",label,val,data->name);
    } else 
      printf("(%s)\n",label);
    free(label);
  }
}

