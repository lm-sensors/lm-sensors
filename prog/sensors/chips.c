/*
    chips.c - Part of sensors, a user-space program for hardware monitoring
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

#include <stdio.h>
#include <stdlib.h>

#include "chips.h"
#include "lib/sensors.h"
#include "lib/chips.h"
#include "kernel/include/sensors.h"

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
    printf("%6.1f C (limit: %6.1f C, hysteresis: %6.1f C)\n",
           cur,over,hyst);
  } else
    printf("ERROR: Can't get temperature data!\n");
  free_the_label(&label);
}

void print_adm1021(const sensors_chip_name *name)
{
  char *label;
  double cur,hyst,over;
  int alarms,i;

  if (!sensors_get_feature(*name,SENSORS_LM78_ALARMS,&cur)) 
    alarms = cur + 0.5;
  else {
    printf("ERROR: Can't get alarm data!\n");
    alarms = 0;
  }

  if (!sensors_get_label(*name,SENSORS_ADM1021_TEMP,&label) &&
      !sensors_get_feature(*name,SENSORS_ADM1021_TEMP,&cur) &&
      !sensors_get_feature(*name,SENSORS_ADM1021_TEMP_HYST,&hyst) &&
      !sensors_get_feature(*name,SENSORS_ADM1021_TEMP_OVER,&over))  {
    print_label(label,10);
    printf("%4.0f C (limit: %4.0f C, hysteresis: %4.0f C)    ",
           cur,over,hyst);
    if (alarms & (ADM1021_ALARM_TEMP_HIGH | ADM1021_ALARM_TEMP_LOW)) {
      printf("ALARM (");
      i = 0;
      if (alarms & ADM1021_ALARM_TEMP_LOW) {
        printf("LOW");
        i++;
      }
      if (alarms & ADM1021_ALARM_TEMP_HIGH)
        printf("%sHIGH",i?",":"");
      printf(")\n");
    }
  } else
    printf("ERROR: Can't get temperature data!\n");
  free_the_label(&label);

  if (!sensors_get_label(*name,SENSORS_ADM1021_REMOTE_TEMP,&label) &&
      !sensors_get_feature(*name,SENSORS_ADM1021_REMOTE_TEMP,&cur) &&
      !sensors_get_feature(*name,SENSORS_ADM1021_REMOTE_TEMP_HYST,&hyst) &&
      !sensors_get_feature(*name,SENSORS_ADM1021_REMOTE_TEMP_OVER,&over))  {
    print_label(label,10);
    printf("%4.0f C (limit: %4.0f C, hysteresis: %4.0f C)    ",
           cur,over,hyst);
    if (alarms & (ADM1021_ALARM_RTEMP_HIGH | ADM1021_ALARM_RTEMP_LOW |
                  ADM1021_ALARM_RTEMP_NA)) {
      printf("ALARM (");
      i = 0;
      if (alarms & ADM1021_ALARM_RTEMP_NA) {
        printf("N/A");
        i++;
      }
      if (alarms & ADM1021_ALARM_RTEMP_LOW) {
        printf("%sLOW",i?",":"");
        i++;
      }
      if (alarms & ADM1021_ALARM_RTEMP_HIGH)
        printf("%sHIGH",i?",":"");
      printf(")\n");
    }
  } else
    printf("ERROR: Can't get temperature data!\n");
  free_the_label(&label);

  if (!strcmp(name->prefix,"adm1021")) {
    if (!sensors_get_label(*name,SENSORS_ADM1021_DIE_CODE,&label) &&
        !sensors_get_feature(*name,SENSORS_ADM1021_DIE_CODE,&cur)) {
      print_label(label,10);
      printf("%4.0f\n",cur);
    } else
      printf("ERROR: Can't get die-code data!\n");
    free_the_label(&label);
  }
}

void print_adm9240(const sensors_chip_name *name)
{
  char *label = NULL;
  double cur,min,max,fdiv;
  int alarms;

  if (!sensors_get_feature(*name,SENSORS_ADM9240_ALARMS,&cur)) 
    alarms = cur + 0.5;
  else {
    printf("ERROR: Can't get alarm data!\n");
    alarms = 0;
  }

  if (!sensors_get_label(*name,SENSORS_ADM9240_IN0,&label) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_IN0,&cur) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_IN0_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_IN0_MAX,&max)) {
    print_label(label,10);
    printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
           cur,min,max,alarms&ADM9240_ALARM_IN0?"ALARM":"");
  } else
    printf("ERROR: Can't get IN0 data!\n");
  free_the_label(&label);
  if (!sensors_get_label(*name,SENSORS_ADM9240_IN1,&label) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_IN1,&cur) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_IN1_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_IN1_MAX,&max)) {
    print_label(label,10);
    printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
           cur,min,max,alarms&ADM9240_ALARM_IN1?"ALARM":"");
  } else
    printf("ERROR: Can't get IN1 data!\n");
  free_the_label(&label);
  if (!sensors_get_label(*name,SENSORS_ADM9240_IN2,&label) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_IN2,&cur) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_IN2_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_IN2_MAX,&max)) {
    print_label(label,10);
    printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
           cur,min,max,alarms&ADM9240_ALARM_IN2?"ALARM":"");
  } else
    printf("ERROR: Can't get IN2 data!\n");
  free_the_label(&label);
  if (!sensors_get_label(*name,SENSORS_ADM9240_IN3,&label) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_IN3,&cur) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_IN3_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_IN3_MAX,&max)) {
    print_label(label,10);
    printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
           cur,min,max,alarms&ADM9240_ALARM_IN3?"ALARM":"");
  } else
    printf("ERROR: Can't get IN3 data!\n");
  free_the_label(&label);
  if (!sensors_get_label(*name,SENSORS_ADM9240_IN4,&label) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_IN4,&cur) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_IN4_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_IN4_MAX,&max)) {
    print_label(label,10);
    printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
           cur,min,max,alarms&ADM9240_ALARM_IN4?"ALARM":"");
  } else
    printf("ERROR: Can't get IN4 data!\n");
  free_the_label(&label);
  if (!sensors_get_label(*name,SENSORS_ADM9240_IN5,&label) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_IN5,&cur) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_IN5_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_IN5_MAX,&max)) {
    print_label(label,10);
    printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
           cur,min,max,alarms&ADM9240_ALARM_IN5?"ALARM":"");
  } else
    printf("ERROR: Can't get IN5 data!\n");
  free_the_label(&label);

  if (!sensors_get_label(*name,SENSORS_ADM9240_FAN1,&label) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_FAN1,&cur) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_FAN1_DIV,&fdiv) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_FAN1_MIN,&min)) {
    print_label(label,10);
    printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)          %s\n",
           cur,min,fdiv, alarms&ADM9240_ALARM_FAN1?"ALARM":"");
  } else
    printf("ERROR: Can't get FAN1 data!\n");
  free_the_label(&label);
  if (!sensors_get_label(*name,SENSORS_ADM9240_FAN2,&label) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_FAN2,&cur) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_FAN2_DIV,&fdiv) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_FAN2_MIN,&min)) {
    print_label(label,10);
    printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)          %s\n",
           cur,min,fdiv, alarms&ADM9240_ALARM_FAN2?"ALARM":"");
  } else
    printf("ERROR: Can't get FAN2 data!\n");
  free_the_label(&label);

  if (!sensors_get_label(*name,SENSORS_ADM9240_TEMP,&label) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_TEMP,&cur) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_TEMP_HYST,&min) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_TEMP_OVER,&max)) {
    print_label(label,10);
    printf("%+3.0f C     (limit = %+3.0f C,  hysteresis = %+3.0f C) %s\n",
           cur,max,min, alarms&ADM9240_ALARM_TEMP?"ALARM":"");
  } else
    printf("ERROR: Can't get TEMP data!\n");
  free_the_label(&label);

  if (!sensors_get_label(*name,SENSORS_ADM9240_VID,&label) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_VID,&cur)) {
    print_label(label,10);
    printf("%+5.2f V\n",cur);
  }
  free_the_label(&label);
    
  if (!sensors_get_label(*name,SENSORS_ADM9240_ALARMS,&label)) {
    print_label(label,10);
    printf("Chassis intrusion detection                  %s\n",
           alarms & ADM9240_ALARM_CHAS?"ALARM":"     ");
  }
  free_the_label(&label);
}

void print_sis5595(const sensors_chip_name *name)
{
  char *label = NULL;
  double cur,min,max,fdiv;
  int alarms;

  if (!sensors_get_feature(*name,SENSORS_SIS5595_ALARMS,&cur)) 
    alarms = cur + 0.5;
  else {
    printf("ERROR: Can't get alarm data!\n");
    alarms = 0;
  }

  if (!sensors_get_label(*name,SENSORS_SIS5595_IN0,&label) &&
      !sensors_get_feature(*name,SENSORS_SIS5595_IN0,&cur) &&
      !sensors_get_feature(*name,SENSORS_SIS5595_IN0_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_SIS5595_IN0_MAX,&max)) {
    print_label(label,10);
    printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
           cur,min,max,alarms&SIS5595_ALARM_IN0?"ALARM":"");
  } else
    printf("ERROR: Can't get IN0 data!\n");
  free_the_label(&label);
  if (!sensors_get_label(*name,SENSORS_SIS5595_IN1,&label) &&
      !sensors_get_feature(*name,SENSORS_SIS5595_IN1,&cur) &&
      !sensors_get_feature(*name,SENSORS_SIS5595_IN1_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_SIS5595_IN1_MAX,&max)) {
    print_label(label,10);
    printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
           cur,min,max,alarms&SIS5595_ALARM_IN1?"ALARM":"");
  } else
    printf("ERROR: Can't get IN1 data!\n");
  free_the_label(&label);
  if (!sensors_get_label(*name,SENSORS_SIS5595_IN2,&label) &&
      !sensors_get_feature(*name,SENSORS_SIS5595_IN2,&cur) &&
      !sensors_get_feature(*name,SENSORS_SIS5595_IN2_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_SIS5595_IN2_MAX,&max)) {
    print_label(label,10);
    printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
           cur,min,max,alarms&SIS5595_ALARM_IN2?"ALARM":"");
  } else
    printf("ERROR: Can't get IN2 data!\n");
  free_the_label(&label);
  if (!sensors_get_label(*name,SENSORS_SIS5595_IN3,&label) &&
      !sensors_get_feature(*name,SENSORS_SIS5595_IN3,&cur) &&
      !sensors_get_feature(*name,SENSORS_SIS5595_IN3_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_SIS5595_IN3_MAX,&max)) {
    print_label(label,10);
    printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
           cur,min,max,alarms&SIS5595_ALARM_IN3?"ALARM":"");
  } else
    printf("ERROR: Can't get IN3 data!\n");
  free_the_label(&label);

  if (!sensors_get_label(*name,SENSORS_SIS5595_FAN1,&label) &&
      !sensors_get_feature(*name,SENSORS_SIS5595_FAN1,&cur) &&
      !sensors_get_feature(*name,SENSORS_SIS5595_FAN1_DIV,&fdiv) &&
      !sensors_get_feature(*name,SENSORS_SIS5595_FAN1_MIN,&min)) {
    print_label(label,10);
    printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)          %s\n",
           cur,min,fdiv, alarms&SIS5595_ALARM_FAN1?"ALARM":"");
  } else
    printf("ERROR: Can't get FAN1 data!\n");
  free_the_label(&label);
  if (!sensors_get_label(*name,SENSORS_SIS5595_FAN2,&label) &&
      !sensors_get_feature(*name,SENSORS_SIS5595_FAN2,&cur) &&
      !sensors_get_feature(*name,SENSORS_SIS5595_FAN2_DIV,&fdiv) &&
      !sensors_get_feature(*name,SENSORS_SIS5595_FAN2_MIN,&min)) {
    print_label(label,10);
    printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)          %s\n",
           cur,min,fdiv, alarms&SIS5595_ALARM_FAN2?"ALARM":"");
  } else
    printf("ERROR: Can't get FAN2 data!\n");
  free_the_label(&label);

  if (!sensors_get_label(*name,SENSORS_SIS5595_TEMP,&label) &&
      !sensors_get_feature(*name,SENSORS_SIS5595_TEMP,&cur) &&
      !sensors_get_feature(*name,SENSORS_SIS5595_TEMP_HYST,&min) &&
      !sensors_get_feature(*name,SENSORS_SIS5595_TEMP_OVER,&max)) {
    print_label(label,10);
    printf("%+3.0f C     (limit = %+3.0f C,  hysteresis = %+3.0f C) %s\n",
           cur,max,min, alarms&SIS5595_ALARM_TEMP?"ALARM":"");
  } else
    printf("ERROR: Can't get TEMP data!\n");
  free_the_label(&label);

  if (!sensors_get_label(*name,SENSORS_SIS5595_ALARMS,&label)) {
    print_label(label,10);
    printf("Board temperature input (usually LM75 chips) %s\n",
           alarms & SIS5595_ALARM_BTI?"ALARM":"     ");
  }
  free_the_label(&label);
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
    printf("%+3.0f C     (limit = %+3.0f C,  hysteresis = %+3.0f C) %s\n",
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
    print_label(label,10);
    printf("Board temperature input (usually LM75 chips) %s\n",
           alarms & LM78_ALARM_BTI?"ALARM":"");
    print_label(label,10);
    printf("Chassis intrusion detection                  %s\n",
           alarms & LM78_ALARM_CHAS?"ALARM":"     ");
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
  if (!sensors_get_feature(*name,SENSORS_GL518_ALARMS,&cur)) 
    alarms = cur + 0.5;
  else {
    printf("ERROR: Can't get alarm data!\n");
    alarms = 0;
  }
  if (!sensors_get_feature(*name,SENSORS_GL518_BEEPS,&cur)) 
    beeps = cur + 0.5;
  else {
    printf("ERROR: Can't get beep data!\n");
    beeps = 0;
  }

  /* We need special treatment for the R00 chips, because they can't display
     actual readings! We hardcode this, as this is the easiest way. */
  if (is_r00) {
    if (!sensors_get_label(*name,SENSORS_GL518_VDD,&label) &&
        !sensors_get_feature(*name,SENSORS_GL518_VDD,&cur) &&
        !sensors_get_feature(*name,SENSORS_GL518_VDD_MIN,&min) &&
        !sensors_get_feature(*name,SENSORS_GL518_VDD_MAX,&max)) {
      print_label(label,10);
      if (cur == 0.0)
        printf("(n/a)     ");
      else
        printf("%+6.2f V  ",cur);
      printf(  "(min = %+6.2f V, max = %+6.2f V)   %s  %s\n",
             min,max,alarms&GL518_ALARM_VDD?"ALARM":"     ",
             beeps&GL518_ALARM_VDD?"(beep)":"");
    } else
      printf("ERROR: Can't get VDD data!\n");
    free_the_label(&label);

    if (!sensors_get_label(*name,SENSORS_GL518_VIN1,&label) &&
        !sensors_get_feature(*name,SENSORS_GL518_VIN1,&cur) &&
        !sensors_get_feature(*name,SENSORS_GL518_VIN1_MIN,&min) &&
        !sensors_get_feature(*name,SENSORS_GL518_VIN1_MAX,&max)) {
      print_label(label,10);
      if (cur == 0.0)
        printf("(n/a)     ");
      else
        printf("%+6.2f V  ",cur);
      printf("(min = %+6.2f V, max = %+6.2f V)   %s  %s\n",
             min,max,alarms&GL518_ALARM_VIN1?"ALARM":"     ",
             beeps&GL518_ALARM_VIN1?"(beep)":"");
    } else
      printf("ERROR: Can't get VIN1 data!\n");
    free_the_label(&label);
    if (!sensors_get_label(*name,SENSORS_GL518_VIN2,&label) &&
        !sensors_get_feature(*name,SENSORS_GL518_VIN2,&cur) &&
        !sensors_get_feature(*name,SENSORS_GL518_VIN2_MIN,&min) &&
        !sensors_get_feature(*name,SENSORS_GL518_VIN2_MAX,&max)) {
      print_label(label,10);
      if (cur == 0.0)
        printf("(n/a)     ");
      else
        printf("%+6.2f V  ",cur);
      printf("(min = %+6.2f V, max = %+6.2f V)   %s  %s\n",
             min,max,alarms&GL518_ALARM_VIN2?"ALARM":"     ",
             beeps&GL518_ALARM_VIN2?"(beep)":"");
    } else
      printf("ERROR: Can't get IN2 data!\n");
    free_the_label(&label);
  } else {
    if (!sensors_get_label(*name,SENSORS_GL518_VDD,&label) &&
        !sensors_get_feature(*name,SENSORS_GL518_VDD,&cur) &&
        !sensors_get_feature(*name,SENSORS_GL518_VDD_MIN,&min) &&
        !sensors_get_feature(*name,SENSORS_GL518_VDD_MAX,&max)) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s  %s\n",
             cur,min,max,alarms&GL518_ALARM_VDD?"ALARM":"     ",
             beeps&GL518_ALARM_VDD?"(beep)":"");
    } else
      printf("ERROR: Can't get VDD data!\n");
    free_the_label(&label);
    if (!sensors_get_label(*name,SENSORS_GL518_VIN1,&label) &&
        !sensors_get_feature(*name,SENSORS_GL518_VIN1,&cur) &&
        !sensors_get_feature(*name,SENSORS_GL518_VIN1_MIN,&min) &&
        !sensors_get_feature(*name,SENSORS_GL518_VIN1_MAX,&max)) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s  %s\n",
             cur,min,max,alarms&GL518_ALARM_VIN1?"ALARM":"     ",
             beeps&GL518_ALARM_VIN1?"(beep)":"");
    } else
      printf("ERROR: Can't get VIN1 data!\n");
    free_the_label(&label);
    if (!sensors_get_label(*name,SENSORS_GL518_VIN2,&label) &&
        !sensors_get_feature(*name,SENSORS_GL518_VIN2,&cur) &&
        !sensors_get_feature(*name,SENSORS_GL518_VIN2_MIN,&min) &&
        !sensors_get_feature(*name,SENSORS_GL518_VIN2_MAX,&max)) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s  %s\n",
             cur,min,max,alarms&GL518_ALARM_VIN2?"ALARM":"     ",
             beeps&GL518_ALARM_VIN2?"(beep)":"");
    } else
      printf("ERROR: Can't get IN2 data!\n");
    free_the_label(&label);
  }

  if (!sensors_get_label(*name,SENSORS_GL518_VIN3,&label) &&
      !sensors_get_feature(*name,SENSORS_GL518_VIN3,&cur) &&
      !sensors_get_feature(*name,SENSORS_GL518_VIN3_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_GL518_VIN3_MAX,&max)) {
    print_label(label,10);
    printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s  %s\n",
           cur,min,max,alarms&GL518_ALARM_VIN3?"ALARM":"     ",
           beeps&GL518_ALARM_VIN3?"(beep)":"");

  } else
    printf("ERROR: Can't get VIN3 data!\n");
  free_the_label(&label);
  
  if (!sensors_get_label(*name,SENSORS_GL518_FAN1,&label) &&
      !sensors_get_feature(*name,SENSORS_GL518_FAN1,&cur) &&
      !sensors_get_feature(*name,SENSORS_GL518_FAN1_DIV,&fdiv) &&
      !sensors_get_feature(*name,SENSORS_GL518_FAN1_MIN,&min)) {
    print_label(label,10);
    printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)          %s  %s\n",
           cur,min,fdiv, alarms&GL518_ALARM_FAN1?"ALARM":"     ",
           beeps&GL518_ALARM_FAN1?"(beep)":"");
  } else
    printf("ERROR: Can't get FAN1 data!\n");
  free_the_label(&label);
  if (!sensors_get_label(*name,SENSORS_GL518_FAN2,&label) &&
      !sensors_get_feature(*name,SENSORS_GL518_FAN2,&cur) &&
      !sensors_get_feature(*name,SENSORS_GL518_FAN2_DIV,&fdiv) &&
      !sensors_get_feature(*name,SENSORS_GL518_FAN2_MIN,&min)) {
    print_label(label,10);
    printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)          %s  %s\n",
           cur,min,fdiv, alarms&GL518_ALARM_FAN2?"ALARM":"     ",
           beeps&GL518_ALARM_FAN2?"(beep)":"");
  } else
    printf("ERROR: Can't get FAN2 data!\n");
  free_the_label(&label);

  if (!sensors_get_label(*name,SENSORS_GL518_TEMP,&label) &&
      !sensors_get_feature(*name,SENSORS_GL518_TEMP,&cur) &&
      !sensors_get_feature(*name,SENSORS_GL518_TEMP_OVER,&max) &&
      !sensors_get_feature(*name,SENSORS_GL518_TEMP_HYST,&min)) {
    print_label(label,10);
    printf("%+3.0f C     (limit = %+3.0f C,  hysteresis = %+3.0f C) %s  %s\n",
           cur,max,min, alarms&GL518_ALARM_TEMP?"ALARM":"     ",
           beeps&GL518_ALARM_TEMP?"(beep)":"");
  } else
    printf("ERROR: Can't get TEMP data!\n");
  free_the_label(&label);

  if (!sensors_get_label(*name,SENSORS_GL518_BEEP_ENABLE,&label) &&
      !sensors_get_feature(*name,SENSORS_GL518_BEEP_ENABLE,&cur)) {
    print_label(label,10);
    if (cur < 0.5) 
      printf("Sound alarm disabled\n");
    else
      printf("Sound alarm enabled\n");
  } else
    printf("ERROR: Can't get BEEP data!\n");
  free_the_label(&label);
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
    printf("%+3.2f C (hot:limit = %+3.0f C,  hysteresis = %+3.0f C) %s\n",
           cur,max,min, alarms&LM80_ALARM_TEMP_HOT?"ALARM":"");
    printf("         (os: limit = %+3.0f C,  hysteresis = %+3.0f C) %s\n",
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

void print_w83781d(const sensors_chip_name *name)
{
  char *label = NULL;
  double cur,min,max,fdiv,sens;
  int alarms,beeps;
  int is82d, is83s;

  is82d = (!strcmp(name->prefix,"w83782d")) ||
          (!strcmp(name->prefix,"w83627hf")) ||
          (!strcmp(name->prefix,"as99127f"));
  is83s = !strcmp(name->prefix,"w83782s");
  if (!sensors_get_feature(*name,SENSORS_W83781D_ALARMS,&cur)) 
    alarms = cur + 0.5;
  else {
    printf("ERROR: Can't get alarm data!\n");
    alarms = 0;
  }

  if (!sensors_get_feature(*name,SENSORS_W83781D_BEEPS,&cur)) 
    beeps = cur + 0.5;
  else {
    printf("ERROR: Can't get beep data!\n");
    beeps = 0;
  }

  if (!sensors_get_label(*name,SENSORS_W83781D_IN0,&label) &&
      !sensors_get_feature(*name,SENSORS_W83781D_IN0,&cur) &&
      !sensors_get_feature(*name,SENSORS_W83781D_IN0_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_W83781D_IN0_MAX,&max)) {
    print_label(label,10);
    printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)       %s  %s\n",
           cur,min,max,alarms&W83781D_ALARM_IN0?"ALARM":"     ",
           beeps&W83781D_ALARM_IN0?"(beep)":"");
  } else
    printf("ERROR: Can't get IN0 data!\n");
  free_the_label(&label);
  if (!is83s) {
    if (!sensors_get_label(*name,SENSORS_W83781D_IN1,&label) &&
        !sensors_get_feature(*name,SENSORS_W83781D_IN1,&cur) &&
        !sensors_get_feature(*name,SENSORS_W83781D_IN1_MIN,&min) &&
        !sensors_get_feature(*name,SENSORS_W83781D_IN1_MAX,&max)) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)       %s  %s\n",
             cur,min,max,alarms&W83781D_ALARM_IN1?"ALARM":"     ",
             beeps&W83781D_ALARM_IN1?"(beep)":"");
    } else
      printf("ERROR: Can't get IN1 data!\n");
    free_the_label(&label);
  }
  if (!sensors_get_label(*name,SENSORS_W83781D_IN2,&label) &&
      !sensors_get_feature(*name,SENSORS_W83781D_IN2,&cur) &&
      !sensors_get_feature(*name,SENSORS_W83781D_IN2_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_W83781D_IN2_MAX,&max)) {
    print_label(label,10);
    printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)       %s  %s\n",
           cur,min,max,alarms&W83781D_ALARM_IN2?"ALARM":"     ",
           beeps&W83781D_ALARM_IN2?"(beep)":"");
  } else
    printf("ERROR: Can't get IN2 data!\n");
  free_the_label(&label);
  if (!sensors_get_label(*name,SENSORS_W83781D_IN3,&label) &&
      !sensors_get_feature(*name,SENSORS_W83781D_IN3,&cur) &&
      !sensors_get_feature(*name,SENSORS_W83781D_IN3_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_W83781D_IN3_MAX,&max)) {
    print_label(label,10);
    printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)       %s  %s\n",
           cur,min,max,alarms&W83781D_ALARM_IN3?"ALARM":"     ",
           beeps&W83781D_ALARM_IN3?"(beep)":"");
  } else
    printf("ERROR: Can't get IN3 data!\n");
  free_the_label(&label);
  if (!sensors_get_label(*name,SENSORS_W83781D_IN4,&label) &&
      !sensors_get_feature(*name,SENSORS_W83781D_IN4,&cur) &&
      !sensors_get_feature(*name,SENSORS_W83781D_IN4_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_W83781D_IN4_MAX,&max)) {
    print_label(label,10);
    printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)       %s  %s\n",
           cur,min,max,alarms&W83781D_ALARM_IN4?"ALARM":"     ",
           beeps&W83781D_ALARM_IN4?"(beep)":"");
  } else
    printf("ERROR: Can't get IN4 data!\n");
  free_the_label(&label);
  if (!sensors_get_label(*name,SENSORS_W83781D_IN5,&label) &&
      !sensors_get_feature(*name,SENSORS_W83781D_IN5,&cur) &&
      !sensors_get_feature(*name,SENSORS_W83781D_IN5_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_W83781D_IN5_MAX,&max)) {
    print_label(label,10);
    printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)       %s  %s\n",
           cur,min,max,alarms&W83781D_ALARM_IN5?"ALARM":"     ",
           beeps&W83781D_ALARM_IN5?"(beep)":"");
  } else
    printf("ERROR: Can't get IN5 data!\n");
  free_the_label(&label);
  if (!is83s) {
    if (!sensors_get_label(*name,SENSORS_W83781D_IN6,&label) &&
        !sensors_get_feature(*name,SENSORS_W83781D_IN6,&cur) &&
        !sensors_get_feature(*name,SENSORS_W83781D_IN6_MIN,&min) &&
        !sensors_get_feature(*name,SENSORS_W83781D_IN6_MAX,&max)) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)       %s  %s\n",
             cur,min,max,alarms&W83781D_ALARM_IN6?"ALARM":"     ",
             beeps&W83781D_ALARM_IN6?"(beep)":"");
    } else
      printf("ERROR: Can't get IN6 data!\n");
    free_the_label(&label);
  }
  if (is82d) {
    if (!sensors_get_label(*name,SENSORS_W83782D_IN7,&label) &&
        !sensors_get_feature(*name,SENSORS_W83782D_IN7,&cur) &&
        !sensors_get_feature(*name,SENSORS_W83782D_IN7_MIN,&min) &&
        !sensors_get_feature(*name,SENSORS_W83782D_IN7_MAX,&max)) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)       %s  %s\n",
             cur,min,max,alarms&W83782D_ALARM_IN7?"ALARM":"     ",
             beeps&W83782D_ALARM_IN7?"(beep)":"");
    } else
      printf("ERROR: Can't get IN7 data!\n");
    free_the_label(&label);
    if (!sensors_get_label(*name,SENSORS_W83782D_IN8,&label) &&
        !sensors_get_feature(*name,SENSORS_W83782D_IN8,&cur) &&
        !sensors_get_feature(*name,SENSORS_W83782D_IN8_MIN,&min) &&
        !sensors_get_feature(*name,SENSORS_W83782D_IN8_MAX,&max)) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)       %s  %s\n",
             cur,min,max,alarms&W83782D_ALARM_IN8?"ALARM":"     ",
             beeps&W83782D_ALARM_IN8?"(beep)":"");
    } else
      printf("ERROR: Can't get IN6 data!\n");
    free_the_label(&label);
  }

  if (!sensors_get_label(*name,SENSORS_W83781D_FAN1,&label) &&
      !sensors_get_feature(*name,SENSORS_W83781D_FAN1,&cur) &&
      !sensors_get_feature(*name,SENSORS_W83781D_FAN1_DIV,&fdiv) &&
      !sensors_get_feature(*name,SENSORS_W83781D_FAN1_MIN,&min)) {
    print_label(label,10);
    printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)              %s  %s\n",
           cur,min,fdiv, alarms&W83781D_ALARM_FAN1?"ALARM":"     ",
           beeps&W83781D_ALARM_FAN1?"(beep)":"");
  } else
    printf("ERROR: Can't get FAN1 data!\n");
  free_the_label(&label);
  if (!sensors_get_label(*name,SENSORS_W83781D_FAN2,&label) &&
      !sensors_get_feature(*name,SENSORS_W83781D_FAN2,&cur) &&
      !sensors_get_feature(*name,SENSORS_W83781D_FAN2_DIV,&fdiv) &&
      !sensors_get_feature(*name,SENSORS_W83781D_FAN2_MIN,&min)) {
    print_label(label,10);
    printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)              %s  %s\n",
           cur,min,fdiv, alarms&W83781D_ALARM_FAN2?"ALARM":"     ",
           beeps&W83781D_ALARM_FAN2?"(beep)":"");
  } else
    printf("ERROR: Can't get FAN2 data!\n");
  free_the_label(&label);
  if (!sensors_get_label(*name,SENSORS_W83781D_FAN3,&label) &&
      !sensors_get_feature(*name,SENSORS_W83781D_FAN3,&cur) &&
      !sensors_get_feature(*name,SENSORS_W83781D_FAN3_DIV,&fdiv) &&
      !sensors_get_feature(*name,SENSORS_W83781D_FAN3_MIN,&min)) {
    print_label(label,10);
    printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)              %s  %s\n",
           cur,min,fdiv, alarms&W83781D_ALARM_FAN3?"ALARM":"     ",
           beeps&W83781D_ALARM_FAN3?"(beep)":"");
  } else
    printf("ERROR: Can't get FAN3 data!\n");
  free_the_label(&label);

  if (!sensors_get_label(*name,SENSORS_W83781D_TEMP1,&label) &&
      !sensors_get_feature(*name,SENSORS_W83781D_TEMP1,&cur) &&
      !sensors_get_feature(*name,SENSORS_W83781D_TEMP1_HYST,&min) &&
      !sensors_get_feature(*name,SENSORS_W83781D_TEMP1_OVER,&max)) {
    if((!is82d) && (!is83s)) {
      print_label(label,10);
      printf("%+3.0f C   (limit = %+3.0f C, hysteresis = %+3.0f C) %s  %s\n",
             cur,max,min, alarms&W83781D_ALARM_TEMP1 ?"ALARM":"     ",
             beeps&W83781D_ALARM_TEMP1?"(beep)":"");
    } else {
      if(!sensors_get_feature(*name,SENSORS_W83781D_SENS1,&sens)) {
        print_label(label,10);
        printf(
"%+3.0f C  (limit = %+3.0f C, hysteresis = %+3.0f C, sensor = %s) %s  %s\n",
               cur,max,min,
               (((int)sens)==1)?"PII/Celeron diode":(((int)sens)==2)?
               "3904 transistor":"thermistor",
               alarms&W83781D_ALARM_TEMP1?"ALARM":"     ",
               beeps&W83781D_ALARM_TEMP1?"(beep)":"");
      } else {
        printf("ERROR: Can't get TEMP1 data!\n");
      }
    }
  } else
    printf("ERROR: Can't get TEMP1 data!\n");
  free_the_label(&label);

  if (!sensors_get_label(*name,SENSORS_W83781D_TEMP2,&label) &&
      !sensors_get_feature(*name,SENSORS_W83781D_TEMP2,&cur) &&
      !sensors_get_feature(*name,SENSORS_W83781D_TEMP2_HYST,&min) &&
      !sensors_get_feature(*name,SENSORS_W83781D_TEMP2_OVER,&max)) {
    if((!is82d) && (!is83s)) {
      print_label(label,10);
      printf("%+3.1f C   (limit = %+3.1f C, hysteresis = %+3.1f C) %s  %s\n",
             cur,max,min, alarms&W83781D_ALARM_TEMP23 ?"ALARM":"     ",
             beeps&W83781D_ALARM_TEMP23?"(beep)":"");
    } else {
      if(!sensors_get_feature(*name,SENSORS_W83781D_SENS2,&sens)) {
        print_label(label,10);
        printf(
"%+3.1f C  (limit = %+3.1f C, hysteresis = %+3.1f C, sensor = %s) %s  %s\n",
               cur,max,min,
               (((int)sens)==1)?"PII/Celeron diode":(((int)sens)==2)?
               "3904 transistor":"thermistor",
               alarms&W83781D_ALARM_TEMP2?"ALARM":"     ",
               beeps&W83781D_ALARM_TEMP2?"(beep)":"");
      } else {
        printf("ERROR: Can't get TEMP2 data!\n");
      }
    }
  } else
    printf("ERROR: Can't get TEMP2 data!\n");
  free_the_label(&label);

  if (!is83s) {
    if (!sensors_get_label(*name,SENSORS_W83781D_TEMP3,&label) &&
        !sensors_get_feature(*name,SENSORS_W83781D_TEMP3,&cur) &&
        !sensors_get_feature(*name,SENSORS_W83781D_TEMP3_HYST,&min) &&
        !sensors_get_feature(*name,SENSORS_W83781D_TEMP3_OVER,&max)) {
      if(!is82d) {
        print_label(label,10);
        printf("%+3.1f C   (limit = %+3.1f C, hysteresis = %+3.1f C) %s  %s\n",
               cur,max,min, alarms&W83781D_ALARM_TEMP23 ?"ALARM":"     ",
               beeps&W83781D_ALARM_TEMP23?"(beep)":"");
      } else {
        if(!sensors_get_feature(*name,SENSORS_W83781D_SENS3,&sens)) {
          print_label(label,10);
          printf(
"%+3.1f C  (limit = %+3.1f C, hysteresis = %+3.1f C, sensor = %s) %s  %s\n",
                 cur,max,min,
                 (((int)sens)==1)?"PII/Celeron diode":(((int)sens)==2)?
                 "3904 transistor":"thermistor",
                 alarms&W83781D_ALARM_TEMP3?"ALARM":"     ",
                 beeps&W83781D_ALARM_TEMP3?"(beep)":"");
        } else {
          printf("ERROR: Can't get TEMP3 data!\n");
        }
      }
    } else
      printf("ERROR: Can't get TEMP3 data!\n");
    free_the_label(&label);
  }

  if (!sensors_get_label(*name,SENSORS_W83781D_VID,&label) &&
      !sensors_get_feature(*name,SENSORS_W83781D_VID,&cur)) {
    print_label(label,10);
    printf("%+5.2f V\n",cur);
  }
  free_the_label(&label);
    
  if (!sensors_get_label(*name,SENSORS_W83781D_ALARMS,&label)) {
    print_label(label,10);
    printf("Chassis intrusion detection                      %s  %s\n",
           alarms & W83781D_ALARM_CHAS?"ALARM":"     ",
           beeps & W83781D_ALARM_CHAS?"(beep)":"");
  }
  free_the_label(&label);

  if (!sensors_get_label(*name,SENSORS_W83781D_BEEP_ENABLE,&label) &&
      !sensors_get_feature(*name,SENSORS_W83781D_BEEP_ENABLE,&cur)) {
    print_label(label,10);
    if (cur < 0.5) 
      printf("Sound alarm disabled\n");
    else
      printf("Sound alarm enabled\n");
  } else
    printf("ERROR: Can't get BEEP data!\n");
  free_the_label(&label);
}

