/*
    chips.h - Part of libsensors, a Linux library for reading sensor data.
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

/* This files contains, for each type of chip, the kind of values you can
   read and write. The actual value of each define is completely arbitrary,
   as long as, for one specific chip, each defined value is unique. I tried
   to give similar features similar values, but you can't trust that.
   Some entries are very alike (for example the LM78, LM78-J and LM79
   defines). Where documented, you can mix the defines. If chip prefixes
   are different, they get their own entry. */

/* If you add a chip here, do not forget to add the entry in chips.c too.
   Other than that (and a recompile, and a bump up of the library number
   for shared libs) nothing should need to be done to support new chips. */


/* LM78 chips */

#ifndef LIB_SENSORS_CHIPS_H
#define LIB_SENSORS_CHIP_H

#define SENSORS_LM78_PREFIX "lm78"

#define SENSORS_LM78_IN0 1 /* R */
#define SENSORS_LM78_IN1 2 /* R */
#define SENSORS_LM78_IN2 3 /* R */
#define SENSORS_LM78_IN3 4 /* R */
#define SENSORS_LM78_IN4 5 /* R */
#define SENSORS_LM78_IN5 6 /* R */
#define SENSORS_LM78_IN6 7 /* R */
#define SENSORS_LM78_IN0_MIN 11 /* RW */
#define SENSORS_LM78_IN1_MIN 12 /* RW */
#define SENSORS_LM78_IN2_MIN 13 /* RW */
#define SENSORS_LM78_IN3_MIN 14 /* RW */
#define SENSORS_LM78_IN4_MIN 15 /* RW */
#define SENSORS_LM78_IN5_MIN 16 /* RW */
#define SENSORS_LM78_IN6_MIN 17 /* RW */
#define SENSORS_LM78_IN0_MAX 21 /* RW */
#define SENSORS_LM78_IN1_MAX 22 /* RW */
#define SENSORS_LM78_IN2_MAX 23 /* RW */
#define SENSORS_LM78_IN3_MAX 24 /* RW */
#define SENSORS_LM78_IN4_MAX 25 /* RW */
#define SENSORS_LM78_IN5_MAX 26 /* RW */
#define SENSORS_LM78_IN6_MAX 27 /* RW */
#define SENSORS_LM78_FAN1 31 /* R */
#define SENSORS_LM78_FAN2 32 /* R */
#define SENSORS_LM78_FAN3 33 /* R */
#define SENSORS_LM78_FAN1_MIN 41 /* RW */
#define SENSORS_LM78_FAN2_MIN 42 /* RW */
#define SENSORS_LM78_FAN3_MIN 43 /* RW */
#define SENSORS_LM78_TEMP 51 /* R */
#define SENSORS_LM78_TEMP_HYST 52 /* RW */
#define SENSORS_LM78_TEMP_OVER 53 /* RW */
#define SENSORS_LM78_VID 61 /* R */
#define SENSORS_LM78_FAN1_DIV 71 /* RW */
#define SENSORS_LM78_FAN2_DIV 72 /* RW */
#define SENSORS_LM78_FAN3_DIV 73 /* R (yes, really! */
#define SENSORS_LM78_ALARMS 81 /* R */


/* LM78-J chips. It is actually safe to use the LM78 defines instead, but it
   is better practice to use these. */

#define SENSORS_LM78J_PREFIX "lm78-j"

#define SENSORS_LM78J_IN0 1 /* R */
#define SENSORS_LM78J_IN1 2 /* R */
#define SENSORS_LM78J_IN2 3 /* R */
#define SENSORS_LM78J_IN3 4 /* R */
#define SENSORS_LM78J_IN4 5 /* R */
#define SENSORS_LM78J_IN5 6 /* R */
#define SENSORS_LM78J_IN6 7 /* R */
#define SENSORS_LM78J_IN0_MIN 11 /* RW */
#define SENSORS_LM78J_IN1_MIN 12 /* RW */
#define SENSORS_LM78J_IN2_MIN 13 /* RW */
#define SENSORS_LM78J_IN3_MIN 14 /* RW */
#define SENSORS_LM78J_IN4_MIN 15 /* RW */
#define SENSORS_LM78J_IN5_MIN 16 /* RW */
#define SENSORS_LM78J_IN6_MIN 17 /* RW */
#define SENSORS_LM78J_IN0_MAX 21 /* RW */
#define SENSORS_LM78J_IN1_MAX 22 /* RW */
#define SENSORS_LM78J_IN2_MAX 23 /* RW */
#define SENSORS_LM78J_IN3_MAX 24 /* RW */
#define SENSORS_LM78J_IN4_MAX 25 /* RW */
#define SENSORS_LM78J_IN5_MAX 26 /* RW */
#define SENSORS_LM78J_IN6_MAX 27 /* RW */
#define SENSORS_LM78J_FAN1 31 /* R */
#define SENSORS_LM78J_FAN2 32 /* R */
#define SENSORS_LM78J_FAN3 33  /* R */
#define SENSORS_LM78J_FAN1_MIN 41 /* RW */
#define SENSORS_LM78J_FAN2_MIN 42 /* RW */
#define SENSORS_LM78J_FAN3_MIN 43 /* RW */
#define SENSORS_LM78J_TEMP 51 /* R */
#define SENSORS_LM78J_TEMP_HYST 52 /* RW */
#define SENSORS_LM78J_TEMP_OVER 53 /* RW */
#define SENSORS_LM78J_VID 61 /* R */
#define SENSORS_LM78J_FAN1_DIV 71 /* RW */
#define SENSORS_LM78J_FAN2_DIV 72 /* RW */
#define SENSORS_LM78J_FAN3_DIV 73 /* R (yes, really!) */
#define SENSORS_LM78J_ALARMS 81 /* R */


