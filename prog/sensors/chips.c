/*
    chips.c - Part of sensors, a user-space program for hardware monitoring
    Copyright (c) 1998-2003 Frodo Looijaard <frodol@dds.nl>, Mark D.
    Studebaker <mdsxyz123@yahoo.com> and the lm_sensors team

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

#include "chips.h"
#include "lib/sensors.h"
#include "lib/chips.h"
#include "lib/error.h"
#include "kernel/include/sensors.h"

static void print_label(const char *label, int space);
static void free_the_label(char **label);
static void print_temp_info(float, float, float, int, int, int);
static inline float deg_ctof( float );

extern int fahrenheit;
extern char degstr[5];

inline float deg_ctof( float cel )
{
   return ( cel * ( 9.0F / 5.0F ) + 32.0F );
}

#define HYST 0
#define MINMAX 1
#define MAXONLY 2
#define CRIT 3
#define SINGLE 4
#define HYSTONLY 5
/* minmax = 0 for limit/hysteresis, 1 for max/min, 2 for max only;
   curprec and limitprec are # of digits after decimal point
   for the current temp and the limits */
void print_temp_info(float n_cur, float n_over, float n_hyst,
                     int minmax, int curprec, int limitprec)
{
   if (fahrenheit) {
      n_cur  = deg_ctof(n_cur);
      n_over = deg_ctof(n_over);
      n_hyst = deg_ctof(n_hyst);
   }

/* use %* to pass precision as an argument */
   if(minmax == MINMAX)
	printf("%+6.*f%s  (low  = %+5.*f%s, high = %+5.*f%s)  ",
	    curprec, n_cur, degstr,
	    limitprec, n_hyst, degstr,
	    limitprec, n_over, degstr);
   else if(minmax == MAXONLY)
	printf("%+6.*f%s  (high = %+5.*f%s)                    ",
	    curprec, n_cur, degstr,
	    limitprec, n_over, degstr);
   else if(minmax == CRIT)
	printf("%+6.*f%s  (high = %+5.*f%s, crit = %+5.*f%s)  ",
	    curprec, n_cur, degstr,
	    limitprec, n_over, degstr,
	    limitprec, n_hyst, degstr);
   else if(minmax == HYST)
	printf("%+6.*f%s  (high = %+5.*f%s, hyst = %+5.*f%s)  ",
	    curprec, n_cur, degstr,
	    limitprec, n_over, degstr,
	    limitprec, n_hyst, degstr);
   else if(minmax == SINGLE)
	printf("%+6.*f%s",
	    curprec, n_cur, degstr);
   else if(minmax == HYSTONLY)
	printf( "%+6.*f%s  (hyst = %+5.*f%s)                   ",
	    curprec, n_cur, degstr,
	    limitprec, n_over, degstr);
   else
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

void free_the_label(char **label)
{
  if (*label)
    free(*label);
  *label = NULL;
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

void print_vid_info(const sensors_chip_name *name, int f_vid, int f_vrm)
{
  char *label = NULL;
  int valid;
  double vid, vrm;

  if (!sensors_get_label_and_valid(*name,f_vid,&label,&valid)
      && !sensors_get_feature(*name,f_vid,&vid) ) {
    if (valid) {
      print_label(label,10);
      if(!sensors_get_feature(*name,f_vrm,&vrm))
	printf("%+6.3f V  (VRM Version %.1f)\n",vid,vrm);
      else
	printf("%+6.3f V\n",vid);
    }
  }
  free_the_label(&label);
}

/* Chip-specific print routines start here */

void print_ds1621(const sensors_chip_name *name)
{
  char *label;
  double cur,hyst,over;
  int alarms, valid;

  if (!sensors_get_feature(*name,SENSORS_DS1621_ALARMS,&cur)) 
    alarms = cur + 0.5;
  else {
    printf("ERROR: Can't get alarm data!\n");
    alarms = 0;
  }

  if (!sensors_get_label_and_valid(*name,SENSORS_DS1621_TEMP,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_DS1621_TEMP,&cur) &&
      !sensors_get_feature(*name,SENSORS_DS1621_TEMP_HYST,&hyst) &&
      !sensors_get_feature(*name,SENSORS_DS1621_TEMP_OVER,&over))  {
    if (valid) {
      print_label(label,10);
      print_temp_info( cur, over, hyst, MINMAX, 2, 1);
      if (alarms & (DS1621_ALARM_TEMP_HIGH | DS1621_ALARM_TEMP_LOW)) {
        printf("ALARM (");
        if (alarms & DS1621_ALARM_TEMP_LOW) {
          printf("LOW");
        }
        if (alarms & DS1621_ALARM_TEMP_HIGH)
          printf("%sHIGH",(alarms & DS1621_ALARM_TEMP_LOW)?",":"");
        printf(")");
      }
      printf("\n");
    }
  } else
    printf("ERROR: Can't get temperature data!\n");
  free_the_label(&label);
}

void print_lm75(const sensors_chip_name *name)
{
  char *label;
  double cur,hyst,over;
  int valid;

  if (!sensors_get_label_and_valid(*name,SENSORS_LM75_TEMP,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM75_TEMP,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM75_TEMP_HYST,&hyst) &&
      !sensors_get_feature(*name,SENSORS_LM75_TEMP_OVER,&over))  {
    if (valid) {
      print_label(label,10);
      print_temp_info( cur, over, hyst, HYST, 1, 1);
      printf( "\n" );
    }
  } else
    printf("ERROR: Can't get temperature data!\n");
  free_the_label(&label);
}

void print_adm1021(const sensors_chip_name *name)
{
  char *label;
  double cur,hyst,over;
  int alarms,i,valid;

  if (!sensors_get_feature(*name,SENSORS_ADM1021_ALARMS,&cur)) 
    alarms = cur + 0.5;
  else {
    printf("ERROR: Can't get alarm data!\n");
    alarms = 0;
  }

  if (!sensors_get_label_and_valid(*name,SENSORS_ADM1021_TEMP,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_ADM1021_TEMP,&cur) &&
      !sensors_get_feature(*name,SENSORS_ADM1021_TEMP_HYST,&hyst) &&
      !sensors_get_feature(*name,SENSORS_ADM1021_TEMP_OVER,&over))  {
    if (valid) {
      print_label(label,10);
      print_temp_info( cur, over, hyst, MINMAX, 0, 0);
      if (alarms & (ADM1021_ALARM_TEMP_HIGH | ADM1021_ALARM_TEMP_LOW)) {
        printf("ALARM (");
        i = 0;
        if (alarms & ADM1021_ALARM_TEMP_LOW) {
          printf("LOW");
          i++;
        }
        if (alarms & ADM1021_ALARM_TEMP_HIGH)
          printf("%sHIGH",i?",":"");
        printf(")");
      }
      printf("\n");
    }
  } else
    printf("ERROR: Can't get temperature data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_ADM1021_REMOTE_TEMP,
                                   &label,&valid) &&
      !sensors_get_feature(*name,SENSORS_ADM1021_REMOTE_TEMP,&cur) &&
      !sensors_get_feature(*name,SENSORS_ADM1021_REMOTE_TEMP_HYST,&hyst) &&
      !sensors_get_feature(*name,SENSORS_ADM1021_REMOTE_TEMP_OVER,&over))  {
    if (valid) {
      print_label(label,10);
      print_temp_info( cur, over, hyst, MINMAX, 0, 0);
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
        printf(")");
      }
      printf("\n");
    }
  } else
    printf("ERROR: Can't get temperature data!\n");
  free_the_label(&label);

  if (!strcmp(name->prefix,"adm1021")) {
    if (!sensors_get_label_and_valid(*name,SENSORS_ADM1021_DIE_CODE,
                                     &label,&valid) &&
        !sensors_get_feature(*name,SENSORS_ADM1021_DIE_CODE,&cur)) {
      if (valid) {
        print_label(label,10);
        printf("%4.0f\n",cur);
      }
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
  int valid;

  if (!sensors_get_feature(*name,SENSORS_ADM9240_ALARMS,&cur)) 
    alarms = cur + 0.5;
  else {
    printf("ERROR: Can't get alarm data!\n");
    alarms = 0;
  }

  if (!sensors_get_label_and_valid(*name,SENSORS_ADM9240_IN0,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_IN0,&cur) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_IN0_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_IN0_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf( "%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&ADM9240_ALARM_IN0?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN0 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_ADM9240_IN1,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_IN1,&cur) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_IN1_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_IN1_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&ADM9240_ALARM_IN1?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN1 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_ADM9240_IN2,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_IN2,&cur) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_IN2_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_IN2_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&ADM9240_ALARM_IN2?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN2 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_ADM9240_IN3,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_IN3,&cur) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_IN3_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_IN3_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&ADM9240_ALARM_IN3?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN3 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_ADM9240_IN4,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_IN4,&cur) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_IN4_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_IN4_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&ADM9240_ALARM_IN4?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN4 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_ADM9240_IN5,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_IN5,&cur) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_IN5_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_IN5_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&ADM9240_ALARM_IN5?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN5 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_ADM9240_FAN1,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_FAN1,&cur) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_FAN1_DIV,&fdiv) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_FAN1_MIN,&min)) {
    if (valid) {
      print_label(label,10);
      printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)          %s\n",
             cur,min,fdiv, alarms&ADM9240_ALARM_FAN1?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get FAN1 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_ADM9240_FAN2,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_FAN2,&cur) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_FAN2_DIV,&fdiv) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_FAN2_MIN,&min)) {
    if (valid) {
      print_label(label,10);
      printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)          %s\n",
             cur,min,fdiv, alarms&ADM9240_ALARM_FAN2?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get FAN2 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_ADM9240_TEMP,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_TEMP,&cur) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_TEMP_HYST,&min) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_TEMP_OVER,&max)) {
    if (valid) {
      print_label(label,10);
      print_temp_info( cur, max, min, HYST, 1, 0);
      printf( " %s\n", alarms & ADM9240_ALARM_TEMP ? "ALARM" : "" );
    }
  } else
    printf("ERROR: Can't get TEMP data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_ADM9240_VID,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_ADM9240_VID,&cur)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V\n",cur);
    }
  }
  free_the_label(&label);
    
  if (!sensors_get_label_and_valid(*name,SENSORS_ADM9240_ALARMS,&label,&valid)) {
    if (valid) {
      print_label(label,10);
      if(alarms & ADM9240_ALARM_CHAS)
        printf("Chassis intrusion detection                  ALARM\n");
      else
        printf("\n");
    }
  }
  free_the_label(&label);
}

void print_adm1024(const sensors_chip_name *name)
{
  char *label = NULL;
  double cur,min,max,fdiv;
  int alarms;
  int valid;

  if (!sensors_get_feature(*name,SENSORS_ADM1024_ALARMS,&cur)) 
    alarms = cur + 0.5;
  else {
    printf("ERROR: Can't get alarm data!\n");
    alarms = 0;
  }

  if (!sensors_get_label_and_valid(*name,SENSORS_ADM1024_IN0,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_ADM1024_IN0,&cur) &&
      !sensors_get_feature(*name,SENSORS_ADM1024_IN0_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_ADM1024_IN0_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf( "%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&ADM1024_ALARM_IN0?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN0 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_ADM1024_IN1,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_ADM1024_IN1,&cur) &&
      !sensors_get_feature(*name,SENSORS_ADM1024_IN1_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_ADM1024_IN1_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&ADM1024_ALARM_IN1?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN1 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_ADM1024_IN2,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_ADM1024_IN2,&cur) &&
      !sensors_get_feature(*name,SENSORS_ADM1024_IN2_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_ADM1024_IN2_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&ADM1024_ALARM_IN2?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN2 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_ADM1024_IN3,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_ADM1024_IN3,&cur) &&
      !sensors_get_feature(*name,SENSORS_ADM1024_IN3_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_ADM1024_IN3_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&ADM1024_ALARM_IN3?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN3 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_ADM1024_IN4,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_ADM1024_IN4,&cur) &&
      !sensors_get_feature(*name,SENSORS_ADM1024_IN4_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_ADM1024_IN4_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&ADM1024_ALARM_IN4?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN4 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_ADM1024_IN5,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_ADM1024_IN5,&cur) &&
      !sensors_get_feature(*name,SENSORS_ADM1024_IN5_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_ADM1024_IN5_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&ADM1024_ALARM_IN5?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN5 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_ADM1024_FAN1,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_ADM1024_FAN1,&cur) &&
      !sensors_get_feature(*name,SENSORS_ADM1024_FAN1_DIV,&fdiv) &&
      !sensors_get_feature(*name,SENSORS_ADM1024_FAN1_MIN,&min)) {
    if (valid) {
      print_label(label,10);
      printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)          %s\n",
             cur,min,fdiv, alarms&ADM1024_ALARM_FAN1?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get FAN1 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_ADM1024_FAN2,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_ADM1024_FAN2,&cur) &&
      !sensors_get_feature(*name,SENSORS_ADM1024_FAN2_DIV,&fdiv) &&
      !sensors_get_feature(*name,SENSORS_ADM1024_FAN2_MIN,&min)) {
    if (valid) {
      print_label(label,10);
      printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)          %s\n",
             cur,min,fdiv, alarms&ADM1024_ALARM_FAN2?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get FAN2 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_ADM1024_TEMP,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_ADM1024_TEMP,&cur) &&
      !sensors_get_feature(*name,SENSORS_ADM1024_TEMP_HYST,&min) &&
      !sensors_get_feature(*name,SENSORS_ADM1024_TEMP_OVER,&max)) {
    if (valid) {
      print_label(label,10);
      print_temp_info( cur, min, max, HYST, 1, 0);
      printf( " %s\n", alarms & ADM1024_ALARM_TEMP ? "ALARM" : "" );
    }
  } else
    printf("ERROR: Can't get TEMP data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_ADM1024_TEMP1,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_ADM1024_TEMP1,&cur) &&
      !sensors_get_feature(*name,SENSORS_ADM1024_TEMP1_HYST,&min) &&
      !sensors_get_feature(*name,SENSORS_ADM1024_TEMP1_OVER,&max)) {
    if (valid) {
      print_label(label,10);
      print_temp_info( cur, min, max, HYST, 1, 0);
      printf( " %s\n", alarms & ADM1024_ALARM_TEMP1 ? "ALARM" : "" );
    }
  } else
    printf("ERROR: Can't get TEMP1 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_ADM1024_TEMP2,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_ADM1024_TEMP2,&cur) &&
      !sensors_get_feature(*name,SENSORS_ADM1024_TEMP2_HYST,&min) &&
      !sensors_get_feature(*name,SENSORS_ADM1024_TEMP2_OVER,&max)) {
    if (valid) {
      print_label(label,10);
      print_temp_info( cur, min, max, HYST, 1, 0);
      printf( " %s\n", alarms & ADM1024_ALARM_TEMP2 ? "ALARM" : "" );
    }
  } else
    printf("ERROR: Can't get TEMP2 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_ADM1024_VID,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_ADM1024_VID,&cur)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V\n",cur);
    }
  }
  free_the_label(&label);
    
  if (!sensors_get_label_and_valid(*name,SENSORS_ADM1024_ALARMS,&label,&valid)) {
    if (valid) {
      print_label(label,10);
      if(alarms & ADM1024_ALARM_CHAS)
        printf("Chassis intrusion detection                  ALARM\n");
      else
        printf("\n");
    }
  }
  free_the_label(&label);
}

void print_sis5595(const sensors_chip_name *name)
{
  char *label = NULL;
  double cur,min,max,fdiv;
  int alarms,valid;

  if (!sensors_get_feature(*name,SENSORS_SIS5595_ALARMS,&cur)) 
    alarms = cur + 0.5;
  else {
    printf("ERROR: Can't get alarm data!\n");
    alarms = 0;
  }

  if (!sensors_get_label_and_valid(*name,SENSORS_SIS5595_IN0,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_SIS5595_IN0,&cur) &&
      !sensors_get_feature(*name,SENSORS_SIS5595_IN0_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_SIS5595_IN0_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&SIS5595_ALARM_IN0?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN0 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_SIS5595_IN1,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_SIS5595_IN1,&cur) &&
      !sensors_get_feature(*name,SENSORS_SIS5595_IN1_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_SIS5595_IN1_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&SIS5595_ALARM_IN1?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN1 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_SIS5595_IN2,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_SIS5595_IN2,&cur) &&
      !sensors_get_feature(*name,SENSORS_SIS5595_IN2_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_SIS5595_IN2_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&SIS5595_ALARM_IN2?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN2 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_SIS5595_IN3,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_SIS5595_IN3,&cur) &&
      !sensors_get_feature(*name,SENSORS_SIS5595_IN3_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_SIS5595_IN3_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&SIS5595_ALARM_IN3?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN3 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_SIS5595_IN4,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_SIS5595_IN4,&cur) &&
      !sensors_get_feature(*name,SENSORS_SIS5595_IN4_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_SIS5595_IN4_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&SIS5595_ALARM_IN4?"ALARM":"");
    }
  } else
  /* No error if IN4 is missing as it will happen with 2.6 kernels when 
     the chip is configured in 4 voltage + 1 temperature sensors. */
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_SIS5595_FAN1,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_SIS5595_FAN1,&cur) &&
      !sensors_get_feature(*name,SENSORS_SIS5595_FAN1_DIV,&fdiv) &&
      !sensors_get_feature(*name,SENSORS_SIS5595_FAN1_MIN,&min)) {
    if (valid) {
      print_label(label,10);
      printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)          %s\n",
             cur,min,fdiv, alarms&SIS5595_ALARM_FAN1?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get FAN1 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_SIS5595_FAN2,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_SIS5595_FAN2,&cur) &&
      !sensors_get_feature(*name,SENSORS_SIS5595_FAN2_DIV,&fdiv) &&
      !sensors_get_feature(*name,SENSORS_SIS5595_FAN2_MIN,&min)) {
    if (valid) {
    print_label(label,10);
    printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)          %s\n",
           cur,min,fdiv, alarms&SIS5595_ALARM_FAN2?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get FAN2 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_SIS5595_TEMP,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_SIS5595_TEMP,&cur) &&
      !sensors_get_feature(*name,SENSORS_SIS5595_TEMP_HYST,&min) &&
      !sensors_get_feature(*name,SENSORS_SIS5595_TEMP_OVER,&max)) {
    if (valid) {
      print_label(label,10);
      print_temp_info( cur, max, min, HYST, 0, 0);
      printf( " %s\n", alarms & SIS5595_ALARM_TEMP ? "ALARM" : "" );
    }
  } else
  /* No error if TEMP is missing as it will happen with 2.6 kernels when 
     the chip is configured in 5 voltage sensors mode. */
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_SIS5595_ALARMS,&label,&valid)
      && valid) {
    print_label(label,10);
    printf("Board temperature input (usually LM75 chips) %s\n",
           alarms & SIS5595_ALARM_BTI ?"ALARM":"     ");
  }
  free_the_label(&label);

}

void print_via686a(const sensors_chip_name *name)
{
  char *label = NULL;
  double cur,min,max,fdiv;
  int alarms,valid;

  if (!sensors_get_feature(*name,SENSORS_VIA686A_ALARMS,&cur)) 
    alarms = cur + 0.5;
  else {
    printf("ERROR: Can't get alarm data!\n");
    alarms = 0;
  }

  if (!sensors_get_label_and_valid(*name,SENSORS_VIA686A_IN0,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_VIA686A_IN0,&cur) &&
      !sensors_get_feature(*name,SENSORS_VIA686A_IN0_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_VIA686A_IN0_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&VIA686A_ALARM_IN0?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN0 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_VIA686A_IN1,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_VIA686A_IN1,&cur) &&
      !sensors_get_feature(*name,SENSORS_VIA686A_IN1_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_VIA686A_IN1_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&VIA686A_ALARM_IN1?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN1 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_VIA686A_IN2,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_VIA686A_IN2,&cur) &&
      !sensors_get_feature(*name,SENSORS_VIA686A_IN2_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_VIA686A_IN2_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&VIA686A_ALARM_IN2?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN2 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_VIA686A_IN3,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_VIA686A_IN3,&cur) &&
      !sensors_get_feature(*name,SENSORS_VIA686A_IN3_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_VIA686A_IN3_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&VIA686A_ALARM_IN3?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN3 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_VIA686A_IN4,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_VIA686A_IN4,&cur) &&
      !sensors_get_feature(*name,SENSORS_VIA686A_IN4_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_VIA686A_IN4_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&VIA686A_ALARM_IN4?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN4 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_VIA686A_FAN1,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_VIA686A_FAN1,&cur) &&
      !sensors_get_feature(*name,SENSORS_VIA686A_FAN1_DIV,&fdiv) &&
      !sensors_get_feature(*name,SENSORS_VIA686A_FAN1_MIN,&min)) {
    if (valid) {
      print_label(label,10);
      printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)          %s\n",
             cur,min,fdiv, alarms&VIA686A_ALARM_FAN1?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get FAN1 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_VIA686A_FAN2,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_VIA686A_FAN2,&cur) &&
      !sensors_get_feature(*name,SENSORS_VIA686A_FAN2_DIV,&fdiv) &&
      !sensors_get_feature(*name,SENSORS_VIA686A_FAN2_MIN,&min)) {
    if (valid) {
    print_label(label,10);
    printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)          %s\n",
           cur,min,fdiv, alarms&VIA686A_ALARM_FAN2?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get FAN2 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_VIA686A_TEMP,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_VIA686A_TEMP,&cur) &&
      !sensors_get_feature(*name,SENSORS_VIA686A_TEMP_HYST,&min) &&
      !sensors_get_feature(*name,SENSORS_VIA686A_TEMP_OVER,&max)) {
    if (valid) {
      print_label(label,10);
      print_temp_info( cur, max, min, HYST, 1, 0);
      printf(" %s\n", alarms & VIA686A_ALARM_TEMP ? "ALARM" : "" );
    }
  } else
    printf("ERROR: Can't get TEMP data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_VIA686A_TEMP2,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_VIA686A_TEMP2,&cur) &&
      !sensors_get_feature(*name,SENSORS_VIA686A_TEMP2_HYST,&min) &&
      !sensors_get_feature(*name,SENSORS_VIA686A_TEMP2_OVER,&max)) {
    if (valid) {
      print_label(label,10);
      print_temp_info( cur, max, min, HYST, 1, 0);
      printf(" %s\n", alarms & VIA686A_ALARM_TEMP2 ? "ALARM" : "" );
    }
  } else
    printf("ERROR: Can't get TEMP2 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_VIA686A_TEMP3,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_VIA686A_TEMP3,&cur) &&
      !sensors_get_feature(*name,SENSORS_VIA686A_TEMP3_HYST,&min) &&
      !sensors_get_feature(*name,SENSORS_VIA686A_TEMP3_OVER,&max)) {
    if (valid) {
      print_label(label,10);
      print_temp_info( cur, max, min, HYST, 1, 0);
      printf(" %s\n", alarms & VIA686A_ALARM_TEMP3 ? "ALARM" : "" );
    }
  } else
    printf("ERROR: Can't get TEMP3 data!\n");
  free_the_label(&label);

}