void print_maxilife(const sensors_chip_name *name)
{
   char  *label = NULL;
   double cur, min, max;
   int    alarms;

   if (!sensors_get_feature(*name, SENSORS_MAXI_CG_ALARMS, &cur)) 
      alarms = cur + 0.5;
   else {
      printf("ERROR: Can't get alarm data!\n");
      alarms = 0;
   }

   if (!sensors_get_label(*name, SENSORS_MAXI_CG_TEMP1, &label) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_TEMP1, &cur) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_TEMP1_MAX, &max) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_TEMP1_HYST, &min)) {
      if (cur || max || min) {
         print_label(label, 12);
         printf("%+3.1f C     (limit = %+3.1f C, hysteresis = %+3.1f C)\n",
                cur, max, min);
      }
   } else
      printf("ERROR: Can't get TEMP1 data!\n");
   free_the_label(&label);

   if (!sensors_get_label(*name, SENSORS_MAXI_CG_TEMP2, &label) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_TEMP2, &cur) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_TEMP2_MAX, &max) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_TEMP2_HYST, &min)) {
      if (cur || max || min) {
         print_label(label, 12);
         printf("%+3.1f C     (limit = %+3.1f C, hysteresis = %+3.1f C) %s\n",
                cur, max, min, alarms&MAXI_ALARM_TEMP2 ? "ALARM" : "");
      }
   } else
      printf("ERROR: Can't get TEMP2 data!\n");
   free_the_label(&label);

   if (!sensors_get_label(*name, SENSORS_MAXI_CG_TEMP3, &label) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_TEMP3, &cur) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_TEMP3_MAX, &max) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_TEMP3_HYST, &min)) {
      if (cur || max || min) {
         print_label(label, 12);
         printf("%+3.1f C     (limit = %+3.1f C, hysteresis = %+3.1f C)\n",
                cur, max, min);
      }
   } else
      printf("ERROR: Can't get TEMP3 data!\n");
   free_the_label(&label);

   if (!sensors_get_label(*name, SENSORS_MAXI_CG_TEMP4, &label) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_TEMP4, &cur) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_TEMP4_MAX, &max) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_TEMP4_HYST, &min)) {
      if (cur || max || min) {
         print_label(label, 12);
         printf("%+3.1f C     (limit = %+3.1f C, hysteresis = %+3.1f C) %s\n",
                cur, max, min, alarms&MAXI_ALARM_TEMP4 ? "ALARM" : "");
      }
   } else
      printf("ERROR: Can't get TEMP4 data!\n");
   free_the_label(&label);

   if (!sensors_get_label(*name, SENSORS_MAXI_CG_TEMP5, &label) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_TEMP5, &cur) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_TEMP5_MAX, &max) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_TEMP5_HYST, &min)) {
      if (cur || max || min) {
         print_label(label, 12);
         printf("%+3.1f C     (limit = %+3.1f C, hysteresis = %+3.1f C) %s\n",
                cur, max, min, alarms&MAXI_ALARM_TEMP5 ? "ALARM" : "");
      }
   } else
      printf("ERROR: Can't get TEMP5 data!\n");
   free_the_label(&label);
   
   if (!sensors_get_label(*name, SENSORS_MAXI_CG_FAN1, &label) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_FAN1, &cur) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_FAN1_MIN, &min) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_FAN1_DIV, &max)) {
      if (cur || min || max) {
         print_label(label, 12);
         if (cur < 0)
            printf("  OFF       (min = %4.0f RPM, div = %1.0f)      %s\n",
                   min/max, max, alarms&MAXI_ALARM_FAN1 ? "ALARM" : "");
         else
            printf("%5.0f RPM   (min = %4.0f RPM, div = %1.0f)      %s\n",
                   cur/max, min/max, max, alarms&MAXI_ALARM_FAN1 ? "ALARM" : "");
      }
   } else
      printf("ERROR: Can't get FAN1 data!\n");
   free_the_label(&label);

   if (!sensors_get_label(*name, SENSORS_MAXI_CG_FAN2, &label) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_FAN2, &cur) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_FAN2_MIN, &min) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_FAN2_DIV, &max)) {
      if (cur || min || max) {
         print_label(label, 12);
         if (cur < 0)
            printf("  OFF       (min = %4.0f RPM, div = %1.0f)      %s\n",
                   min/max, max, alarms&MAXI_ALARM_FAN2 ? "ALARM" : "");
         else
            printf("%5.0f RPM   (min = %4.0f RPM, div = %1.0f)      %s\n",
                   cur/max, min/max, max, alarms&MAXI_ALARM_FAN2 ? "ALARM" : "");
      }
   } else
      printf("ERROR: Can't get FAN2 data!\n");
   free_the_label(&label);

   if (!sensors_get_label(*name, SENSORS_MAXI_CG_FAN3, &label) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_FAN3, &cur) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_FAN3_MIN, &min) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_FAN3_DIV, &max)) {
      if (cur || min || max) {
         print_label(label, 12);
         if (cur < 0)
            printf("  OFF       (min = %4.0f RPM, div = %1.0f)      %s\n",
                   min/max, max, alarms&MAXI_ALARM_FAN3 ? "ALARM" : "");
         else
            printf("%5.0f RPM   (min = %4.0f RPM, div = %1.0f)      %s\n",
                   cur/max, min/max, max, alarms&MAXI_ALARM_FAN3 ? "ALARM" : "");
      }
   } else
      printf("ERROR: Can't get FAN3 data!\n");
   free_the_label(&label);

   if (!sensors_get_label(*name, SENSORS_MAXI_CG_PLL, &label) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_PLL, &cur) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_PLL_MIN, &min) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_PLL_MAX, &max)) {
      if (cur || min || max) {
         print_label(label, 12);
         printf("%4.2f MHz   (min = %4.2f MHz, max = %4.2f MHz) %s\n",
                cur, min, max, alarms&MAXI_ALARM_PLL ? "ALARM" : "");
      }
   } else
      printf("ERROR: Can't get PLL data!\n");
   free_the_label(&label);

   if (!sensors_get_label(*name, SENSORS_MAXI_CG_VID1, &label) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_VID1, &cur) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_VID1_MIN, &min) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_VID1_MAX, &max)) {
      if (cur || min || max) {
         print_label(label, 12);
         printf("%+6.2f V    (min = %+6.2f V, max = %+6.2f V)   %s\n",
                cur, min, max, alarms&MAXI_ALARM_VID1 ? "ALARM" : "");
      }
   } else
      printf("ERROR: Can't get VID1 data!\n");
   free_the_label(&label);

   if (!sensors_get_label(*name, SENSORS_MAXI_CG_VID2, &label) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_VID2, &cur) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_VID2_MIN, &min) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_VID2_MAX, &max)) {
      if (cur || min || max) {
         print_label(label, 12);
         printf("%+6.2f V    (min = %+6.2f V, max = %+6.2f V)   %s\n",
                cur, min, max, alarms&MAXI_ALARM_VID2 ? "ALARM" : "");
      }
   } else
      printf("ERROR: Can't get VID2 data!\n");
   free_the_label(&label);

   if (!sensors_get_label(*name, SENSORS_MAXI_CG_VID3, &label) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_VID3, &cur) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_VID3_MIN, &min) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_VID3_MAX, &max)) {
      if (cur || min || max) {
         print_label(label, 12);
         printf("%+6.2f V    (min = %+6.2f V, max = %+6.2f V)   %s\n",
                cur, min, max, alarms&MAXI_ALARM_VID3 ? "ALARM" : "");
      }
   } else
      printf("ERROR: Can't get VID3 data!\n");
   free_the_label(&label);

   if (!sensors_get_label(*name, SENSORS_MAXI_CG_VID4, &label) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_VID4, &cur) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_VID4_MIN, &min) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_VID4_MAX, &max)) {
      if (cur || min || max) {
         print_label(label, 12);
         printf("%+6.2f V    (min = %+6.2f V, max = %+6.2f V)   %s\n",
                cur, min, max, alarms&MAXI_ALARM_VID4 ? "ALARM" : "");
      }
   } else
      printf("ERROR: Can't get VID4 data!\n");
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