/* LM79 chips. It is actually safe to use the LM78 defines instead, but it
   is better practice to use these. */

#define SENSORS_LM79_PREFIX "lm79"

#define SENSORS_LM79_IN0 1 /* R */
#define SENSORS_LM79_IN1 2 /* R */
#define SENSORS_LM79_IN2 3 /* R */
#define SENSORS_LM79_IN3 4 /* R */
#define SENSORS_LM79_IN4 5 /* R */
#define SENSORS_LM79_IN5 6 /* R */
#define SENSORS_LM79_IN6 7 /* R */
#define SENSORS_LM79_IN0_MIN 11 /* RW */
#define SENSORS_LM79_IN1_MIN 12 /* RW */
#define SENSORS_LM79_IN2_MIN 13 /* RW */
#define SENSORS_LM79_IN3_MIN 14 /* RW */
#define SENSORS_LM79_IN4_MIN 15 /* RW */
#define SENSORS_LM79_IN5_MIN 16 /* RW */
#define SENSORS_LM79_IN6_MIN 17 /* RW */
#define SENSORS_LM79_IN0_MAX 21 /* RW */
#define SENSORS_LM79_IN1_MAX 22 /* RW */
#define SENSORS_LM79_IN2_MAX 23 /* RW */
#define SENSORS_LM79_IN3_MAX 24 /* RW */
#define SENSORS_LM79_IN4_MAX 25 /* R */
#define SENSORS_LM79_IN5_MAX 26 /* R */
#define SENSORS_LM79_IN6_MAX 27 /* R */
#define SENSORS_LM79_FAN1 31 /* R */
#define SENSORS_LM79_FAN2 32 /* R */
#define SENSORS_LM79_FAN3 33 /* R */
#define SENSORS_LM79_FAN1_MIN 41 /* RW */
#define SENSORS_LM79_FAN2_MIN 42 /* RW */
#define SENSORS_LM79_FAN3_MIN 43 /* RW */
#define SENSORS_LM79_TEMP 51 /* R */
#define SENSORS_LM79_TEMP_HYST 52 /* RW */
#define SENSORS_LM79_TEMP_OVER 53 /* RW */
#define SENSORS_LM79_VID 61 /* R */
#define SENSORS_LM79_FAN1_DIV 71 /* RW */
#define SENSORS_LM79_FAN2_DIV 72 /* RW */
#define SENSORS_LM79_FAN3_DIV 73 /* R (yes, really! */
#define SENSORS_LM79_ALARMS 81 /* R */


/* LM75 chips. */

#define SENSORS_LM75_PREFIX "lm75"

#define SENSORS_LM75_TEMP 51 /* R */
#define SENSORS_LM75_TEMP_HYST 52 /* RW */
#define SENSORS_LM75_TEMP_OVER 53 /* RW */

/* ADM1021 chips. */

#define SENSORS_ADM1021_PREFIX "adm1021"
/* Cheat on LM84,GL523,THMC10 for now - no separate #defines */
#define SENSORS_LM84_PREFIX "lm84"
#define SENSORS_GL523_PREFIX "gl523"
#define SENSORS_THMC10_PREFIX "thmc10"

#define SENSORS_ADM1021_TEMP 51 /* R */
#define SENSORS_ADM1021_TEMP_HYST 52 /* RW */
#define SENSORS_ADM1021_TEMP_OVER 53 /* RW */
#define SENSORS_ADM1021_REMOTE_TEMP 54 /* R */
#define SENSORS_ADM1021_REMOTE_TEMP_HYST 55 /* RW */
#define SENSORS_ADM1021_REMOTE_TEMP_OVER 56 /* RW */
#define SENSORS_ADM1021_ALARMS 81 /* R */
#define SENSORS_ADM1021_DIE_CODE 90 /* R */

/* MAX1617 chips. */

#define SENSORS_MAX1617_PREFIX "max1617"

#define SENSORS_MAX1617_TEMP 51 /* R */
#define SENSORS_MAX1617_TEMP_HYST 52 /* RW */
#define SENSORS_MAX1617_TEMP_OVER 53 /* RW */
#define SENSORS_MAX1617_REMOTE_TEMP 54 /* R */
#define SENSORS_MAX1617_REMOTE_TEMP_HYST 55 /* RW */
#define SENSORS_MAX1617_REMOTE_TEMP_OVER 56 /* RW */
#define SENSORS_MAX1617_ALARMS 81 /* R */

/* MAX1617A chips. */

#define SENSORS_MAX1617A_PREFIX "max1617a"

#define SENSORS_MAX1617A_TEMP 51 /* R */
#define SENSORS_MAX1617A_TEMP_HYST 52 /* RW */
#define SENSORS_MAX1617A_TEMP_OVER 53 /* RW */
#define SENSORS_MAX1617A_REMOTE_TEMP 54 /* R */
#define SENSORS_MAX1617A_REMOTE_TEMP_HYST 55 /* RW */
#define SENSORS_MAX1617A_REMOTE_TEMP_OVER 56 /* RW */
#define SENSORS_MAX1617A_ALARMS 81 /* R */

/* GL518SM chips */

#define SENSORS_GL518_PREFIX "gl518sm"