void print_lm78(const sensors_chip_name *name)
{
  char *label = NULL;
  double cur,min,max,fdiv;
  int alarms,valid;

  if (!sensors_get_feature(*name,SENSORS_LM78_ALARMS,&cur)) 
    alarms = cur + 0.5;
  else {
    printf("ERROR: Can't get alarm data!\n");
    alarms = 0;
  }


  if (!sensors_get_label_and_valid(*name,SENSORS_LM78_IN0,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM78_IN0,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM78_IN0_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_LM78_IN0_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&LM78_ALARM_IN0?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN0 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_LM78_IN1,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM78_IN1,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM78_IN1_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_LM78_IN1_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&LM78_ALARM_IN1?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN1 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_LM78_IN2,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM78_IN2,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM78_IN2_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_LM78_IN2_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&LM78_ALARM_IN2?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN2 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_LM78_IN3,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM78_IN3,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM78_IN3_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_LM78_IN3_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&LM78_ALARM_IN3?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN3 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_LM78_IN4,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM78_IN4,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM78_IN4_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_LM78_IN4_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&LM78_ALARM_IN4?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN4 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_LM78_IN5,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM78_IN5,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM78_IN5_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_LM78_IN5_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&LM78_ALARM_IN5?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN5 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_LM78_IN6,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM78_IN6,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM78_IN6_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_LM78_IN6_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&LM78_ALARM_IN6?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN6 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_LM78_FAN1,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM78_FAN1,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM78_FAN1_DIV,&fdiv) &&
      !sensors_get_feature(*name,SENSORS_LM78_FAN1_MIN,&min)) {
    if (valid) {
      print_label(label,10);
      printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)          %s\n",
             cur,min,fdiv, alarms&LM78_ALARM_FAN1?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get FAN1 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_LM78_FAN2,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM78_FAN2,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM78_FAN2_DIV,&fdiv) &&
      !sensors_get_feature(*name,SENSORS_LM78_FAN2_MIN,&min)) {
    if (valid) {
      print_label(label,10);
      printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)          %s\n",
             cur,min,fdiv, alarms&LM78_ALARM_FAN2?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get FAN2 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_LM78_FAN3,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM78_FAN3,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM78_FAN3_DIV,&fdiv) &&
      !sensors_get_feature(*name,SENSORS_LM78_FAN3_MIN,&min)) {
    if (valid) {
      print_label(label,10);
      printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)          %s\n",
             cur,min,fdiv, alarms&LM78_ALARM_FAN3?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get FAN3 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_LM78_TEMP,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM78_TEMP,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM78_TEMP_HYST,&min) &&
      !sensors_get_feature(*name,SENSORS_LM78_TEMP_OVER,&max)) {
    if (valid) {
      print_label(label,10);
      print_temp_info( cur, max, min, HYST, 1, 0);
      printf( " %s\n", alarms & LM78_ALARM_TEMP ? "ALARM" : "" );
    }
  } else
    printf("ERROR: Can't get TEMP data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_LM78_VID,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM78_VID,&cur)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V\n",cur);
    }
  }
  free_the_label(&label);
    
  if (!sensors_get_label_and_valid(*name,SENSORS_LM78_ALARMS,&label,&valid)
      && valid) {
    if(alarms & LM78_ALARM_BTI) {
      print_label(label,10);
      printf("Board temperature input (LM75)               ALARM\n");
    }
    if(alarms & LM78_ALARM_CHAS) {
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
  int alarms,beeps,valid;

  if (!sensors_get_feature(*name,SENSORS_GL518_ALARMS,&cur)) 
    alarms = cur + 0.5;
  else {
    printf("ERROR: Can't get ALARMS data!\n");
    alarms = 0;
  }
  if (!sensors_get_feature(*name,SENSORS_GL518_BEEPS,&cur)) 
    beeps = cur + 0.5;
  else {
    printf("ERROR: Can't get BEEPS data!\n");
    beeps = 0;
  }

/* VCC, VIN1 and VIN2 are handled specially because on early releases (0x00)
 * of the GL518SM, we cannot read their values. This is detected when the
 * reading would be 0.0 V and displayed as "(n/a)" instead. */
  if (!sensors_get_label_and_valid(*name,SENSORS_GL518_VDD,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_GL518_VDD,&cur) &&
      !sensors_get_feature(*name,SENSORS_GL518_VDD_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_GL518_VDD_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      if (cur == 0.0)
        printf("(n/a)     ");
      else
        printf("%+6.2f V  ",cur);
      printf(  "(min = %+6.2f V, max = %+6.2f V)   %s  %s\n",
             min,max,alarms&GL518_ALARM_VDD?"ALARM":"     ",
             beeps&GL518_ALARM_VDD?"(beep)":"");
    }
  } else
    printf("ERROR: Can't get VDD data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_GL518_VIN1,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_GL518_VIN1,&cur) &&
      !sensors_get_feature(*name,SENSORS_GL518_VIN1_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_GL518_VIN1_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      if (cur == 0.0)
        printf("(n/a)     ");
      else
        printf("%+6.2f V  ",cur);
      printf("(min = %+6.2f V, max = %+6.2f V)   %s  %s\n",
             min,max,alarms&GL518_ALARM_VIN1?"ALARM":"     ",
             beeps&GL518_ALARM_VIN1?"(beep)":"");
    }
  } else
    printf("ERROR: Can't get VIN1 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_GL518_VIN2,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_GL518_VIN2,&cur) &&
      !sensors_get_feature(*name,SENSORS_GL518_VIN2_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_GL518_VIN2_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      if (cur == 0.0)
        printf("(n/a)     ");
      else
        printf("%+6.2f V  ",cur);
      printf("(min = %+6.2f V, max = %+6.2f V)   %s  %s\n",
             min,max,alarms&GL518_ALARM_VIN2?"ALARM":"     ",
             beeps&GL518_ALARM_VIN2?"(beep)":"");
    }
  } else
    printf("ERROR: Can't get VIN2 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_GL518_VIN3,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_GL518_VIN3,&cur) &&
      !sensors_get_feature(*name,SENSORS_GL518_VIN3_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_GL518_VIN3_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s  %s\n",
             cur,min,max,alarms&GL518_ALARM_VIN3?"ALARM":"     ",
             beeps&GL518_ALARM_VIN3?"(beep)":"");
     }
  } else
    printf("ERROR: Can't get VIN3 data!\n");
  free_the_label(&label);
  
  if (!sensors_get_label_and_valid(*name,SENSORS_GL518_FAN1,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_GL518_FAN1,&cur) &&
      !sensors_get_feature(*name,SENSORS_GL518_FAN1_DIV,&fdiv) &&
      !sensors_get_feature(*name,SENSORS_GL518_FAN1_MIN,&min)) {
    if (valid) {
      print_label(label,10);
      printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)          %s  %s\n",
             cur,min,fdiv, alarms&GL518_ALARM_FAN1?"ALARM":"     ",
             beeps&GL518_ALARM_FAN1?"(beep)":"");
    }
  } else
    printf("ERROR: Can't get FAN1 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_GL518_FAN2,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_GL518_FAN2,&cur) &&
      !sensors_get_feature(*name,SENSORS_GL518_FAN2_DIV,&fdiv) &&
      !sensors_get_feature(*name,SENSORS_GL518_FAN2_MIN,&min)) {
    if (valid) {
      print_label(label,10);
      printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)          %s  %s\n",
             cur,min,fdiv, alarms&GL518_ALARM_FAN2?"ALARM":"     ",
             beeps&GL518_ALARM_FAN2?"(beep)":"");
    }
  } else
    printf("ERROR: Can't get FAN2 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_GL518_TEMP,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_GL518_TEMP,&cur) &&
      !sensors_get_feature(*name,SENSORS_GL518_TEMP_OVER,&max) &&
      !sensors_get_feature(*name,SENSORS_GL518_TEMP_HYST,&min)) {
    if (valid) {
      print_label(label,10);
      print_temp_info( cur, max, min, HYST, 1, 0);
      printf("%s  %s\n", alarms&GL518_ALARM_TEMP?"ALARM":"     ",
             beeps&GL518_ALARM_TEMP?"(beep)":"");
    }
  } else
    printf("ERROR: Can't get TEMP data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_GL518_BEEP_ENABLE,&label,&valid)
      && valid) {
    if (!sensors_get_feature(*name,SENSORS_GL518_BEEP_ENABLE,&cur)) {
      print_label(label,10);
      if (cur < 0.5) 
        printf("Sound alarm disabled\n");
      else
        printf("Sound alarm enabled\n");
    } else
      printf("ERROR: Can't get BEEP ENABLE data!\n");
  }
  free_the_label(&label);
}

void print_gl520(const sensors_chip_name *name)
{
  char *label = NULL;
  double cur,min,max;
  int alarms,beeps,valid;
  int two_temps = 0;

  if (!sensors_get_feature(*name,SENSORS_GL520_ALARMS,&cur))
    alarms = cur + 0.5;
  else {
    printf("ERROR: Can't get ALARMS data!\n");
    alarms = 0;
  }
  if (!sensors_get_feature(*name,SENSORS_GL520_BEEPS,&cur)) 
    beeps = cur + 0.5;
  else {
    printf("ERROR: Can't get BEEPS data!\n");
    beeps = 0;
  }

  if (!sensors_get_label_and_valid(*name,SENSORS_GL520_VID,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_GL520_VID,&cur)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V\n",cur);
    }
  } else
    printf("ERROR: Can't get VID data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_GL520_VDD,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_GL520_VDD,&cur) &&
      !sensors_get_feature(*name,SENSORS_GL520_VDD_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_GL520_VDD_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  ",cur);
      printf(  "(min = %+6.2f V, max = %+6.2f V)   %s  %s\n",
             min,max,alarms&GL520_ALARM_VDD?"ALARM":"     ",
             beeps&GL520_ALARM_VDD?"(beep)":"");
    }
  } else
    printf("ERROR: Can't get VDD data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_GL520_VIN1,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_GL520_VIN1,&cur) &&
      !sensors_get_feature(*name,SENSORS_GL520_VIN1_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_GL520_VIN1_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  ",cur);
      printf("(min = %+6.2f V, max = %+6.2f V)   %s  %s\n",
             min,max,alarms&GL520_ALARM_VIN1?"ALARM":"     ",
             beeps&GL520_ALARM_VIN1?"(beep)":"");
    }
  } else
    printf("ERROR: Can't get VIN1 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_GL520_VIN2,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_GL520_VIN2,&cur) &&
      !sensors_get_feature(*name,SENSORS_GL520_VIN2_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_GL520_VIN2_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  ",cur);
      printf("(min = %+6.2f V, max = %+6.2f V)   %s  %s\n",
             min,max,alarms&GL520_ALARM_VIN2?"ALARM":"     ",
             beeps&GL520_ALARM_VIN2?"(beep)":"");
    }
  } else
    printf("ERROR: Can't get VIN2 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_GL520_VIN3,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_GL520_VIN3,&cur) &&
      !sensors_get_feature(*name,SENSORS_GL520_VIN3_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_GL520_VIN3_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s  %s\n",
             cur,min,max,alarms&GL520_ALARM_VIN3?"ALARM":"     ",
             beeps&GL520_ALARM_VIN3?"(beep)":"");
     }
  } else
    printf("ERROR: Can't get VIN3 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_GL520_VIN4,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_GL520_VIN4,&cur) &&
      !sensors_get_feature(*name,SENSORS_GL520_VIN4_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_GL520_VIN4_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s  %s\n",
             cur,min,max,alarms&GL520_ALARM_VIN4?"ALARM":"     ",
             beeps&GL520_ALARM_VIN4?"(beep)":"");
     }
  } else
    two_temps = 1;
  free_the_label(&label);
  
  if (!sensors_get_label_and_valid(*name,SENSORS_GL520_FAN1,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_GL520_FAN1,&cur) &&
      !sensors_get_feature(*name,SENSORS_GL520_FAN1_DIV,&max) &&
      !sensors_get_feature(*name,SENSORS_GL520_FAN1_MIN,&min)) {
    if (valid) {
      print_label(label,10);
      printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)          %s  %s\n",
             cur,min,max, alarms&GL520_ALARM_FAN1?"ALARM":"     ",
             beeps&GL520_ALARM_FAN1?"(beep)":"");
    }
  } else
    printf("ERROR: Can't get FAN1 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_GL520_FAN2,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_GL520_FAN2,&cur) &&
      !sensors_get_feature(*name,SENSORS_GL520_FAN2_DIV,&max) &&
      !sensors_get_feature(*name,SENSORS_GL520_FAN2_MIN,&min)) {
    if (valid) {
      print_label(label,10);
      printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)          %s  %s\n",
             cur,min,max, alarms&GL520_ALARM_FAN2?"ALARM":"     ",
             beeps&GL520_ALARM_FAN2?"(beep)":"");
    }
  } else
    printf("ERROR: Can't get FAN2 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_GL520_TEMP1,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_GL520_TEMP1,&cur) &&
      !sensors_get_feature(*name,SENSORS_GL520_TEMP1_OVER,&max) &&
      !sensors_get_feature(*name,SENSORS_GL520_TEMP1_HYST,&min)) {
    if (valid) {
      print_label(label,10);
      print_temp_info( cur, max, min, HYST, 1, 0);
      printf("%s  %s\n", alarms&GL520_ALARM_TEMP1?"ALARM":"     ",
             beeps&GL520_ALARM_TEMP1?"(beep)":"");
    }
  } else
    printf("ERROR: Can't get TEMP1 data!\n");
  free_the_label(&label);

  if (two_temps) {
    if (!sensors_get_label_and_valid(*name,SENSORS_GL520_TEMP2,&label,&valid) &&
	!sensors_get_feature(*name,SENSORS_GL520_TEMP2,&cur) &&
	!sensors_get_feature(*name,SENSORS_GL520_TEMP2_OVER,&max) &&
	!sensors_get_feature(*name,SENSORS_GL520_TEMP2_HYST,&min)) {
      if (valid) {
	print_label(label,10);
	print_temp_info( cur, max, min, HYST, 1, 0);
	printf("%s  %s\n", alarms&GL520_ALARM_TEMP2?"ALARM":"     ",
	       beeps&GL520_ALARM_TEMP2?"(beep)":"");
      }
    } else 
      printf("ERROR: Can't get TEMP2 or VIN4 data!\n");
    free_the_label(&label);
  }

  if (!sensors_get_label_and_valid(*name,SENSORS_GL520_BEEP_ENABLE,&label,&valid)
      && valid) {
    if (!sensors_get_feature(*name,SENSORS_GL520_BEEP_ENABLE,&cur)) {
      print_label(label,10);
      if (cur < 0.5) 
        printf("Sound alarm disabled\n");
      else
        printf("Sound alarm enabled\n");
    } else
      printf("ERROR: Can't get BEEP ENABLE data!\n");
  }
  free_the_label(&label);
}

void print_adm1025(const sensors_chip_name *name)
{
  char *label = NULL;
  double cur,min,max;
  int alarms,valid,i;

  if (!sensors_get_feature(*name,SENSORS_ADM1025_ALARMS,&cur)) 
    alarms = cur + 0.5;
  else {
    printf("ERROR: Can't get alarm data!\n");
    alarms = 0;
  }

  for (i=0; i<6; i++) {
    if (!sensors_get_label_and_valid(*name,SENSORS_ADM1025_IN0+i,&label,&valid) &&
        !sensors_get_feature(*name,SENSORS_ADM1025_IN0+i,&cur) &&
        !sensors_get_feature(*name,SENSORS_ADM1025_IN0_MIN+i,&min) &&
        !sensors_get_feature(*name,SENSORS_ADM1025_IN0_MAX+i,&max)) {
      if (valid) {
        print_label(label,10);
        printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&(ADM1025_ALARM_IN0<<(i+(i/4)*4))?"ALARM":"");
      }
    } else
      if (i!=4) /* Chip may have +12V input used for VID instead */
        printf("ERROR: Can't get IN%d data!\n", i);
    free_the_label(&label);
  }

  for (i=0; i<2; i++) {
    if (!sensors_get_label_and_valid(*name,SENSORS_ADM1025_TEMP1+3*i,&label,&valid) &&
        !sensors_get_feature(*name,SENSORS_ADM1025_TEMP1+3*i,&cur) &&
        !sensors_get_feature(*name,SENSORS_ADM1025_TEMP1_LOW+3*i,&min) &&
        !sensors_get_feature(*name,SENSORS_ADM1025_TEMP1_HIGH+3*i,&max)) {
      if (valid) {
        print_label(label,10);
        print_temp_info( cur, max, min, MINMAX, 1, 0);
        printf(" %s\n", i==0&&(alarms&ADM1025_ALARM_RFAULT)?"FAULT":
                        alarms&(ADM1025_ALARM_RTEMP>>i)?"ALARM":"");
      }
    } else
      printf("ERROR: Can't get TEMP%d data!\n", i+1);
    free_the_label(&label);
  }

  print_vid_info(name, SENSORS_ADM1025_VID, SENSORS_ADM1025_VRM);
}

void print_lm80(const sensors_chip_name *name)
{
  char *label = NULL;
  double cur,min,max,min2,max2,fdiv;
  int alarms,valid;

  if (!sensors_get_feature(*name,SENSORS_LM80_ALARMS,&cur)) 
    alarms = cur + 0.5;
  else {
    printf("ERROR: Can't get alarm data!\n");
    alarms = 0;
  }

  if (!sensors_get_label_and_valid(*name,SENSORS_LM80_IN0,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM80_IN0,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM80_IN0_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_LM80_IN0_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
           cur,min,max,alarms&LM80_ALARM_IN0?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN0 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_LM80_IN1,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM80_IN1,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM80_IN1_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_LM80_IN1_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
           cur,min,max,alarms&LM80_ALARM_IN1?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN1 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_LM80_IN2,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM80_IN2,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM80_IN2_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_LM80_IN2_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
           cur,min,max,alarms&LM80_ALARM_IN2?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN2 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_LM80_IN3,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM80_IN3,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM80_IN3_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_LM80_IN3_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
           cur,min,max,alarms&LM80_ALARM_IN3?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN3 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_LM80_IN4,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM80_IN4,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM80_IN4_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_LM80_IN4_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
           cur,min,max,alarms&LM80_ALARM_IN4?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN4 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_LM80_IN5,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM80_IN5,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM80_IN5_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_LM80_IN5_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
           cur,min,max,alarms&LM80_ALARM_IN5?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN5 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_LM80_IN6,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM80_IN6,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM80_IN6_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_LM80_IN6_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
           cur,min,max,alarms&LM80_ALARM_IN6?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN6 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_LM80_FAN1,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM80_FAN1,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM80_FAN1_DIV,&fdiv) &&
      !sensors_get_feature(*name,SENSORS_LM80_FAN1_MIN,&min)) {
    if (valid) {
      print_label(label,10);
      printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)          %s\n",
           cur,min,fdiv, alarms&LM80_ALARM_FAN1?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get FAN1 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_LM80_FAN2,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM80_FAN2,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM80_FAN2_DIV,&fdiv) &&
      !sensors_get_feature(*name,SENSORS_LM80_FAN2_MIN,&min)) {
    if (valid) {
      print_label(label,10);
      printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)          %s\n",
           cur,min,fdiv, alarms&LM80_ALARM_FAN2?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get FAN2 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_LM80_TEMP,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM80_TEMP,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM80_TEMP_HOT_HYST,&min) &&
      !sensors_get_feature(*name,SENSORS_LM80_TEMP_HOT_MAX,&max) &&
      !sensors_get_feature(*name,SENSORS_LM80_TEMP_OS_HYST,&min2) &&
      !sensors_get_feature(*name,SENSORS_LM80_TEMP_OS_MAX,&max2)) {
    if (valid) {
      if (fahrenheit) {
        cur = deg_ctof(cur);
        max = deg_ctof(max);
        min = deg_ctof(min);
        max2 = deg_ctof(max2);
        min2 = deg_ctof(min2);
      }

      print_label(label,10);
      printf("%+6.2f%s (hot: limit = %+3.0f%s, hyst = %+3.0f%s) %s\n",
             cur, degstr, max, degstr, min, degstr,
             alarms&LM80_ALARM_TEMP_HOT?"ALARM":"");
      print_label("",10);
      printf("         (os:  limit = %+3.0f%s, hyst = %+3.0f%s) %s\n",
             max2, degstr, min2, degstr,
             alarms&LM80_ALARM_TEMP_OS?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get TEMP data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_LM80_ALARMS,&label,&valid)
      && valid) {
    if (alarms & LM80_ALARM_BTI) {
      print_label(label,10);
      printf("Board temperature input (LM75)               ALARM\n");
    }
    if (alarms & LM80_ALARM_CHAS) {
      print_label(label,10);
      printf("Chassis intrusion detection                  ALARM\n");
    }
  }
  free_the_label(&label);
}

void print_lm85(const sensors_chip_name *name)
{
  char *label = NULL;
  double cur, min, max;
  int alarms, alarm_mask = 0, valid;
  int is85, is1027, is6d100;

  is85 = !strcmp(name->prefix,"lm85")
         || !strcmp(name->prefix,"lm85b")
         || !strcmp(name->prefix,"lm85c") ;
  is1027 = !strcmp(name->prefix,"adm1027")
           || !strcmp(name->prefix,"adt7463") ;
  is6d100 = !strcmp(name->prefix,"emc6d100") ;

  if (!sensors_get_feature(*name,SENSORS_LM85_ALARMS,&cur)) 
    alarms = cur + 0.5;
  else {
    printf("ERROR: Can't get alarm data!\n");
    alarms = 0;
  }

  if (is1027 &&
      !sensors_get_feature(*name, SENSORS_ADM1027_ALARM_MASK, &cur)) {
    alarm_mask = cur + 0.5;
  }

  if (!sensors_get_label_and_valid(*name,SENSORS_LM85_IN0,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM85_IN0,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM85_IN0_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_LM85_IN0_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+7.*f V  (min = %+6.2f V, max = %+6.2f V)   %s",
           (is1027?3:2),cur,min,max,alarms&LM85_ALARM_IN0?"ALARM":"");
      if (is1027) { printf(alarm_mask&LM85_ALARM_IN0?" MASKED":""); }
      putchar( '\n' );
    }
  } else
    printf("ERROR: Can't get IN0 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_LM85_IN1,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM85_IN1,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM85_IN1_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_LM85_IN1_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+7.*f V  (min = %+6.2f V, max = %+6.2f V)   %s",
           (is1027?3:2),cur,min,max,alarms&LM85_ALARM_IN1?"ALARM":"");
      if (is1027) { printf(alarm_mask&LM85_ALARM_IN1?" MASKED":""); }
      putchar( '\n' );
    }
  } else
    printf("ERROR: Can't get IN1 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_LM85_IN2,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM85_IN2,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM85_IN2_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_LM85_IN2_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+7.*f V  (min = %+6.2f V, max = %+6.2f V)   %s",
           (is1027?3:2),cur,min,max,alarms&LM85_ALARM_IN2?"ALARM":"");
      if (is1027) { printf(alarm_mask&LM85_ALARM_IN2?" MASKED":""); }
      putchar( '\n' );
    }
  } else
    printf("ERROR: Can't get IN2 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_LM85_IN3,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM85_IN3,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM85_IN3_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_LM85_IN3_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.*f V  (min = %+6.2f V, max = %+6.2f V)   %s",
           (is1027?3:2),cur,min,max,alarms&LM85_ALARM_IN3?"ALARM":"");
      if (is1027) { printf(alarm_mask&LM85_ALARM_IN3?" MASKED":""); }
      putchar( '\n' );
    }
  } else
    printf("ERROR: Can't get IN3 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_LM85_IN4,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM85_IN4,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM85_IN4_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_LM85_IN4_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.*f V  (min = %+6.2f V, max = %+6.2f V)   %s",
           (is1027?3:2),cur,min,max,alarms&LM85_ALARM_IN4?"ALARM":"");
      if (is1027) { printf(alarm_mask&LM85_ALARM_IN4?" MASKED":""); }
      putchar( '\n' );
    }
  } else
    printf("ERROR: Can't get IN4 data!\n");
  free_the_label(&label);

  if( is6d100 ) {
    if (!sensors_get_label_and_valid(*name,SENSORS_LM85_IN5,&label,&valid) &&
        !sensors_get_feature(*name,SENSORS_LM85_IN5,&cur) &&
        !sensors_get_feature(*name,SENSORS_LM85_IN5_MIN,&min) &&
        !sensors_get_feature(*name,SENSORS_LM85_IN5_MAX,&max)) {
      if (valid) {
        print_label(label,10);
        printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&LM85_ALARM_IN5?"ALARM":"");
      }
    } else
      printf("ERROR: Can't get IN5 data!\n");
    free_the_label(&label);
    if (!sensors_get_label_and_valid(*name,SENSORS_LM85_IN6,&label,&valid) &&
        !sensors_get_feature(*name,SENSORS_LM85_IN6,&cur) &&
        !sensors_get_feature(*name,SENSORS_LM85_IN6_MIN,&min) &&
        !sensors_get_feature(*name,SENSORS_LM85_IN6_MAX,&max)) {
      if (valid) {
        print_label(label,10);
        printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&LM85_ALARM_IN6?"ALARM":"");
      }
    } else
      printf("ERROR: Can't get IN6 data!\n");
    free_the_label(&label);
    if (!sensors_get_label_and_valid(*name,SENSORS_LM85_IN7,&label,&valid) &&
        !sensors_get_feature(*name,SENSORS_LM85_IN7,&cur) &&
        !sensors_get_feature(*name,SENSORS_LM85_IN7_MIN,&min) &&
        !sensors_get_feature(*name,SENSORS_LM85_IN7_MAX,&max)) {
      if (valid) {
        print_label(label,10);
        printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&LM85_ALARM_IN7?"ALARM":"");
      }
    } else
      printf("ERROR: Can't get IN7 data!\n");
    free_the_label(&label);
  }

  if (!sensors_get_label_and_valid(*name,SENSORS_LM85_FAN1,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM85_FAN1,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM85_FAN1_MIN,&min)) {
    if (valid) {
      print_label(label,10);
      printf("%5.0f RPM  (min = %4.0f RPM)                     %s",
           cur,min, alarms&LM85_ALARM_FAN1?"ALARM":"");
      if (is1027) { printf(alarm_mask&LM85_ALARM_FAN1?" MASKED":""); }
      putchar( '\n' );
    }
  } else
    printf("ERROR: Can't get FAN1 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_LM85_FAN2,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM85_FAN2,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM85_FAN2_MIN,&min)) {
    if (valid) {
      print_label(label,10);
      printf("%5.0f RPM  (min = %4.0f RPM)                     %s",
           cur,min, alarms&LM85_ALARM_FAN2?"ALARM":"");
      if (is1027) { printf(alarm_mask&LM85_ALARM_FAN2?" MASKED":""); }
      putchar( '\n' );
    }
  } else
    printf("ERROR: Can't get FAN2 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_LM85_FAN3,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM85_FAN3,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM85_FAN3_MIN,&min)) {
    if (valid) {
      print_label(label,10);
      printf("%5.0f RPM  (min = %4.0f RPM)                     %s",
           cur,min, alarms&LM85_ALARM_FAN3?"ALARM":"");
      if (is1027) { printf(alarm_mask&LM85_ALARM_FAN3?" MASKED":""); }
      putchar( '\n' );
    }
  } else
    printf("ERROR: Can't get FAN3 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_LM85_FAN4,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM85_FAN4,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM85_FAN4_MIN,&min)) {
    if (valid) {
      print_label(label,10);
      printf("%5.0f RPM  (min = %4.0f RPM)                     %s",
           cur,min, alarms&LM85_ALARM_FAN4?"ALARM":"");
      if (is1027) { printf(alarm_mask&LM85_ALARM_FAN4?" MASKED":""); }
      putchar( '\n' );
    }
  } else
    printf("ERROR: Can't get FAN4 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_LM85_TEMP1,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM85_TEMP1,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM85_TEMP1_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_LM85_TEMP1_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      print_temp_info( cur, max, min, MINMAX, (is1027 ? 2 : 0), 0);
      printf( "   %s %s %s\n",
                 alarms&LM85_ALARM_TEMP1?"ALARM":"",
                 alarms&LM85_ALARM_TEMP1_FAULT?"FAULT":"",
                 is1027&&(alarm_mask&LM85_ALARM_TEMP1)?"MASKED":""
            );
    }
  } else
    printf("ERROR: Can't get TEMP1 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_LM85_TEMP2,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM85_TEMP2,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM85_TEMP2_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_LM85_TEMP2_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      print_temp_info( cur, max, min, MINMAX, (is1027 ? 2 : 0), 0);
      printf( "   %s %s\n",
                 alarms&LM85_ALARM_TEMP2?"ALARM":"",
                 is1027&&(alarm_mask&LM85_ALARM_TEMP2)?"MASKED":""
            );
    }
  } else
    printf("ERROR: Can't get TEMP2 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_LM85_TEMP3,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM85_TEMP3,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM85_TEMP3_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_LM85_TEMP3_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      print_temp_info( cur, max, min, MINMAX, (is1027 ? 2 : 0), 0);
      printf( "   %s %s %s\n",
                 alarms&LM85_ALARM_TEMP3?"ALARM":"",
                 alarms&LM85_ALARM_TEMP3_FAULT?"FAULT":"",
                 is1027&&(alarm_mask&LM85_ALARM_TEMP3)?"MASKED":""
            );
    }
  } else
    printf("ERROR: Can't get TEMP3 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_LM85_PWM1,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM85_PWM1,&cur)) {
    if (valid) {
      print_label(label,10);
      printf("%4.0f\n", cur);
    }
  } else
    printf("ERROR: Can't get PWM1 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_LM85_PWM2,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM85_PWM2,&cur)) {
    if (valid) {
      print_label(label,10);
      printf("%4.0f\n", cur);
    }
  } else
    printf("ERROR: Can't get PWM2 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_LM85_PWM3,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM85_PWM3,&cur)) {
    if (valid) {
      print_label(label,10);
      printf("%4.0f\n", cur);
    }
  } else
    printf("ERROR: Can't get PWM3 data!\n");
  free_the_label(&label);

  print_vid_info(name, SENSORS_LM85_VID, SENSORS_LM85_VRM);
}

