/*
    chips.c - Part of sensors, a user-space program for hardware monitoring
    Copyright (c) 1998  Frodo Looijaard <frodol@dds.nl>

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

#include "chips.h"
#include "lib/sensors.h"
#include "lib/chips.h"
#include "src/sensors.h"

static char *spacestr(int n);
static void print_label(const char *label, int space);
static void free_the_label(char **label);

char *spacestr(int n)
{
  static char buf[80];
  int i;
  for (i = 0; i < n; i++)
    buf[i]=' ';
  buf[n] = '\0';
  return buf;
}

void print_label(const char *label, int space)
{
  int len=strlen(label)+1;
  if (len > space)
    printf("%s:\n%s",label,spacestr(space));
  else
    printf("%s:%s",label,spacestr(space - len));
}

void free_the_label(char **label)
{
  if (*label)
    free(*label);
  *label = NULL;
}

void print_lm75(const sensors_chip_name *name)
{
  char *label;
  double cur,hyst,over;

  if (!sensors_get_label(*name,SENSORS_LM75_TEMP,&label) &&
      !sensors_get_feature(*name,SENSORS_LM75_TEMP,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM75_TEMP_HYST,&hyst) &&
      !sensors_get_feature(*name,SENSORS_LM75_TEMP_OVER,&over))  {
    print_label(label,10);
    printf("%6.1f C (limit: %6.1f C, hysteris: %6.1f C)\n",
           cur,over,hyst);
  } else
    printf("ERROR: Can't get temperature data!\n");
  free(label);
}

void print_lm78(const sensors_chip_name *name)
{
  char *label = NULL;
  double cur,min,max,fdiv;
  int alarms;

  if (!sensors_get_feature(*name,SENSORS_LM78_ALARMS,&cur)) 
    alarms = cur + 0.5;
  else {
    printf("ERROR: Can't get alarm data!\n");
    alarms = 0;
  }

  if (!sensors_get_label(*name,SENSORS_LM78_IN0,&label) &&
      !sensors_get_feature(*name,SENSORS_LM78_IN0,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM78_IN0_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_LM78_IN0_MAX,&max)) {
    print_label(label,10);
    printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
           cur,min,max,alarms&LM78_ALARM_IN0?"ALARM":"");
  } else
    printf("ERROR: Can't get IN0 data!\n");
  free_the_label(&label);
  if (!sensors_get_label(*name,SENSORS_LM78_IN1,&label) &&
      !sensors_get_feature(*name,SENSORS_LM78_IN1,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM78_IN1_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_LM78_IN1_MAX,&max)) {
    print_label(label,10);
    printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
           cur,min,max,alarms&LM78_ALARM_IN1?"ALARM":"");
  } else
    printf("ERROR: Can't get IN1 data!\n");
  free_the_label(&label);
  if (!sensors_get_label(*name,SENSORS_LM78_IN2,&label) &&
      !sensors_get_feature(*name,SENSORS_LM78_IN2,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM78_IN2_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_LM78_IN2_MAX,&max)) {
    print_label(label,10);
    printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
           cur,min,max,alarms&LM78_ALARM_IN2?"ALARM":"");
  } else
    printf("ERROR: Can't get IN2 data!\n");
  free_the_label(&label);
  if (!sensors_get_label(*name,SENSORS_LM78_IN3,&label) &&
      !sensors_get_feature(*name,SENSORS_LM78_IN3,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM78_IN3_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_LM78_IN3_MAX,&max)) {
    print_label(label,10);
    printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
           cur,min,max,alarms&LM78_ALARM_IN3?"ALARM":"");
  } else
    printf("ERROR: Can't get IN3 data!\n");
  free_the_label(&label);
  if (!sensors_get_label(*name,SENSORS_LM78_IN4,&label) &&
      !sensors_get_feature(*name,SENSORS_LM78_IN4,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM78_IN4_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_LM78_IN4_MAX,&max)) {
    print_label(label,10);
    printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
           cur,min,max,alarms&LM78_ALARM_IN4?"ALARM":"");
  } else
    printf("ERROR: Can't get IN4 data!\n");
  free_the_label(&label);
  if (!sensors_get_label(*name,SENSORS_LM78_IN5,&label) &&
      !sensors_get_feature(*name,SENSORS_LM78_IN5,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM78_IN5_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_LM78_IN5_MAX,&max)) {
    print_label(label,10);
    printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
           cur,min,max,alarms&LM78_ALARM_IN5?"ALARM":"");
  } else
    printf("ERROR: Can't get IN5 data!\n");
  free_the_label(&label);
  if (!sensors_get_label(*name,SENSORS_LM78_IN6,&label) &&
      !sensors_get_feature(*name,SENSORS_LM78_IN6,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM78_IN6_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_LM78_IN6_MAX,&max)) {
    print_label(label,10);
    printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
           cur,min,max,alarms&LM78_ALARM_IN6?"ALARM":"");
  } else
    printf("ERROR: Can't get IN6 data!\n");
  free_the_label(&label);

  if (!sensors_get_label(*name,SENSORS_LM78_FAN1,&label) &&
      !sensors_get_feature(*name,SENSORS_LM78_FAN1,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM78_FAN1_DIV,&fdiv) &&
      !sensors_get_feature(*name,SENSORS_LM78_FAN1_MIN,&min)) {
    print_label(label,10);
    printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)          %s\n",
           cur,min,fdiv, alarms&LM78_ALARM_FAN1?"ALARM":"");
  } else
    printf("ERROR: Can't get FAN1 data!\n");
  free_the_label(&label);
  if (!sensors_get_label(*name,SENSORS_LM78_FAN2,&label) &&
      !sensors_get_feature(*name,SENSORS_LM78_FAN2,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM78_FAN2_DIV,&fdiv) &&
      !sensors_get_feature(*name,SENSORS_LM78_FAN2_MIN,&min)) {
    print_label(label,10);
    printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)          %s\n",
           cur,min,fdiv, alarms&LM78_ALARM_FAN2?"ALARM":"");
  } else
    printf("ERROR: Can't get FAN2 data!\n");
  free_the_label(&label);
  if (!sensors_get_label(*name,SENSORS_LM78_FAN3,&label) &&
      !sensors_get_feature(*name,SENSORS_LM78_FAN3,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM78_FAN3_DIV,&fdiv) &&
      !sensors_get_feature(*name,SENSORS_LM78_FAN3_MIN,&min)) {
    print_label(label,10);
    printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)          %s\n",
           cur,min,fdiv, alarms&LM78_ALARM_FAN3?"ALARM":"");
  } else
    printf("ERROR: Can't get FAN3 data!\n");
  free_the_label(&label);

  if (!sensors_get_label(*name,SENSORS_LM78_TEMP,&label) &&
      !sensors_get_feature(*name,SENSORS_LM78_TEMP,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM78_TEMP_HYST,&min) &&
      !sensors_get_feature(*name,SENSORS_LM78_TEMP_OVER,&max)) {
    print_label(label,10);
    printf("%+3.0f C     (limit = %+3.0f C,  hysteris = %+3.0f C) %s\n",
           cur,max,min, alarms&LM78_ALARM_TEMP?"ALARM":"");
  } else
    printf("ERROR: Can't get TEMP data!\n");
  free_the_label(&label);

  if (!sensors_get_label(*name,SENSORS_LM78_VID,&label) &&
      !sensors_get_feature(*name,SENSORS_LM78_VID,&cur)) {
    print_label(label,10);
    printf("%+5.2f V\n",cur);
  }
  free_the_label(&label);
    
  if (!sensors_get_label(*name,SENSORS_LM78_ALARMS,&label)) {
    if (alarms & LM78_ALARM_BTI) {
      print_label(label,10);
      printf("Board temperature input (a LM75 perhaps?)    ALARM\n");
    }
    if (alarms & LM78_ALARM_CHAS) {
      print_label(label,10);
      printf("Chassis intrusion detection                  ALARM\n");
    }
  }
  free_the_label(&label);
}

void print_gl518(const sensors_chip_name *name)
{
  char *label = NULL;
  double cur,min,max,fdiv;
  int alarms,beeps;
  int is_r00;

  is_r00 = !strcmp(name->prefix,"gl518sm-r00");
  if (!sensors_get_feature(*name,SENSORS_GL518R00_ALARMS,&cur)) 
    alarms = cur + 0.5;
  else {
    printf("ERROR: Can't get alarm data!\n");
    alarms = 0;
  }
  if (!sensors_get_feature(*name,SENSORS_GL518R00_BEEPS,&cur)) 
    beeps = cur + 0.5;
  else {
    printf("ERROR: Can't get beep data!\n");
    beeps = 0;
  }

  /* We need special treatment for the R00 chips, because they can't display
     actual readings! We hardcode this, as this is the easiest way. */
  if (is_r00) {
    if (!sensors_get_label(*name,SENSORS_GL518R00_VDD,&label) &&
        !sensors_get_feature(*name,SENSORS_GL518R00_VDD_MIN,&min) &&
        !sensors_get_feature(*name,SENSORS_GL518R00_VDD_MAX,&max)) {
      print_label(label,10);
      printf("          (min = %+6.2f V, max = %+6.2f V)   %s  %s\n",
             min,max,alarms&GL518_ALARM_VDD?"ALARM":"",
             beeps&GL518_ALARM_VDD?"(beep)":"");
    } else
      printf("ERROR: Can't get VDD data!\n");
    free_the_label(&label);

    if (!sensors_get_label(*name,SENSORS_GL518R00_VIN1,&label) &&
        !sensors_get_feature(*name,SENSORS_GL518R00_VIN1_MIN,&min) &&
        !sensors_get_feature(*name,SENSORS_GL518R00_VIN1_MAX,&max)) {
      print_label(label,10);
      printf("          (min = %+6.2f V, max = %+6.2f V)   %s  %s\n",
             min,max,alarms&GL518_ALARM_VIN1?"ALARM":"",
             beeps&GL518_ALARM_VIN1?"(beep)":"");
    } else
      printf("ERROR: Can't get VIN1 data!\n");
    free_the_label(&label);
    if (!sensors_get_label(*name,SENSORS_GL518R00_VIN2,&label) &&
        !sensors_get_feature(*name,SENSORS_GL518R00_VIN2_MIN,&min) &&
        !sensors_get_feature(*name,SENSORS_GL518R00_VIN2_MAX,&max)) {
      print_label(label,10);
      printf("          (min = %+6.2f V, max = %+6.2f V)   %s  %s\n",
             min,max,alarms&GL518_ALARM_VIN2?"ALARM":"",
             beeps&GL518_ALARM_VIN2?"(beep)":"");
    } else
      printf("ERROR: Can't get IN2 data!\n");
    free_the_label(&label);
  } else {
    if (!sensors_get_label(*name,SENSORS_GL518R00_VDD,&label) &&
        !sensors_get_feature(*name,SENSORS_GL518R00_VDD,&cur) &&
        !sensors_get_feature(*name,SENSORS_GL518R00_VDD_MIN,&min) &&
        !sensors_get_feature(*name,SENSORS_GL518R00_VDD_MAX,&max)) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s  %s\n",
             cur,min,max,alarms&GL518_ALARM_VDD?"ALARM":"",
             beeps&GL518_ALARM_VDD?"(beep)":"");
    } else
      printf("ERROR: Can't get VDD data!\n");
    free_the_label(&label);
    if (!sensors_get_label(*name,SENSORS_GL518R00_VIN1,&label) &&
        !sensors_get_feature(*name,SENSORS_GL518R00_VIN1,&cur) &&
        !sensors_get_feature(*name,SENSORS_GL518R00_VIN1_MIN,&min) &&
        !sensors_get_feature(*name,SENSORS_GL518R00_VIN1_MAX,&max)) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s  %s\n",
             cur,min,max,alarms&GL518_ALARM_VIN1?"ALARM":"",
             beeps&GL518_ALARM_VIN1?"(beep)":"");
    } else
      printf("ERROR: Can't get VIN1 data!\n");
    free_the_label(&label);
    if (!sensors_get_label(*name,SENSORS_GL518R00_VIN2,&label) &&
        !sensors_get_feature(*name,SENSORS_GL518R00_VIN2,&cur) &&
        !sensors_get_feature(*name,SENSORS_GL518R00_VIN2_MIN,&min) &&
        !sensors_get_feature(*name,SENSORS_GL518R00_VIN2_MAX,&max)) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s  %s\n",
             cur,min,max,alarms&GL518_ALARM_VIN2?"ALARM":"",
             beeps&GL518_ALARM_VIN2?"(beep)":"");
    } else
      printf("ERROR: Can't get IN2 data!\n");
    free_the_label(&label);
  }

  if (!sensors_get_label(*name,SENSORS_GL518R00_VIN3,&label) &&
      !sensors_get_feature(*name,SENSORS_GL518R00_VIN3,&cur) &&
      !sensors_get_feature(*name,SENSORS_GL518R00_VIN3_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_GL518R00_VIN3_MAX,&max)) {
    print_label(label,10);
    printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s  %s\n",
           cur,min,max,alarms&GL518_ALARM_VIN3?"ALARM":"",
           beeps&GL518_ALARM_VIN3?"(beep)":"");

  } else
    printf("ERROR: Can't get VIN3 data!\n");
  free_the_label(&label);
  
  if (!sensors_get_label(*name,SENSORS_GL518R00_FAN1,&label) &&
      !sensors_get_feature(*name,SENSORS_GL518R00_FAN1,&cur) &&
      !sensors_get_feature(*name,SENSORS_GL518R00_FAN1_DIV,&fdiv) &&
      !sensors_get_feature(*name,SENSORS_GL518R00_FAN1_MIN,&min)) {
    print_label(label,10);
    printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)          %s  %s\n",
           cur,min,fdiv, alarms&GL518_ALARM_FAN1?"ALARM":"",
           beeps&GL518_ALARM_FAN1?"(beep)":"");
  } else
    printf("ERROR: Can't get FAN1 data!\n");
  free_the_label(&label);
  if (!sensors_get_label(*name,SENSORS_GL518R00_FAN2,&label) &&
      !sensors_get_feature(*name,SENSORS_GL518R00_FAN2,&cur) &&
      !sensors_get_feature(*name,SENSORS_GL518R00_FAN2_DIV,&fdiv) &&
      !sensors_get_feature(*name,SENSORS_GL518R00_FAN2_MIN,&min)) {
    print_label(label,10);
    printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)          %s  %s\n",
           cur,min,fdiv, alarms&GL518_ALARM_FAN2?"ALARM":"",
           beeps&GL518_ALARM_FAN2?"(beep)":"");
  } else
    printf("ERROR: Can't get FAN2 data!\n");
  free_the_label(&label);

  if (!sensors_get_label(*name,SENSORS_GL518R00_TEMP,&label) &&
      !sensors_get_feature(*name,SENSORS_GL518R00_TEMP,&cur) &&
      !sensors_get_feature(*name,SENSORS_GL518R00_TEMP_HYST,&min) &&
      !sensors_get_feature(*name,SENSORS_GL518R00_TEMP_OVER,&max)) {
    print_label(label,10);
    printf("%+3.0f C     (limit = %+3.0f C,  hysteris = %+3.0f C) %s  %s\n",
           cur,max,min, alarms&GL518_ALARM_TEMP?"ALARM":"",
           beeps&GL518_ALARM_TEMP?"(beep)":"");
  } else
    printf("ERROR: Can't get TEMP data!\n");
  free_the_label(&label);

  if (!sensors_get_label(*name,SENSORS_GL518R00_BEEP_ENABLE,&label) &&
      !sensors_get_feature(*name,SENSORS_GL518R00_BEEP_ENABLE,&cur)) {
    print_label(label,10);
    if (cur < 0.5) 
      printf("Sound alarm disabled\n");
    else
      printf("Sound alarm enabled\n");
  } else
    printf("ERROR: Can't get BEEP data!\n");
  free_the_label(&label);
}