#define SENSORS_GL518_VDD 1 /* R */
#define SENSORS_GL518_VIN1 2 /* R */
#define SENSORS_GL518_VIN2 3 /* R */
#define SENSORS_GL518_VIN3 4 /* R */
#define SENSORS_GL518_VDD_MIN 11 /* RW */
#define SENSORS_GL518_VIN1_MIN 12 /* RW */
#define SENSORS_GL518_VIN2_MIN 13 /* RW */
#define SENSORS_GL518_VIN3_MIN 14 /* RW */
#define SENSORS_GL518_VDD_MAX 21 /* RW */
#define SENSORS_GL518_VIN1_MAX 22 /* RW */
#define SENSORS_GL518_VIN2_MAX 23 /* RW */
#define SENSORS_GL518_VIN3_MAX 24 /* RW */
#define SENSORS_GL518_FAN1 31 /* R */
#define SENSORS_GL518_FAN2 32 /* R */
#define SENSORS_GL518_FAN1_MIN 41 /* RW */
#define SENSORS_GL518_FAN2_MIN 42 /* RW */
#define SENSORS_GL518_TEMP 51 /* R */
#define SENSORS_GL518_TEMP_HYST 52 /* RW */
#define SENSORS_GL518_TEMP_OVER 53 /* RW */
#define SENSORS_GL518_FAN1_DIV 71 /* RW */
#define SENSORS_GL518_FAN2_DIV 72 /* RW */
#define SENSORS_GL518_ALARMS 81 /* R */
#define SENSORS_GL518_BEEP_ENABLE 82 /* RW */
#define SENSORS_GL518_BEEPS 83 /* RW */
#define SENSORS_GL518_ITERATE 84 /* RW */
#define SENSORS_GL518_FAN1OFF 85 /* RW */
#define SENSORS_GL518_FAN1PIN 86 /* RW */

/* GL520SM chips */

#define SENSORS_GL520_PREFIX "gl520sm"

#define SENSORS_GL520_VDD 1 /* R */
#define SENSORS_GL520_VIN1 2 /* R */
#define SENSORS_GL520_VIN2 3 /* R */
#define SENSORS_GL520_VIN3 4 /* R */
#define SENSORS_GL520_VIN4 4 /* R */
#define SENSORS_GL520_VDD_MIN 11 /* RW */
#define SENSORS_GL520_VIN1_MIN 12 /* RW */
#define SENSORS_GL520_VIN2_MIN 13 /* RW */
#define SENSORS_GL520_VIN3_MIN 14 /* RW */
#define SENSORS_GL520_VIN4_MIN 15 /* RW */
#define SENSORS_GL520_VDD_MAX 21 /* RW */
#define SENSORS_GL520_VIN1_MAX 22 /* RW */
#define SENSORS_GL520_VIN2_MAX 23 /* RW */
#define SENSORS_GL520_VIN3_MAX 24 /* RW */
#define SENSORS_GL520_VIN4_MAX 25 /* RW */
#define SENSORS_GL520_FAN1 31 /* R */
#define SENSORS_GL520_FAN2 32 /* R */
#define SENSORS_GL520_FAN1_MIN 41 /* RW */
#define SENSORS_GL520_FAN2_MIN 42 /* RW */
#define SENSORS_GL520_TEMP1 51 /* R */
#define SENSORS_GL520_TEMP1_HYST 52 /* RW */
#define SENSORS_GL520_TEMP1_OVER 53 /* RW */
#define SENSORS_GL520_TEMP2 54 /* R */
#define SENSORS_GL520_TEMP2_HYST 55 /* RW */
#define SENSORS_GL520_TEMP2_OVER 56 /* RW */
#define SENSORS_GL520_VID 61 /* R */
#define SENSORS_GL520_FAN1_DIV 71 /* RW */
#define SENSORS_GL520_FAN2_DIV 72 /* RW */
#define SENSORS_GL520_ALARMS 81 /* R */
#define SENSORS_GL520_BEEP_ENABLE 82 /* RW */
#define SENSORS_GL520_BEEPS 83 /* RW */
#define SENSORS_GL520_TWOTEMPS 84 /* RW */
#define SENSORS_GL520_FAN1OFF 85 /* RW */

/* LM80 chips */

#define SENSORS_LM80_PREFIX "lm80"

#define SENSORS_LM80_IN0 1 /* R */
#define SENSORS_LM80_IN1 2 /* R */
#define SENSORS_LM80_IN2 3 /* R */
#define SENSORS_LM80_IN3 4 /* R */
#define SENSORS_LM80_IN4 5 /* R */
#define SENSORS_LM80_IN5 6 /* R */
#define SENSORS_LM80_IN6 7 /* R */
#define SENSORS_LM80_IN0_MIN 11 /* RW */
#define SENSORS_LM80_IN1_MIN 12 /* RW */
#define SENSORS_LM80_IN2_MIN 13 /* RW */
#define SENSORS_LM80_IN3_MIN 14 /* RW */
#define SENSORS_LM80_IN4_MIN 15 /* RW */
#define SENSORS_LM80_IN5_MIN 16 /* RW */
#define SENSORS_LM80_IN6_MIN 17 /* RW */
#define SENSORS_LM80_IN0_MAX 21 /* RW */
#define SENSORS_LM80_IN1_MAX 22 /* RW */
#define SENSORS_LM80_IN2_MAX 23 /* RW */
#define SENSORS_LM80_IN3_MAX 24 /* RW */
#define SENSORS_LM80_IN4_MAX 25 /* R */
#define SENSORS_LM80_IN5_MAX 26 /* R */
#define SENSORS_LM80_IN6_MAX 27 /* R */
#define SENSORS_LM80_FAN1 31 /* R */
#define SENSORS_LM80_FAN2 32 /* R */
#define SENSORS_LM80_FAN1_MIN 41 /* RW */
#define SENSORS_LM80_FAN2_MIN 42 /* RW */
#define SENSORS_LM80_TEMP 51 /* R */
#define SENSORS_LM80_TEMP_HOT_HYST 52 /* RW */
#define SENSORS_LM80_TEMP_HOT_MAX 53 /* RW */
#define SENSORS_LM80_TEMP_OS_HYST 54 /* RW */
#define SENSORS_LM80_TEMP_OS_MAX 55 /* RW */
#define SENSORS_LM80_FAN1_DIV 71 /* RW */
#define SENSORS_LM80_FAN2_DIV 72 /* RW */
#define SENSORS_LM80_ALARMS 81 /* R */