void print_lm87(const sensors_chip_name *name)
{
  char *label = NULL;
  double cur,min,max,fdiv;
  int alarms,valid;

  if (!sensors_get_feature(*name,SENSORS_LM87_ALARMS,&cur))
    alarms = cur + 0.5;
  else {
    printf("ERROR: Can't get alarm data!\n");
    alarms = 0;
  }


  if (!sensors_get_label_and_valid(*name,SENSORS_LM87_IN0,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM87_IN0,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM87_IN0_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_LM87_IN0_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&LM87_ALARM_IN0?"ALARM":"");
    }
  }
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_LM87_IN1,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM87_IN1,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM87_IN1_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_LM87_IN1_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&LM87_ALARM_IN1?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN1 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_LM87_IN2,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM87_IN2,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM87_IN2_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_LM87_IN2_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&LM87_ALARM_IN2?"ALARM":"");
    }
  }
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_LM87_IN3,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM87_IN3,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM87_IN3_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_LM87_IN3_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&LM87_ALARM_IN3?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN3 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_LM87_IN4,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM87_IN4,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM87_IN4_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_LM87_IN4_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&LM87_ALARM_IN4?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN4 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_LM87_IN5,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM87_IN5,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM87_IN5_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_LM87_IN5_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&LM87_ALARM_IN5?"ALARM":"");
    }
  }
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_LM87_AIN1,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM87_AIN1,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM87_AIN1_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_LM87_AIN1_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&LM87_ALARM_FAN1?"ALARM":"");
    }
  }
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_LM87_AIN2,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM87_AIN2,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM87_AIN2_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_LM87_AIN2_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&LM87_ALARM_FAN2?"ALARM":"");
    }
  }
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_LM87_FAN1,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM87_FAN1,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM87_FAN1_DIV,&fdiv) &&
      !sensors_get_feature(*name,SENSORS_LM87_FAN1_MIN,&min)) {
    if (valid) {
      print_label(label,10);
      printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)          %s\n",
             cur,min,fdiv, alarms&LM87_ALARM_FAN1?"ALARM":"");
    }
  }
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_LM87_FAN2,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM87_FAN2,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM87_FAN2_DIV,&fdiv) &&
      !sensors_get_feature(*name,SENSORS_LM87_FAN2_MIN,&min)) {
    if (valid) {
      print_label(label,10);
      printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)          %s\n",
             cur,min,fdiv, alarms&LM87_ALARM_FAN2 ?"ALARM":"");
    }
  }
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_LM87_TEMP1,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM87_TEMP1,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM87_TEMP1_HYST,&min) &&
      !sensors_get_feature(*name,SENSORS_LM87_TEMP1_OVER,&max)) {
    if (valid) {
      print_label(label,10);
      print_temp_info( cur, max, min, MINMAX, 0, 0);
      printf(" %s%s\n", alarms&LM87_ALARM_TEMP1?"ALARM":"",
      	alarms&LM87_ALARM_THERM_SIG?" THERM#":"");
    }
  } else
    printf("ERROR: Can't get TEMP1 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_LM87_TEMP2,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM87_TEMP2,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM87_TEMP2_HYST,&min) &&
      !sensors_get_feature(*name,SENSORS_LM87_TEMP2_OVER,&max)) {
    if (valid) {
      print_label(label,10);
      print_temp_info( cur, max, min, MINMAX, 0, 0);
      printf(" %s%s\n", alarms&LM87_ALARM_TEMP2?"ALARM":"",
      	alarms&LM87_ALARM_TEMP2_FAULT?" FAULT":"");
    }
  } else
    printf("ERROR: Can't get TEMP2 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_LM87_TEMP3,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM87_TEMP3,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM87_TEMP3_HYST,&min) &&
      !sensors_get_feature(*name,SENSORS_LM87_TEMP3_OVER,&max)) {
    if (valid) {
      print_label(label,10);
      print_temp_info( cur, max, min, MINMAX, 0, 0);
      printf(" %s%s\n", alarms&LM87_ALARM_TEMP3?"ALARM":"",
      	alarms&LM87_ALARM_TEMP3_FAULT?" FAULT":"");
    }
  }
  free_the_label(&label);

  print_vid_info(name, SENSORS_LM87_VID, SENSORS_LM87_VRM);
}

void print_mtp008(const sensors_chip_name *name)
{
  char *label = NULL;
  double cur,min,max,fdiv;
  int alarms,valid;

  if (!sensors_get_feature(*name,SENSORS_MTP008_ALARMS,&cur))
    alarms = cur + 0.5;
  else {
    printf("ERROR: Can't get alarm data!\n");
    alarms = 0;
  }


  if (!sensors_get_label_and_valid(*name,SENSORS_MTP008_IN0,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_MTP008_IN0,&cur) &&
      !sensors_get_feature(*name,SENSORS_MTP008_IN0_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_MTP008_IN0_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&MTP008_ALARM_IN0?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN0 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_MTP008_IN1,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_MTP008_IN1,&cur) &&
      !sensors_get_feature(*name,SENSORS_MTP008_IN1_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_MTP008_IN1_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&MTP008_ALARM_IN1?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN1 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_MTP008_IN2,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_MTP008_IN2,&cur) &&
      !sensors_get_feature(*name,SENSORS_MTP008_IN2_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_MTP008_IN2_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&MTP008_ALARM_IN2?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN2 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_MTP008_IN3,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_MTP008_IN3,&cur) &&
      !sensors_get_feature(*name,SENSORS_MTP008_IN3_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_MTP008_IN3_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&MTP008_ALARM_IN3?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN3 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_MTP008_IN4,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_MTP008_IN4,&cur) &&
      !sensors_get_feature(*name,SENSORS_MTP008_IN4_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_MTP008_IN4_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&MTP008_ALARM_IN4?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN4 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_MTP008_IN5,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_MTP008_IN5,&cur) &&
      !sensors_get_feature(*name,SENSORS_MTP008_IN5_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_MTP008_IN5_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&MTP008_ALARM_IN5?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN5 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_MTP008_IN6,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_MTP008_IN6,&cur) &&
      !sensors_get_feature(*name,SENSORS_MTP008_IN6_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_MTP008_IN6_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&MTP008_ALARM_IN6?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN6 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_MTP008_FAN1,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_MTP008_FAN1,&cur) &&
      !sensors_get_feature(*name,SENSORS_MTP008_FAN1_DIV,&fdiv) &&
      !sensors_get_feature(*name,SENSORS_MTP008_FAN1_MIN,&min)) {
    if (valid) {
      print_label(label,10);
      printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)          %s\n",
             cur,min,fdiv, alarms&MTP008_ALARM_FAN1?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get FAN1 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_MTP008_FAN2,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_MTP008_FAN2,&cur) &&
      !sensors_get_feature(*name,SENSORS_MTP008_FAN2_DIV,&fdiv) &&
      !sensors_get_feature(*name,SENSORS_MTP008_FAN2_MIN,&min)) {
    if (valid) {
      print_label(label,10);
      printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)          %s\n",
             cur,min,fdiv, alarms&MTP008_ALARM_FAN2?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get FAN2 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_MTP008_FAN3,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_MTP008_FAN3,&cur) &&
      !sensors_get_feature(*name,SENSORS_MTP008_FAN3_DIV,&fdiv) &&
      !sensors_get_feature(*name,SENSORS_MTP008_FAN3_MIN,&min)) {
    if (valid) {
      print_label(label,10);
      printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)          %s\n",
             cur,min,fdiv, alarms&MTP008_ALARM_FAN3?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get FAN3 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_MTP008_TEMP1,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_MTP008_TEMP1,&cur) &&
      !sensors_get_feature(*name,SENSORS_MTP008_TEMP1_HYST,&min) &&
      !sensors_get_feature(*name,SENSORS_MTP008_TEMP1_OVER,&max)) {
    if (valid) {
      print_label(label,10);
      print_temp_info( cur, max, min, HYST, 0, 0);
      printf(" %s\n", alarms&MTP008_ALARM_TEMP1?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get TEMP1 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_MTP008_TEMP2,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_MTP008_TEMP2,&cur) &&
      !sensors_get_feature(*name,SENSORS_MTP008_TEMP2_HYST,&min) &&
      !sensors_get_feature(*name,SENSORS_MTP008_TEMP2_OVER,&max)) {
    if (valid) {
      print_label(label,10);
      print_temp_info( cur, max, min, HYST, 0, 0);
      printf(" %s\n", alarms&MTP008_ALARM_TEMP2?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get TEMP2 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_MTP008_TEMP3,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_MTP008_TEMP3,&cur) &&
      !sensors_get_feature(*name,SENSORS_MTP008_TEMP3_HYST,&min) &&
      !sensors_get_feature(*name,SENSORS_MTP008_TEMP3_OVER,&max)) {
    if (valid) {
      print_label(label,10);
      print_temp_info( cur, max, min, HYST, 0, 0);
      printf(" %s\n", alarms&MTP008_ALARM_TEMP3?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get TEMP3 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_MTP008_VID,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_MTP008_VID,&cur)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V\n",cur);
    }
  }
  free_the_label(&label);
}

void print_w83781d(const sensors_chip_name *name)
{
  char *label = NULL;
  double cur,min,max,fdiv,sens;
  int alarms,beeps;
  int is81d, is82d, is83s, is697hf, is627thf, valid;

  is81d = !strcmp(name->prefix,"w83781d");
  is82d = (!strcmp(name->prefix,"w83782d")) ||
          (!strcmp(name->prefix,"w83627hf")) ||
          (!strcmp(name->prefix,"w83637hf")) ||
          (!strcmp(name->prefix,"w83627thf"));
  is83s = !strcmp(name->prefix,"w83783s");
  is627thf = (!strcmp(name->prefix,"w83627thf")) ||
             (!strcmp(name->prefix,"w83637hf"));
  is697hf  = !strcmp(name->prefix,"w83697hf");

  if (!sensors_get_feature(*name,SENSORS_W83781D_ALARMS,&cur)) 
    alarms = cur + 0.5;
  else {
    printf("ERROR: Can't get alarm data!\n");
    alarms = 0;
  }

  if (!sensors_get_feature(*name,SENSORS_W83781D_BEEPS,&cur)) {
    beeps = cur + 0.5;
  } else {
    printf("ERROR: Can't get beep data!\n");
    beeps = 0;
  }

  if (!sensors_get_label_and_valid(*name,SENSORS_W83781D_IN0,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_W83781D_IN0,&cur) &&
      !sensors_get_feature(*name,SENSORS_W83781D_IN0_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_W83781D_IN0_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)       %s  %s\n",
           cur,min,max,alarms&W83781D_ALARM_IN0?"ALARM":"     ",
           beeps&W83781D_ALARM_IN0?"(beep)":"");
    }
  } else
    printf("ERROR: Can't get IN0 data!\n");
  free_the_label(&label);
  if ((!is83s) && (!is697hf)) {
    if (!sensors_get_label_and_valid(*name,SENSORS_W83781D_IN1,&label,&valid) &&
        !sensors_get_feature(*name,SENSORS_W83781D_IN1,&cur) &&
        !sensors_get_feature(*name,SENSORS_W83781D_IN1_MIN,&min) &&
        !sensors_get_feature(*name,SENSORS_W83781D_IN1_MAX,&max)) {
      if (valid) {
        print_label(label,10);
        printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)       %s  %s\n",
             cur,min,max,alarms&W83781D_ALARM_IN1?"ALARM":"     ",
             beeps&W83781D_ALARM_IN1?"(beep)":"");
      }
    } else
      printf("ERROR: Can't get IN1 data!\n");
    free_the_label(&label);
  }
  if (!sensors_get_label_and_valid(*name,SENSORS_W83781D_IN2,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_W83781D_IN2,&cur) &&
      !sensors_get_feature(*name,SENSORS_W83781D_IN2_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_W83781D_IN2_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)       %s  %s\n",
           cur,min,max,alarms&W83781D_ALARM_IN2?"ALARM":"     ",
           beeps&W83781D_ALARM_IN2?"(beep)":"");
    }
  } else
    printf("ERROR: Can't get IN2 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_W83781D_IN3,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_W83781D_IN3,&cur) &&
      !sensors_get_feature(*name,SENSORS_W83781D_IN3_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_W83781D_IN3_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)       %s  %s\n",
           cur,min,max,alarms&W83781D_ALARM_IN3?"ALARM":"     ",
           beeps&W83781D_ALARM_IN3?"(beep)":"");
    }
  } else
    printf("ERROR: Can't get IN3 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_W83781D_IN4,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_W83781D_IN4,&cur) &&
      !sensors_get_feature(*name,SENSORS_W83781D_IN4_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_W83781D_IN4_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)       %s  %s\n",
           cur,min,max,alarms&W83781D_ALARM_IN4?"ALARM":"     ",
           beeps&W83781D_ALARM_IN4?"(beep)":"");
    }
  } else
    printf("ERROR: Can't get IN4 data!\n");
  free_the_label(&label);
  if (!is627thf) {
    if (!sensors_get_label_and_valid(*name,SENSORS_W83781D_IN5,&label,&valid) &&
        !sensors_get_feature(*name,SENSORS_W83781D_IN5,&cur) &&
        !sensors_get_feature(*name,SENSORS_W83781D_IN5_MIN,&min) &&
        !sensors_get_feature(*name,SENSORS_W83781D_IN5_MAX,&max)) {
      if (valid) {
        print_label(label,10);
        printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)       %s  %s\n",
             cur,min,max,alarms&W83781D_ALARM_IN5?"ALARM":"     ",
             beeps&W83781D_ALARM_IN5?"(beep)":"");
      }
    } else
      printf("ERROR: Can't get IN5 data!\n");
    free_the_label(&label);
    if (!sensors_get_label_and_valid(*name,SENSORS_W83781D_IN6,&label,&valid) &&
        !sensors_get_feature(*name,SENSORS_W83781D_IN6,&cur) &&
        !sensors_get_feature(*name,SENSORS_W83781D_IN6_MIN,&min) &&
        !sensors_get_feature(*name,SENSORS_W83781D_IN6_MAX,&max)) {
      if (valid) {
        print_label(label,10);
        printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)       %s  %s\n",
             cur,min,max,alarms&W83781D_ALARM_IN6?"ALARM":"     ",
             beeps&W83781D_ALARM_IN6?"(beep)":"");
      }
    } else
      printf("ERROR: Can't get IN6 data!\n");
    free_the_label(&label);
  } /* !is627thf */
  if (is82d || is697hf || is627thf) {
    if (!sensors_get_label_and_valid(*name,SENSORS_W83782D_IN7,&label,&valid) &&
        !sensors_get_feature(*name,SENSORS_W83782D_IN7,&cur) &&
        !sensors_get_feature(*name,SENSORS_W83782D_IN7_MIN,&min) &&
        !sensors_get_feature(*name,SENSORS_W83782D_IN7_MAX,&max)) {
      if (valid) {
        print_label(label,10);
        printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)       %s  %s\n",
             cur,min,max,alarms&W83782D_ALARM_IN7?"ALARM":"     ",
             beeps&W83782D_ALARM_IN7?"(beep)":"");
      }
    } else
      printf("ERROR: Can't get IN7 data!\n");
    free_the_label(&label);
    if (!sensors_get_label_and_valid(*name,SENSORS_W83782D_IN8,&label,&valid) &&
        !sensors_get_feature(*name,SENSORS_W83782D_IN8,&cur) &&
        !sensors_get_feature(*name,SENSORS_W83782D_IN8_MIN,&min) &&
        !sensors_get_feature(*name,SENSORS_W83782D_IN8_MAX,&max)) {
      if (valid) {
        print_label(label,10);
        printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)       %s  %s\n",
             cur,min,max,alarms&W83782D_ALARM_IN8?"ALARM":"     ",
             beeps&W83782D_ALARM_IN8?"(beep)":"");
      }
    } else
      printf("ERROR: Can't get IN8 data!\n");
    free_the_label(&label);
  }

  if (!sensors_get_label_and_valid(*name,SENSORS_W83781D_FAN1,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_W83781D_FAN1,&cur) &&
      !sensors_get_feature(*name,SENSORS_W83781D_FAN1_DIV,&fdiv) &&
      !sensors_get_feature(*name,SENSORS_W83781D_FAN1_MIN,&min)) {
    if (valid) {
      print_label(label,10);
      printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)              %s  %s\n",
           cur,min,fdiv, alarms&W83781D_ALARM_FAN1?"ALARM":"     ",
           beeps&W83781D_ALARM_FAN1?"(beep)":"");
    }
  } else
    printf("ERROR: Can't get FAN1 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_W83781D_FAN2,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_W83781D_FAN2,&cur) &&
      !sensors_get_feature(*name,SENSORS_W83781D_FAN2_DIV,&fdiv) &&
      !sensors_get_feature(*name,SENSORS_W83781D_FAN2_MIN,&min)) {
    if (valid) {
      print_label(label,10);
      printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)              %s  %s\n",
           cur,min,fdiv, alarms&W83781D_ALARM_FAN2?"ALARM":"     ",
           beeps&W83781D_ALARM_FAN2?"(beep)":"");
    }
  } else
    printf("ERROR: Can't get FAN2 data!\n");
  free_the_label(&label);

  if(!is697hf) {
    if (!sensors_get_label_and_valid(*name,SENSORS_W83781D_FAN3,&label,&valid) &&
        !sensors_get_feature(*name,SENSORS_W83781D_FAN3,&cur) &&
        !sensors_get_feature(*name,SENSORS_W83781D_FAN3_DIV,&fdiv) &&
        !sensors_get_feature(*name,SENSORS_W83781D_FAN3_MIN,&min)) {
      if (valid) {
        print_label(label,10);
        printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)              %s  %s\n",
             cur,min,fdiv, alarms&W83781D_ALARM_FAN3?"ALARM":"     ",
             beeps&W83781D_ALARM_FAN3?"(beep)":"");
      }
    } else
      printf("ERROR: Can't get FAN3 data!\n");
    free_the_label(&label);
  }

  if (!sensors_get_label_and_valid(*name,SENSORS_W83781D_TEMP1,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_W83781D_TEMP1,&cur) &&
      !sensors_get_feature(*name,SENSORS_W83781D_TEMP1_HYST,&min) &&
      !sensors_get_feature(*name,SENSORS_W83781D_TEMP1_OVER,&max)) {
    if (valid) {
      if((!is82d) && (!is83s) && (!is697hf)) {
        print_label(label,10);
	if(min == 127)
          print_temp_info( cur, max, 0, MAXONLY, 0, 0);
	else
          print_temp_info( cur, max, min, HYST, 0, 0);
        printf(" %s  %s\n", alarms&W83781D_ALARM_TEMP1 ?"ALARM":"     ",
               beeps&W83781D_ALARM_TEMP1?"(beep)":"");
      } else {
        if(!sensors_get_feature(*name,SENSORS_W83781D_SENS1,&sens)) {
          print_label(label,10);
	  if(min == 127)
            print_temp_info( cur, max, 0, MAXONLY, 0, 0);
	  else
            print_temp_info( cur, max, min, HYST, 0, 0);
          printf( " sensor = %s   %s   %s\n",
                 (((int)sens)==1)?"diode":(((int)sens)==2)?
                 "transistor":"thermistor",
                 alarms&W83781D_ALARM_TEMP1?"ALARM":"     ",
                 beeps&W83781D_ALARM_TEMP1?"(beep)":"");
        } else {
          printf("ERROR: Can't get TEMP1 data!\n");
        }
      }
    }
  } else
    printf("ERROR: Can't get TEMP1 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_W83781D_TEMP2,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_W83781D_TEMP2,&cur) &&
      !sensors_get_feature(*name,SENSORS_W83781D_TEMP2_HYST,&min) &&
      !sensors_get_feature(*name,SENSORS_W83781D_TEMP2_OVER,&max)) {
    if (valid) {
      if((!is82d) && (!is83s) && (!is697hf)) {
        print_label(label,10);
        print_temp_info( cur, max, min, HYST, 1, 0);
        printf(" %s  %s\n", alarms&W83781D_ALARM_TEMP2 ?"ALARM":"     ",
               beeps&W83781D_ALARM_TEMP2?"(beep)":"");
      } else {
        if(!sensors_get_feature(*name,SENSORS_W83781D_SENS2,&sens)) {
          print_label(label,10);
          print_temp_info( cur, max, min, HYST, 1, 0);
          printf( " sensor = %s   %s   %s\n",
                 (((int)sens)==1)?"diode":(((int)sens)==2)?
                 "transistor":"thermistor",
                 alarms&W83781D_ALARM_TEMP2?"ALARM":"     ",
                 beeps&W83781D_ALARM_TEMP2?"(beep)":"");
        } else {
          printf("ERROR: Can't get TEMP2 data!\n");
        }
      }
    }
  } else
    printf("ERROR: Can't get TEMP2 data!\n");
  free_the_label(&label);

  if ((!is83s) && (!is697hf)) {
    if (!sensors_get_label_and_valid(*name,SENSORS_W83781D_TEMP3,&label,&valid) &&
        !sensors_get_feature(*name,SENSORS_W83781D_TEMP3,&cur) &&
        !sensors_get_feature(*name,SENSORS_W83781D_TEMP3_HYST,&min) &&
        !sensors_get_feature(*name,SENSORS_W83781D_TEMP3_OVER,&max)) {
      if (valid) {
        if(!is82d) {
          print_label(label,10);
          print_temp_info( cur, max, min, HYST, 1, 0);
          if (!is81d)
            printf(" %s  %s\n", alarms&W83781D_ALARM_TEMP3?"ALARM":"     ",
                   beeps&W83781D_ALARM_TEMP3?"(beep)":"");
          else
            printf(" %s  %s\n", alarms&W83781D_ALARM_TEMP23?"ALARM":"     ",
                   beeps&W83781D_ALARM_TEMP23?"(beep)":"");
        } else {
          if(!sensors_get_feature(*name,SENSORS_W83781D_SENS3,&sens)) {
            print_label(label,10);
            print_temp_info( cur, max, min, HYST, 1, 0);
            printf( " sensor = %s   %s   %s\n",
                   (((int)sens)==1)?"diode":(((int)sens)==2)?
                   "transistor":"thermistor",
                   alarms&W83781D_ALARM_TEMP3?"ALARM":"     ",
                   beeps&W83781D_ALARM_TEMP3?"(beep)":"");
          } else {
            printf("ERROR: Can't get TEMP3 data!\n");
          }
        }
      }
    } else
      printf("ERROR: Can't get TEMP3 data!\n");
    free_the_label(&label);
  }

  if(!is697hf) {
    print_vid_info(name, SENSORS_W83781D_VID, SENSORS_W83781D_VRM);
  }
    
  if (!sensors_get_label_and_valid(*name,SENSORS_W83781D_ALARMS,&label,&valid)
      && valid && !is83s) {
    print_label(label,10);
    if (alarms & W83781D_ALARM_CHAS)
      printf("Chassis intrusion detection                      ALARM\n");
    else
      printf("\n");
  }
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_W83781D_BEEP_ENABLE,&label,&valid)
      && valid) {
    if (!sensors_get_feature(*name,SENSORS_W83781D_BEEP_ENABLE,&cur)) {
      print_label(label,10);
      if (cur < 0.5) 
        printf("Sound alarm disabled\n");
      else
        printf("Sound alarm enabled\n");
    } else
      printf("ERROR: Can't get BEEP data!\n");
  }
  free_the_label(&label);
}