void print_unknown_chip(const sensors_chip_name *name)
{
  int a,b;
  const sensors_feature_data *data;
  char *label;
  double val;
 
  a=b=0;
  while((data=sensors_get_all_features(*name,&a,&b))) {
    if (sensors_get_label(*name,data->number,&label)) {
      printf("ERROR: Can't get feature `%s' data!",data->name);
      continue;
    }
    if (data->mode & SENSORS_MODE_R) {
      if(sensors_get_feature(*name,data->number,&val)) {
        printf("ERROR: Can't get feature `%s' data!",data->name);
        continue;
      }
      if (data->mapping != SENSORS_NO_MAPPING)
        printf("  %s: %.2f (%s)\n",label,val,data->name);
      else
        printf("%s: %.2f (%s)\n",label,val,data->name);
    } else 
      printf("(%s)",label);
  }
}

void print_lm80(const sensors_chip_name *name)
{
  char *label = NULL;
  double cur,min,max,min2,max2,fdiv;
  int alarms;

  if (!sensors_get_feature(*name,SENSORS_LM80_ALARMS,&cur)) 
    alarms = cur + 0.5;
  else {
    printf("ERROR: Can't get alarm data!\n");
    alarms = 0;
  }

  if (!sensors_get_label(*name,SENSORS_LM80_IN0,&label) &&
      !sensors_get_feature(*name,SENSORS_LM80_IN0,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM80_IN0_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_LM80_IN0_MAX,&max)) {
    print_label(label,10);
    printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
           cur,min,max,alarms&LM80_ALARM_IN0?"ALARM":"");
  } else
    printf("ERROR: Can't get IN0 data!\n");
  free_the_label(&label);
  if (!sensors_get_label(*name,SENSORS_LM80_IN1,&label) &&
      !sensors_get_feature(*name,SENSORS_LM80_IN1,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM80_IN1_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_LM80_IN1_MAX,&max)) {
    print_label(label,10);
    printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
           cur,min,max,alarms&LM80_ALARM_IN1?"ALARM":"");
  } else
    printf("ERROR: Can't get IN1 data!\n");
  free_the_label(&label);
  if (!sensors_get_label(*name,SENSORS_LM80_IN2,&label) &&
      !sensors_get_feature(*name,SENSORS_LM80_IN2,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM80_IN2_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_LM80_IN2_MAX,&max)) {
    print_label(label,10);
    printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
           cur,min,max,alarms&LM80_ALARM_IN2?"ALARM":"");
  } else
    printf("ERROR: Can't get IN2 data!\n");
  free_the_label(&label);
  if (!sensors_get_label(*name,SENSORS_LM80_IN3,&label) &&
      !sensors_get_feature(*name,SENSORS_LM80_IN3,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM80_IN3_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_LM80_IN3_MAX,&max)) {
    print_label(label,10);
    printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
           cur,min,max,alarms&LM80_ALARM_IN3?"ALARM":"");
  } else
    printf("ERROR: Can't get IN3 data!\n");
  free_the_label(&label);
  if (!sensors_get_label(*name,SENSORS_LM80_IN4,&label) &&
      !sensors_get_feature(*name,SENSORS_LM80_IN4,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM80_IN4_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_LM80_IN4_MAX,&max)) {
    print_label(label,10);
    printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
           cur,min,max,alarms&LM80_ALARM_IN4?"ALARM":"");
  } else
    printf("ERROR: Can't get IN4 data!\n");
  free_the_label(&label);
  if (!sensors_get_label(*name,SENSORS_LM80_IN5,&label) &&
      !sensors_get_feature(*name,SENSORS_LM80_IN5,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM80_IN5_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_LM80_IN5_MAX,&max)) {
    print_label(label,10);
    printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
           cur,min,max,alarms&LM80_ALARM_IN5?"ALARM":"");
  } else
    printf("ERROR: Can't get IN5 data!\n");
  free_the_label(&label);
  if (!sensors_get_label(*name,SENSORS_LM80_IN6,&label) &&
      !sensors_get_feature(*name,SENSORS_LM80_IN6,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM80_IN6_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_LM80_IN6_MAX,&max)) {
    print_label(label,10);
    printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
           cur,min,max,alarms&LM80_ALARM_IN6?"ALARM":"");
  } else
    printf("ERROR: Can't get IN6 data!\n");
  free_the_label(&label);

  if (!sensors_get_label(*name,SENSORS_LM80_FAN1,&label) &&
      !sensors_get_feature(*name,SENSORS_LM80_FAN1,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM80_FAN1_DIV,&fdiv) &&
      !sensors_get_feature(*name,SENSORS_LM80_FAN1_MIN,&min)) {
    print_label(label,10);
    printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)          %s\n",
           cur,min,fdiv, alarms&LM80_ALARM_FAN1?"ALARM":"");
  } else
    printf("ERROR: Can't get FAN1 data!\n");
  free_the_label(&label);
  if (!sensors_get_label(*name,SENSORS_LM80_FAN2,&label) &&
      !sensors_get_feature(*name,SENSORS_LM80_FAN2,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM80_FAN2_DIV,&fdiv) &&
      !sensors_get_feature(*name,SENSORS_LM80_FAN2_MIN,&min)) {
    print_label(label,10);
    printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)          %s\n",
           cur,min,fdiv, alarms&LM80_ALARM_FAN2?"ALARM":"");
  } else
    printf("ERROR: Can't get FAN2 data!\n");
  free_the_label(&label);

  if (!sensors_get_label(*name,SENSORS_LM80_TEMP,&label) &&
      !sensors_get_feature(*name,SENSORS_LM80_TEMP,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM80_TEMP_HOT_HYST,&min) &&
      !sensors_get_feature(*name,SENSORS_LM80_TEMP_HOT_MAX,&max) &&
      !sensors_get_feature(*name,SENSORS_LM80_TEMP_OS_HYST,&min2) &&
      !sensors_get_feature(*name,SENSORS_LM80_TEMP_OS_MAX,&max2)) {
    print_label(label,10);
    printf("%+3.2f C (hot:limit = %+3.0f C,  hysteris = %+3.0f C) %s\n",
           cur,max,min, alarms&LM80_ALARM_TEMP_HOT?"ALARM":"");
    printf("         (os: limit = %+3.0f C,  hysteris = %+3.0f C) %s\n",
           max2,min2, alarms&LM80_ALARM_TEMP_HOT?"ALARM":"");
  } else
    printf("ERROR: Can't get TEMP data!\n");
  free_the_label(&label);

  if (!sensors_get_label(*name,SENSORS_LM80_ALARMS,&label)) {
    if (alarms & LM80_ALARM_BTI) {
      print_label(label,10);
      printf("Board temperature input (a LM75 perhaps?)    ALARM\n");
    }
    if (alarms & LM80_ALARM_CHAS) {
      print_label(label,10);
      printf("Chassis intrusion detection                  ALARM\n");
    }
  }
  free_the_label(&label);
}