/* Winbond W83781D chips */

#define SENSORS_W83781D_PREFIX "w83781d"

#define SENSORS_W83781D_IN0 1 /* R */
#define SENSORS_W83781D_IN1 2 /* R */
#define SENSORS_W83781D_IN2 3 /* R */
#define SENSORS_W83781D_IN3 4 /* R */
#define SENSORS_W83781D_IN4 5 /* R */
#define SENSORS_W83781D_IN5 6 /* R */
#define SENSORS_W83781D_IN6 7 /* R */
#define SENSORS_W83781D_IN0_MIN 11 /* RW */
#define SENSORS_W83781D_IN1_MIN 12 /* RW */
#define SENSORS_W83781D_IN2_MIN 13 /* RW */
#define SENSORS_W83781D_IN3_MIN 14 /* RW */
#define SENSORS_W83781D_IN4_MIN 15 /* RW */
#define SENSORS_W83781D_IN5_MIN 16 /* RW */
#define SENSORS_W83781D_IN6_MIN 17 /* RW */
#define SENSORS_W83781D_IN0_MAX 21 /* RW */
#define SENSORS_W83781D_IN1_MAX 22 /* RW */
#define SENSORS_W83781D_IN2_MAX 23 /* RW */
#define SENSORS_W83781D_IN3_MAX 24 /* RW */
#define SENSORS_W83781D_IN4_MAX 25 /* RW */
#define SENSORS_W83781D_IN5_MAX 26 /* RW */
#define SENSORS_W83781D_IN6_MAX 27 /* RW */
#define SENSORS_W83781D_FAN1 31 /* R */
#define SENSORS_W83781D_FAN2 32 /* R */
#define SENSORS_W83781D_FAN3 33 /* R */
#define SENSORS_W83781D_FAN1_MIN 41 /* RW */
#define SENSORS_W83781D_FAN2_MIN 42 /* RW */
#define SENSORS_W83781D_FAN3_MIN 43 /* RW */
#define SENSORS_W83781D_TEMP1 51 /* R */
#define SENSORS_W83781D_TEMP1_HYST 52 /* RW */
#define SENSORS_W83781D_TEMP1_OVER 53 /* RW */
#define SENSORS_W83781D_TEMP2 54 /* R */
#define SENSORS_W83781D_TEMP2_HYST 55 /* RW */
#define SENSORS_W83781D_TEMP2_OVER 56 /* RW */
#define SENSORS_W83781D_TEMP3 57 /* R */
#define SENSORS_W83781D_TEMP3_HYST 58 /* RW */
#define SENSORS_W83781D_TEMP3_OVER 59 /* RW */
#define SENSORS_W83781D_VID 61 /* R */
#define SENSORS_W83781D_FAN1_DIV 71 /* RW */
#define SENSORS_W83781D_FAN2_DIV 72 /* RW */
#define SENSORS_W83781D_FAN3_DIV 73 /* R (yes, really! */
#define SENSORS_W83781D_ALARMS 81 /* R */
#define SENSORS_W83781D_BEEP_ENABLE 82 /* RW */
#define SENSORS_W83781D_BEEPS 83 /* RW */
#define SENSORS_W83781D_SENS1 91 /* RW */
#define SENSORS_W83781D_SENS2 92 /* RW */
#define SENSORS_W83781D_SENS3 93 /* RW */


/* Winbond W83782D chips */
/* Cheat on 627HF for now - no separate #defines */
/* Cheat on 127F for now - no separate #defines */

#define SENSORS_W83782D_PREFIX "w83782d"
#define SENSORS_W83627HF_PREFIX "w83627hf"
#define SENSORS_AS99127F_PREFIX "as99127f"