void print_w83792d(const sensors_chip_name *name)
{
  char *label = NULL;
  double cur,min,max,fdiv;
  int valid = 0;

  if (!sensors_get_label_and_valid(*name,SENSORS_W83792D_IN0,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_W83792D_IN0,&cur) &&
      !sensors_get_feature(*name,SENSORS_W83792D_IN0_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_W83792D_IN0_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)       %s\n",
           cur,min,max, ((cur<min)||(cur>max))?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN0 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_W83792D_IN1,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_W83792D_IN1,&cur) &&
      !sensors_get_feature(*name,SENSORS_W83792D_IN1_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_W83792D_IN1_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)       %s\n",
           cur,min,max, ((cur<min)||(cur>max))?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN1 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_W83792D_IN2,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_W83792D_IN2,&cur) &&
      !sensors_get_feature(*name,SENSORS_W83792D_IN2_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_W83792D_IN2_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)       %s\n",
           cur,min,max, ((cur<min)||(cur>max))?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN2 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_W83792D_IN3,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_W83792D_IN3,&cur) &&
      !sensors_get_feature(*name,SENSORS_W83792D_IN3_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_W83792D_IN3_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)       %s\n",
           cur,min,max, ((cur<min)||(cur>max))?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN3 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_W83792D_IN4,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_W83792D_IN4,&cur) &&
      !sensors_get_feature(*name,SENSORS_W83792D_IN4_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_W83792D_IN4_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)       %s\n",
           cur,min,max, ((cur<min)||(cur>max))?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN4 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_W83792D_IN5,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_W83792D_IN5,&cur) &&
      !sensors_get_feature(*name,SENSORS_W83792D_IN5_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_W83792D_IN5_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)       %s\n",
           cur,min,max, ((cur<min)||(cur>max))?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN5 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_W83792D_IN6,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_W83792D_IN6,&cur) &&
      !sensors_get_feature(*name,SENSORS_W83792D_IN6_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_W83792D_IN6_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)       %s\n",
           cur,min,max, ((cur<min)||(cur>max))?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN6 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_W83782D_IN7,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_W83782D_IN7,&cur) &&
      !sensors_get_feature(*name,SENSORS_W83782D_IN7_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_W83782D_IN7_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)       %s\n",
           cur,min,max, ((cur<min)||(cur>max))?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN7 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_W83782D_IN8,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_W83782D_IN8,&cur) &&
      !sensors_get_feature(*name,SENSORS_W83782D_IN8_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_W83782D_IN8_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)       %s\n",
           cur,min,max, ((cur<min)||(cur>max))?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN8 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_W83792D_FAN1,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_W83792D_FAN1,&cur) &&
      !sensors_get_feature(*name,SENSORS_W83792D_FAN1_DIV,&fdiv) &&
      !sensors_get_feature(*name,SENSORS_W83792D_FAN1_MIN,&min)) {
    if (valid) {
      print_label(label,10);
      printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)              %s\n",
           cur,min,fdiv, ((cur<min)&&(cur!=0))?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get FAN1 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_W83792D_FAN2,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_W83792D_FAN2,&cur) &&
      !sensors_get_feature(*name,SENSORS_W83792D_FAN2_DIV,&fdiv) &&
      !sensors_get_feature(*name,SENSORS_W83792D_FAN2_MIN,&min)) {
    if (valid) {
      print_label(label,10);
      printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)              %s\n",
           cur,min,fdiv, ((cur<min)&&(cur!=0))?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get FAN2 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_W83792D_FAN3,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_W83792D_FAN3,&cur) &&
      !sensors_get_feature(*name,SENSORS_W83792D_FAN3_DIV,&fdiv) &&
      !sensors_get_feature(*name,SENSORS_W83792D_FAN3_MIN,&min)) {
    if (valid) {
      print_label(label,10);
      printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)              %s\n",
           cur,min,fdiv, ((cur<min)&&(cur!=0))?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get FAN3 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_W83792D_FAN4,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_W83792D_FAN4,&cur) &&
      !sensors_get_feature(*name,SENSORS_W83792D_FAN4_DIV,&fdiv) &&
      !sensors_get_feature(*name,SENSORS_W83792D_FAN4_MIN,&min)) {
    if (valid) {
      print_label(label,10);
      printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)              %s\n",
           cur,min,fdiv, ((cur<min)&&(cur!=0))?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get FAN4 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_W83792D_FAN5,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_W83792D_FAN5,&cur) &&
      !sensors_get_feature(*name,SENSORS_W83792D_FAN5_DIV,&fdiv) &&
      !sensors_get_feature(*name,SENSORS_W83792D_FAN5_MIN,&min)) {
    if (valid) {
      print_label(label,10);
      printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)              %s\n",
           cur,min,fdiv, ((cur<min)&&(cur!=0))?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get FAN5 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_W83792D_FAN6,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_W83792D_FAN6,&cur) &&
      !sensors_get_feature(*name,SENSORS_W83792D_FAN6_DIV,&fdiv) &&
      !sensors_get_feature(*name,SENSORS_W83792D_FAN6_MIN,&min)) {
    if (valid) {
      print_label(label,10);
      printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)              %s\n",
           cur,min,fdiv, ((cur<min)&&(cur!=0))?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get FAN6 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_W83792D_FAN7,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_W83792D_FAN7,&cur) &&
      !sensors_get_feature(*name,SENSORS_W83792D_FAN7_DIV,&fdiv) &&
      !sensors_get_feature(*name,SENSORS_W83792D_FAN7_MIN,&min)) {
    if (valid) {
      print_label(label,10);
      printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)              %s\n",
           cur,min,fdiv, ((cur<min)&&(cur!=0))?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get FAN7 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_W83792D_TEMP1,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_W83792D_TEMP1,&cur) &&
      !sensors_get_feature(*name,SENSORS_W83792D_TEMP1_HYST,&min) &&
      !sensors_get_feature(*name,SENSORS_W83792D_TEMP1_OVER,&max)) {
    if (valid) {
      print_label(label,10);
      if(min == 127){
        print_temp_info( cur, max, 0, MAXONLY, 1, 1);
        printf(" %s\n", (cur>max)?"ALARM":"");
      }else{
        print_temp_info( cur, max, min, HYST, 1, 1);
        printf(" %s\n", ((cur<min)||(cur>max))?"ALARM":"");
      }
    }
  } else
    printf("ERROR: Can't get TEMP1 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_W83792D_TEMP2,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_W83792D_TEMP2,&cur) &&
      !sensors_get_feature(*name,SENSORS_W83792D_TEMP2_HYST,&min) &&
      !sensors_get_feature(*name,SENSORS_W83792D_TEMP2_OVER,&max)) {
    if (valid) {
      print_label(label,10);
      print_temp_info( cur, max, min, HYST, 1, 1);
      printf(" %s\n", ((cur<min)||(cur>max))?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get TEMP2 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_W83792D_TEMP3,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_W83792D_TEMP3,&cur) &&
      !sensors_get_feature(*name,SENSORS_W83792D_TEMP3_HYST,&min) &&
      !sensors_get_feature(*name,SENSORS_W83792D_TEMP3_OVER,&max)) {
    if (valid) {
      print_label(label,10);
      print_temp_info( cur, max, min, HYST, 1, 1);
      printf(" %s\n", ((cur<min)||(cur>max))?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get TEMP3 data!\n");
  free_the_label(&label);

  /* print_vid_info(name, SENSORS_W83792D_VID, SENSORS_W83792D_VRM); */
    
  if (!sensors_get_label_and_valid(*name,SENSORS_W83792D_CHASSIS,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_W83792D_CHASSIS,&cur) )
  {
    if (valid) {
      print_label(label,10);
      if (cur==1) {
        printf("Chassis intrusion detection                  ALARM\n");
      }else{
        printf("Chassis is normal.\n");
      }
    }
  }else{
    printf("ERROR: Can't get Chassis data!\n");
  }
  free_the_label(&label);
}

void print_w83l785ts(const sensors_chip_name *name)
{
  char *label;
  double cur, over;
  int valid;

  if (!sensors_get_label_and_valid(*name, SENSORS_W83L785TS_TEMP, &label, &valid)
   && !sensors_get_feature(*name, SENSORS_W83L785TS_TEMP, &cur)
   && !sensors_get_feature(*name, SENSORS_W83L785TS_TEMP_OVER, &over)) {
    if (valid) {
      print_label(label, 10);
      print_temp_info(cur, over, 0, MAXONLY, 0, 0);
      printf("\n");
    }
  } else
    printf("ERROR: Can't get temperature data!\n");
  free_the_label(&label);
}

void print_w83627ehf(const sensors_chip_name *name)
{
  char *label;
  int i, valid;

  for (i = 0; i < 5; i++) {
    double cur, min, div;
    if (!sensors_get_label_and_valid(*name, SENSORS_W83627EHF_FAN1+i,
        &label, &valid)
     && !sensors_get_feature(*name, SENSORS_W83627EHF_FAN1+i, &cur)
     && !sensors_get_feature(*name, SENSORS_W83627EHF_FAN1_MIN+i, &min)) {
      if (valid) {
        print_label(label,10);
        printf("%4.0f RPM  (min = %4.0f RPM", cur, min);
        if (!sensors_get_feature(*name, SENSORS_W83627EHF_FAN1_DIV+i, &div))
          printf(", div = %1.0f", div);
        printf(")\n");
      }
    } else if (i < 3)
      printf("ERROR: Can't get FAN%d data!\n", i + 1);
    free_the_label(&label);
  }

  for (i = 0; i < 3; i++) {
    double cur, over, hyst;
    if (!sensors_get_label_and_valid(*name, SENSORS_W83627EHF_TEMP1+i,
        &label, &valid)
     && !sensors_get_feature(*name, SENSORS_W83627EHF_TEMP1+i, &cur)
     && !sensors_get_feature(*name, SENSORS_W83627EHF_TEMP1_OVER+i, &over)
     && !sensors_get_feature(*name, SENSORS_W83627EHF_TEMP1_HYST+i, &hyst)) {
      if (valid) {
        print_label(label,10);
        print_temp_info(cur, over, hyst, HYST, i ? 1 : 0, i ? 1 : 0);
        printf("\n");
      }
    } else
      printf("ERROR: Can't get TEMP%d data!\n", i + 1);
    free_the_label(&label);
  }
}

void print_maxilife(const sensors_chip_name *name)
{
   char  *label = NULL;
   double cur, min, max;
   int    alarms,valid;

   if (!sensors_get_feature(*name, SENSORS_MAXI_CG_ALARMS, &cur)) 
      alarms = cur + 0.5;
   else {
      printf("ERROR: Can't get alarm data!\n");
      alarms = 0;
   }

   if (!sensors_get_label_and_valid(*name, SENSORS_MAXI_CG_TEMP1, &label,&valid) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_TEMP1, &cur) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_TEMP1_MAX, &max) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_TEMP1_HYST, &min)) {
      if (valid && (cur || max || min)) {
         print_label(label, 12);
	 print_temp_info( cur, max, min, HYST, 1, 0);
         printf("\n");
      }
   } else
      printf("ERROR: Can't get TEMP1 data!\n");
   free_the_label(&label);

   if (!sensors_get_label_and_valid(*name, SENSORS_MAXI_CG_TEMP2, &label,&valid) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_TEMP2, &cur) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_TEMP2_MAX, &max) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_TEMP2_HYST, &min)) {
      if (valid && (cur || max || min)) {
         print_label(label, 12);
	 print_temp_info( cur, max, min, HYST, 1, 0);
         printf(" %s\n", alarms&MAXI_ALARM_TEMP2 ? "ALARM" : "");
      }
   } else
      printf("ERROR: Can't get TEMP2 data!\n");
   free_the_label(&label);

   if (!sensors_get_label_and_valid(*name, SENSORS_MAXI_CG_TEMP3, &label,&valid) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_TEMP3, &cur) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_TEMP3_MAX, &max) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_TEMP3_HYST, &min)) {
      if (valid && (cur || max || min)) {
         print_label(label, 12);
	 print_temp_info( cur, max, min, HYST, 1, 0);
         printf("\n");
      }
   } else
      printf("ERROR: Can't get TEMP3 data!\n");
   free_the_label(&label);

   if (!sensors_get_label_and_valid(*name, SENSORS_MAXI_CG_TEMP4, &label,&valid) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_TEMP4, &cur) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_TEMP4_MAX, &max) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_TEMP4_HYST, &min)) {
      if (valid && (cur || max || min)) {
         print_label(label, 12);
	 print_temp_info( cur, max, min, HYST, 1, 0);
         printf(" %s\n", alarms&MAXI_ALARM_TEMP4 ? "ALARM" : "");
      }
   } else
      printf("ERROR: Can't get TEMP4 data!\n");
   free_the_label(&label);

   if (!sensors_get_label_and_valid(*name, SENSORS_MAXI_CG_TEMP5, &label,&valid) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_TEMP5, &cur) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_TEMP5_MAX, &max) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_TEMP5_HYST, &min)) {
      if (valid && (cur || max || min)) {
         print_label(label, 12);
	 print_temp_info( cur, max, min, HYST, 1, 0);
         printf(" %s\n", alarms&MAXI_ALARM_TEMP5 ? "ALARM" : "");
      }
   } else
      printf("ERROR: Can't get TEMP5 data!\n");
   free_the_label(&label);
   
   if (!sensors_get_label_and_valid(*name, SENSORS_MAXI_CG_FAN1, &label,&valid) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_FAN1, &cur) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_FAN1_MIN, &min) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_FAN1_DIV, &max)) {
      if (valid && (cur || min || max)) {
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

   if (!sensors_get_label_and_valid(*name, SENSORS_MAXI_CG_FAN2, &label,&valid) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_FAN2, &cur) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_FAN2_MIN, &min) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_FAN2_DIV, &max)) {
      if (valid && (cur || min || max)) {
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

   if (!sensors_get_label_and_valid(*name, SENSORS_MAXI_CG_FAN3, &label,&valid) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_FAN3, &cur) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_FAN3_MIN, &min) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_FAN3_DIV, &max)) {
      if (valid && (cur || min || max)) {
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

   if (!sensors_get_label_and_valid(*name, SENSORS_MAXI_CG_PLL, &label,&valid) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_PLL, &cur) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_PLL_MIN, &min) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_PLL_MAX, &max)) {
      if (valid && (cur || min || max)) {
         print_label(label, 12);
         printf("%4.2f MHz   (min = %4.2f MHz, max = %4.2f MHz) %s\n",
                cur, min, max, alarms&MAXI_ALARM_PLL ? "ALARM" : "");
      }
   } else
      printf("ERROR: Can't get PLL data!\n");
   free_the_label(&label);

   if (!sensors_get_label_and_valid(*name, SENSORS_MAXI_CG_VID1, &label,&valid) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_VID1, &cur) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_VID1_MIN, &min) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_VID1_MAX, &max)) {
      if (valid && (cur || min || max)) {
         print_label(label, 12);
         printf("%+6.2f V    (min = %+6.2f V, max = %+6.2f V)   %s\n",
                cur, min, max, alarms&MAXI_ALARM_VID1 ? "ALARM" : "");
      }
   } else
      printf("ERROR: Can't get VID1 data!\n");
   free_the_label(&label);

   if (!sensors_get_label_and_valid(*name, SENSORS_MAXI_CG_VID2, &label,&valid) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_VID2, &cur) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_VID2_MIN, &min) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_VID2_MAX, &max)) {
      if (valid && (cur || min || max)) {
         print_label(label, 12);
         printf("%+6.2f V    (min = %+6.2f V, max = %+6.2f V)   %s\n",
                cur, min, max, alarms&MAXI_ALARM_VID2 ? "ALARM" : "");
      }
   } else
      printf("ERROR: Can't get VID2 data!\n");
   free_the_label(&label);

   if (!sensors_get_label_and_valid(*name, SENSORS_MAXI_CG_VID3, &label,&valid) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_VID3, &cur) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_VID3_MIN, &min) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_VID3_MAX, &max)) {
      if (valid && (cur || min || max)) {
         print_label(label, 12);
         printf("%+6.2f V    (min = %+6.2f V, max = %+6.2f V)   %s\n",
                cur, min, max, alarms&MAXI_ALARM_VID3 ? "ALARM" : "");
      }
   } else
      printf("ERROR: Can't get VID3 data!\n");
   free_the_label(&label);

   if (!sensors_get_label_and_valid(*name, SENSORS_MAXI_CG_VID4, &label,&valid) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_VID4, &cur) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_VID4_MIN, &min) &&
       !sensors_get_feature(*name, SENSORS_MAXI_CG_VID4_MAX, &max)) {
      if (valid && (cur || min || max)) {
         print_label(label, 12);
         printf("%+6.2f V    (min = %+6.2f V, max = %+6.2f V)   %s\n",
                cur, min, max, alarms&MAXI_ALARM_VID4 ? "ALARM" : "");
      }
   } else
      printf("ERROR: Can't get VID4 data!\n");
   free_the_label(&label);
}

void print_ddcmon(const sensors_chip_name *name)
{
	char  *label = NULL;
	double a, b, c;
	int    valid, i;

   if (!sensors_get_label_and_valid(*name, SENSORS_DDCMON_MAN_ID, &label, &valid)
    && !sensors_get_feature(*name, SENSORS_DDCMON_MAN_ID, &a)) {
      if (valid) {
         i = (int) a;	
         print_label(label, 24);
         printf("%c%c%c\n",
          ((i >> 10) & 0x1f) + 'A' - 1, ((i >> 5) & 0x1f) + 'A' - 1,
          (i & 0x1f) + 'A' - 1);
      }
   } else
      printf("ERROR: data 1\n");
   free_the_label(&label);

   if (!sensors_get_label_and_valid(*name, SENSORS_DDCMON_PROD_ID, &label, &valid)
    && !sensors_get_feature(*name, SENSORS_DDCMON_PROD_ID, &a)) {
      if (valid) {
         i = (int) a;	
         print_label(label, 24);
         printf("0x%04X\n", i);
      }
   } else
      printf("ERROR: data 2\n");
   free_the_label(&label);

   if (!sensors_get_label_and_valid(*name, SENSORS_DDCMON_SERIAL, &label,&valid) &&
       !sensors_get_feature(*name, SENSORS_DDCMON_SERIAL, &a)) {
      if (valid) {
         print_label(label, 24);
         printf("%d\n", (int) a);
      }
   } else
      printf("ERROR: data 3\n");
   free_the_label(&label);

   if (!sensors_get_label_and_valid(*name, SENSORS_DDCMON_YEAR, &label, &valid)
    && !sensors_get_feature(*name, SENSORS_DDCMON_YEAR, &a)
    && !sensors_get_feature(*name, SENSORS_DDCMON_WEEK, &b)) {
      if (valid) {
         print_label(label, 24);
         printf("%d-W%d\n", (int) a, (int) b);
      }
   } else
      printf("ERROR: data 4\n");
   free_the_label(&label);

   if (!sensors_get_label_and_valid(*name, SENSORS_DDCMON_EDID_VER, &label, &valid)
    && !sensors_get_feature(*name, SENSORS_DDCMON_EDID_VER, &a)
    && !sensors_get_feature(*name, SENSORS_DDCMON_EDID_REV, &b)) {
      if (valid) {
         print_label(label, 24);
         printf("%d.%d\n", (int) a, (int) b);
      }
   } else
      printf("ERROR: data 5\n");
   free_the_label(&label);

   if (!sensors_get_label_and_valid(*name, SENSORS_DDCMON_VERSIZE, &label,&valid) &&
       !sensors_get_feature(*name, SENSORS_DDCMON_VERSIZE, &a) &&
       !sensors_get_feature(*name, SENSORS_DDCMON_HORSIZE, &b)) {
      if (valid) {
         print_label(label, 24);
         printf("%dx%d\n", (int) a, (int) b);
      }
   } else
      printf("ERROR: data 6\n");
   free_the_label(&label);

   if (!sensors_get_label_and_valid(*name, SENSORS_DDCMON_GAMMA, &label, &valid)
    && !sensors_get_feature(*name, SENSORS_DDCMON_GAMMA, &a)) {
      if (valid) {
         print_label(label, 24);
         printf("%.02f\n", a);
      }
   } else
      printf("ERROR: data 7\n");
   free_the_label(&label);

   if (!sensors_get_label_and_valid(*name, SENSORS_DDCMON_DPMS, &label, &valid)
    && !sensors_get_feature(*name, SENSORS_DDCMON_DPMS, &a)) {
      if (valid) {
         i = (int) a;
         print_label(label, 24);
         if (i & 0xe0) {
            printf("%s%s%s%s%s\n",
               i & 0x20 ? "Active Off" : "",
               (i & 0x40) && (i & 0x20) ? ", ": "",
               i & 0x40 ? "Suspend" : "",
               (i & 0x80) && (i & 0x60) ? ", ": "",
               i & 0x80 ? "Standby" : "");
         } else {
            printf("None supported\n");
         }
      }
   } else
      printf("ERROR: data 8\n");
   free_the_label(&label);

   if (!sensors_get_label_and_valid(*name, SENSORS_DDCMON_TIMINGS, &label,&valid) &&
       !sensors_get_feature(*name, SENSORS_DDCMON_TIMINGS, &a)) {
      if (valid) {
         i = (int) a;
         if (i & 0x03) { /* 720x400 */
            print_label(label, 24);
            printf("720x400 @ %s%s%s Hz\n",
               i & 0x01 ? "70" : "",
               (i & 0x02) && (i & 0x01) ? "/" : "",
               i & 0x02 ? "88" : "");
         }
         if (i & 0x3c) { /* 640x480 */
            print_label(label, 24);
            printf("640x480 @ %s%s%s%s%s%s%s Hz\n",
               i & 0x04 ? "60" : "",
               (i & 0x08) && (i & 0x04) ? "/" : "",
               i & 0x08 ? "67" : "",
               (i & 0x10) && (i & 0x0c) ? "/" : "",
               i & 0x10 ? "72" : "",
               (i & 0x20) && (i & 0x1c) ? "/" : "",
               i & 0x20 ? "75" : "");
         }
         i >>= 6;
         if (i & 0x0f) { /* 800x600 */
            print_label(label, 24);
            printf("800x600 @ %s%s%s%s%s%s%s Hz\n",
               i & 0x01 ? "56" : "",
               (i & 0x02) && (i & 0x01) ? "/" : "",
               i & 0x02 ? "60" : "",
               (i & 0x04) && (i & 0x03) ? "/" : "",
               i & 0x04 ? "72" : "",
               (i & 0x08) && (i & 0x07) ? "/" : "",
               i & 0x08 ? "75" : "");
         }
         if (i & 0x10) { /* 832x624 */
            print_label(label, 24);
            printf("832x624 @ 75 Hz\n");
         }
         i >>= 5;
         if (i & 0x0f) { /* 1024x768 */
            print_label(label, 24);
            printf("1024x768 @ %s%s%s%s%s%s%s Hz\n",
               i & 0x01 ? "87i" : "",
               (i & 0x02) && (i & 0x01) ? "/" : "",
               i & 0x02 ? "60" : "",
               (i & 0x04) && (i & 0x03) ? "/" : "",
               i & 0x04 ? "70" : "",
               (i & 0x08) && (i & 0x07) ? "/" : "",
               i & 0x08 ? "75" : "");
         }
         if (i & 0x100) { /* 1152x870 */
            print_label(label, 24);
            printf("1152x870 @ 75 Hz\n");
         }
         if (i & 0x10) { /* 1280x1024 */
            print_label(label, 24);
            printf("1280x1024 @ 75 Hz\n");
         }
      }
   } else
      printf("ERROR: data 9\n");
   free_the_label(&label);

   for(i = 0; i < 8; i++) {
      if (!sensors_get_label_and_valid(*name, SENSORS_DDCMON_TIMING1_HOR + i * 3, &label, &valid)
       && !sensors_get_feature(*name, SENSORS_DDCMON_TIMING1_HOR + i * 3, &a)
       && !sensors_get_feature(*name, SENSORS_DDCMON_TIMING1_VER + i * 3, &b)
       && !sensors_get_feature(*name, SENSORS_DDCMON_TIMING1_REF + i * 3, &c)) {
         if (valid && ((int) a) != 0) {
            print_label(label, 24);
            printf("%dx%d @ %d Hz\n", (int) a, (int) b, (int) c);
         }
      } else
         printf("ERROR: data 10-%d\n", i+1);
      free_the_label(&label);
   }
   

   if (!sensors_get_label_and_valid(*name, SENSORS_DDCMON_VERSYNCMIN, &label,&valid) &&
       !sensors_get_feature(*name, SENSORS_DDCMON_VERSYNCMIN, &a) &&
       !sensors_get_feature(*name, SENSORS_DDCMON_VERSYNCMAX, &b)) {
      if (valid && ((int) a) != 0) {
         print_label(label, 24);
         printf("%d-%d\n", (int) a, (int) b);
      }
   } else
      printf("ERROR: data 11\n");
   free_the_label(&label);

   if (!sensors_get_label_and_valid(*name, SENSORS_DDCMON_HORSYNCMIN, &label,&valid) &&
       !sensors_get_feature(*name, SENSORS_DDCMON_HORSYNCMIN, &a) &&
       !sensors_get_feature(*name, SENSORS_DDCMON_HORSYNCMAX, &b)) {
      if (valid && ((int) a) != 0) {
         print_label(label, 24);
         printf("%d-%d\n", (int) a, (int) b);
      }
   } else
      printf("ERROR: data 12\n");
   free_the_label(&label);

   if (!sensors_get_label_and_valid(*name, SENSORS_DDCMON_MAXCLOCK, &label, &valid)
    && !sensors_get_feature(*name, SENSORS_DDCMON_MAXCLOCK, &a)) {
      if (valid && ((int) a) != 0) {
         print_label(label, 24);
         printf("%d\n", (int) a);
      }
   } else
      printf("ERROR: data 13\n");
   free_the_label(&label);

   printf("Note that the ddcmon driver is deprecated and will be deleted soon.\n");
   printf("Please use the eeprom driver and the ddcmon and/or decode-edid.pl\n"
          "scripts instead.\n");
}