#define SENSORS_W83782D_IN0 1 /* R */
#define SENSORS_W83782D_IN1 2 /* R */
#define SENSORS_W83782D_IN2 3 /* R */
#define SENSORS_W83782D_IN3 4 /* R */
#define SENSORS_W83782D_IN4 5 /* R */
#define SENSORS_W83782D_IN5 6 /* R */
#define SENSORS_W83782D_IN6 7 /* R */
#define SENSORS_W83782D_IN7 8 /* R */
#define SENSORS_W83782D_IN8 9 /* R */
#define SENSORS_W83782D_IN0_MIN 11 /* RW */
#define SENSORS_W83782D_IN1_MIN 12 /* RW */
#define SENSORS_W83782D_IN2_MIN 13 /* RW */
#define SENSORS_W83782D_IN3_MIN 14 /* RW */
#define SENSORS_W83782D_IN4_MIN 15 /* RW */
#define SENSORS_W83782D_IN5_MIN 16 /* RW */
#define SENSORS_W83782D_IN6_MIN 17 /* RW */
#define SENSORS_W83782D_IN7_MIN 18 /* RW */
#define SENSORS_W83782D_IN8_MIN 19 /* RW */
#define SENSORS_W83782D_IN0_MAX 21 /* RW */
#define SENSORS_W83782D_IN1_MAX 22 /* RW */
#define SENSORS_W83782D_IN2_MAX 23 /* RW */
#define SENSORS_W83782D_IN3_MAX 24 /* RW */
#define SENSORS_W83782D_IN4_MAX 25 /* RW */
#define SENSORS_W83782D_IN5_MAX 26 /* RW */
#define SENSORS_W83782D_IN6_MAX 27 /* RW */
#define SENSORS_W83782D_IN7_MAX 28 /* RW */
#define SENSORS_W83782D_IN8_MAX 29 /* RW */
#define SENSORS_W83782D_FAN1 31 /* R */
#define SENSORS_W83782D_FAN2 32 /* R */
#define SENSORS_W83782D_FAN3 33 /* R */
#define SENSORS_W83782D_FAN1_MIN 41 /* RW */
#define SENSORS_W83782D_FAN2_MIN 42 /* RW */
#define SENSORS_W83782D_FAN3_MIN 43 /* RW */
#define SENSORS_W83782D_TEMP1 51 /* R */
#define SENSORS_W83782D_TEMP1_HYST 52 /* RW */
#define SENSORS_W83782D_TEMP1_OVER 53 /* RW */
#define SENSORS_W83782D_TEMP2 54 /* R */
#define SENSORS_W83782D_TEMP2_HYST 55 /* RW */
#define SENSORS_W83782D_TEMP2_OVER 56 /* RW */
#define SENSORS_W83782D_TEMP3 57 /* R */
#define SENSORS_W83782D_TEMP3_HYST 58 /* RW */
#define SENSORS_W83782D_TEMP3_OVER 59 /* RW */
#define SENSORS_W83782D_VID 61 /* R */
#define SENSORS_W83782D_FAN1_DIV 71 /* RW */
#define SENSORS_W83782D_FAN2_DIV 72 /* RW */
#define SENSORS_W83782D_ALARMS 81 /* R */
#define SENSORS_W83782D_BEEP_ENABLE 82 /* RW */
#define SENSORS_W83782D_BEEPS 83 /* RW */
#define SENSORS_W83782D_SENS1 91 /* RW */
#define SENSORS_W83782D_SENS2 92 /* RW */
#define SENSORS_W83782D_SENS3 93 /* RW */


/* Winbond W83783S chips */

#define SENSORS_W83783S_PREFIX "w83783s"

#define SENSORS_W83783S_IN0 1 /* R */
#define SENSORS_W83783S_IN1 2 /* R */
#define SENSORS_W83783S_IN2 3 /* R */
#define SENSORS_W83783S_IN3 4 /* R */
#define SENSORS_W83783S_IN4 5 /* R */
#define SENSORS_W83783S_IN5 6 /* R */
#define SENSORS_W83783S_IN6 7 /* R */
#define SENSORS_W83783S_IN0_MIN 11 /* RW */
#define SENSORS_W83783S_IN1_MIN 12 /* RW */
#define SENSORS_W83783S_IN2_MIN 13 /* RW */
#define SENSORS_W83783S_IN3_MIN 14 /* RW */
#define SENSORS_W83783S_IN4_MIN 15 /* RW */
#define SENSORS_W83783S_IN5_MIN 16 /* RW */
#define SENSORS_W83783S_IN6_MIN 17 /* RW */
#define SENSORS_W83783S_IN0_MAX 21 /* RW */
#define SENSORS_W83783S_IN1_MAX 22 /* RW */
#define SENSORS_W83783S_IN2_MAX 23 /* RW */
#define SENSORS_W83783S_IN3_MAX 24 /* RW */
#define SENSORS_W83783S_IN4_MAX 25 /* RW */
#define SENSORS_W83783S_IN5_MAX 26 /* RW */
#define SENSORS_W83783S_IN6_MAX 27 /* RW */
#define SENSORS_W83783S_FAN1 31 /* R */
#define SENSORS_W83783S_FAN2 32 /* R */
#define SENSORS_W83783S_FAN3 33 /* R */
#define SENSORS_W83783S_FAN1_MIN 41 /* RW */
#define SENSORS_W83783S_FAN2_MIN 42 /* RW */
#define SENSORS_W83783S_FAN3_MIN 43 /* RW */
#define SENSORS_W83783S_TEMP1 51 /* R */
#define SENSORS_W83783S_TEMP1_HYST 52 /* RW */
#define SENSORS_W83783S_TEMP1_OVER 53 /* RW */
#define SENSORS_W83783S_TEMP2 54 /* R */
#define SENSORS_W83783S_TEMP2_HYST 55 /* RW */
#define SENSORS_W83783S_TEMP2_OVER 56 /* RW */
#define SENSORS_W83783S_VID 61 /* R */
#define SENSORS_W83783S_FAN1_DIV 71 /* RW */
#define SENSORS_W83783S_FAN2_DIV 72 /* RW */
#define SENSORS_W83783S_FAN3_DIV 73 /* R (yes, really! */
#define SENSORS_W83783S_ALARMS 81 /* R */
#define SENSORS_W83783S_BEEP_ENABLE 82 /* RW */
#define SENSORS_W83783S_BEEPS 83 /* RW */
#define SENSORS_W83783S_SENS1 91 /* RW */
#define SENSORS_W83783S_SENS2 92 /* RW */


/* Analog Devices ADM9240 chips */

#define SENSORS_ADM9240_PREFIX "adm9240"
/* Cheat on LM81 for now - no separate #defines */
#define SENSORS_LM81_PREFIX "lm81"

#define SENSORS_ADM9240_IN0 1 /* R */
#define SENSORS_ADM9240_IN1 2 /* R */
#define SENSORS_ADM9240_IN2 3 /* R */
#define SENSORS_ADM9240_IN3 4 /* R */
#define SENSORS_ADM9240_IN4 5 /* R */
#define SENSORS_ADM9240_IN5 6 /* R */
#define SENSORS_ADM9240_IN0_MIN 11 /* RW */
#define SENSORS_ADM9240_IN1_MIN 12 /* RW */
#define SENSORS_ADM9240_IN2_MIN 13 /* RW */
#define SENSORS_ADM9240_IN3_MIN 14 /* RW */
#define SENSORS_ADM9240_IN4_MIN 15 /* RW */
#define SENSORS_ADM9240_IN5_MIN 16 /* RW */
#define SENSORS_ADM9240_IN0_MAX 21 /* RW */
#define SENSORS_ADM9240_IN1_MAX 22 /* RW */
#define SENSORS_ADM9240_IN2_MAX 23 /* RW */
#define SENSORS_ADM9240_IN3_MAX 24 /* RW */
#define SENSORS_ADM9240_IN4_MAX 25 /* RW */
#define SENSORS_ADM9240_IN5_MAX 26 /* RW */
#define SENSORS_ADM9240_FAN1 31 /* R */
#define SENSORS_ADM9240_FAN2 32 /* R */
#define SENSORS_ADM9240_FAN1_MIN 41 /* RW */
#define SENSORS_ADM9240_FAN2_MIN 42 /* RW */
#define SENSORS_ADM9240_TEMP 51 /* R */
#define SENSORS_ADM9240_TEMP_HYST 52 /* RW */
#define SENSORS_ADM9240_TEMP_OVER 53 /* RW */
#define SENSORS_ADM9240_VID 61 /* R */
#define SENSORS_ADM9240_FAN1_DIV 71 /* RW */
#define SENSORS_ADM9240_FAN2_DIV 72 /* RW */
#define SENSORS_ADM9240_ALARMS 81 /* R */
#define SENSORS_ADM9240_ANALOG_OUT 82 /* RW */

/* Analog Devices ADM9240 chips */

#define SENSORS_DS1780_PREFIX "ds1780"

#define SENSORS_DS1780_IN0 1 /* R */
#define SENSORS_DS1780_IN1 2 /* R */
#define SENSORS_DS1780_IN2 3 /* R */
#define SENSORS_DS1780_IN3 4 /* R */
#define SENSORS_DS1780_IN4 5 /* R */
#define SENSORS_DS1780_IN5 6 /* R */
#define SENSORS_DS1780_IN0_MIN 11 /* RW */
#define SENSORS_DS1780_IN1_MIN 12 /* RW */
#define SENSORS_DS1780_IN2_MIN 13 /* RW */
#define SENSORS_DS1780_IN3_MIN 14 /* RW */
#define SENSORS_DS1780_IN4_MIN 15 /* RW */
#define SENSORS_DS1780_IN5_MIN 16 /* RW */
#define SENSORS_DS1780_IN0_MAX 21 /* RW */
#define SENSORS_DS1780_IN1_MAX 22 /* RW */
#define SENSORS_DS1780_IN2_MAX 23 /* RW */
#define SENSORS_DS1780_IN3_MAX 24 /* RW */
#define SENSORS_DS1780_IN4_MAX 25 /* RW */
#define SENSORS_DS1780_IN5_MAX 26 /* RW */
#define SENSORS_DS1780_FAN1 31 /* R */
#define SENSORS_DS1780_FAN2 32 /* R */
#define SENSORS_DS1780_FAN1_MIN 41 /* RW */
#define SENSORS_DS1780_FAN2_MIN 42 /* RW */
#define SENSORS_DS1780_TEMP 51 /* R */
#define SENSORS_DS1780_TEMP_HYST 52 /* RW */
#define SENSORS_DS1780_TEMP_OVER 53 /* RW */
#define SENSORS_DS1780_VID 61 /* R */
#define SENSORS_DS1780_FAN1_DIV 71 /* RW */
#define SENSORS_DS1780_FAN2_DIV 72 /* RW */
#define SENSORS_DS1780_ALARMS 81 /* R */
#define SENSORS_DS1780_ANALOG_OUT 82 /* RW */

/* SiS southbridge with integrated lm78 */

#define SENSORS_SIS5595_PREFIX "sis5595"

#define SENSORS_SIS5595_IN0 1 /* R */
#define SENSORS_SIS5595_IN1 2 /* R */
#define SENSORS_SIS5595_IN2 3 /* R */
#define SENSORS_SIS5595_IN3 4 /* R */
#define SENSORS_SIS5595_IN0_MIN 11 /* RW */
#define SENSORS_SIS5595_IN1_MIN 12 /* RW */
#define SENSORS_SIS5595_IN2_MIN 13 /* RW */
#define SENSORS_SIS5595_IN3_MIN 14 /* RW */
#define SENSORS_SIS5595_IN0_MAX 21 /* RW */
#define SENSORS_SIS5595_IN1_MAX 22 /* RW */
#define SENSORS_SIS5595_IN2_MAX 23 /* RW */
#define SENSORS_SIS5595_IN3_MAX 24 /* RW */
#define SENSORS_SIS5595_FAN1 31 /* R */
#define SENSORS_SIS5595_FAN2 32 /* R */
#define SENSORS_SIS5595_FAN1_MIN 41 /* RW */
#define SENSORS_SIS5595_FAN2_MIN 42 /* RW */
#define SENSORS_SIS5595_TEMP 51 /* R */
#define SENSORS_SIS5595_TEMP_HYST 52 /* RW */
#define SENSORS_SIS5595_TEMP_OVER 53 /* RW */
#define SENSORS_SIS5595_FAN1_DIV 71 /* RW */
#define SENSORS_SIS5595_FAN2_DIV 72 /* RW */
#define SENSORS_SIS5595_ALARMS 81 /* R */