/*
 * (Khali, 2003-07-17) Almost entierly rewritten. Reindented for clarity,
 * simplified at some places, added support for EDID EEPROMs (well,
 * redirection more than support).
 * (Khali, 2003-08-09) Rewrote Sony Vaio EEPROMs detection, and move it
 * to the top. This should prevent such EEPROMs from being accidentally
 * detected as valid memory modules.
 * (Khali, 2004-02-23) Add support for Shuttle EEPROMs, which contain
 * MAC ethernet addresses.
 */
void print_eeprom(const sensors_chip_name *name)
{
	char *label = NULL;
	double a, b, c, d;
	int valid, i, type;

	/* handle Sony Vaio EEPROMs first */
	if (name->addr == 0x57) {
 		char buffer[33];

		/* first make sure it is a Sony Vaio EEPROM */
		if (!sensors_get_label_and_valid(*name, SENSORS_EEPROM_VAIO_NAME, &label, &valid)
		 && valid) {
			for (i = 0; i < 4; i++)
	            if (!sensors_get_feature(*name, SENSORS_EEPROM_VAIO_NAME+i, &a))
					buffer[i] = (char) a;
			if (strncmp(buffer, "PCG-", 4) == 0)
			{
				/* must be a real Sony Vaio EEPROM */
				memset(buffer + 4, '\0', 29);
				for (a = 1; i < 32 && a != 0; i++) /* stop at first zero */
	            	if (!sensors_get_feature(*name, SENSORS_EEPROM_VAIO_NAME+i, &a)
					 && a != 0)
						buffer[i] = (char) a;
				print_label(label, 24);
				printf("%s\n", buffer);
				free_the_label(&label);

				memset(buffer, '\0', i);
				if (!sensors_get_label_and_valid(*name, SENSORS_EEPROM_VAIO_SERIAL, &label, &valid)
				 && valid) {
					for (i = 0, a = 1; i < 32 && a != 0; i++) /* stop at first zero */
						if (!sensors_get_feature(*name, SENSORS_EEPROM_VAIO_SERIAL+i, &a)
						 && a != 0)
							buffer[i] = (char) a;
					print_label(label, 24);
					printf("%s\n", buffer);
				} else
					printf("ERROR: data Vaio 3\n");
				free_the_label(&label);

				return;
			}
		} else
			printf("ERROR: data Vaio 2\n");
		free_the_label(&label);
	}

	/* then Shuttle EEPROMs */
	if (name->addr == 0x53) {
 		unsigned char buffer[6];

		/* first make sure it is a Shuttle EEPROM */
		if (!sensors_get_label_and_valid(*name, SENSORS_EEPROM_SHUTTLE, &label, &valid)
		 && valid) {
			for (i = 0; i < 3; i++)
	            if (!sensors_get_feature(*name, SENSORS_EEPROM_SHUTTLE+i, &a))
					buffer[i] = (unsigned char) a;
			if (buffer[0] == 0x00
			 && buffer[1] == 0x30
			 && buffer[2] == 0x1b)
			{
				/* must be a real Shuttle EEPROM */
				for (i = 4; i < 6; i++)
	            	if (!sensors_get_feature(*name, SENSORS_EEPROM_SHUTTLE+i, &a))
						buffer[i] = (unsigned char) a;

				print_label(label, 24);
				printf("%02X:%02X:%02X:%02X:%02X:%02X\n", buffer[0],
					buffer[1], buffer[2], buffer[3], buffer[4], buffer[5]);
				free_the_label(&label);
				
				return;
			}
		} else
			printf("ERROR: data Shuttle\n");
		free_the_label(&label);
	}

	if (!sensors_get_label_and_valid(*name, SENSORS_EEPROM_TYPE, &label, &valid)
	 && !sensors_get_feature(*name, SENSORS_EEPROM_TYPE, &a)) {
		if (valid) {
			type = (int) a;
			switch (type) {
				case 1:
					print_label(label, 24);
					printf("DRDRAM RIMM\n");
					break;
				case 2:
					print_label(label, 24);
					printf("EDO\n");
					break;
				case 4:
					print_label(label, 24);
					printf("SDR SDRAM DIMM\n");
					break;
				case 7:
					print_label(label, 24);
					printf("DDR SDRAM DIMM\n");
					break;
				case 8:
					print_label(label, 24);
					printf("DDR2 SDRAM DIMM\n");
					break;
				case 17:
					print_label(label, 24);
					printf("RAMBUS RIMM\n");
					break;
				case 255: /* EDID EEPROM? */
					break;
				default:
					printf("Unknown EEPROM type (%d)\n", type);
					free_the_label(&label);
					return;
			}
		} else {
			free_the_label(&label);
			return;
		}
	} else {
		free_the_label(&label);
		printf("Memory type: Unavailable\n");
		return;
	}
	free_the_label(&label);

	if (type == 255) { /* EDID EEPROM */
		/* make sure it is an EDID EEPROM */
		if (!sensors_get_feature(*name, SENSORS_EEPROM_ROWADDR, &a)
		 && !sensors_get_feature(*name, SENSORS_EEPROM_COLADDR, &b)
		 && !sensors_get_feature(*name, SENSORS_EEPROM_NUMROWS, &c)
		 && !sensors_get_feature(*name, SENSORS_EEPROM_EDID_HEADER, &d)) {
			if (((int) a) != 255 || ((int) b) != 255 || ((int) c) != 255
			 || ((int) d) != 0)
				printf("Unknown EEPROM type (255).\n");
			else if (name->addr == 0x50)
				/* must be an EDID EEPROM */
				printf("Use the ddcmon and/or decode-edid.pl scripts.\n");
		} else
			printf("ERROR: data EDID\n");
		return;
	}

	/* regular memory chips */
	if (!sensors_get_label_and_valid(*name, SENSORS_EEPROM_ROWADDR, &label, &valid)
	 && !sensors_get_feature(*name, SENSORS_EEPROM_ROWADDR, &a)
	 && !sensors_get_feature(*name, SENSORS_EEPROM_COLADDR, &b)
	 && !sensors_get_feature(*name, SENSORS_EEPROM_NUMROWS, &c)
	 && !sensors_get_feature(*name, SENSORS_EEPROM_BANKS, &d)
	 && valid) {
	 	int k = 0; /* multiplier, 0 if invalid */
		print_label(label, 24);
		if (type == 17) { /* RAMBUS */
			i = (((int) a) & 0x0f) + (((int) a) >> 4) + (((int) c) & 0x07) - 13;
			k = 1;
		} else if (type == 1) { /* DRDRAM */
			i = (((int) b) & 0x0f) + (((int) b) >> 4) + (((int) c) & 0x07) - 13;
			k = 1;
		} else if (type == 8) { /* DDR2 */
			i = (((int) a) & 0x0f) + (((int) b) & 0x0f) - 17;
			k = ((((int) c) & 0x7) + 1) * ((int) d);
		} else { /* SDRAM */
			i = (((int) a) & 0x0f) + (((int) b) & 0x0f) - 17;
			if (((int) c) <= 8 && ((int) d) <= 8)
				k = ((int) c) * ((int) d);
		}
		if(i > 0 && i <= 12 && k > 0)
			printf("%d\n", (1 << i) * k);
		else
			printf("invalid (%d %d %d %d)\n",
				(int) a, (int) b, (int) c, (int) d);
	} else
		printf("ERROR: data 2\n");
	free_the_label(&label);
}

void print_it87(const sensors_chip_name *name)
{
  char *label = NULL;
  double cur, min, max, fdiv, sens;
  int alarms, valid;

  if (!sensors_get_feature(*name,SENSORS_IT87_ALARMS, &cur)) {
    alarms = cur + 0.5;
  }
  else {
    printf("ERROR: Can't get alarm data!\n");
    alarms = 0;
  }

  if (!sensors_get_label_and_valid(*name,SENSORS_IT87_IN0,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_IT87_IN0,&cur) &&
      !sensors_get_feature(*name,SENSORS_IT87_IN0_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_IT87_IN0_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&IT87_ALARM_IN0?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN0 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_IT87_IN1,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_IT87_IN1,&cur) &&
      !sensors_get_feature(*name,SENSORS_IT87_IN1_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_IT87_IN1_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&IT87_ALARM_IN1?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN1 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_IT87_IN2,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_IT87_IN2,&cur) &&
      !sensors_get_feature(*name,SENSORS_IT87_IN2_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_IT87_IN2_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&IT87_ALARM_IN2?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN2 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_IT87_IN3,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_IT87_IN3,&cur) &&
      !sensors_get_feature(*name,SENSORS_IT87_IN3_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_IT87_IN3_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&IT87_ALARM_IN3?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN3 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_IT87_IN4,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_IT87_IN4,&cur) &&
      !sensors_get_feature(*name,SENSORS_IT87_IN4_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_IT87_IN4_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&IT87_ALARM_IN4?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN4 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_IT87_IN5,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_IT87_IN5,&cur) &&
      !sensors_get_feature(*name,SENSORS_IT87_IN5_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_IT87_IN5_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&IT87_ALARM_IN5?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN5 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_IT87_IN6,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_IT87_IN6,&cur) &&
      !sensors_get_feature(*name,SENSORS_IT87_IN6_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_IT87_IN6_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&IT87_ALARM_IN6?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN6 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_IT87_IN7,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_IT87_IN7,&cur) &&
      !sensors_get_feature(*name,SENSORS_IT87_IN7_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_IT87_IN7_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&IT87_ALARM_IN7?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN7 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_IT87_IN8,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_IT87_IN8,&cur)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V\n", cur);
    }
  } else 
    printf("ERROR: Can't get IN8 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_IT87_FAN1,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_IT87_FAN1,&cur) &&
      !sensors_get_feature(*name,SENSORS_IT87_FAN1_DIV,&fdiv) &&
      !sensors_get_feature(*name,SENSORS_IT87_FAN1_MIN,&min)) {
    if (valid) {
      print_label(label,10);
      printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)          %s\n",
             cur,min,fdiv, alarms&IT87_ALARM_FAN1?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get FAN1 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_IT87_FAN2,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_IT87_FAN2,&cur) &&
      !sensors_get_feature(*name,SENSORS_IT87_FAN2_DIV,&fdiv) &&
      !sensors_get_feature(*name,SENSORS_IT87_FAN2_MIN,&min)) {
    if (valid) {
      print_label(label,10);
      printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)          %s\n",
             cur,min,fdiv, alarms&IT87_ALARM_FAN2?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get FAN2 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_IT87_FAN3,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_IT87_FAN3,&cur) &&
      !sensors_get_feature(*name,SENSORS_IT87_FAN3_DIV,&fdiv) &&
      !sensors_get_feature(*name,SENSORS_IT87_FAN3_MIN,&min)) {
    if (valid) {
      print_label(label,10);
      printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)          %s\n",
             cur,min,fdiv, alarms&IT87_ALARM_FAN3?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get FAN3 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_IT87_TEMP1,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_IT87_TEMP1,&cur) &&
      !sensors_get_feature(*name,SENSORS_IT87_TEMP1_LOW,&min) &&
      !sensors_get_feature(*name,SENSORS_IT87_SENS1,&sens) &&
      !sensors_get_feature(*name,SENSORS_IT87_TEMP1_HIGH,&max)) {
    if (valid) {
      print_label(label,10);
      print_temp_info( cur, max, min, MINMAX, 0, 0);
      printf(" sensor = %s  ", (((int)sens)==3) ? "diode" :
                               (((int)sens)==2) ? "thermistor" :
                               (((int)sens)==0) ? "disabled" : "invalid");
      printf( " %s\n", alarms & IT87_ALARM_TEMP1 ? "ALARM" : "" );
    }
  } else
    printf("ERROR: Can't get TEMP1 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_IT87_TEMP2,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_IT87_TEMP2,&cur) &&
      !sensors_get_feature(*name,SENSORS_IT87_TEMP2_LOW,&min) &&
      !sensors_get_feature(*name,SENSORS_IT87_SENS2,&sens) &&
      !sensors_get_feature(*name,SENSORS_IT87_TEMP2_HIGH,&max)) {
    if (valid) {
      print_label(label,10);
      print_temp_info( cur, max, min, MINMAX, 0, 0);
      printf(" sensor = %s  ", (((int)sens)==3) ? "diode" :
                             (((int)sens)==2) ? "thermistor" :
                             (((int)sens)==0) ? "disabled" : "invalid");
      printf( " %s\n", alarms & IT87_ALARM_TEMP2 ? "ALARM" : "" );
    }
  } else
    printf("ERROR: Can't get TEMP2 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_IT87_TEMP3,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_IT87_TEMP3,&cur) &&
      !sensors_get_feature(*name,SENSORS_IT87_TEMP3_LOW,&min) &&
      !sensors_get_feature(*name,SENSORS_IT87_SENS3,&sens) &&
      !sensors_get_feature(*name,SENSORS_IT87_TEMP3_HIGH,&max)) {
    if (valid) {
      print_label(label,10);
      print_temp_info( cur, max, min, MINMAX, 0, 0);
      printf(" sensor = %s  ", (((int)sens)==3) ? "diode" :
                               (((int)sens)==2) ? "thermistor" :
                               (((int)sens)==0) ? "disabled" : "invalid");
      printf( " %s\n", alarms & IT87_ALARM_TEMP3 ? "ALARM" : "" );
    }
  } else
    printf("ERROR: Can't get TEMP3 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_IT87_VID,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_IT87_VID,&cur)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V\n",cur);
    }
  }
  free_the_label(&label);
}

void print_fscpos(const sensors_chip_name *name)
{
  char *label = NULL;
  double voltage, temp, state, fan;
  int valid;

  if (!sensors_get_label_and_valid(*name,SENSORS_FSCPOS_TEMP1,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_FSCPOS_TEMP1,&temp) &&
      !sensors_get_feature(*name,SENSORS_FSCPOS_TEMP1_STATE,&state)) { 
    if (valid) {
      print_label(label,10);
	if((int) state & 0x01)
	  printf("\t%+6.2f C \n",temp);
	else
      	  printf("\tfailed\n");
    }
  } else
    printf("ERROR: Can't get TEMP1 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_FSCPOS_TEMP2,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_FSCPOS_TEMP2,&temp) &&
      !sensors_get_feature(*name,SENSORS_FSCPOS_TEMP2_STATE,&state)) { 
    if (valid) {
      print_label(label,10);
	if((int) state & 0x01)
	  printf("\t%+6.2f C \n",temp);
	else
	  printf("\tfailed\n");
    }
  } else
    printf("ERROR: Can't get TEMP2 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_FSCPOS_TEMP3,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_FSCPOS_TEMP3,&temp) &&
      !sensors_get_feature(*name,SENSORS_FSCPOS_TEMP3_STATE,&state)) { 
    if (valid) {
      print_label(label,10);
	if((int) state & 0x01)
	  printf("\t%+6.2f C \n",temp);
	else
	  printf("\tfailed\n");
    }
  } else
    printf("ERROR: Can't get TEMP3 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_FSCPOS_FAN1,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_FSCPOS_FAN1,&fan) &&
      !sensors_get_feature(*name,SENSORS_FSCPOS_FAN1_STATE,&state)) { 
    if (valid) {
      print_label(label,10);
	if((int) state & 0x04)
	  printf("\tfaulty\n");
	else
	  printf("\t%6.0f RPM \n",fan);
    }
  } else
    printf("ERROR: Can't get FAN1 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_FSCPOS_FAN2,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_FSCPOS_FAN2,&fan) &&
      !sensors_get_feature(*name,SENSORS_FSCPOS_FAN2_STATE,&state)) { 
    if (valid) {
      print_label(label,10);
	if((int) state & 0x04)
	  printf("\tfaulty\n");
	else
	  printf("\t%6.0f RPM \n",fan);
    }
  } else
    printf("ERROR: Can't get FAN2 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_FSCPOS_FAN3,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_FSCPOS_FAN3,&fan) &&
      !sensors_get_feature(*name,SENSORS_FSCPOS_FAN3_STATE,&state)) { 
    if (valid) {
      print_label(label,10);
	if((int) state & 0x04)
	  printf("\tfaulty\n");
	else
	  printf("\t%6.0f RPM \n",fan);
    }
  } else
    printf("ERROR: Can't get FAN3 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_FSCPOS_VOLTAGE1,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_FSCPOS_VOLTAGE1,&voltage)) {
    if (valid) {
      print_label(label,10);
      printf("\t%+6.2f V\n",voltage);
    }
  } else
    printf("ERROR: Can't get IN0 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_FSCPOS_VOLTAGE2,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_FSCPOS_VOLTAGE2,&voltage)) {
    if (valid) {
      print_label(label,10);
      printf("\t%+6.2f V\n",voltage);
    }
  } else
    printf("ERROR: Can't get IN1 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_FSCPOS_VOLTAGE3,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_FSCPOS_VOLTAGE3,&voltage)) {
    if (valid) {
      print_label(label,10);
      printf("\t%+6.2f V\n",voltage);
    }
  } else
    printf("ERROR: Can't get IN2 data!\n");
  free_the_label(&label);
}

void print_fscscy(const sensors_chip_name *name)
{
  char *label = NULL;
  double voltage, temp, tempmin, tempmax, templim, state, fan;
  int valid;

  if (!sensors_get_label_and_valid(*name,SENSORS_FSCSCY_TEMP1,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_FSCSCY_TEMP1,&temp) &&
      !sensors_get_feature(*name,SENSORS_FSCSCY_TEMP1_LIM,&templim) &&
      !sensors_get_feature(*name,SENSORS_FSCSCY_TEMP1_MIN,&tempmin) &&
      !sensors_get_feature(*name,SENSORS_FSCSCY_TEMP1_MAX,&tempmax) &&
      !sensors_get_feature(*name,SENSORS_FSCSCY_TEMP1_STATE,&state)) { 
    if (valid) {
      print_label(label,10);
	if((int) state & 0x01)
	  printf("\t%+6.2f C (Min = %+6.2f C, Max = %+6.2f C, Lim = %+6.2f C)\n",
		temp,tempmin,tempmax,templim);
	else
	  printf("\tfailed\n");
    }
  } else
    printf("ERROR: Can't get TEMP1 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_FSCSCY_TEMP2,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_FSCSCY_TEMP2,&temp) &&
      !sensors_get_feature(*name,SENSORS_FSCSCY_TEMP2_LIM,&templim) &&
      !sensors_get_feature(*name,SENSORS_FSCSCY_TEMP2_MIN,&tempmin) &&
      !sensors_get_feature(*name,SENSORS_FSCSCY_TEMP2_MAX,&tempmax) &&
      !sensors_get_feature(*name,SENSORS_FSCSCY_TEMP2_STATE,&state)) { 
    if (valid) {
      print_label(label,10);
	if((int) state & 0x01)
	  printf("\t%+6.2f C (Min = %+6.2f C, Max = %+6.2f C, Lim = %+6.2f C)\n",
		temp,tempmin,tempmax,templim);
	else
	  printf("\tfailed\n");
    }
  } else
    printf("ERROR: Can't get TEMP2 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_FSCSCY_TEMP3,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_FSCSCY_TEMP3,&temp) &&
      !sensors_get_feature(*name,SENSORS_FSCSCY_TEMP3_LIM,&templim) &&
      !sensors_get_feature(*name,SENSORS_FSCSCY_TEMP3_MIN,&tempmin) &&
      !sensors_get_feature(*name,SENSORS_FSCSCY_TEMP3_MAX,&tempmax) &&
      !sensors_get_feature(*name,SENSORS_FSCSCY_TEMP3_STATE,&state)) { 
    if (valid) {
      print_label(label,10);
	if((int) state & 0x01)
	  printf("\t%+6.2f C (Min = %+6.2f C, Max = %+6.2f C, Lim = %+6.2f C)\n",
		temp,tempmin,tempmax,templim);
	else
	  printf("\tfailed\n");
    }
  } else
    printf("ERROR: Can't get TEMP3 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_FSCSCY_TEMP4,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_FSCSCY_TEMP4,&temp) &&
      !sensors_get_feature(*name,SENSORS_FSCSCY_TEMP4_LIM,&templim) &&
      !sensors_get_feature(*name,SENSORS_FSCSCY_TEMP4_MIN,&tempmin) &&
      !sensors_get_feature(*name,SENSORS_FSCSCY_TEMP4_MAX,&tempmax) &&
      !sensors_get_feature(*name,SENSORS_FSCSCY_TEMP4_STATE,&state)) { 
    if (valid) {
      print_label(label,10);
	if((int) state & 0x01)
	  printf("\t%+6.2f C (Min = %+6.2f C, Max = %+6.2f C, Lim = %+6.2f C)\n",
		temp,tempmin,tempmax,templim);
	else
	  printf("\tfailed\n");
    }
  } else
    printf("ERROR: Can't get TEMP4 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_FSCSCY_FAN1,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_FSCSCY_FAN1,&fan) &&
      !sensors_get_feature(*name,SENSORS_FSCSCY_FAN1_STATE,&state)) { 
    if (valid) {
      print_label(label,10);
	if((int) state & 0x02)
	  printf("\tfaulty\n");
	else
	  printf("\t%6.0f RPM \n",fan);
    }
  } else
    printf("ERROR: Can't get FAN1 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_FSCSCY_FAN2,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_FSCSCY_FAN2,&fan) &&
      !sensors_get_feature(*name,SENSORS_FSCSCY_FAN2_STATE,&state)) { 
    if (valid) {
      print_label(label,10);
	if((int) state & 0x02)
	  printf("\tfaulty\n");
	else
	  printf("\t%6.0f RPM \n",fan);
    }
  } else
    printf("ERROR: Can't get FAN2 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_FSCSCY_FAN3,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_FSCSCY_FAN3,&fan) &&
      !sensors_get_feature(*name,SENSORS_FSCSCY_FAN3_STATE,&state)) { 
    if (valid) {
      print_label(label,10);
	if((int) state & 0x02)
	  printf("\tfaulty\n");
	else
	  printf("\t%6.0f RPM \n",fan);
    }
  } else
    printf("ERROR: Can't get FAN3 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_FSCSCY_FAN4,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_FSCSCY_FAN4,&fan) &&
      !sensors_get_feature(*name,SENSORS_FSCSCY_FAN4_STATE,&state)) { 
    if (valid) {
      print_label(label,10);
	if((int) state & 0x02)
	  printf("\tfaulty\n");
	else
	  printf("\t%6.0f RPM \n",fan);
    }
  } else
    printf("ERROR: Can't get FAN4 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_FSCSCY_FAN5,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_FSCSCY_FAN5,&fan) &&
      !sensors_get_feature(*name,SENSORS_FSCSCY_FAN5_STATE,&state)) { 
    if (valid) {
      print_label(label,10);
	if((int) state & 0x02)
	  printf("\tfaulty\n");
	else
	  printf("\t%6.0f RPM \n",fan);
    }
  } else
    printf("ERROR: Can't get FAN5 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_FSCSCY_FAN6,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_FSCSCY_FAN6,&fan) &&
      !sensors_get_feature(*name,SENSORS_FSCSCY_FAN6_STATE,&state)) { 
    if (valid) {
      print_label(label,10);
	if((int) state & 0x02)
	  printf("\tfaulty\n");
	else
	  printf("\t%6.0f RPM \n",fan);
    }
  } else
    printf("ERROR: Can't get FAN6 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_FSCSCY_VOLTAGE1,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_FSCSCY_VOLTAGE1,&voltage)) {
    if (valid) {
      print_label(label,10);
      printf("\t%+6.2f V\n",voltage);
    }
  } else
    printf("ERROR: Can't get IN0 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_FSCSCY_VOLTAGE2,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_FSCSCY_VOLTAGE2,&voltage)) {
    if (valid) {
      print_label(label,10);
      printf("\t%+6.2f V\n",voltage);
    }
  } else
    printf("ERROR: Can't get IN1 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_FSCSCY_VOLTAGE3,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_FSCSCY_VOLTAGE3,&voltage)) {
    if (valid) {
      print_label(label,10);
      printf("\t%+6.2f V\n",voltage);
    }
  } else
    printf("ERROR: Can't get IN2 data!\n");
  free_the_label(&label);
}

void print_fscher(const sensors_chip_name *name)
{
  char *label = NULL;
  double voltage, temp, state, fan;
  int valid;
  
  if (!sensors_get_label_and_valid(*name,SENSORS_FSCHER_TEMP1,&label,&valid)
      && !sensors_get_feature(*name,SENSORS_FSCHER_TEMP1,&temp)
      && !sensors_get_feature(*name,SENSORS_FSCHER_TEMP1_STATE,&state)) { 
    if (valid) {
      print_label(label,10);
      if((int) state & 0x01)
        printf("\t%+6.2f C \n",temp);
      else
        printf("\tfailed\n");
    }
  } else
    printf("ERROR: Can't get TEMP1 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_FSCHER_TEMP2,&label,&valid)
      && !sensors_get_feature(*name,SENSORS_FSCHER_TEMP2,&temp)
      && !sensors_get_feature(*name,SENSORS_FSCHER_TEMP2_STATE,&state)) { 
    if (valid) {
      print_label(label,10);
      if((int) state & 0x01)
        printf("\t%+6.2f C \n",temp);
      else
        printf("\tfailed\n");
    }
  } else
    printf("ERROR: Can't get TEMP2 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_FSCHER_TEMP3,&label,&valid)
      && !sensors_get_feature(*name,SENSORS_FSCHER_TEMP3,&temp)
      && !sensors_get_feature(*name,SENSORS_FSCHER_TEMP3_STATE,&state)) { 
    if (valid) {
      print_label(label,10);
      if((int) state & 0x01)
        printf("\t%+6.2f C \n",temp);
      else
        printf("\tfailed\n");
    }
  } else
    printf("ERROR: Can't get TEMP3 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_FSCHER_FAN1,&label,&valid)
      && !sensors_get_feature(*name,SENSORS_FSCHER_FAN1,&fan)
      && !sensors_get_feature(*name,SENSORS_FSCHER_FAN1_STATE,&state)) { 
    if (valid) {
      print_label(label,10);
      if((int) state & 0x04)
        printf("\tfaulty\n");
      else
        printf("\t%6.0f RPM \n",fan);
    }
  } else
    printf("ERROR: Can't get FAN1 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_FSCHER_FAN2,&label,&valid)
      && !sensors_get_feature(*name,SENSORS_FSCHER_FAN2,&fan)
      && !sensors_get_feature(*name,SENSORS_FSCHER_FAN2_STATE,&state)) { 
    if (valid) {
      print_label(label,10);
      if((int) state & 0x04)
        printf("\tfaulty\n");
      else
        printf("\t%6.0f RPM \n",fan);
    }
  } else
    printf("ERROR: Can't get FAN2 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_FSCHER_FAN3,&label,&valid)
      && !sensors_get_feature(*name,SENSORS_FSCHER_FAN3,&fan)
      && !sensors_get_feature(*name,SENSORS_FSCHER_FAN3_STATE,&state)) { 
    if (valid) {
      print_label(label,10);
      if((int) state & 0x04)
        printf("\tfaulty\n");
      else
        printf("\t%6.0f RPM \n",fan);
    }
  } else
    printf("ERROR: Can't get FAN3 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_FSCHER_VOLTAGE1,&label,&valid)
      && !sensors_get_feature(*name,SENSORS_FSCHER_VOLTAGE1,&voltage)) {
    if (valid) {
      print_label(label,10);
      printf("\t%+6.2f V\n",voltage);
    }
  } else
    printf("ERROR: Can't get IN0 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_FSCHER_VOLTAGE2,&label,&valid)
      && !sensors_get_feature(*name,SENSORS_FSCHER_VOLTAGE2,&voltage)) {
    if (valid) {
      print_label(label,10);
      printf("\t%+6.2f V\n",voltage);
    }
  } else
    printf("ERROR: Can't get IN1 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_FSCHER_VOLTAGE3,&label,&valid)
      && !sensors_get_feature(*name,SENSORS_FSCHER_VOLTAGE3,&voltage)) {
    if (valid) {
      print_label(label,10);
      printf("\t%+6.2f V\n",voltage);
    }
  } else
    printf("ERROR: Can't get IN2 data!\n");
  free_the_label(&label);
}

void print_pcf8591(const sensors_chip_name *name)
{
  char *label;
  int ain_conf = -1;
  double ain;
  double aout_enable, aout;
  int valid;

  if (!sensors_get_label_and_valid(*name,SENSORS_PCF8591_AIN_CONF,&label,&valid) &&
      !sensors_get_feature(*name, SENSORS_PCF8591_AIN_CONF, &ain)) {
        ain_conf = (int)ain;
        if (valid) {
          print_label(label,10);
          switch (ain_conf)
          {
            case 0: printf("four single ended inputs\n");
                    break;
            case 1: printf("three differential inputs\n");
                    break;
            case 2: printf("single ended and differential mixed\n");
                    break;
            case 3: printf("two differential inputs\n");
                    break;
          }
        }
      }
  /* display no error, 2.6 driver doesn't have that file */
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_PCF8591_CH0,&label,&valid) &&
      !sensors_get_feature(*name, SENSORS_PCF8591_CH0, &ain)) {
        if (valid) {
          print_label(label,10);
          printf("%6.2f V\n", ain);
        }
      }
  else printf("ERROR: Can't read ch0!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_PCF8591_CH1,&label,&valid) &&
      !sensors_get_feature(*name, SENSORS_PCF8591_CH1, &ain)) {
        if (valid) {
          print_label(label,10);
          printf("%6.2f V\n", ain);
        }
      }
  else printf("ERROR: Can't read ch1!\n");
  free_the_label(&label);

  if (ain_conf != 3) {
    if (!sensors_get_label_and_valid(*name,SENSORS_PCF8591_CH2,&label,&valid) &&
        !sensors_get_feature(*name, SENSORS_PCF8591_CH2, &ain)) {
          if (valid) {
            print_label(label,10);
            printf("%6.2f V\n", ain);
          }
        }
    else if (ain_conf >= 0) /* hide error for 2.6 kernel driver */
      printf("ERROR: Can't read ch2!\n");
    free_the_label(&label);
  }

  if (ain_conf <= 0) {
    if (!sensors_get_label_and_valid(*name,SENSORS_PCF8591_CH3,&label,&valid) &&
        !sensors_get_feature(*name, SENSORS_PCF8591_CH3, &ain)) {
          if (valid) {
            print_label(label,10);
            printf("%6.2f V\n", ain);
          }
        }
    else if (ain_conf >= 0) /* hide error for 2.6 kernel driver */
      printf("ERROR: Can't read ch3!\n");
    free_the_label(&label);
  }

  if (!sensors_get_label_and_valid(*name,SENSORS_PCF8591_AOUT,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_PCF8591_AOUT,&aout) &&
      !sensors_get_feature(*name,SENSORS_PCF8591_AOUT_ENABLE,&aout_enable)) {
        if (valid) {
          print_label(label,10);
          printf("%6.2f V (%s)\n", aout, aout_enable?"enabled":"disabled");
        }
      }
  else printf("ERROR: Can't read aout!\n");
  free_the_label(&label);

}

void print_vt1211(const sensors_chip_name *name)
{
  char *label = NULL;
  double cur,min,max,fdiv;
  int alarms,valid;
  int err;

  /* per vt1211.c driver, temps 1,5,6,7 and inputs 0,1,6 are optional */
  /* supress error messages reading from them */

  if (!sensors_get_feature(*name,SENSORS_VT1211_ALARMS,&cur)) 
    alarms = cur + 0.5;
  else {
    printf("ERROR: Can't get alarm data!\n");
    alarms = 0;
  }

  if (sensors_get_label_and_valid(*name,SENSORS_VT1211_IN0,&label,&valid)) {
    printf("ERROR: Can't get IN0 config!\n");
  } else if (valid) {
    if (!(err = sensors_get_feature(*name,SENSORS_VT1211_IN0,&cur)) &&
        !(err = sensors_get_feature(*name,SENSORS_VT1211_IN0_MIN,&min)) &&
        !(err = sensors_get_feature(*name,SENSORS_VT1211_IN0_MAX,&max))) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&VT1211_ALARM_IN0?"ALARM":"");
    } else if (err != -SENSORS_ERR_PROC)
      printf("ERROR: Can't get IN0 data!\n");
  }
  free_the_label(&label);
  if (sensors_get_label_and_valid(*name,SENSORS_VT1211_IN1,&label,&valid)) {
    printf("ERROR: Can't get IN1 config!\n");
  } else if (valid) {
    if(!(err = sensors_get_feature(*name,SENSORS_VT1211_IN1,&cur)) &&
       !(err = sensors_get_feature(*name,SENSORS_VT1211_IN1_MIN,&min)) &&
       !(err = sensors_get_feature(*name,SENSORS_VT1211_IN1_MAX,&max))) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&VT1211_ALARM_IN1?"ALARM":"");
    } else if (err != -SENSORS_ERR_PROC)
      printf("ERROR: Can't get IN1 data!\n");
  }
  free_the_label(&label);
  if (sensors_get_label_and_valid(*name,SENSORS_VT1211_IN2,&label,&valid)) {
    printf("ERROR: Can't get IN2 config!\n");
  } else if (valid) {
    if(!(err = sensors_get_feature(*name,SENSORS_VT1211_IN2,&cur)) &&
       !(err = sensors_get_feature(*name,SENSORS_VT1211_IN2_MIN,&min)) &&
       !(err = sensors_get_feature(*name,SENSORS_VT1211_IN2_MAX,&max))) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&VT1211_ALARM_IN2?"ALARM":"");
    } else
      printf("ERROR: Can't get IN2 data!\n");
  }
  free_the_label(&label);
  if (sensors_get_label_and_valid(*name,SENSORS_VT1211_IN3,&label,&valid)) {
    printf("ERROR: Can't get IN3 config!\n");
  } else if (valid) {
    if(!(err = sensors_get_feature(*name,SENSORS_VT1211_IN3,&cur)) &&
       !(err = sensors_get_feature(*name,SENSORS_VT1211_IN3_MIN,&min)) &&
       !(err = sensors_get_feature(*name,SENSORS_VT1211_IN3_MAX,&max))) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&VT1211_ALARM_IN3?"ALARM":"");
    } else
      printf("ERROR: Can't get IN3 data!\n");
  }
  free_the_label(&label);
  if (sensors_get_label_and_valid(*name,SENSORS_VT1211_IN4,&label,&valid)) {
    printf("ERROR: Can't get IN4 config!\n");
  } else if (valid) {
    if(!(err = sensors_get_feature(*name,SENSORS_VT1211_IN4,&cur)) &&
       !(err = sensors_get_feature(*name,SENSORS_VT1211_IN4_MIN,&min)) &&
       !(err = sensors_get_feature(*name,SENSORS_VT1211_IN4_MAX,&max))) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&VT1211_ALARM_IN4?"ALARM":"");
    } else
      printf("ERROR: Can't get IN4 data!\n");
  }
  free_the_label(&label);
  if (sensors_get_label_and_valid(*name,SENSORS_VT1211_IN5,&label,&valid)) {
    printf("ERROR: Can't get IN5 config!\n");
  } else if (valid) {
    if(!(err = sensors_get_feature(*name,SENSORS_VT1211_IN5,&cur)) &&
       !(err = sensors_get_feature(*name,SENSORS_VT1211_IN5_MIN,&min)) &&
       !(err = sensors_get_feature(*name,SENSORS_VT1211_IN5_MAX,&max))) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&VT1211_ALARM_IN5?"ALARM":"");
    } else
      printf("ERROR: Can't get IN5 data!\n");
  }
  free_the_label(&label);
  if (sensors_get_label_and_valid(*name,SENSORS_VT1211_FAN1,&label,&valid)) {
    printf("ERROR: Can't get FAN1 config!\n");
  } else if (valid) {
    if(!(err = sensors_get_feature(*name,SENSORS_VT1211_FAN1,&cur)) &&
       !(err = sensors_get_feature(*name,SENSORS_VT1211_FAN1_DIV,&fdiv)) &&
       !(err = sensors_get_feature(*name,SENSORS_VT1211_FAN1_MIN,&min))) {
      print_label(label,10);
      printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)          %s\n",
             cur,min,fdiv, alarms&VT1211_ALARM_FAN1?"ALARM":"");
    } else
      printf("ERROR: Can't get FAN1 data!\n");
  }
  free_the_label(&label);
  if (sensors_get_label_and_valid(*name,SENSORS_VT1211_FAN2,&label,&valid)) {
    printf("ERROR: Can't get FAN2 config!\n");
  } else if (valid) {
    if(!(err = sensors_get_feature(*name,SENSORS_VT1211_FAN2,&cur)) &&
       !(err = sensors_get_feature(*name,SENSORS_VT1211_FAN2_DIV,&fdiv)) &&
       !(err = sensors_get_feature(*name,SENSORS_VT1211_FAN2_MIN,&min))) {
      print_label(label,10);
      printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)          %s\n",
             cur,min,fdiv, alarms&VT1211_ALARM_FAN2?"ALARM":"");
    } else
      printf("ERROR: Can't get FAN2 data!\n");
  }
  free_the_label(&label);

  /* no temp 1 */
  if (sensors_get_label_and_valid(*name,SENSORS_VT1211_TEMP2,&label,&valid)) {
    printf("ERROR: Can't get TEMP2 config!\n");
  } else if (valid) {
    if(!(err = sensors_get_feature(*name,SENSORS_VT1211_TEMP2,&cur)) &&
       !(err = sensors_get_feature(*name,SENSORS_VT1211_TEMP2_HYST,&min)) &&
       !(err = sensors_get_feature(*name,SENSORS_VT1211_TEMP2_OVER,&max))) {
      print_label(label,10);
      print_temp_info( cur, max, min, HYST, 1, 0);
      printf(" %s\n", alarms & VT1211_ALARM_TEMP2 ? "ALARM" : "" );
    } else
      printf("ERROR: Can't get TEMP2 data!\n");
  }
  free_the_label(&label);
  if (sensors_get_label_and_valid(*name,SENSORS_VT1211_TEMP3,&label,&valid)) {
    printf("ERROR: Can't get TEMP3 config!\n");
  } else if (valid) {
    if (!(err = sensors_get_feature(*name,SENSORS_VT1211_TEMP3,&cur)) &&
	!(err = sensors_get_feature(*name,SENSORS_VT1211_TEMP3_HYST,&min)) &&
	!(err = sensors_get_feature(*name,SENSORS_VT1211_TEMP3_OVER,&max))) {
      print_label(label,10);
      print_temp_info( cur, max, min, HYST, 1, 0);
      printf(" %s\n", alarms & VT1211_ALARM_TEMP3 ? "ALARM" : "" );
    } else
      printf("ERROR: Can't get TEMP3 data!\n");
  }
  free_the_label(&label);
  if (sensors_get_label_and_valid(*name,SENSORS_VT1211_TEMP4,&label,&valid)) {
    printf("ERROR: Can't get TEMP4 config!\n");
  } else if (valid) {
    if(!(err = sensors_get_feature(*name,SENSORS_VT1211_TEMP4,&cur)) &&
       !(err = sensors_get_feature(*name,SENSORS_VT1211_TEMP4_HYST,&min)) &&
       !(err = sensors_get_feature(*name,SENSORS_VT1211_TEMP4_OVER,&max))) {
      print_label(label,10);
      print_temp_info( cur, max, min, HYST, 1, 0);
      printf(" %s\n", alarms & VT1211_ALARM_TEMP4 ? "ALARM" : "" );
    } else
      printf("ERROR: Can't get TEMP4 data!\n");
  }
  free_the_label(&label);
  if (sensors_get_label_and_valid(*name,SENSORS_VT1211_TEMP5,&label,&valid)) {
    printf("ERROR: Can't get TEMP5 config!\n");
  } else if (valid) {
    if(!(err = sensors_get_feature(*name,SENSORS_VT1211_TEMP5,&cur)) &&
       !(err = sensors_get_feature(*name,SENSORS_VT1211_TEMP5_HYST,&min)) &&
       !(err = sensors_get_feature(*name,SENSORS_VT1211_TEMP5_OVER,&max))) {
      print_label(label,10);
      print_temp_info( cur, max, min, HYST, 1, 0);
      printf(" %s\n", alarms & VT1211_ALARM_TEMP5 ? "ALARM" : "" );
    } else if (err != -SENSORS_ERR_PROC)
      printf("ERROR: Can't get TEMP5 data!\n");
  }
  free_the_label(&label);
  if (sensors_get_label_and_valid(*name,SENSORS_VT1211_TEMP6,&label,&valid)) {
    printf("ERROR: Can't get TEMP6 config!\n");
  } else if (valid) {
    if(!(err = sensors_get_feature(*name,SENSORS_VT1211_TEMP6,&cur)) &&
       !(err = sensors_get_feature(*name,SENSORS_VT1211_TEMP6_HYST,&min)) &&
       !(err = sensors_get_feature(*name,SENSORS_VT1211_TEMP6_OVER,&max))) {
      print_label(label,10);
      print_temp_info( cur, max, min, HYST, 1, 0);
      printf(" %s\n", alarms & VT1211_ALARM_TEMP6 ? "ALARM" : "" );
    } else if (err != -SENSORS_ERR_PROC)
      printf("ERROR: Can't get TEMP6 data!\n");
  }
  free_the_label(&label);
  if (sensors_get_label_and_valid(*name,SENSORS_VT1211_TEMP7,&label,&valid)) {
    printf("ERROR: Can't get TEMP7 config!\n");
  } else if (valid) {
    if(!(err = sensors_get_feature(*name,SENSORS_VT1211_TEMP7,&cur)) &&
       !(err = sensors_get_feature(*name,SENSORS_VT1211_TEMP7_HYST,&min)) &&
       !(err = sensors_get_feature(*name,SENSORS_VT1211_TEMP7_OVER,&max))) {
      print_label(label,10);
      print_temp_info( cur, max, min, HYST, 1, 0);
      printf(" %s\n", alarms & VT1211_ALARM_TEMP7 ? "ALARM" : "" );
    } else if (err != -SENSORS_ERR_PROC)
      printf("ERROR: Can't get TEMP7 data!\n");
  }
  free_the_label(&label);

  print_vid_info(name, SENSORS_VT1211_VID, SENSORS_VT1211_VRM);
}

void print_smsc47m1(const sensors_chip_name *name)
{
  char *label = NULL;
  double cur, min, div;
  int alarms, valid, i;

  if (!sensors_get_feature(*name,SENSORS_SMSC47M1_ALARMS,&cur)) 
    alarms = cur + 0.5;
  else {
    printf("ERROR: Can't get alarm data!\n");
    alarms = 0;
  }

  for (i=0; i<2; i++) { /* 2 fans */
    if (!sensors_get_label_and_valid(*name, SENSORS_SMSC47M1_FAN1+i, &label, &valid)
     && !sensors_get_feature(*name, SENSORS_SMSC47M1_FAN1+i, &cur)
     && !sensors_get_feature(*name, SENSORS_SMSC47M1_FAN1_DIV+i, &div)
     && !sensors_get_feature(*name, SENSORS_SMSC47M1_FAN1_MIN+i, &min)) {
      if (valid) {
        print_label(label, 10);
        printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)          %s\n",
               cur, min, div, alarms&(SMSC47M1_ALARM_FAN1<<i) ?
               "ALARM" : "");
      }
    }
    /* In Linux 2.6, the original chip configuration is respected, so channels
       may be missing without it being considered an error; thus we don't
       print any error if this happens. */
    free_the_label(&label);
  }
}

void print_pc87360(const sensors_chip_name *name)
{
  char *label = NULL;
  double cur, min, fdiv, tmp;
  int status, valid;

  if (!sensors_get_label_and_valid(*name, SENSORS_PC87360_FAN1, &label, &valid)
   && !sensors_get_feature(*name, SENSORS_PC87360_FAN1, &cur)
   && !sensors_get_feature(*name, SENSORS_PC87360_FAN1_DIV, &fdiv)
   && !sensors_get_feature(*name, SENSORS_PC87360_FAN1_MIN, &min)
   && !sensors_get_feature(*name, SENSORS_PC87360_FAN1_STATUS, &tmp)) {
    if (valid) {
      print_label(label, 10);
      status = tmp + 0.5;
      printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)          %s\n",
             cur, min, fdiv,
             status&PC87360_STATUS_FAN_OVERFLOW?"OVERFLOW":
             status&PC87360_STATUS_FAN_LOW?"ALARM":"");
    }
  }
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name, SENSORS_PC87360_FAN2, &label, &valid)
   && !sensors_get_feature(*name, SENSORS_PC87360_FAN2, &cur)
   && !sensors_get_feature(*name, SENSORS_PC87360_FAN2_DIV, &fdiv)
   && !sensors_get_feature(*name, SENSORS_PC87360_FAN2_MIN, &min)
   && !sensors_get_feature(*name, SENSORS_PC87360_FAN2_STATUS, &tmp)) {
    if (valid) {
      print_label(label, 10);
      status = tmp + 0.5;
      printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)          %s\n",
             cur, min, fdiv,
             status&PC87360_STATUS_FAN_OVERFLOW?"OVERFLOW":
             status&PC87360_STATUS_FAN_LOW?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get FAN2 data!\n");
  free_the_label(&label);
}

void print_pc87364(const sensors_chip_name *name)
{
  char *label = NULL;
  double cur, min, fdiv, tmp;
  int status, valid;

  print_pc87360(name);

  if (!sensors_get_label_and_valid(*name, SENSORS_PC87360_FAN3, &label, &valid)
   && !sensors_get_feature(*name, SENSORS_PC87360_FAN3, &cur)
   && !sensors_get_feature(*name, SENSORS_PC87360_FAN3_DIV, &fdiv)
   && !sensors_get_feature(*name, SENSORS_PC87360_FAN3_MIN, &min)
   && !sensors_get_feature(*name, SENSORS_PC87360_FAN3_STATUS, &tmp)) {
    if (valid) {
      print_label(label, 10);
      status = tmp + 0.5;
      printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)          %s\n",
             cur, min, fdiv,
             status&PC87360_STATUS_FAN_OVERFLOW?"OVERFLOW":
             status&PC87360_STATUS_FAN_LOW?"ALARM":"");
    }
  }
  free_the_label(&label);
}