/* HP MaxiLife chips */

#define SENSORS_MAXI_CG_PREFIX "maxilife-cg"
#define SENSORS_MAXI_CO_PREFIX "maxilife-co"
#define SENSORS_MAXI_AS_PREFIX "maxilife-as"

#define SENSORS_MAXI_CG_FAN1 1 /* R */
#define SENSORS_MAXI_CG_FAN2 2 /* R */
#define SENSORS_MAXI_CG_FAN3 3 /* R */
#define SENSORS_MAXI_CG_FAN1_MIN 11 /* RW */
#define SENSORS_MAXI_CG_FAN2_MIN 12 /* RW */
#define SENSORS_MAXI_CG_FAN3_MIN 13 /* RW */
#define SENSORS_MAXI_CG_FAN1_DIV 14 /* R */
#define SENSORS_MAXI_CG_FAN2_DIV 15 /* R */
#define SENSORS_MAXI_CG_FAN3_DIV 16 /* R */
#define SENSORS_MAXI_CG_TEMP1 21 /* R */
#define SENSORS_MAXI_CG_TEMP2 22 /* R */
#define SENSORS_MAXI_CG_TEMP3 23 /* R */
#define SENSORS_MAXI_CG_TEMP4 24 /* R */
#define SENSORS_MAXI_CG_TEMP5 25 /* R */
#define SENSORS_MAXI_CG_TEMP1_MAX 31 /* R */
#define SENSORS_MAXI_CG_TEMP2_MAX 32 /* R */
#define SENSORS_MAXI_CG_TEMP3_MAX 33 /* R */
#define SENSORS_MAXI_CG_TEMP4_MAX 34 /* R */
#define SENSORS_MAXI_CG_TEMP5_MAX 35 /* R */
#define SENSORS_MAXI_CG_TEMP1_HYST 41 /* R */
#define SENSORS_MAXI_CG_TEMP2_HYST 42 /* R */
#define SENSORS_MAXI_CG_TEMP3_HYST 43 /* R */
#define SENSORS_MAXI_CG_TEMP4_HYST 44 /* R */
#define SENSORS_MAXI_CG_TEMP5_HYST 45 /* R */
#define SENSORS_MAXI_CG_PLL 51 /* R */
#define SENSORS_MAXI_CG_PLL_MIN 52 /* RW */
#define SENSORS_MAXI_CG_PLL_MAX 53 /* RW */
#define SENSORS_MAXI_CG_VID1 61 /* R */
#define SENSORS_MAXI_CG_VID2 62 /* R */
#define SENSORS_MAXI_CG_VID3 63 /* R */
#define SENSORS_MAXI_CG_VID4 64 /* R */
#define SENSORS_MAXI_CG_VID1_MIN 71 /* RW */
#define SENSORS_MAXI_CG_VID2_MIN 72 /* RW */
#define SENSORS_MAXI_CG_VID3_MIN 73 /* RW */
#define SENSORS_MAXI_CG_VID4_MIN 74 /* RW */
#define SENSORS_MAXI_CG_VID1_MAX 81 /* RW */
#define SENSORS_MAXI_CG_VID2_MAX 82 /* RW */
#define SENSORS_MAXI_CG_VID3_MAX 83 /* RW */
#define SENSORS_MAXI_CG_VID4_MAX 84 /* RW */
#define SENSORS_MAXI_CG_ALARMS 91 /* R */

#define SENSORS_MAXI_CO_FAN1 1 /* R */
#define SENSORS_MAXI_CO_FAN2 2 /* R */
#define SENSORS_MAXI_CO_FAN3 3 /* R */
#define SENSORS_MAXI_CO_FAN1_MIN 11 /* RW */
#define SENSORS_MAXI_CO_FAN2_MIN 12 /* RW */
#define SENSORS_MAXI_CO_FAN3_MIN 13 /* RW */
#define SENSORS_MAXI_CO_FAN1_DIV 14 /* R */
#define SENSORS_MAXI_CO_FAN2_DIV 15 /* R */
#define SENSORS_MAXI_CO_FAN3_DIV 16 /* R */
#define SENSORS_MAXI_CO_TEMP1 21 /* R */
#define SENSORS_MAXI_CO_TEMP2 22 /* R */
#define SENSORS_MAXI_CO_TEMP3 23 /* R */
#define SENSORS_MAXI_CO_TEMP4 24 /* R */
#define SENSORS_MAXI_CO_TEMP5 25 /* R */
#define SENSORS_MAXI_CO_TEMP1_MAX 31 /* R */
#define SENSORS_MAXI_CO_TEMP2_MAX 32 /* R */
#define SENSORS_MAXI_CO_TEMP3_MAX 33 /* R */
#define SENSORS_MAXI_CO_TEMP4_MAX 34 /* R */
#define SENSORS_MAXI_CO_TEMP5_MAX 35 /* R */
#define SENSORS_MAXI_CO_TEMP1_HYST 41 /* R */
#define SENSORS_MAXI_CO_TEMP2_HYST 42 /* R */
#define SENSORS_MAXI_CO_TEMP3_HYST 43 /* R */
#define SENSORS_MAXI_CO_TEMP4_HYST 44 /* R */
#define SENSORS_MAXI_CO_TEMP5_HYST 45 /* R */
#define SENSORS_MAXI_CO_PLL 51 /* R */
#define SENSORS_MAXI_CO_PLL_MIN 52 /* RW */
#define SENSORS_MAXI_CO_PLL_MAX 53 /* RW */
#define SENSORS_MAXI_CO_VID1 61 /* R */
#define SENSORS_MAXI_CO_VID2 62 /* R */
#define SENSORS_MAXI_CO_VID3 63 /* R */
#define SENSORS_MAXI_CO_VID4 64 /* R */
#define SENSORS_MAXI_CO_VID1_MIN 71 /* RW */
#define SENSORS_MAXI_CO_VID2_MIN 72 /* RW */
#define SENSORS_MAXI_CO_VID3_MIN 73 /* RW */
#define SENSORS_MAXI_CO_VID4_MIN 74 /* RW */
#define SENSORS_MAXI_CO_VID1_MAX 81 /* RW */
#define SENSORS_MAXI_CO_VID2_MAX 82 /* RW */
#define SENSORS_MAXI_CO_VID3_MAX 83 /* RW */
#define SENSORS_MAXI_CO_VID4_MAX 84 /* RW */
#define SENSORS_MAXI_CO_ALARMS 91 /* R */