void print_pc87366(const sensors_chip_name *name)
{
  char *label = NULL;
  double cur, min, max;
  int status, valid, i, tempnr = 2;

  for (i = 0; i < 11; i++) {
    if (!sensors_get_feature(*name, SENSORS_PC87360_IN0_STATUS + i, &cur))
      status = cur + 0.5;
    else {
      printf("ERROR: Can't get IN%d status data!\n", i);
      status = 0;
    }

    if (!sensors_get_label_and_valid(*name, SENSORS_PC87360_IN0 + i, &label, &valid)
     && !sensors_get_feature(*name, SENSORS_PC87360_IN0 + i, &cur)
     && !sensors_get_feature(*name, SENSORS_PC87360_IN0_MIN + i, &min)
     && !sensors_get_feature(*name, SENSORS_PC87360_IN0_MAX + i, &max)) {
      if (valid) {
        print_label(label, 10);
        printf("%+6.2f V  (min = %+6.2f V, max = %6.2f V)       %s\n",
               cur, min, max,
               status&(PC87365_STATUS_IN_MIN
                      |PC87365_STATUS_IN_MAX)?"ALARM":"");
      }
    } else
      printf("ERROR: Can't get IN%d data!\n", i);
    free_the_label(&label);
  }

  print_pc87364(name);

  if (!strcmp(name->prefix, "pc87366"))
    tempnr = 6;

  for (i = 0; i < tempnr; i++) {
    if (!sensors_get_feature(*name, SENSORS_PC87360_TEMP1_STATUS + i, &cur))
      status = cur + 0.5;
    else {
      printf("ERROR: Can't get TEMP%d status data!\n", i + 1);
      status = 0;
    }

    if (!sensors_get_label_and_valid(*name, SENSORS_PC87360_TEMP1 + i, &label, &valid)
     && !sensors_get_feature(*name, SENSORS_PC87360_TEMP1 + i, &cur)
     && !sensors_get_feature(*name, SENSORS_PC87360_TEMP1_MIN + i, &min)
     && !sensors_get_feature(*name, SENSORS_PC87360_TEMP1_MAX + i, &max)) {
      if (valid) {
        print_label(label, 10);
        print_temp_info(cur, max, min, MINMAX, 0, 0);
        if (i<3 && status&PC87365_STATUS_TEMP_OPEN)
          printf(" OPEN");
        else if (status&(PC87365_STATUS_TEMP_MIN|PC87365_STATUS_TEMP_MAX))
          printf(" ALARM");
        printf("\n");
      }
    } else
      printf("ERROR: Can't get TEMP%d data!\n", i + 1);
    free_the_label(&label);

    if (!sensors_get_label_and_valid(*name, SENSORS_PC87360_TEMP1_CRIT + i, &label, &valid)
     && !sensors_get_feature(*name, SENSORS_PC87360_TEMP1_CRIT + i, &cur)) {
      if (valid) {
        print_label(label, 10);
        print_temp_info(cur, 0, 0, SINGLE, 0, 0);
        if (status&PC87365_STATUS_TEMP_CRIT)
          printf(" ALARM");
        printf("\n");
      }
    } else
      printf("ERROR: Can't get TEMP%d overtemperature data!\n", i + 1);
    free_the_label(&label);
  }
  
  print_vid_info(name, SENSORS_PC87360_VID, SENSORS_PC87360_VRM);
}

static void lm92_print_temp (float n_cur,float n_high,float n_low,float n_crit,float n_hyst)
{
	if (fahrenheit) {
		n_cur = deg_ctof (n_cur);
		n_high = deg_ctof (n_high);
		n_low = deg_ctof (n_low);
		n_crit = deg_ctof (n_crit);
		n_hyst = deg_ctof (n_hyst);
	}

	printf ("%+6.4f%s (high = %+6.4f%s, low = %+6.4f%s, crit = %+6.4f%s, hyst = %+6.4f%s)",
			n_cur, degstr,
			n_high, degstr,
			n_low, degstr,
			n_crit, degstr,
			n_hyst, degstr);
}

void print_lm92 (const sensors_chip_name *name)
{
	char *label = NULL;
	double temp[5];
	int valid,alarms;

	if (!sensors_get_feature (*name,SENSORS_LM92_ALARMS,temp)) {
		alarms = *temp + 0.5;
	} else {
		printf ("ERROR: Can't get alarm data!\n");
		return;
	}

	if (!sensors_get_label_and_valid (*name,SENSORS_LM92_TEMP,&label,&valid) &&
		!sensors_get_feature (*name,SENSORS_LM92_TEMP,temp) &&
		!sensors_get_feature (*name,SENSORS_LM92_TEMP_HIGH,temp + 1) &&
		!sensors_get_feature (*name,SENSORS_LM92_TEMP_LOW,temp + 2) &&
		!sensors_get_feature (*name,SENSORS_LM92_TEMP_CRIT,temp + 3) &&
		!sensors_get_feature (*name,SENSORS_LM92_TEMP_HYST,temp + 4)) {
		if (valid) {
			print_label (label,10);
			lm92_print_temp (temp[0],temp[1],temp[2],temp[3],temp[4]);
			if (alarms) {
				printf (" ALARMS (");

				if ((alarms & LM92_ALARM_TEMP_HIGH))
					printf ("HIGH");

				if (alarms & LM92_ALARM_TEMP_LOW)
					printf ("%sLOW",(alarms & LM92_ALARM_TEMP_HIGH) ? "," : "");

				if (alarms & LM92_ALARM_TEMP_CRIT)
					printf ("%sCRIT",(alarms & (LM92_ALARM_TEMP_HIGH | LM92_ALARM_TEMP_LOW)) ? "," : "");

				printf (")");
			}
			printf ("\n");
		}
	} else printf ("ERROR: Can't get temperature data!\n");

	free_the_label (&label);
}

void print_vt8231(const sensors_chip_name *name)
{
  char *label = NULL;
  double cur,min,max,fdiv;
  int alarms,valid;

  if (!sensors_get_feature(*name,SENSORS_VT8231_ALARMS,&cur)) 
    alarms = cur + 0.5;
  else {
    printf("ERROR: Can't get alarm data!\n");
    alarms = 0;
  }

  if (!sensors_get_label_and_valid(*name,SENSORS_VT8231_IN0,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_VT8231_IN0,&cur) &&
      !sensors_get_feature(*name,SENSORS_VT8231_IN0_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_VT8231_IN0_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&VT8231_ALARM_IN0?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN0 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_VT8231_IN1,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_VT8231_IN1,&cur) &&
      !sensors_get_feature(*name,SENSORS_VT8231_IN1_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_VT8231_IN1_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&VT8231_ALARM_IN1?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN1 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_VT8231_IN2,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_VT8231_IN2,&cur) &&
      !sensors_get_feature(*name,SENSORS_VT8231_IN2_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_VT8231_IN2_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&VT8231_ALARM_IN2?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN2 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_VT8231_IN3,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_VT8231_IN3,&cur) &&
      !sensors_get_feature(*name,SENSORS_VT8231_IN3_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_VT8231_IN3_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&VT8231_ALARM_IN3?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN3 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_VT8231_IN4,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_VT8231_IN4,&cur) &&
      !sensors_get_feature(*name,SENSORS_VT8231_IN4_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_VT8231_IN4_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&VT8231_ALARM_IN4?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN4 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_VT8231_IN5,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_VT8231_IN5,&cur) &&
      !sensors_get_feature(*name,SENSORS_VT8231_IN5_MIN,&min) &&
      !sensors_get_feature(*name,SENSORS_VT8231_IN5_MAX,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
             cur,min,max,alarms&VT8231_ALARM_IN5?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get IN5 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_VT8231_FAN1,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_VT8231_FAN1,&cur) &&
      !sensors_get_feature(*name,SENSORS_VT8231_FAN1_DIV,&fdiv) &&
      !sensors_get_feature(*name,SENSORS_VT8231_FAN1_MIN,&min)) {
    if (valid) {
      print_label(label,10);
      printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)          %s\n",
             cur,min,fdiv, alarms&VT8231_ALARM_FAN1?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get FAN1 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_VT8231_FAN2,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_VT8231_FAN2,&cur) &&
      !sensors_get_feature(*name,SENSORS_VT8231_FAN2_DIV,&fdiv) &&
      !sensors_get_feature(*name,SENSORS_VT8231_FAN2_MIN,&min)) {
    if (valid) {
    print_label(label,10);
    printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)          %s\n",
           cur,min,fdiv, alarms&VT8231_ALARM_FAN2?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get FAN2 data!\n");
  free_the_label(&label);

  /* no temp 1 */
  if (!sensors_get_label_and_valid(*name,SENSORS_VT8231_TEMP2,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_VT8231_TEMP2,&cur) &&
      !sensors_get_feature(*name,SENSORS_VT8231_TEMP2_HYST,&min) &&
      !sensors_get_feature(*name,SENSORS_VT8231_TEMP2_OVER,&max)) {
    if (valid) {
      print_label(label,10);
      print_temp_info( cur, max, min, HYST, 1, 0);
      printf(" %s\n", alarms & VT8231_ALARM_TEMP2 ? "ALARM" : "" );
    }
  } else
    printf("ERROR: Can't get TEMP2 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_VT8231_TEMP3,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_VT8231_TEMP3,&cur) &&
      !sensors_get_feature(*name,SENSORS_VT8231_TEMP3_HYST,&min) &&
      !sensors_get_feature(*name,SENSORS_VT8231_TEMP3_OVER,&max)) {
    if (valid) {
      print_label(label,10);
      print_temp_info( cur, max, min, HYST, 1, 0);
      printf(" %s\n", alarms & VT8231_ALARM_TEMP3 ? "ALARM" : "" );
    }
  } else
    printf("ERROR: Can't get TEMP3 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_VT8231_TEMP4,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_VT8231_TEMP4,&cur) &&
      !sensors_get_feature(*name,SENSORS_VT8231_TEMP4_HYST,&min) &&
      !sensors_get_feature(*name,SENSORS_VT8231_TEMP4_OVER,&max)) {
    if (valid) {
      print_label(label,10);
      print_temp_info( cur, max, min, HYST, 1, 0);
      printf(" %s\n", alarms & VT8231_ALARM_TEMP4 ? "ALARM" : "" );
    }
  } else
    printf("ERROR: Can't get TEMP4 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_VT8231_TEMP5,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_VT8231_TEMP5,&cur) &&
      !sensors_get_feature(*name,SENSORS_VT8231_TEMP5_HYST,&min) &&
      !sensors_get_feature(*name,SENSORS_VT8231_TEMP5_OVER,&max)) {
    if (valid) {
      print_label(label,10);
      print_temp_info( cur, max, min, HYST, 1, 0);
      printf(" %s\n", alarms & VT8231_ALARM_TEMP5 ? "ALARM" : "" );
    }
  } else
    printf("ERROR: Can't get TEMP5 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_VT8231_TEMP6,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_VT8231_TEMP6,&cur) &&
      !sensors_get_feature(*name,SENSORS_VT8231_TEMP6_HYST,&min) &&
      !sensors_get_feature(*name,SENSORS_VT8231_TEMP6_OVER,&max)) {
    if (valid) {
      print_label(label,10);
      print_temp_info( cur, max, min, HYST, 1, 0);
      printf(" %s\n", alarms & VT8231_ALARM_TEMP6 ? "ALARM" : "" );
    }
  } else
    printf("ERROR: Can't get TEMP6 data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name,SENSORS_VT8231_TEMP7,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_VT8231_TEMP7,&cur) &&
      !sensors_get_feature(*name,SENSORS_VT8231_TEMP7_HYST,&min) &&
      !sensors_get_feature(*name,SENSORS_VT8231_TEMP7_OVER,&max)) {
    if (valid) {
      print_label(label,10);
      print_temp_info( cur, max, min, HYST, 1, 0);
      printf(" %s\n", alarms & VT8231_ALARM_TEMP7 ? "ALARM" : "" );
    }
  } else
    printf("ERROR: Can't get TEMP7 data!\n");
  free_the_label(&label);

  print_vid_info(name, SENSORS_VT8231_VID, SENSORS_VT8231_VRM);
}

#define BMC_MAX_INS 10
#define BMC_MAX_FANS 10
#define BMC_MAX_TEMPS 10

void print_bmc(const sensors_chip_name *name)
{
  char *label = NULL;
  double cur,min,max;
  int alarms, valid, i;

/*
  if (!sensors_get_feature(*name,SENSORS_VT8231_ALARMS,&cur)) 
    alarms = cur + 0.5;
  else {
    printf("ERROR: Can't get alarm data!\n");
    alarms = 0;
  }
*/
#define BMC_ALARM_IN1 0
#define BMC_ALARM_FAN1 0
#define BMC_ALARM_TEMP1 0

    alarms = 0;
  for(i = 0; i < BMC_MAX_INS; i++) {
	  if (!sensors_get_label_and_valid(*name,SENSORS_BMC_IN1+i,&label,&valid) &&
	      !sensors_get_feature(*name,SENSORS_BMC_IN1+i,&cur) &&
	      !sensors_get_feature(*name,SENSORS_BMC_IN1_MIN+i,&min) &&
	      !sensors_get_feature(*name,SENSORS_BMC_IN1_MAX+i,&max)) {
	    if (valid) {
	      print_label(label,10);
	      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
	             cur,min,max,alarms&BMC_ALARM_IN1?"ALARM":"");
	    }
	  }
	  free_the_label(&label);
  }

  for(i = 0; i < BMC_MAX_FANS; i++) {
	  if (!sensors_get_label_and_valid(*name,SENSORS_BMC_FAN1+i,&label,&valid) &&
	      !sensors_get_feature(*name,SENSORS_BMC_FAN1+i,&cur) &&
	      !sensors_get_feature(*name,SENSORS_BMC_FAN1_MIN+i,&min)) {
	    if (valid) {
	      print_label(label,10);
	      printf("%4.0f RPM  (min = %4.0f RPM)                  %s\n",
	             cur,min,alarms&BMC_ALARM_FAN1?"ALARM":"");
	    }
	  }
	  free_the_label(&label);
  }

  for(i = 0; i < BMC_MAX_TEMPS; i++) {
	  if (!sensors_get_label_and_valid(*name,SENSORS_BMC_TEMP1+i,&label,&valid) &&
	      !sensors_get_feature(*name,SENSORS_BMC_TEMP1+i,&cur) &&
	      !sensors_get_feature(*name,SENSORS_BMC_TEMP1_MIN+i,&min) &&
	      !sensors_get_feature(*name,SENSORS_BMC_TEMP1_MAX+i,&max)) {
	    if (valid) {
	      print_label(label,10);
	      print_temp_info( cur, max, min, HYST, 1, 0);
	      printf(" %s\n", alarms & BMC_ALARM_TEMP1 ? "ALARM" : "" );
	    }
	  }
	  free_the_label(&label);
  }	
}

static long adm1026_alarms_in[] = {
  ADM1026_ALARM_IN0,  ADM1026_ALARM_IN1,  ADM1026_ALARM_IN2,
  ADM1026_ALARM_IN3,  ADM1026_ALARM_IN4,  ADM1026_ALARM_IN5,
  ADM1026_ALARM_IN6,  ADM1026_ALARM_IN7,  ADM1026_ALARM_IN8,
  ADM1026_ALARM_IN9,  ADM1026_ALARM_IN10, ADM1026_ALARM_IN11,
  ADM1026_ALARM_IN12, ADM1026_ALARM_IN13, ADM1026_ALARM_IN14,
  ADM1026_ALARM_IN15, ADM1026_ALARM_IN16
};
static long adm1026_alarms_temp[] = {
  ADM1026_ALARM_TEMP1,  ADM1026_ALARM_TEMP2,  ADM1026_ALARM_TEMP3
};

void print_adm1026(const sensors_chip_name *name)
{
  char *label = NULL;
  double cur,min,max;
  long alarms;
  int valid, i;

  if (!sensors_get_feature(*name,SENSORS_ADM1026_ALARMS,&cur)) {
    alarms = cur + 0.5;
  } else {
    printf("ERROR: Can't get alarm data!\n");
    alarms = 0;
  }

  /* Seventeen voltage sensors */
  for (i = 0; i <= 16 ; ++i) {
    int  feat_base = SENSORS_ADM1026_IN0 + (3 * i);
    int  feat_max = feat_base +1, feat_min = feat_base +2;
    if (!sensors_get_label_and_valid(*name,feat_base,&label,&valid) &&
        !sensors_get_feature(*name,feat_base,&cur) &&
        !sensors_get_feature(*name,feat_min,&min) &&
        !sensors_get_feature(*name,feat_max,&max)) {
      if (valid) {
        print_label(label,10);
        printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)   %s\n",
               cur,min,max,(alarms&adm1026_alarms_in[i])?"ALARM":"");
      }
    } else {
      printf("ERROR: Can't get IN%d data!\n",i);
    }
    free_the_label(&label);
  };

  /* Eight fan sensors */
  for (i = 0; i <= 7 ; ++i) {
    int  feat_base = SENSORS_ADM1026_FAN0 + (3 * i);
    int  feat_div = feat_base +1, feat_min = feat_base +2;
    if (!sensors_get_label_and_valid(*name,feat_base,&label,&valid) &&
        !sensors_get_feature(*name,feat_base,&cur) &&
        !sensors_get_feature(*name,feat_min,&min) &&
        !sensors_get_feature(*name,feat_div,&max)) {
      if (valid) {
        print_label(label,10);
        printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)   %s\n",
               cur,min,max,(alarms&(ADM1026_ALARM_FAN0<<i))?"ALARM":"");
      }
    } else {
      printf("ERROR: Can't get FAN%d data!\n",i);
    }
    free_the_label(&label);
  };

  /* Three temperature sensors
   * NOTE:  6 config values per temperature
   *      0  current
   *      1  max
   *      2  min
   *      3  offset   (to current)
   *      4  therm    (SMBAlert)
   *      5  tmin     (AFC)
   */
  for (i = 0; i <= 2 ; ++i) {
    int  feat_base = SENSORS_ADM1026_TEMP1 + (6 * i);
    int  feat_max = feat_base +1;
    int  feat_min = feat_base +2;

    if (!sensors_get_label_and_valid(*name,feat_base,&label,&valid) &&
        !sensors_get_feature(*name,feat_base,&cur) &&
        !sensors_get_feature(*name,feat_min,&min) &&
        !sensors_get_feature(*name,feat_max,&max)) {
      if (valid) {
        print_label(label,10);
        print_temp_info( cur, max, min, MINMAX, 0, 0);
	puts( (alarms&adm1026_alarms_temp[i])?"   ALARM":"" );
      }
    } else {
      printf("ERROR: Can't get TEMP%d data!\n",i+1);
    }
    free_the_label(&label);
  };

  /* VID/VRM */
  print_vid_info(name, SENSORS_ADM1026_VID, SENSORS_ADM1026_VRM);
}

void print_lm83(const sensors_chip_name *name)
{
  char *label;
  double cur,high,crit;
  int valid,alarms;

  if (!sensors_get_feature(*name,SENSORS_LM83_ALARMS,&cur))
    alarms = cur + 0.5;
  else {
    printf("ERROR: Can't get alarm data!\n");
    alarms = 0;
  }

  if (sensors_get_feature(*name,SENSORS_LM83_TCRIT,&crit)) {
    printf("ERROR: Can't get tcrit data!\n");
    crit = 127;
  }

  if (!sensors_get_label_and_valid(*name,SENSORS_LM83_LOCAL_TEMP,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM83_LOCAL_TEMP,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM83_LOCAL_HIGH,&high))  {
    if (valid) {
      print_label(label,10);
      print_temp_info( cur, high, crit, CRIT, 0, 0);
      printf(" %s\n",
      	alarms&LM83_ALARM_LOCAL_CRIT?"CRITICAL":
      	alarms&LM83_ALARM_LOCAL_HIGH?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get local temperature data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_LM83_REMOTE1_TEMP,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM83_REMOTE1_TEMP,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM83_REMOTE1_HIGH,&high))  {
    if (valid) {
      print_label(label,10);
      print_temp_info( cur, high, crit, CRIT, 0, 0);
      printf(" %s\n",
      	alarms&LM83_ALARM_REMOTE1_OPEN?"DISCONNECT":
      	alarms&LM83_ALARM_REMOTE1_CRIT?"CRITICAL":
      	alarms&LM83_ALARM_REMOTE1_HIGH?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get remote temperature 1 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_LM83_REMOTE2_TEMP,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM83_REMOTE2_TEMP,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM83_REMOTE2_HIGH,&high))  {
    if (valid) {
      print_label(label,10);
      print_temp_info( cur, high, crit, CRIT, 0, 0);
      printf(" %s\n",
      	alarms&LM83_ALARM_REMOTE2_OPEN?"DISCONNECT":
      	alarms&LM83_ALARM_REMOTE2_CRIT?"CRITICAL":
      	alarms&LM83_ALARM_REMOTE2_HIGH?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get remote temperature 2 data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name,SENSORS_LM83_REMOTE3_TEMP,&label,&valid) &&
      !sensors_get_feature(*name,SENSORS_LM83_REMOTE3_TEMP,&cur) &&
      !sensors_get_feature(*name,SENSORS_LM83_REMOTE3_HIGH,&high))  {
    if (valid) {
      print_label(label,10);
      print_temp_info( cur, high, crit, CRIT, 0, 0);
      printf(" %s\n",
      	alarms&LM83_ALARM_REMOTE3_OPEN?"DISCONNECT":
      	alarms&LM83_ALARM_REMOTE3_CRIT?"CRITICAL":
      	alarms&LM83_ALARM_REMOTE3_HIGH?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get remote temperature 3 data!\n");
  free_the_label(&label);
}

void print_lm90(const sensors_chip_name *name)
{
  char *label;
  double cur, high, low, hyst;
  int valid, alarms;

  if (!sensors_get_feature(*name, SENSORS_LM90_ALARMS, &cur))
    alarms = cur + 0.5;
  else {
    printf("ERROR: Can't get alarm data!\n");
    alarms = 0;
  }

  if (!sensors_get_label_and_valid(*name, SENSORS_LM90_LOCAL_TEMP,
      &label, &valid)
   && !sensors_get_feature(*name, SENSORS_LM90_LOCAL_TEMP, &cur)
   && !sensors_get_feature(*name, SENSORS_LM90_LOCAL_HIGH, &high)
   && !sensors_get_feature(*name, SENSORS_LM90_LOCAL_LOW, &low)) {
    if (valid) {
      print_label(label, 10);
      print_temp_info(cur, high, low, MINMAX, 0, 0);
      printf(" %s\n",
        alarms&LM90_ALARM_LOCAL_CRIT?"CRITICAL":
      	alarms&(LM90_ALARM_LOCAL_HIGH|LM90_ALARM_LOCAL_LOW)?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get local temperature data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name, SENSORS_LM90_REMOTE_TEMP,
      &label, &valid)
   && !sensors_get_feature(*name, SENSORS_LM90_REMOTE_TEMP, &cur)
   && !sensors_get_feature(*name, SENSORS_LM90_REMOTE_HIGH, &high)
   && !sensors_get_feature(*name, SENSORS_LM90_REMOTE_LOW, &low)) {
    if (valid) {
      print_label(label, 10);
      print_temp_info(cur, high, low, MINMAX, 1, 1);
      printf(" %s\n",
        alarms&LM90_ALARM_REMOTE_OPEN?"DISCONNECT":
        alarms&LM90_ALARM_REMOTE_CRIT?"CRITICAL":
      	alarms&(LM90_ALARM_REMOTE_HIGH|LM90_ALARM_REMOTE_LOW)?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get remote temperature data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name, SENSORS_LM90_LOCAL_TCRIT,
      &label, &valid)
   && !sensors_get_feature(*name, SENSORS_LM90_LOCAL_TCRIT, &high)
   && !sensors_get_feature(*name, SENSORS_LM90_LOCAL_TCRIT_HYST, &hyst)) {
    if (valid) {
      print_label(label, 10);
      print_temp_info(high, hyst, 0, HYSTONLY, 0, 0);
      printf("\n");
    }
  } else
    printf("ERROR: Can't get local tcrit data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name, SENSORS_LM90_REMOTE_TCRIT,
      &label, &valid)
   && !sensors_get_feature(*name, SENSORS_LM90_REMOTE_TCRIT, &high)
   && !sensors_get_feature(*name, SENSORS_LM90_REMOTE_TCRIT_HYST, &hyst)) {
    if (valid) {
      print_label(label, 10);
      print_temp_info(high, hyst, 0, HYSTONLY, 0, 0);
      printf("\n");
    }
  } else
    printf("ERROR: Can't get remote tcrit data!\n");
  free_the_label(&label);
}

void print_lm63(const sensors_chip_name *name)
{
  char *label;
  double cur, high, low;
  int valid, alarms;

  if (!sensors_get_feature(*name, SENSORS_LM63_ALARMS, &cur))
    alarms = cur + 0.5;
  else {
    printf("ERROR: Can't get alarm data!\n");
    alarms = 0;
  }

  if (!sensors_get_label_and_valid(*name, SENSORS_LM63_LOCAL_TEMP,
      &label, &valid)
   && !sensors_get_feature(*name, SENSORS_LM63_LOCAL_TEMP, &cur)
   && !sensors_get_feature(*name, SENSORS_LM63_LOCAL_HIGH, &high)) {
    if (valid) {
      print_label(label, 10);
      print_temp_info(cur, high, 0, MAXONLY, 0, 0);
      printf(" %s\n",
      	alarms&LM63_ALARM_LOCAL_HIGH?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get local temperature data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name, SENSORS_LM63_REMOTE_TEMP,
      &label, &valid)
   && !sensors_get_feature(*name, SENSORS_LM63_REMOTE_TEMP, &cur)
   && !sensors_get_feature(*name, SENSORS_LM63_REMOTE_HIGH, &high)
   && !sensors_get_feature(*name, SENSORS_LM63_REMOTE_LOW, &low)) {
    if (valid) {
      print_label(label, 10);
      print_temp_info(cur, high, low, MINMAX, 1, 1);
      printf(" %s\n",
        alarms&LM63_ALARM_REMOTE_OPEN?"OPEN":
        alarms&LM63_ALARM_REMOTE_CRIT?"CRITICAL":
      	alarms&(LM63_ALARM_REMOTE_HIGH|LM63_ALARM_REMOTE_LOW)?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get remote temperature data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name, SENSORS_LM63_REMOTE_TCRIT,
      &label, &valid)
   && !sensors_get_feature(*name, SENSORS_LM63_REMOTE_TCRIT, &high)
   && !sensors_get_feature(*name, SENSORS_LM63_REMOTE_TCRIT_HYST, &low)) {
    if (valid) {
      print_label(label, 10);
      print_temp_info(high, low, 0, HYSTONLY, 0, 0);
      printf("\n");
    }
  } else
    printf("ERROR: Can't get remote tcrit data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name, SENSORS_LM63_FAN,
      &label, &valid)
   && !sensors_get_feature(*name, SENSORS_LM63_FAN, &cur)
   && !sensors_get_feature(*name, SENSORS_LM63_FAN_LOW, &low)) {
    if (valid) {
      print_label(label, 10);
      printf("%4.0f RPM  (min = %4.0f RPM) %s\n", cur, low,
	alarms&LM63_ALARM_FAN_LOW?"ALARM":"");
    }
  }
  /* No error if fan files are missing as it will happen with 2.6
     kernels when the tachometer pin is used as an alarm output. */
  free_the_label(&label);
}

void print_adm1031(const sensors_chip_name *name)
{
  char *label;
  double cur, high, low, crit, div;
  int valid, alarms, i;
  int is_1031 = !strncmp("adm1031", name->prefix, 7);

  if (!sensors_get_feature(*name, SENSORS_ADM1031_ALARMS, &cur))
      alarms = cur;
  else {
      printf("ERROR: Can't get alarm data!\n");
      alarms = 0;
  }
  
  for(i=0; i<(is_1031?2:1);i++) {
      if (!sensors_get_label_and_valid(*name, SENSORS_ADM1031_FAN1+i*10,
				       &label, &valid)
	  && !sensors_get_feature(*name, SENSORS_ADM1031_FAN1+i*10, &cur)
	  && !sensors_get_feature(*name, SENSORS_ADM1031_FAN1_MIN+i*10, &low)
	  && !sensors_get_feature(*name, SENSORS_ADM1031_FAN1_DIV+i*10, &div)) {
	  if (valid) {
	      print_label(label, 10);
	      printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)", cur, low, div);
	      printf(" %s\n",
		     alarms&(ADM1031_ALARM_FAN1_FLT<<(i*8))?"FAULT":
		     alarms&(ADM1031_ALARM_FAN1_MIN<<(i*8))?"ALARM":"");
	  }
      } else
	  printf("ERROR: Can't get fan%d data!\n", i+1);
      free_the_label(&label);
  }

  if (!sensors_get_label_and_valid(*name, SENSORS_ADM1031_TEMP1,
				   &label, &valid)
      && !sensors_get_feature(*name, SENSORS_ADM1031_TEMP1, &cur)
      && !sensors_get_feature(*name, SENSORS_ADM1031_TEMP1_MIN, &low)
      && !sensors_get_feature(*name, SENSORS_ADM1031_TEMP1_MAX, &high)
      && !sensors_get_feature(*name, SENSORS_ADM1031_TEMP1_CRIT, &crit)) {
      if (valid) {
	  print_label(label, 10);
	  print_temp_info(cur, high, low, MINMAX, 1, 0);
	  printf(" %s\n",
		 alarms&(ADM1031_ALARM_TEMP1_HIGH|ADM1031_ALARM_TEMP1_LOW)?"ALARM":"");
      }
  } else
      printf("ERROR: Can't get temp1 temperature data!\n");
  free_the_label(&label);
  if (!sensors_get_label_and_valid(*name, SENSORS_ADM1031_TEMP1_CRIT,
				   &label, &valid)
      && !sensors_get_feature(*name, SENSORS_ADM1031_TEMP1_CRIT, &cur)) {
      if (valid) {
	  print_label(label, 10);
	  print_temp_info(cur, 0, 0, SINGLE, 0, 0);
	  printf(" %s\n",
		 alarms&ADM1031_ALARM_TEMP1_CRIT?"CRITICAL":"");
      }
  } else
      printf("ERROR: Can't get temp1 temperature data!\n");
  free_the_label(&label);
  
  for (i=0; i < (is_1031 ? 2 : 1); i++) {
      if (!sensors_get_label_and_valid(*name, SENSORS_ADM1031_TEMP2+i*10,
				       &label, &valid)
	  && !sensors_get_feature(*name, SENSORS_ADM1031_TEMP2+i*10, &cur)
	  && !sensors_get_feature(*name, SENSORS_ADM1031_TEMP2_MIN+i*10, &low)
	  && !sensors_get_feature(*name, SENSORS_ADM1031_TEMP2_MAX+i*10, &high)) {
	  if (valid) {
	      print_label(label, 10);
	      print_temp_info(cur, high, low, MINMAX, 1, 0);
	      printf(" %s\n",
		     alarms&((ADM1031_ALARM_TEMP2_DIODE<<(i*8)))?"DISCONNECT":
		     alarms&((ADM1031_ALARM_TEMP2_HIGH<<(i*8))|
		      	     (ADM1031_ALARM_TEMP2_LOW<<(i*8)))?"ALARM":"");
	  }
      } else
	  printf("ERROR: Can't get temp%d temperature data!\n", i+2);
      free_the_label(&label);
      if (!sensors_get_label_and_valid(*name, SENSORS_ADM1031_TEMP2_CRIT+i*10,
				       &label, &valid)
	  && !sensors_get_feature(*name, SENSORS_ADM1031_TEMP2_CRIT+i*10, &cur)) {
	  if (valid) {
	      print_label(label, 10);
	      print_temp_info(cur, 0, 0, SINGLE, 0, 0);
	      printf(" %s\n",
		     alarms&((ADM1031_ALARM_TEMP2_CRIT<<(i*8)))?"CRITICAL":"");
	  }
      } else
	  printf("ERROR: Can't get temp%d crit temperature data!\n", i+2);
      free_the_label(&label);
  }
}

void print_xeontemp(const sensors_chip_name *name)
{
  char *label;
  double cur,hyst,over;
  int alarms,i,valid;

  if (!sensors_get_feature(*name,SENSORS_XEONTEMP_ALARMS,&cur)) 
    alarms = cur + 0.5;
  else {
    printf("ERROR: Can't get alarm data!\n");
    alarms = 0;
  }

  if (!sensors_get_label_and_valid(*name,SENSORS_XEONTEMP_REMOTE_TEMP,
                                   &label,&valid) &&
      !sensors_get_feature(*name,SENSORS_XEONTEMP_REMOTE_TEMP,&cur) &&
      !sensors_get_feature(*name,SENSORS_XEONTEMP_REMOTE_TEMP_HYST,&hyst) &&
      !sensors_get_feature(*name,SENSORS_XEONTEMP_REMOTE_TEMP_OVER,&over))  {
    if (valid) {
      print_label(label,10);
      print_temp_info( cur, over, hyst, MINMAX, 0, 0);
      if (alarms & (XEONTEMP_ALARM_RTEMP_HIGH | XEONTEMP_ALARM_RTEMP_LOW |
                    XEONTEMP_ALARM_RTEMP_NA)) {
        printf("ALARM (");
        i = 0;
          if (alarms & XEONTEMP_ALARM_RTEMP_NA) {
          printf("N/A");
          i++;
        }
        if (alarms & XEONTEMP_ALARM_RTEMP_LOW) {
          printf("%sLOW",i?",":"");
          i++;
        }
        if (alarms & XEONTEMP_ALARM_RTEMP_HIGH)
          printf("%sHIGH",i?",":"");
        printf(")");
      }
      printf("\n");
    }
  } else
    printf("ERROR: Can't get temperature data!\n");
  free_the_label(&label);
}


void print_max1619(const sensors_chip_name *name)
{
  char *label;
  double cur, high, low;
  int valid, alarms;

  if (!sensors_get_feature(*name, SENSORS_MAX1619_ALARMS, &cur))
    alarms = cur + 0.5;
  else {
    printf("ERROR: Can't get alarm data!\n");
    alarms = 0;
  }

  if (!sensors_get_label_and_valid(*name, SENSORS_MAX1619_LOCAL_TEMP,
                                   &label, &valid)
   && !sensors_get_feature(*name, SENSORS_MAX1619_LOCAL_TEMP, &cur)) {
    if (valid) {
        print_label(label, 10);
        print_temp_info(cur,0 ,0 ,SINGLE , 0, 0);
        printf("\n");
    }
  } else
    printf("ERROR: Can't get local temperature data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name, SENSORS_MAX1619_REMOTE_TEMP,
                                   &label, &valid)
   && !sensors_get_feature(*name, SENSORS_MAX1619_REMOTE_TEMP, &cur)
   && !sensors_get_feature(*name, SENSORS_MAX1619_REMOTE_LOW, &low)
   && !sensors_get_feature(*name, SENSORS_MAX1619_REMOTE_HIGH, &high)) {
    if (valid) {
      print_label(label, 10);
      print_temp_info(cur, low, high, MINMAX, 0, 0);
      printf(" %s\n",
        alarms&MAX1619_ALARM_REMOTE_OPEN?"DISCONNECT":
        alarms&(MAX1619_ALARM_REMOTE_THIGH|MAX1619_ALARM_REMOTE_TLOW)?"ALARM":"");
    }
  } else
    printf("ERROR: Can't get remote temperature data!\n");
  free_the_label(&label);

  if (!sensors_get_label_and_valid(*name, SENSORS_MAX1619_REMOTE_MAX,
                                   &label, &valid)
   && !sensors_get_feature(*name, SENSORS_MAX1619_REMOTE_MAX, &high)
   && !sensors_get_feature(*name, SENSORS_MAX1619_REMOTE_HYST, &low)) {
    if (valid) {
      print_label(label, 10);
      print_temp_info(high, low, 0, HYSTONLY, 0, 0);
      printf("\n");
    }
  } else
    printf("ERROR: Can't get remote temperature max data!\n");
  free_the_label(&label);
}

void print_max6650(const sensors_chip_name *name)
{
  char *label = NULL;
  double tach, speed;
  int valid, i;

  static const struct
  {
    int tag;
    char *name;
  }
  tach_list[] =
  {
    { SENSORS_MAX6650_FAN1_TACH, "FAN1" },
    { SENSORS_MAX6650_FAN2_TACH, "FAN2" },
    { SENSORS_MAX6650_FAN3_TACH, "FAN3" },
    { SENSORS_MAX6650_FAN4_TACH, "FAN4" }
  };

  /* Display full config for fan1, which is controlled */

  if (!sensors_get_label_and_valid(*name,tach_list[0].tag,&label,&valid) &&
      !sensors_get_feature(*name,tach_list[0].tag,&tach) &&
      !sensors_get_feature(*name,SENSORS_MAX6650_SPEED,&speed)) {
    if (valid) {
      print_label(label,10);
      printf("configured %4.0f RPM, actual %4.0f RPM.\n", speed, tach);
    }
  } else
    printf("ERROR: Can't get %s data!\n", tach_list[0].name);
  free_the_label(&label);
  
  /* Just display the measured speed for the other three, uncontrolled fans */
  
  for (i = 1; i < 4; i++)
  {
    if (!sensors_get_label_and_valid(*name,tach_list[i].tag,&label,&valid) &&
        !sensors_get_feature(*name,tach_list[i].tag,&tach)) {
      if (valid) {
        print_label(label,10);
        printf("%4.0f RPM  \n", tach);
      }
    } else
      printf("ERROR: Can't get %s data!\n", tach_list[i].name);
 
    free_the_label(&label);
  }
}

/* print_asb100_in()
 *   where in, in_min, and in_max are sensors feature IDs
 */
static void print_asb100_in(const sensors_chip_name *name, int alarm,
	int in, int in_min, int in_max)
{
  char *label = NULL;
  double cur, min, max;
  int valid;

  if (!sensors_get_label_and_valid(*name,in,&label,&valid) &&
      !sensors_get_feature(*name,in,&cur) &&
      !sensors_get_feature(*name,in_min,&min) &&
      !sensors_get_feature(*name,in_max,&max)) {
    if (valid) {
      print_label(label,10);
      printf("%+6.2f V  (min = %+6.2f V, max = %+6.2f V)       %s\n",
           cur, min, max, alarm ? "ALARM" : "");
    }
  } else
    printf("ERROR: Can't get IN data! (0x%04x)\n", in);
  free_the_label(&label);
}

#define PRINT_ASB100_IN(num, name, alarms) \
	print_asb100_in((name), ((alarms) & ASB100_ALARM_IN##num), \
		(SENSORS_ASB100_IN##num), \
		(SENSORS_ASB100_IN##num##_MIN), \
		(SENSORS_ASB100_IN##num##_MAX))

/* print_asb100_fan()
 *   where fan, fan_div, and fan_min are sensors feature IDs
 */
static void print_asb100_fan(const sensors_chip_name *name, int alarm,
	int fan, int fan_div, int fan_min)
{
  char *label = NULL;
  double cur, div, min;
  int valid;

  if (!sensors_get_label_and_valid(*name,fan,&label,&valid) &&
      !sensors_get_feature(*name,fan,&cur) &&
      !sensors_get_feature(*name,fan_div,&div) &&
      !sensors_get_feature(*name,fan_min,&min)) {
    if (valid) {
      print_label(label,10);
      printf("%4.0f RPM  (min = %4.0f RPM, div = %1.0f)              %s\n",
           cur, min, div, alarm ? "ALARM" : "");
    }
  } else
    printf("ERROR: Can't get FAN data! (0x%04x)\n", fan);
  free_the_label(&label);
}

#define PRINT_ASB100_FAN(num, name, alarms) \
	print_asb100_fan((name), ((alarms) & ASB100_ALARM_FAN##num), \
		(SENSORS_ASB100_FAN##num), \
		(SENSORS_ASB100_FAN##num##_DIV), \
		(SENSORS_ASB100_FAN##num##_MIN))

/* print_asb100_temp()
 * where temp, temp_max, and temp_hyst are sensors feature IDs
 */
static void print_asb100_temp(const sensors_chip_name *name, int alarm,
	int temp, int temp_max, int temp_hyst)
{
  char *label = NULL;
  double cur, max, hyst;
  int valid;

  if (!sensors_get_label_and_valid(*name,temp,&label,&valid) &&
      !sensors_get_feature(*name,temp,&cur) &&
      !sensors_get_feature(*name,temp_max,&max) &&
      !sensors_get_feature(*name,temp_hyst,&hyst)) {
    if (valid) {
      print_label(label,10);
      if (hyst == 127)
	print_temp_info(cur, max, 0, MAXONLY, 0, 0);
      else
	print_temp_info(cur, max, hyst, HYST, 0, 0);

      printf(" %s\n", alarm ? "ALARM" : "");
    }
  } else
    printf("ERROR: Can't get TEMP data! (0x%04x)\n", temp);

  free_the_label(&label);
}

#ifndef ASB100_ALARM_TEMP4 
#define ASB100_ALARM_TEMP4 0
#endif

#define PRINT_ASB100_TEMP(num, name, alarms) \
	print_asb100_temp((name), ((alarms) & ASB100_ALARM_TEMP##num), \
		(SENSORS_ASB100_TEMP##num), \
		(SENSORS_ASB100_TEMP##num##_OVER), \
		(SENSORS_ASB100_TEMP##num##_HYST))


void print_asb100(const sensors_chip_name *name)
{
  char *label = NULL;
  double cur;
  int valid, alarms = 0;

  if (!sensors_get_feature(*name,SENSORS_ASB100_ALARMS,&cur)) 
    alarms = cur + 0.5;
  else
    printf("ERROR: Can't get alarm data!\n");

  PRINT_ASB100_IN(0, name, alarms);
  PRINT_ASB100_IN(1, name, alarms);
  PRINT_ASB100_IN(2, name, alarms);
  PRINT_ASB100_IN(3, name, alarms);
  PRINT_ASB100_IN(4, name, alarms);
  PRINT_ASB100_IN(5, name, alarms);
  PRINT_ASB100_IN(6, name, alarms);

  PRINT_ASB100_FAN(1, name, alarms);
  PRINT_ASB100_FAN(2, name, alarms);
  PRINT_ASB100_FAN(3, name, alarms);

  PRINT_ASB100_TEMP(1, name, alarms);
  PRINT_ASB100_TEMP(2, name, alarms);
  PRINT_ASB100_TEMP(3, name, alarms);
  PRINT_ASB100_TEMP(4, name, alarms);

  print_vid_info(name, SENSORS_ASB100_VID, SENSORS_ASB100_VRM);

  if (!sensors_get_label_and_valid(*name,SENSORS_ASB100_ALARMS,&label,&valid)) {
    print_label(label,10);
    if (alarms & ASB100_ALARM_CHAS)
      printf("Chassis intrusion detection                      ALARM\n");
    else
      printf("\n");
  }
  free_the_label(&label);

}

/* happens to be similar to asb100*/
#define PRINT_LM93_IN(num, name, alarms) \
	print_asb100_in((name), ((alarms) & LM93_ALARM_IN##num), \
		(SENSORS_LM93_IN##num), \
		(SENSORS_LM93_IN##num##_MIN), \
		(SENSORS_LM93_IN##num##_MAX))

/* print_lm93_fan()
 *   where fan and fan_min are sensors feature IDs
 */
static void print_lm93_fan(const sensors_chip_name *name, int alarm,
	int fan, int fan_min)
{
  char *label = NULL;
  double cur, min;
  int valid;

  if (!sensors_get_label_and_valid(*name,fan,&label,&valid) &&
      !sensors_get_feature(*name,fan,&cur) &&
      !sensors_get_feature(*name,fan_min,&min)) {
    if (valid) {
      print_label(label,10);
      printf("%4.0f RPM  (min = %4.0f RPM)                       %s\n",
           cur, min, alarm ? "ALARM" : "");
    }
  } else
    printf("ERROR: Can't get FAN data! (0x%04x)\n", fan);
  free_the_label(&label);
}

#define PRINT_LM93_FAN(num, name, alarms) \
	print_lm93_fan((name), ((alarms) & LM93_ALARM_FAN##num), \
		(SENSORS_LM93_FAN##num), \
		(SENSORS_LM93_FAN##num##_MIN))

/* print_lm93_temp()
 * where temp, temp_min, and temp_max are sensors feature IDs
 */
static void print_lm93_temp(const sensors_chip_name *name, int alarm,
	int temp, int temp_min, int temp_max)
{
  char *label = NULL;
  double cur, min, max;
  int valid;

  if (!sensors_get_label_and_valid(*name,temp,&label,&valid) &&
      !sensors_get_feature(*name,temp,&cur) &&
      !sensors_get_feature(*name,temp_min,&min) &&
      !sensors_get_feature(*name,temp_max,&max)) {
    if (valid) {
      print_label(label,10);
      print_temp_info(cur, max, min, MINMAX, 0, 0);
      printf("     %s\n", alarm ? "ALARM" : "");
    }
  } else
    printf("ERROR: Can't get TEMP data! (0x%04x)\n", temp);

  free_the_label(&label);
}

#define PRINT_LM93_TEMP(num, name, alarms) \
	print_lm93_temp((name), ((alarms) & LM93_ALARM_TEMP##num), \
		(SENSORS_LM93_TEMP##num), \
		(SENSORS_LM93_TEMP##num##_MIN), \
		(SENSORS_LM93_TEMP##num##_MAX))

/* print_lm93_vid()
 * where vid is a sensors feature ID
 */
static void print_lm93_vid(const sensors_chip_name *name, int vid)
{
  char *label = NULL;
  double cur;
  int valid;

  if (!sensors_get_label_and_valid(*name,vid,&label,&valid) &&
      !sensors_get_feature(*name,vid,&cur)) {
    if (valid) {
      print_label(label,10);
      printf("%+5.3f V\n",cur);
    }
  } else {
    printf("ERROR: Can't get VID data! (0x%04x)\n", vid);
  }
  free_the_label(&label);
}

void print_lm93(const sensors_chip_name *name)
{
  double cur;
  int alarms = 0;

  if (!sensors_get_feature(*name,SENSORS_LM93_ALARMS,&cur)) 
    alarms = cur + 0.5;
  else
    printf("ERROR: Can't get alarm data!\n");

  PRINT_LM93_IN(1, name, alarms);
  PRINT_LM93_IN(2, name, alarms);
  PRINT_LM93_IN(3, name, alarms);
  PRINT_LM93_IN(4, name, alarms);
  PRINT_LM93_IN(5, name, alarms);
  PRINT_LM93_IN(6, name, alarms);
  PRINT_LM93_IN(7, name, alarms);
  PRINT_LM93_IN(8, name, alarms);
  PRINT_LM93_IN(9, name, alarms);
  PRINT_LM93_IN(10, name, alarms);
  PRINT_LM93_IN(11, name, alarms);
  PRINT_LM93_IN(12, name, alarms);
  PRINT_LM93_IN(13, name, alarms);
  PRINT_LM93_IN(14, name, alarms);
  PRINT_LM93_IN(15, name, alarms);
  PRINT_LM93_IN(16, name, alarms);

  PRINT_LM93_FAN(1, name, alarms);
  PRINT_LM93_FAN(2, name, alarms);
  PRINT_LM93_FAN(3, name, alarms);
  PRINT_LM93_FAN(4, name, alarms);

  PRINT_LM93_TEMP(1, name, alarms);
  PRINT_LM93_TEMP(2, name, alarms);
  PRINT_LM93_TEMP(3, name, alarms);

  print_lm93_vid(name, SENSORS_LM93_VID1);
  print_lm93_vid(name, SENSORS_LM93_VID2);
}

/* print_smsc47b397_temp()
 * where temp is a sensors feature ID
 */
static void print_smsc47b397_temp(const sensors_chip_name *name, int temp)
{
  char *label = NULL;
  double cur;
  int valid;

  if (!sensors_get_label_and_valid(*name,temp,&label,&valid) &&
      !sensors_get_feature(*name,temp,&cur)) {
    if (valid) {
      print_label(label,10);
      print_temp_info(cur, 0, 0, SINGLE, 0, 0);
      printf("\n");
    }
  } else
    printf("ERROR: Can't get TEMP data! (0x%04x)\n", temp);

  free_the_label(&label);
}

#define PRINT_SMSC47B397_TEMP(num, name) \
	print_smsc47b397_temp((name), (SENSORS_SMSC47B397_TEMP1 + (num)))

/* print_smsc47b397_fan()
 *   where fan is a sensors feature ID
 */
static void print_smsc47b397_fan(const sensors_chip_name *name, int fan)
{
  char *label = NULL;
  double cur;
  int valid;

  if (!sensors_get_label_and_valid(*name,fan,&label,&valid) &&
      !sensors_get_feature(*name,fan,&cur)) {
    if (valid) {
      print_label(label,10);
      printf("%4.0f RPM\n", cur);
    }
  } else
    printf("ERROR: Can't get FAN data! (0x%04x)\n", fan);
  free_the_label(&label);
}

#define PRINT_SMSC47B397_FAN(num, name) \
        print_smsc47b397_fan((name), (SENSORS_SMSC47B397_FAN1 + (num)))

void print_smsc47b397(const sensors_chip_name *name)
{
  int ii;

  for (ii=0;ii<4;ii++)
    PRINT_SMSC47B397_TEMP(ii, name);

  for (ii=0;ii<4;ii++)
    PRINT_SMSC47B397_FAN(ii, name);
}

void print_unknown_chip(const sensors_chip_name *name)
{
  int a,b,valid;
  const sensors_feature_data *data;
  char *label;
  double val;
 
  a=b=0;
  while((data=sensors_get_all_features(*name,&a,&b))) {
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
  }
}