#define SENSORS_MAXI_AS_FAN1 1 /* R */
#define SENSORS_MAXI_AS_FAN2 2 /* R */
#define SENSORS_MAXI_AS_FAN3 3 /* R */
#define SENSORS_MAXI_AS_FAN1_MIN 11 /* RW */
#define SENSORS_MAXI_AS_FAN2_MIN 12 /* RW */
#define SENSORS_MAXI_AS_FAN3_MIN 13 /* RW */
#define SENSORS_MAXI_AS_FAN1_DIV 14 /* R */
#define SENSORS_MAXI_AS_FAN2_DIV 15 /* R */
#define SENSORS_MAXI_AS_FAN3_DIV 16 /* R */
#define SENSORS_MAXI_AS_TEMP1 21 /* R */
#define SENSORS_MAXI_AS_TEMP2 22 /* R */
#define SENSORS_MAXI_AS_TEMP3 23 /* R */
#define SENSORS_MAXI_AS_TEMP4 24 /* R */
#define SENSORS_MAXI_AS_TEMP5 25 /* R */
#define SENSORS_MAXI_AS_TEMP1_MAX 31 /* R */
#define SENSORS_MAXI_AS_TEMP2_MAX 32 /* R */
#define SENSORS_MAXI_AS_TEMP3_MAX 33 /* R */
#define SENSORS_MAXI_AS_TEMP4_MAX 34 /* R */
#define SENSORS_MAXI_AS_TEMP5_MAX 35 /* R */
#define SENSORS_MAXI_AS_TEMP1_HYST 41 /* R */
#define SENSORS_MAXI_AS_TEMP2_HYST 42 /* R */
#define SENSORS_MAXI_AS_TEMP3_HYST 43 /* R */
#define SENSORS_MAXI_AS_TEMP4_HYST 44 /* R */
#define SENSORS_MAXI_AS_TEMP5_HYST 45 /* R */
#define SENSORS_MAXI_AS_PLL 51 /* R */
#define SENSORS_MAXI_AS_PLL_MIN 52 /* RW */
#define SENSORS_MAXI_AS_PLL_MAX 53 /* RW */
#define SENSORS_MAXI_AS_VID1 61 /* R */
#define SENSORS_MAXI_AS_VID2 62 /* R */
#define SENSORS_MAXI_AS_VID3 63 /* R */
#define SENSORS_MAXI_AS_VID4 64 /* R */
#define SENSORS_MAXI_AS_VID1_MIN 71 /* RW */
#define SENSORS_MAXI_AS_VID2_MIN 72 /* RW */
#define SENSORS_MAXI_AS_VID3_MIN 73 /* RW */
#define SENSORS_MAXI_AS_VID4_MIN 74 /* RW */
#define SENSORS_MAXI_AS_VID1_MAX 81 /* RW */
#define SENSORS_MAXI_AS_VID2_MAX 82 /* RW */
#define SENSORS_MAXI_AS_VID3_MAX 83 /* RW */
#define SENSORS_MAXI_AS_VID4_MAX 84 /* RW */
#define SENSORS_MAXI_AS_ALARMS 91 /* R */

/* THMC50/ADM1022 chips */

#define SENSORS_THMC50_PREFIX "thmc50"
/* Cheat on LM84,GL523,THMC10 for now - no separate #defines */
#define SENSORS_ADM1022_PREFIX "adm1022"

#define SENSORS_THMC50_TEMP 51 /* R */
#define SENSORS_THMC50_TEMP_HYST 52 /* RW */
#define SENSORS_THMC50_TEMP_OVER 53 /* RW */
#define SENSORS_THMC50_REMOTE_TEMP 54 /* R */
#define SENSORS_THMC50_REMOTE_TEMP_HYST 55 /* RW */
#define SENSORS_THMC50_REMOTE_TEMP_OVER 56 /* RW */
#define SENSORS_THMC50_ANALOG_OUT 71 /* RW */
#define SENSORS_THMC50_INTER 81 /* R */
#define SENSORS_THMC50_INTER_MASK 82 /* RW */
#define SENSORS_THMC50_DIE_CODE 90 /* R */

#endif /* def LIB_SENSORS_CHIPS_H */
