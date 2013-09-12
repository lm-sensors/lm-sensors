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
#define LIB_SENSORS_CHIPS_H

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
/* Cheat on LM84,GL523,THMC10,1023 for now - no separate #defines */
#define SENSORS_ADM1023_PREFIX "adm1023"
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

/* ADM1029 chips */

#define SENSORS_ADM1029_PREFIX "adm1029"

#define SENSORS_ADM1029_TEMP1		51
#define SENSORS_ADM1029_TEMP1_MAX	52
#define SENSORS_ADM1029_TEMP1_MIN	53

#define SENSORS_ADM1029_TEMP2		61
#define SENSORS_ADM1029_TEMP2_MAX	62
#define SENSORS_ADM1029_TEMP2_MIN	63

#define SENSORS_ADM1029_TEMP3		71
#define SENSORS_ADM1029_TEMP3_MAX	72
#define SENSORS_ADM1029_TEMP3_MIN	73

#define SENSORS_ADM1029_FAN1		81
#define SENSORS_ADM1029_FAN1_MIN	82
#define SENSORS_ADM1029_FAN1_DIV	83

#define SENSORS_ADM1029_FAN2		91
#define SENSORS_ADM1029_FAN2_MIN	92
#define SENSORS_ADM1029_FAN2_DIV	93


/* ADM1030 and ADM1031 chips */

#define SENSORS_ADM1030_PREFIX "adm1030"
#define SENSORS_ADM1031_PREFIX "adm1031"

#define SENSORS_ADM1031_TEMP1      51 /* R */
#define SENSORS_ADM1031_TEMP1_MIN  52 /* RW */
#define SENSORS_ADM1031_TEMP1_MAX  53 /* RW */
#define SENSORS_ADM1031_TEMP1_CRIT 54 /* RW */

#define SENSORS_ADM1031_TEMP2      61 /* R */
#define SENSORS_ADM1031_TEMP2_MIN  62 /* RW */
#define SENSORS_ADM1031_TEMP2_MAX  63 /* RW */
#define SENSORS_ADM1031_TEMP2_CRIT 64 /* RW */

#define SENSORS_ADM1031_TEMP3      71 /* R */
#define SENSORS_ADM1031_TEMP3_MIN  72 /* RW */
#define SENSORS_ADM1031_TEMP3_MAX  73 /* RW */
#define SENSORS_ADM1031_TEMP3_CRIT 74 /* RW */


#define SENSORS_ADM1031_FAN1       80 /* R */
#define SENSORS_ADM1031_FAN1_MIN   81 /* RW */
#define SENSORS_ADM1031_FAN1_DIV   82 /* RW */
#define SENSORS_ADM1031_FAN2       90 /* R */
#define SENSORS_ADM1031_FAN2_MIN   91 /* RW */
#define SENSORS_ADM1031_FAN2_DIV   92 /* RW */

#define SENSORS_ADM1031_ALARMS    100 /* R */


/* MAX1617 chips. */

#define SENSORS_MAX1617_PREFIX "max1617"
#define SENSORS_MC1066_PREFIX "mc1066"

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
#define SENSORS_GL520_VIN4 5 /* R */
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

/* LM83 chips */

#define SENSORS_LM83_PREFIX "lm83"

#define SENSORS_LM83_LOCAL_TEMP 51 /* R */
#define SENSORS_LM83_LOCAL_HIGH 52 /* RW */
#define SENSORS_LM83_REMOTE1_TEMP 54 /* R */
#define SENSORS_LM83_REMOTE1_HIGH 55 /* RW */
#define SENSORS_LM83_REMOTE2_TEMP 57 /* R */
#define SENSORS_LM83_REMOTE2_HIGH 58 /* RW */
#define SENSORS_LM83_REMOTE3_TEMP 60 /* R */
#define SENSORS_LM83_REMOTE3_HIGH 61 /* RW */
#define SENSORS_LM83_TCRIT 80 /* RW */
#define SENSORS_LM83_ALARMS 81 /* R */

/* LM85 chips */

#define SENSORS_LM85_PREFIX "lm85"
#define SENSORS_LM85B_PREFIX "lm85b"
#define SENSORS_LM85C_PREFIX "lm85c"
#define SENSORS_ADM1027_PREFIX "adm1027"
#define SENSORS_ADT7463_PREFIX "adt7463"
#define SENSORS_EMC6D100_PREFIX "emc6d100"
#define SENSORS_EMC6D102_PREFIX "emc6d102"
#define SENSORS_EMC6D103_PREFIX "emc6d103"

#define SENSORS_ADM1027_ALARM_MASK           1  /* RW -- alarm_mask  */
#define SENSORS_ADM1027_FAN1_PPR             2  /* RW -- fan1_ppr  */
#define SENSORS_ADM1027_FAN1_TACH_MODE       3  /* RW -- fan1_tach_mode  */
#define SENSORS_ADM1027_FAN2_PPR             4  /* RW -- fan2_ppr  */
#define SENSORS_ADM1027_FAN2_TACH_MODE       5  /* RW -- fan2_tach_mode  */
#define SENSORS_ADM1027_FAN3_PPR             6  /* RW -- fan3_ppr  */
#define SENSORS_ADM1027_FAN3_TACH_MODE       7  /* RW -- fan3_tach_mode  */
#define SENSORS_ADM1027_FAN4_PPR             8  /* RW -- fan4_ppr  */
#define SENSORS_ADM1027_FAN4_TACH_MODE       9  /* RW -- fan4_tach_mode  */
#define SENSORS_ADM1027_PWM1_SMOOTH         10  /* RW -- pwm1_smooth  */
#define SENSORS_ADM1027_PWM2_SMOOTH         11  /* RW -- pwm2_smooth  */
#define SENSORS_ADM1027_PWM3_SMOOTH         12  /* RW -- pwm3_smooth  */
#define SENSORS_ADM1027_TEMP1_OFFSET        13  /* RW -- temp1_offset  */
#define SENSORS_ADM1027_TEMP2_OFFSET        14  /* RW -- temp2_offset  */
#define SENSORS_ADM1027_TEMP3_OFFSET        15  /* RW -- temp3_offset  */
#define SENSORS_LM85_ALARMS                 16  /* R  -- alarms  */
#define SENSORS_LM85_VID                    17  /* R  -- vid  */
#define SENSORS_LM85_VRM                    18  /* RW -- vrm  */
#define SENSORS_LM85_FAN1                   19  /* R  -- fan1  */
#define SENSORS_LM85_FAN1_MIN               20  /* RW -- fan1_min  */
#define SENSORS_LM85_FAN1_TACH_MODE         21  /* RW -- fan1_tach_mode  */
#define SENSORS_LM85_FAN2                   22  /* R  -- fan2  */
#define SENSORS_LM85_FAN2_MIN               23  /* RW -- fan2_min  */
#define SENSORS_LM85_FAN2_TACH_MODE         24  /* RW -- fan2_tach_mode  */
#define SENSORS_LM85_FAN3                   25  /* R  -- fan3  */
#define SENSORS_LM85_FAN3_MIN               26  /* RW -- fan3_min  */
#define SENSORS_LM85_FAN3_TACH_MODE         27  /* RW -- fan3_tach_mode  */
#define SENSORS_LM85_FAN4                   28  /* R  -- fan4  */
#define SENSORS_LM85_FAN4_MIN               29  /* RW -- fan4_min  */
#define SENSORS_LM85_IN0                    30  /* R  -- in0  */
#define SENSORS_LM85_IN0_MAX                31  /* RW -- in0_max  */
#define SENSORS_LM85_IN0_MIN                32  /* RW -- in0_min  */
#define SENSORS_LM85_IN1                    33  /* R  -- in1  */
#define SENSORS_LM85_IN1_MAX                34  /* RW -- in1_max  */
#define SENSORS_LM85_IN1_MIN                35  /* RW -- in1_min  */
#define SENSORS_LM85_IN2                    36  /* R  -- in2  */
#define SENSORS_LM85_IN2_MAX                37  /* RW -- in2_max  */
#define SENSORS_LM85_IN2_MIN                38  /* RW -- in2_min  */
#define SENSORS_LM85_IN3                    39  /* R  -- in3  */
#define SENSORS_LM85_IN3_MAX                40  /* RW -- in3_max  */
#define SENSORS_LM85_IN3_MIN                41  /* RW -- in3_min  */
#define SENSORS_LM85_IN4                    42  /* R  -- in4  */
#define SENSORS_LM85_IN4_MAX                43  /* RW -- in4_max  */
#define SENSORS_LM85_IN4_MIN                44  /* RW -- in4_min  */
#define SENSORS_LM85_IN5                    45  /* R  -- in5  */
#define SENSORS_LM85_IN5_MAX                46  /* RW -- in5_max  */
#define SENSORS_LM85_IN5_MIN                47  /* RW -- in5_min  */
#define SENSORS_LM85_IN6                    48  /* R  -- in6  */
#define SENSORS_LM85_IN6_MAX                49  /* RW -- in6_max  */
#define SENSORS_LM85_IN6_MIN                50  /* RW -- in6_min  */
#define SENSORS_LM85_IN7                    51  /* R  -- in7  */
#define SENSORS_LM85_IN7_MAX                52  /* RW -- in7_max  */
#define SENSORS_LM85_IN7_MIN                53  /* RW -- in7_min  */
#define SENSORS_LM85_PWM1                   54  /* RW -- pwm1  */
#define SENSORS_LM85_PWM1_FREQ              55  /* RW -- pwm1_freq  */
#define SENSORS_LM85_PWM1_INVERT            56  /* RW -- pwm1_invert  */
#define SENSORS_LM85_PWM1_MIN               57  /* RW -- pwm1_min  */
#define SENSORS_LM85_PWM1_MIN_CTL           58  /* RW -- pwm1_min_ctl  */
#define SENSORS_LM85_PWM1_SPINUP            59  /* RW -- pwm1_spinup  */
#define SENSORS_LM85_PWM1_SPINUP_CTL        60  /* RW -- pwm1_spinup_ctl  */
#define SENSORS_LM85_PWM1_ZONE              61  /* RW -- pwm1_zone  */
#define SENSORS_LM85_PWM2                   62  /* RW -- pwm2  */
#define SENSORS_LM85_PWM2_FREQ              63  /* RW -- pwm2_freq  */
#define SENSORS_LM85_PWM2_INVERT            64  /* RW -- pwm2_invert  */
#define SENSORS_LM85_PWM2_MIN               65  /* RW -- pwm2_min  */
#define SENSORS_LM85_PWM2_MIN_CTL           66  /* RW -- pwm2_min_ctl  */
#define SENSORS_LM85_PWM2_SPINUP            67  /* RW -- pwm2_spinup  */
#define SENSORS_LM85_PWM2_SPINUP_CTL        68  /* RW -- pwm2_spinup_ctl  */
#define SENSORS_LM85_PWM2_ZONE              69  /* RW -- pwm2_zone  */
#define SENSORS_LM85_PWM3                   70  /* RW -- pwm3  */
#define SENSORS_LM85_PWM3_FREQ              71  /* RW -- pwm3_freq  */
#define SENSORS_LM85_PWM3_INVERT            72  /* RW -- pwm3_invert  */
#define SENSORS_LM85_PWM3_MIN               73  /* RW -- pwm3_min  */
#define SENSORS_LM85_PWM3_MIN_CTL           74  /* RW -- pwm3_min_ctl  */
#define SENSORS_LM85_PWM3_SPINUP            75  /* RW -- pwm3_spinup  */
#define SENSORS_LM85_PWM3_SPINUP_CTL        76  /* RW -- pwm3_spinup_ctl  */
#define SENSORS_LM85_PWM3_ZONE              77  /* RW -- pwm3_zone  */
#define SENSORS_LM85_TEMP1                  78  /* R  -- temp1  */
#define SENSORS_LM85_TEMP1_MAX              79  /* RW -- temp1_max  */
#define SENSORS_LM85_TEMP1_MIN              80  /* RW -- temp1_min  */
#define SENSORS_LM85_TEMP2                  81  /* R  -- temp2  */
#define SENSORS_LM85_TEMP2_MAX              82  /* RW -- temp2_max  */
#define SENSORS_LM85_TEMP2_MIN              83  /* RW -- temp2_min  */
#define SENSORS_LM85_TEMP3                  84  /* R  -- temp3  */
#define SENSORS_LM85_TEMP3_MAX              85  /* RW -- temp3_max  */
#define SENSORS_LM85_TEMP3_MIN              86  /* RW -- temp3_min  */
#define SENSORS_LM85_ZONE1_CRITICAL         87  /* RW -- zone1_critical  */
#define SENSORS_LM85_ZONE1_HYST             88  /* RW -- zone1_hyst  */
#define SENSORS_LM85_ZONE1_LIMIT            89  /* RW -- zone1_limit  */
#define SENSORS_LM85_ZONE1_RANGE            90  /* RW -- zone1_range  */
#define SENSORS_LM85_ZONE1_SMOOTH           91  /* RW -- zone1_smooth  */
#define SENSORS_LM85_ZONE2_CRITICAL         92  /* RW -- zone2_critical  */
#define SENSORS_LM85_ZONE2_HYST             93  /* RW -- zone2_hyst  */
#define SENSORS_LM85_ZONE2_LIMIT            94  /* RW -- zone2_limit  */
#define SENSORS_LM85_ZONE2_RANGE            95  /* RW -- zone2_range  */
#define SENSORS_LM85_ZONE2_SMOOTH           96  /* RW -- zone2_smooth  */
#define SENSORS_LM85_ZONE3_CRITICAL         97  /* RW -- zone3_critical  */
#define SENSORS_LM85_ZONE3_HYST             98  /* RW -- zone3_hyst  */
#define SENSORS_LM85_ZONE3_LIMIT            99  /* RW -- zone3_limit  */
#define SENSORS_LM85_ZONE3_RANGE           100  /* RW -- zone3_range  */
#define SENSORS_LM85_ZONE3_SMOOTH          101  /* RW -- zone3_smooth  */

/* LM86/LM89/LM90/LM99/ADM1032/MAX6657/MAX6680/ADT7461 chips */

#define SENSORS_LM90_PREFIX "lm90"
#define SENSORS_ADM1032_PREFIX "adm1032"
#define SENSORS_LM89_PREFIX "lm89"
#define SENSORS_LM99_PREFIX "lm99"
#define SENSORS_LM86_PREFIX "lm86"
#define SENSORS_MAX6657_PREFIX "max6657"
#define SENSORS_MAX6658_PREFIX "max6658"
#define SENSORS_MAX6659_PREFIX "max6659"
#define SENSORS_ADT7461_PREFIX "adt7461"
#define SENSORS_MAX6680_PREFIX "max6680"
#define SENSORS_MAX6681_PREFIX "max6681"
#define SENSORS_MAX6646_PREFIX "max6646"
#define SENSORS_MAX6647_PREFIX "max6647"
#define SENSORS_MAX6649_PREFIX "max6649"

#define SENSORS_LM90_LOCAL_TEMP 51 /* R */
#define SENSORS_LM90_LOCAL_HIGH 52 /* RW */
#define SENSORS_LM90_LOCAL_LOW 53 /* RW */
#define SENSORS_LM90_LOCAL_TCRIT 54 /* RW */
#define SENSORS_LM90_REMOTE_TEMP 57 /* R */
#define SENSORS_LM90_REMOTE_HIGH 58 /* RW */
#define SENSORS_LM90_REMOTE_LOW 59 /* RW */
#define SENSORS_LM90_REMOTE_TCRIT 60 /* RW */
#define SENSORS_LM90_LOCAL_TCRIT_HYST 79 /* RW */
#define SENSORS_LM90_REMOTE_TCRIT_HYST 80 /* R, see driver source */
#define SENSORS_LM90_ALARMS 81 /* R */

/* LM63 chips */

#define SENSORS_LM63_PREFIX "lm63"

#define SENSORS_LM63_LOCAL_TEMP			51 /* R  */
#define SENSORS_LM63_LOCAL_HIGH			52 /* RW */
#define SENSORS_LM63_REMOTE_TEMP		57 /* R  */
#define SENSORS_LM63_REMOTE_HIGH		58 /* RW */
#define SENSORS_LM63_REMOTE_LOW			59 /* RW */
#define SENSORS_LM63_REMOTE_TCRIT		60 /* R  */
#define SENSORS_LM63_REMOTE_TCRIT_HYST		80 /* RW */
#define SENSORS_LM63_ALARMS			81 /* R  */
#define SENSORS_LM63_FAN			84 /* R  */
#define SENSORS_LM63_FAN_LOW			85 /* RW */
#define SENSORS_LM63_PWM			87 /* RW */
#define SENSORS_LM63_PWM_ENABLE			88 /* RW */

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
#define SENSORS_W83781D_VRM 62 /* RW */
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
#define SENSORS_W83627THF_PREFIX "w83627thf"
#define SENSORS_W83637HF_PREFIX "w83637hf"
#define SENSORS_W83687THF_PREFIX "w83687thf"

#define SENSORS_W83791D_PREFIX "w83791d"


#define SENSORS_W83791D_IN0 1 /* R */
#define SENSORS_W83791D_IN1 2 /* R */
#define SENSORS_W83791D_IN2 3 /* R */
#define SENSORS_W83791D_IN3 4 /* R */
#define SENSORS_W83791D_IN4 5 /* R */
#define SENSORS_W83791D_IN5 6 /* R */
#define SENSORS_W83791D_IN6 7 /* R */
#define SENSORS_W83791D_IN7 8 /* R */
#define SENSORS_W83791D_IN8 9 /* R */
#define SENSORS_W83791D_IN9 10 /* R */

#define SENSORS_W83791D_IN0_MIN 11 /* RW */
#define SENSORS_W83791D_IN1_MIN 12 /* RW */
#define SENSORS_W83791D_IN2_MIN 13 /* RW */
#define SENSORS_W83791D_IN3_MIN 14 /* RW */
#define SENSORS_W83791D_IN4_MIN 15 /* RW */
#define SENSORS_W83791D_IN5_MIN 16 /* RW */
#define SENSORS_W83791D_IN6_MIN 17 /* RW */
#define SENSORS_W83791D_IN7_MIN 18 /* RW */
#define SENSORS_W83791D_IN8_MIN 19 /* RW */
#define SENSORS_W83791D_IN9_MIN 20 /* RW */

#define SENSORS_W83791D_IN0_MAX 21 /* RW */
#define SENSORS_W83791D_IN1_MAX 22 /* RW */
#define SENSORS_W83791D_IN2_MAX 23 /* RW */
#define SENSORS_W83791D_IN3_MAX 24 /* RW */
#define SENSORS_W83791D_IN4_MAX 25 /* RW */
#define SENSORS_W83791D_IN5_MAX 26 /* RW */
#define SENSORS_W83791D_IN6_MAX 27 /* RW */
#define SENSORS_W83791D_IN7_MAX 28 /* RW */
#define SENSORS_W83791D_IN8_MAX 29 /* RW */
#define SENSORS_W83791D_IN9_MAX 30 /* RW */

#define SENSORS_W83791D_FAN1 31 /* R */
#define SENSORS_W83791D_FAN2 32 /* R */
#define SENSORS_W83791D_FAN3 33 /* R */
#define SENSORS_W83791D_FAN4 34 /* R */
#define SENSORS_W83791D_FAN5 35 /* R */

#define SENSORS_W83791D_FAN1_MIN 41 /* RW */
#define SENSORS_W83791D_FAN2_MIN 42 /* RW */
#define SENSORS_W83791D_FAN3_MIN 43 /* RW */
#define SENSORS_W83791D_FAN4_MIN 44 /* RW */
#define SENSORS_W83791D_FAN5_MIN 45 /* RW */

#define SENSORS_W83791D_TEMP1 51 /* R */
#define SENSORS_W83791D_TEMP1_HYST 52 /* RW */
#define SENSORS_W83791D_TEMP1_OVER 53 /* RW */
#define SENSORS_W83791D_TEMP2 54 /* R */
#define SENSORS_W83791D_TEMP2_HYST 55 /* RW */
#define SENSORS_W83791D_TEMP2_OVER 56 /* RW */
#define SENSORS_W83791D_TEMP3 57 /* R */
#define SENSORS_W83791D_TEMP3_HYST 58 /* RW */
#define SENSORS_W83791D_TEMP3_OVER 59 /* RW */
#define SENSORS_W83791D_VID 61 /* R */
#define SENSORS_W83791D_VRM 62 /* RW */

#define SENSORS_W83791D_FAN1_DIV 71 /* RW */
#define SENSORS_W83791D_FAN2_DIV 72 /* RW */
#define SENSORS_W83791D_FAN3_DIV 73 /* R (yes, really! */
#define SENSORS_W83791D_FAN4_DIV 74 /* R (yes, really! */
#define SENSORS_W83791D_FAN5_DIV 75 /* R (yes, really! */

#define SENSORS_W83791D_ALARMS 81 /* R */
#define SENSORS_W83791D_BEEP_ENABLE 82 /* RW */
#define SENSORS_W83791D_BEEPS 83 /* RW */
#define SENSORS_W83791D_SENS1 91 /* RW */
#define SENSORS_W83791D_SENS2 92 /* RW */
#define SENSORS_W83791D_SENS3 93 /* RW */



/* Winbond W83792AD/D chip */
#define SENSORS_W83792D_PREFIX "w83792d"


#define SENSORS_W83792D_IN0 1 /* R */
#define SENSORS_W83792D_IN1 2 /* R */
#define SENSORS_W83792D_IN2 3 /* R */
#define SENSORS_W83792D_IN3 4 /* R */
#define SENSORS_W83792D_IN4 5 /* R */
#define SENSORS_W83792D_IN5 6 /* R */
#define SENSORS_W83792D_IN6 7 /* R */
#define SENSORS_W83792D_IN7 8 /* R */
#define SENSORS_W83792D_IN8 9 /* R */
#define SENSORS_W83792D_IN9 10 /* R */

#define SENSORS_W83792D_IN0_MIN 11 /* RW */
#define SENSORS_W83792D_IN1_MIN 12 /* RW */
#define SENSORS_W83792D_IN2_MIN 13 /* RW */
#define SENSORS_W83792D_IN3_MIN 14 /* RW */
#define SENSORS_W83792D_IN4_MIN 15 /* RW */
#define SENSORS_W83792D_IN5_MIN 16 /* RW */
#define SENSORS_W83792D_IN6_MIN 17 /* RW */
#define SENSORS_W83792D_IN7_MIN 18 /* RW */
#define SENSORS_W83792D_IN8_MIN 19 /* RW */
#define SENSORS_W83792D_IN9_MIN 20 /* RW */

#define SENSORS_W83792D_IN0_MAX 21 /* RW */
#define SENSORS_W83792D_IN1_MAX 22 /* RW */
#define SENSORS_W83792D_IN2_MAX 23 /* RW */
#define SENSORS_W83792D_IN3_MAX 24 /* RW */
#define SENSORS_W83792D_IN4_MAX 25 /* RW */
#define SENSORS_W83792D_IN5_MAX 26 /* RW */
#define SENSORS_W83792D_IN6_MAX 27 /* RW */
#define SENSORS_W83792D_IN7_MAX 28 /* RW */
#define SENSORS_W83792D_IN8_MAX 29 /* RW */
#define SENSORS_W83792D_IN9_MAX 30 /* RW */

#define SENSORS_W83792D_FAN1 31 /* R */
#define SENSORS_W83792D_FAN2 32 /* R */
#define SENSORS_W83792D_FAN3 33 /* R */
#define SENSORS_W83792D_FAN4 34 /* R */
#define SENSORS_W83792D_FAN5 35 /* R */
#define SENSORS_W83792D_FAN6 36 /* R */
#define SENSORS_W83792D_FAN7 37 /* R */

#define SENSORS_W83792D_FAN1_MIN 41 /* RW */
#define SENSORS_W83792D_FAN2_MIN 42 /* RW */
#define SENSORS_W83792D_FAN3_MIN 43 /* RW */
#define SENSORS_W83792D_FAN4_MIN 44 /* RW */
#define SENSORS_W83792D_FAN5_MIN 45 /* RW */
#define SENSORS_W83792D_FAN6_MIN 46 /* RW */
#define SENSORS_W83792D_FAN7_MIN 47 /* RW */

#define SENSORS_W83792D_TEMP1 51      /* R */
#define SENSORS_W83792D_TEMP1_HYST 52 /* RW */
#define SENSORS_W83792D_TEMP1_OVER 53 /* RW */
#define SENSORS_W83792D_TEMP2 54      /* R */
#define SENSORS_W83792D_TEMP2_HYST 55 /* RW */
#define SENSORS_W83792D_TEMP2_OVER 56 /* RW */
#define SENSORS_W83792D_TEMP3 57      /* R */
#define SENSORS_W83792D_TEMP3_HYST 58 /* RW */
#define SENSORS_W83792D_TEMP3_OVER 59 /* RW */
/*#define SENSORS_W83792D_VID 61 */   /* R */
/*#define SENSORS_W83792D_VRM 62 */   /* RW */
#define SENSORS_W83792D_CHASSIS 63    /* R */
#define SENSORS_W83792D_ALARMS 64     /* R */

#define SENSORS_W83792D_FAN1_DIV 71 /* RW */
#define SENSORS_W83792D_FAN2_DIV 72 /* RW */
#define SENSORS_W83792D_FAN3_DIV 73 /* RW */
#define SENSORS_W83792D_FAN4_DIV 74 /* RW */
#define SENSORS_W83792D_FAN5_DIV 75 /* RW */
#define SENSORS_W83792D_FAN6_DIV 76 /* RW */
#define SENSORS_W83792D_FAN7_DIV 77 /* RW */


/* Winbond W83793R chip */
#define SENSORS_W83793_PREFIX "w83793"

#define SENSORS_W83793_IN(n)		(0 + (n))  /* n(0-9) R */
#define SENSORS_W83793_IN_MIN(n)	(20 + (n)) /* n(0-9) RW */
#define SENSORS_W83793_IN_MAX(n)	(40 + (n)) /* n(0-9) RW */
#define SENSORS_W83793_IN_ALARM(n)	(60 + (n)) /* n(0-9) R */

#define SENSORS_W83793_FAN(n)		(80 + (n))  /* n(1-12) R */
#define SENSORS_W83793_FAN_MIN(n)	(100 + (n)) /* n(1-12) RW */
#define SENSORS_W83793_FAN_ALARM(n)	(120 + (n)) /* n(1-12) R */


#define SENSORS_W83793_TEMP(n)		(140 + (n)) /* n(1-6) R */
#define SENSORS_W83793_TEMP_CRIT(n)	(160 + (n)) /* n(1-6) RW */
#define SENSORS_W83793_TEMP_CRIT_HYST(n) (180 + (n)) /* n(1-6) RW */
#define SENSORS_W83793_TEMP_ALARM(n)	(200 + (n)) /* n(1-6) R */

#define SENSORS_W83793_VID0		221 /* R */
#define SENSORS_W83793_VID1		222 /* R */
#define SENSORS_W83793_VRM		230 /* RW */
#define SENSORS_W83793_CHASSIS		240 /* RW */


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
#define SENSORS_W83782D_VRM 62 /* RW */
#define SENSORS_W83782D_FAN1_DIV 71 /* RW */
#define SENSORS_W83782D_FAN2_DIV 72 /* RW */
#define SENSORS_W83782D_FAN3_DIV 73 /* R (yes, really! */
#define SENSORS_W83782D_ALARMS 81 /* R */
#define SENSORS_W83782D_BEEP_ENABLE 82 /* RW */
#define SENSORS_W83782D_BEEPS 83 /* RW */
#define SENSORS_W83782D_SENS1 91 /* RW */
#define SENSORS_W83782D_SENS2 92 /* RW */
#define SENSORS_W83782D_SENS3 93 /* RW */


/* Winbond W83783S chips */
/* Cheat on 697HF for now - no separate #defines */

#define SENSORS_W83783S_PREFIX "w83783s"
#define SENSORS_W83697HF_PREFIX "w83697hf"

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
#define SENSORS_W83783S_VRM 62 /* RW */
#define SENSORS_W83783S_FAN1_DIV 71 /* RW */
#define SENSORS_W83783S_FAN2_DIV 72 /* RW */
#define SENSORS_W83783S_FAN3_DIV 73 /* R (yes, really! */
#define SENSORS_W83783S_ALARMS 81 /* R */
#define SENSORS_W83783S_BEEP_ENABLE 82 /* RW */
#define SENSORS_W83783S_BEEPS 83 /* RW */
#define SENSORS_W83783S_SENS1 91 /* RW */
#define SENSORS_W83783S_SENS2 92 /* RW */


/* W83L785TS-S chips */

#define SENSORS_W83L785TS_PREFIX "w83l785ts"

#define SENSORS_W83L785TS_TEMP		51 /* R */
#define SENSORS_W83L785TS_TEMP_OVER	52 /* R for now */


/* Winbond W83627EHF & W83627DHG Super-I/O chips */
/* (W83627DHG is similar to W83627EHF but no in9) */

#define SENSORS_W83627EHF_PREFIX	"w83627ehf"
#define SENSORS_W83627DHG_PREFIX	"w83627dhg"

#define SENSORS_W83627EHF_IN0		1 /* R */
#define SENSORS_W83627EHF_IN1		2 /* R */
#define SENSORS_W83627EHF_IN2		3 /* R */
#define SENSORS_W83627EHF_IN3		4 /* R */
#define SENSORS_W83627EHF_IN4		5 /* R */
#define SENSORS_W83627EHF_IN5		6 /* R */
#define SENSORS_W83627EHF_IN6		7 /* R */
#define SENSORS_W83627EHF_IN7		8 /* R */
#define SENSORS_W83627EHF_IN8		9 /* R */
#define SENSORS_W83627EHF_IN9 		10 /* R */
#define SENSORS_W83627EHF_IN0_ALARM	111 /* R */
#define SENSORS_W83627EHF_IN1_ALARM	112 /* R */
#define SENSORS_W83627EHF_IN2_ALARM	113 /* R */
#define SENSORS_W83627EHF_IN3_ALARM	114 /* R */
#define SENSORS_W83627EHF_IN4_ALARM	115 /* R */
#define SENSORS_W83627EHF_IN5_ALARM	116 /* R */
#define SENSORS_W83627EHF_IN6_ALARM	117 /* R */
#define SENSORS_W83627EHF_IN7_ALARM	118 /* R */
#define SENSORS_W83627EHF_IN8_ALARM	119 /* R */
#define SENSORS_W83627EHF_IN9_ALARM	120 /* R */
#define SENSORS_W83627EHF_IN0_MIN	11 /* RW */
#define SENSORS_W83627EHF_IN1_MIN	12 /* RW */
#define SENSORS_W83627EHF_IN2_MIN	13 /* RW */
#define SENSORS_W83627EHF_IN3_MIN	14 /* RW */
#define SENSORS_W83627EHF_IN4_MIN	15 /* RW */
#define SENSORS_W83627EHF_IN5_MIN	16 /* RW */
#define SENSORS_W83627EHF_IN6_MIN	17 /* RW */
#define SENSORS_W83627EHF_IN7_MIN	18 /* RW */
#define SENSORS_W83627EHF_IN8_MIN	19 /* RW */
#define SENSORS_W83627EHF_IN9_MIN	20 /* RW */
#define SENSORS_W83627EHF_IN0_MAX	21 /* RW */
#define SENSORS_W83627EHF_IN1_MAX	22 /* RW */
#define SENSORS_W83627EHF_IN2_MAX	23 /* RW */
#define SENSORS_W83627EHF_IN3_MAX	24 /* RW */
#define SENSORS_W83627EHF_IN4_MAX	25 /* RW */
#define SENSORS_W83627EHF_IN5_MAX	26 /* RW */
#define SENSORS_W83627EHF_IN6_MAX	27 /* RW */
#define SENSORS_W83627EHF_IN7_MAX	28 /* RW */
#define SENSORS_W83627EHF_IN8_MAX	29 /* RW */
#define SENSORS_W83627EHF_IN9_MAX	30 /* RW */
#define SENSORS_W83627EHF_FAN1		31 /* R  */
#define SENSORS_W83627EHF_FAN2		32 /* R  */
#define SENSORS_W83627EHF_FAN3		33 /* R  */
#define SENSORS_W83627EHF_FAN4		34 /* R  */
#define SENSORS_W83627EHF_FAN5		35 /* R  */
#define SENSORS_W83627EHF_FAN1_ALARM	131 /* R  */
#define SENSORS_W83627EHF_FAN2_ALARM	132 /* R  */
#define SENSORS_W83627EHF_FAN3_ALARM	133 /* R  */
#define SENSORS_W83627EHF_FAN4_ALARM	134 /* R  */
#define SENSORS_W83627EHF_FAN5_ALARM	135 /* R  */
#define SENSORS_W83627EHF_FAN1_MIN	41 /* RW */
#define SENSORS_W83627EHF_FAN2_MIN	42 /* RW */
#define SENSORS_W83627EHF_FAN3_MIN	43 /* RW */
#define SENSORS_W83627EHF_FAN4_MIN	44 /* RW */
#define SENSORS_W83627EHF_FAN5_MIN	45 /* RW */
#define SENSORS_W83627EHF_FAN1_DIV	51 /* RW */
#define SENSORS_W83627EHF_FAN2_DIV	52 /* RW */
#define SENSORS_W83627EHF_FAN3_DIV	53 /* RW */
#define SENSORS_W83627EHF_FAN4_DIV	54 /* RW */
#define SENSORS_W83627EHF_FAN5_DIV	55 /* RW */
#define SENSORS_W83627EHF_TEMP1		61 /* R  */
#define SENSORS_W83627EHF_TEMP2		62 /* R  */
#define SENSORS_W83627EHF_TEMP3		63 /* R  */
#define SENSORS_W83627EHF_TEMP1_ALARM	161 /* R  */
#define SENSORS_W83627EHF_TEMP2_ALARM	162 /* R  */
#define SENSORS_W83627EHF_TEMP3_ALARM	163 /* R  */
#define SENSORS_W83627EHF_TEMP1_OVER	71 /* RW */
#define SENSORS_W83627EHF_TEMP2_OVER	72 /* RW */
#define SENSORS_W83627EHF_TEMP3_OVER	73 /* RW */
#define SENSORS_W83627EHF_TEMP1_HYST	81 /* RW */
#define SENSORS_W83627EHF_TEMP2_HYST	82 /* RW */
#define SENSORS_W83627EHF_TEMP3_HYST	83 /* RW */
#define SENSORS_W83627EHF_TEMP1_TYPE	91 /* R  */
#define SENSORS_W83627EHF_TEMP2_TYPE	92 /* R  */
#define SENSORS_W83627EHF_TEMP3_TYPE	93 /* R  */
#define SENSORS_W83627EHF_VID		245 /* R  */
#define SENSORS_W83627EHF_VRM		249 /* R  */
#define SENSORS_W83627EHF_ALARMS	250 /* R  */


/* Analog Devices ADM9240 chips */

#define SENSORS_ADM9240_PREFIX "adm9240"
#define SENSORS_LM81_PREFIX "lm81"
#define SENSORS_DS1780_PREFIX "ds1780"

/* All three chips have the same features */
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


/* SiS southbridge with integrated lm78 */

#define SENSORS_SIS5595_PREFIX "sis5595"

#define SENSORS_SIS5595_IN0 1 /* R */
#define SENSORS_SIS5595_IN1 2 /* R */
#define SENSORS_SIS5595_IN2 3 /* R */
#define SENSORS_SIS5595_IN3 4 /* R */
#define SENSORS_SIS5595_IN4 5 /* R */
#define SENSORS_SIS5595_IN0_MIN 11 /* RW */
#define SENSORS_SIS5595_IN1_MIN 12 /* RW */
#define SENSORS_SIS5595_IN2_MIN 13 /* RW */
#define SENSORS_SIS5595_IN3_MIN 14 /* RW */
#define SENSORS_SIS5595_IN4_MIN 15 /* RW */
#define SENSORS_SIS5595_IN0_MAX 21 /* RW */
#define SENSORS_SIS5595_IN1_MAX 22 /* RW */
#define SENSORS_SIS5595_IN2_MAX 23 /* RW */
#define SENSORS_SIS5595_IN3_MAX 24 /* RW */
#define SENSORS_SIS5595_IN4_MAX 25 /* RW */
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
#define SENSORS_MAXI_NBA_PREFIX "maxilife-nba"

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
/* Cheat on ADM1022 for now - no separate #defines */
#define SENSORS_ADM1022_PREFIX "adm1022"

#define SENSORS_THMC50_TEMP 51 /* R */
#define SENSORS_THMC50_TEMP_HYST 52 /* RW */
#define SENSORS_THMC50_TEMP_OVER 53 /* RW */
#define SENSORS_THMC50_REMOTE_TEMP 54 /* R */
#define SENSORS_THMC50_REMOTE_TEMP_HYST 55 /* RW */
#define SENSORS_THMC50_REMOTE_TEMP_OVER 56 /* RW */
#define SENSORS_ADM1022_REMOTE_TEMP2 57 /* R */
#define SENSORS_ADM1022_REMOTE_TEMP2_HYST 58 /* RW */
#define SENSORS_ADM1022_REMOTE_TEMP2_OVER 59 /* RW */
#define SENSORS_THMC50_ANALOG_OUT 71 /* RW */
#define SENSORS_THMC50_INTER 81 /* R */
#define SENSORS_THMC50_INTER_MASK 82 /* RW */
#define SENSORS_THMC50_DIE_CODE 90 /* R */

/* ADM1025 chip */

#define SENSORS_ADM1025_PREFIX "adm1025"
#define SENSORS_NE1619_PREFIX "ne1619"

#define SENSORS_ADM1025_IN0 1 /* R */
#define SENSORS_ADM1025_IN1 2 /* R */
#define SENSORS_ADM1025_IN2 3 /* R */
#define SENSORS_ADM1025_IN3 4 /* R */
#define SENSORS_ADM1025_IN4 5 /* R */
#define SENSORS_ADM1025_IN5 6 /* R */
#define SENSORS_ADM1025_IN0_MIN 11 /* RW */
#define SENSORS_ADM1025_IN1_MIN 12 /* RW */
#define SENSORS_ADM1025_IN2_MIN 13 /* RW */
#define SENSORS_ADM1025_IN3_MIN 14 /* RW */
#define SENSORS_ADM1025_IN4_MIN 15 /* RW */
#define SENSORS_ADM1025_IN5_MIN 16 /* RW */
#define SENSORS_ADM1025_IN0_MAX 21 /* RW */
#define SENSORS_ADM1025_IN1_MAX 22 /* RW */
#define SENSORS_ADM1025_IN2_MAX 23 /* RW */
#define SENSORS_ADM1025_IN3_MAX 24 /* RW */
#define SENSORS_ADM1025_IN4_MAX 25 /* RW */
#define SENSORS_ADM1025_IN5_MAX 26 /* RW */
#define SENSORS_ADM1025_TEMP1 51 /* R */
#define SENSORS_ADM1025_TEMP1_LOW 52 /* RW */
#define SENSORS_ADM1025_TEMP1_HIGH 53 /* RW */
#define SENSORS_ADM1025_TEMP2 54 /* R */
#define SENSORS_ADM1025_TEMP2_LOW 55 /* RW */
#define SENSORS_ADM1025_TEMP2_HIGH 56 /* RW */
#define SENSORS_ADM1025_VID 61 /* R */
#define SENSORS_ADM1025_VRM 62 /* R */
#define SENSORS_ADM1025_ALARMS 81 /* R */


#define SENSORS_ADM1026_PREFIX "adm1026"
/* NOTE: print_adm1026 (sensors) depends on the ordering
 *    of these entries in each group.  For example
 *         fan#, fan#_div, fan#_min
 *         temp#, temp#_max, temp#_min, temp#_offset
 *    and the ordering of the groups
 *         in0, in1, ... in9, in10, in11
 */
#define SENSORS_ADM1026_AFC_DAC            1  /* RW -- afc_analog_out  */
#define SENSORS_ADM1026_AFC_PWM            2  /* RW -- afc_pwm  */
#define SENSORS_ADM1026_ALARMS             3  /* R  -- alarms  */
#define SENSORS_ADM1026_ALARM_MASK         4  /* R  -- alarm_mask  */
#define SENSORS_ADM1026_DAC                5  /* RW -- analog_out  */
#define SENSORS_ADM1026_GPIO               6  /* R  -- gpio  */
#define SENSORS_ADM1026_GPIO_MASK          7  /* R  -- gpio_mask  */
#define SENSORS_ADM1026_PWM                8  /* RW -- pwm  */
#define SENSORS_ADM1026_VID                9  /* RW -- vid  */
#define SENSORS_ADM1026_VRM               10  /* RW -- vrm  */
#define SENSORS_ADM1026_FAN0              11  /* R  -- fan0  */
#define SENSORS_ADM1026_FAN0_DIV          12  /* RW -- fan0_div  */
#define SENSORS_ADM1026_FAN0_MIN          13  /* RW -- fan0_min  */
#define SENSORS_ADM1026_FAN1              14  /* R  -- fan1  */
#define SENSORS_ADM1026_FAN1_DIV          15  /* RW -- fan1_div  */
#define SENSORS_ADM1026_FAN1_MIN          16  /* RW -- fan1_min  */
#define SENSORS_ADM1026_FAN2              17  /* R  -- fan2  */
#define SENSORS_ADM1026_FAN2_DIV          18  /* RW -- fan2_div  */
#define SENSORS_ADM1026_FAN2_MIN          19  /* RW -- fan2_min  */
#define SENSORS_ADM1026_FAN3              20  /* R  -- fan3  */
#define SENSORS_ADM1026_FAN3_DIV          21  /* RW -- fan3_div  */
#define SENSORS_ADM1026_FAN3_MIN          22  /* RW -- fan3_min  */
#define SENSORS_ADM1026_FAN4              23  /* R  -- fan4  */
#define SENSORS_ADM1026_FAN4_DIV          24  /* RW -- fan4_div  */
#define SENSORS_ADM1026_FAN4_MIN          25  /* RW -- fan4_min  */
#define SENSORS_ADM1026_FAN5              26  /* R  -- fan5  */
#define SENSORS_ADM1026_FAN5_DIV          27  /* RW -- fan5_div  */
#define SENSORS_ADM1026_FAN5_MIN          28  /* RW -- fan5_min  */
#define SENSORS_ADM1026_FAN6              29  /* R  -- fan6  */
#define SENSORS_ADM1026_FAN6_DIV          30  /* RW -- fan6_div  */
#define SENSORS_ADM1026_FAN6_MIN          31  /* RW -- fan6_min  */
#define SENSORS_ADM1026_FAN7              32  /* R  -- fan7  */
#define SENSORS_ADM1026_FAN7_DIV          33  /* RW -- fan7_div  */
#define SENSORS_ADM1026_FAN7_MIN          34  /* RW -- fan7_min  */
#define SENSORS_ADM1026_IN0               35  /* R  -- in0  */
#define SENSORS_ADM1026_IN0_MAX           36  /* RW -- in0_max  */
#define SENSORS_ADM1026_IN0_MIN           37  /* RW -- in0_min  */
#define SENSORS_ADM1026_IN1               38  /* R  -- in1  */
#define SENSORS_ADM1026_IN1_MAX           39  /* RW -- in1_max  */
#define SENSORS_ADM1026_IN1_MIN           40  /* RW -- in1_min  */
#define SENSORS_ADM1026_IN2               41  /* R  -- in2  */
#define SENSORS_ADM1026_IN2_MAX           42  /* RW -- in2_max  */
#define SENSORS_ADM1026_IN2_MIN           43  /* RW -- in2_min  */
#define SENSORS_ADM1026_IN3               44  /* R  -- in3  */
#define SENSORS_ADM1026_IN3_MAX           45  /* RW -- in3_max  */
#define SENSORS_ADM1026_IN3_MIN           46  /* RW -- in3_min  */
#define SENSORS_ADM1026_IN4               47  /* R  -- in4  */
#define SENSORS_ADM1026_IN4_MAX           48  /* RW -- in4_max  */
#define SENSORS_ADM1026_IN4_MIN           49  /* RW -- in4_min  */
#define SENSORS_ADM1026_IN5               50  /* R  -- in5  */
#define SENSORS_ADM1026_IN5_MAX           51  /* RW -- in5_max  */
#define SENSORS_ADM1026_IN5_MIN           52  /* RW -- in5_min  */
#define SENSORS_ADM1026_IN6               53  /* R  -- in6  */
#define SENSORS_ADM1026_IN6_MAX           54  /* RW -- in6_max  */
#define SENSORS_ADM1026_IN6_MIN           55  /* RW -- in6_min  */
#define SENSORS_ADM1026_IN7               56  /* R  -- in7  */
#define SENSORS_ADM1026_IN7_MAX           57  /* RW -- in7_max  */
#define SENSORS_ADM1026_IN7_MIN           58  /* RW -- in7_min  */
#define SENSORS_ADM1026_IN8               59  /* R  -- in8  */
#define SENSORS_ADM1026_IN8_MAX           60  /* RW -- in8_max  */
#define SENSORS_ADM1026_IN8_MIN           61  /* RW -- in8_min  */
#define SENSORS_ADM1026_IN9               62  /* R  -- in9  */
#define SENSORS_ADM1026_IN9_MAX           63  /* RW -- in9_max  */
#define SENSORS_ADM1026_IN9_MIN           64  /* RW -- in9_min  */
#define SENSORS_ADM1026_IN10              65  /* R  -- in10  */
#define SENSORS_ADM1026_IN10_MAX          66  /* RW -- in10_max  */
#define SENSORS_ADM1026_IN10_MIN          67  /* RW -- in10_min  */
#define SENSORS_ADM1026_IN11              68  /* R  -- in11  */
#define SENSORS_ADM1026_IN11_MAX          69  /* RW -- in11_max  */
#define SENSORS_ADM1026_IN11_MIN          70  /* RW -- in11_min  */
#define SENSORS_ADM1026_IN12              71  /* R  -- in12  */
#define SENSORS_ADM1026_IN12_MAX          72  /* RW -- in12_max  */
#define SENSORS_ADM1026_IN12_MIN          73  /* RW -- in12_min  */
#define SENSORS_ADM1026_IN13              74  /* R  -- in13  */
#define SENSORS_ADM1026_IN13_MAX          75  /* RW -- in13_max  */
#define SENSORS_ADM1026_IN13_MIN          76  /* RW -- in13_min  */
#define SENSORS_ADM1026_IN14              77  /* R  -- in14  */
#define SENSORS_ADM1026_IN14_MAX          78  /* RW -- in14_max  */
#define SENSORS_ADM1026_IN14_MIN          79  /* RW -- in14_min  */
#define SENSORS_ADM1026_IN15              80  /* R  -- in15  */
#define SENSORS_ADM1026_IN15_MAX          81  /* RW -- in15_max  */
#define SENSORS_ADM1026_IN15_MIN          82  /* RW -- in15_min  */
#define SENSORS_ADM1026_IN16              83  /* R  -- in16  */
#define SENSORS_ADM1026_IN16_MAX          84  /* RW -- in16_max  */
#define SENSORS_ADM1026_IN16_MIN          85  /* RW -- in16_min  */
#define SENSORS_ADM1026_TEMP1             86  /* R  -- temp1  */
#define SENSORS_ADM1026_TEMP1_MAX         87  /* RW -- temp1_max  */
#define SENSORS_ADM1026_TEMP1_MIN         88  /* RW -- temp1_min  */
#define SENSORS_ADM1026_TEMP1_OFFSET      89  /* RW -- temp1_offset  */
#define SENSORS_ADM1026_TEMP1_THERM       90  /* RW -- temp1_therm  */
#define SENSORS_ADM1026_TEMP1_TMIN        91  /* RW -- temp1_tmin  */
#define SENSORS_ADM1026_TEMP2             92  /* R  -- temp2  */
#define SENSORS_ADM1026_TEMP2_MAX         93  /* RW -- temp2_max  */
#define SENSORS_ADM1026_TEMP2_MIN         94  /* RW -- temp2_min  */
#define SENSORS_ADM1026_TEMP2_OFFSET      95  /* RW -- temp2_offset  */
#define SENSORS_ADM1026_TEMP2_THERM       96  /* RW -- temp2_therm  */
#define SENSORS_ADM1026_TEMP2_TMIN        97  /* RW -- temp2_tmin  */
#define SENSORS_ADM1026_TEMP3             98  /* R  -- temp3  */
#define SENSORS_ADM1026_TEMP3_MAX         99  /* RW -- temp3_max  */
#define SENSORS_ADM1026_TEMP3_MIN        100  /* RW -- temp3_min  */
#define SENSORS_ADM1026_TEMP3_OFFSET     101  /* RW -- temp3_offset  */
#define SENSORS_ADM1026_TEMP3_THERM      102  /* RW -- temp3_therm  */
#define SENSORS_ADM1026_TEMP3_TMIN       103  /* RW -- temp3_tmin  */


#define SENSORS_VIA686A_PREFIX "via686a"

#define SENSORS_VIA686A_IN0 1 /* R */
#define SENSORS_VIA686A_IN1 2 /* R */
#define SENSORS_VIA686A_IN2 3 /* R */
#define SENSORS_VIA686A_IN3 4 /* R */
#define SENSORS_VIA686A_IN4 5 /* R */
#define SENSORS_VIA686A_IN0_MIN 11 /* RW */
#define SENSORS_VIA686A_IN1_MIN 12 /* RW */
#define SENSORS_VIA686A_IN2_MIN 13 /* RW */
#define SENSORS_VIA686A_IN3_MIN 14 /* RW */
#define SENSORS_VIA686A_IN4_MIN 15 /* RW */
#define SENSORS_VIA686A_IN0_MAX 21 /* RW */
#define SENSORS_VIA686A_IN1_MAX 22 /* RW */
#define SENSORS_VIA686A_IN2_MAX 23 /* RW */
#define SENSORS_VIA686A_IN3_MAX 24 /* RW */
#define SENSORS_VIA686A_IN4_MAX 25 /* RW */
#define SENSORS_VIA686A_FAN1 31 /* R */
#define SENSORS_VIA686A_FAN2 32 /* R */
#define SENSORS_VIA686A_FAN1_MIN 41 /* RW */
#define SENSORS_VIA686A_FAN2_MIN 42 /* RW */
#define SENSORS_VIA686A_TEMP 51 /* R */
#define SENSORS_VIA686A_TEMP_HYST 52 /* RW */
#define SENSORS_VIA686A_TEMP_OVER 53 /* RW */
#define SENSORS_VIA686A_TEMP2 54 /* R */
#define SENSORS_VIA686A_TEMP2_HYST 55 /* RW */
#define SENSORS_VIA686A_TEMP2_OVER 56 /* RW */
#define SENSORS_VIA686A_TEMP3 57 /* R */
#define SENSORS_VIA686A_TEMP3_HYST 58 /* RW */
#define SENSORS_VIA686A_TEMP3_OVER 59 /* RW */
#define SENSORS_VIA686A_FAN1_DIV 71 /* RW */
#define SENSORS_VIA686A_FAN2_DIV 72 /* RW */
#define SENSORS_VIA686A_ALARMS 81 /* R */


/* DDC Monitor */

#define SENSORS_DDCMON_PREFIX "ddcmon"

#define SENSORS_DDCMON_MAN_ID 10
#define SENSORS_DDCMON_PROD_ID 11
#define SENSORS_DDCMON_YEAR 13
#define SENSORS_DDCMON_WEEK 14
#define SENSORS_DDCMON_EDID_VER 15
#define SENSORS_DDCMON_EDID_REV 16
#define SENSORS_DDCMON_HORSIZE 21
#define SENSORS_DDCMON_VERSIZE 22
#define SENSORS_DDCMON_GAMMA 23
#define SENSORS_DDCMON_DPMS 24
#define SENSORS_DDCMON_HORSYNCMIN 31
#define SENSORS_DDCMON_HORSYNCMAX 32
#define SENSORS_DDCMON_VERSYNCMIN 33
#define SENSORS_DDCMON_VERSYNCMAX 34
#define SENSORS_DDCMON_MAXCLOCK 35
#define SENSORS_DDCMON_SERIAL 40
#define SENSORS_DDCMON_TIMINGS 50
#define SENSORS_DDCMON_TIMING1_HOR 51
#define SENSORS_DDCMON_TIMING1_VER 52
#define SENSORS_DDCMON_TIMING1_REF 53
#define SENSORS_DDCMON_TIMING2_HOR 54
#define SENSORS_DDCMON_TIMING2_VER 55
#define SENSORS_DDCMON_TIMING2_REF 56
#define SENSORS_DDCMON_TIMING3_HOR 57
#define SENSORS_DDCMON_TIMING3_VER 58
#define SENSORS_DDCMON_TIMING3_REF 59
#define SENSORS_DDCMON_TIMING4_HOR 60
#define SENSORS_DDCMON_TIMING4_VER 61
#define SENSORS_DDCMON_TIMING4_REF 62
#define SENSORS_DDCMON_TIMING5_HOR 63
#define SENSORS_DDCMON_TIMING5_VER 64
#define SENSORS_DDCMON_TIMING5_REF 65
#define SENSORS_DDCMON_TIMING6_HOR 66
#define SENSORS_DDCMON_TIMING6_VER 67
#define SENSORS_DDCMON_TIMING6_REF 68
#define SENSORS_DDCMON_TIMING7_HOR 69
#define SENSORS_DDCMON_TIMING7_VER 70
#define SENSORS_DDCMON_TIMING7_REF 71
#define SENSORS_DDCMON_TIMING8_HOR 72
#define SENSORS_DDCMON_TIMING8_VER 73
#define SENSORS_DDCMON_TIMING8_REF 74

/* EEPROM (SDRAM DIMM) */

#define SENSORS_EEPROM_PREFIX "eeprom"

#define SENSORS_EEPROM_TYPE 10
#define SENSORS_EEPROM_ROWADDR 12
#define SENSORS_EEPROM_COLADDR 13
#define SENSORS_EEPROM_NUMROWS 14
#define SENSORS_EEPROM_BANKS 15

#define SENSORS_EEPROM_VAIO_NAME 128
/* 129 to 159: reserved, do not use! */
#define SENSORS_EEPROM_VAIO_SERIAL 160
/* 161 to 191: reserved, do not use! */

#define SENSORS_EEPROM_EDID_HEADER 32
/* 33 to 39: reserved, do not use! */

#define SENSORS_EEPROM_SHUTTLE 40
/* 41 to 45: reserved, do not use! */

/* Analog Devices LM87 chips */

#define SENSORS_LM87_PREFIX "lm87"

#define SENSORS_LM87_IN0              1 /* R */
#define SENSORS_LM87_IN1              2 /* R */
#define SENSORS_LM87_IN2              3 /* R */
#define SENSORS_LM87_IN3              4 /* R */
#define SENSORS_LM87_IN4              5 /* R */
#define SENSORS_LM87_IN5              6 /* R */
#define SENSORS_LM87_AIN1             7 /* R */
#define SENSORS_LM87_AIN2             8 /* R */
#define SENSORS_LM87_IN0_MIN         11 /* RW */
#define SENSORS_LM87_IN1_MIN         12 /* RW */
#define SENSORS_LM87_IN2_MIN         13 /* RW */
#define SENSORS_LM87_IN3_MIN         14 /* RW */
#define SENSORS_LM87_IN4_MIN         15 /* RW */
#define SENSORS_LM87_IN5_MIN         16 /* RW */
#define SENSORS_LM87_AIN1_MIN        17 /* RW */
#define SENSORS_LM87_AIN2_MIN        18 /* RW */
#define SENSORS_LM87_IN0_MAX         21 /* RW */
#define SENSORS_LM87_IN1_MAX         22 /* RW */
#define SENSORS_LM87_IN2_MAX         23 /* RW */
#define SENSORS_LM87_IN3_MAX         24 /* RW */
#define SENSORS_LM87_IN4_MAX         25 /* RW */
#define SENSORS_LM87_IN5_MAX         26 /* RW */
#define SENSORS_LM87_AIN1_MAX        27 /* RW */
#define SENSORS_LM87_AIN2_MAX        28 /* RW */
#define SENSORS_LM87_FAN1            31 /* R */
#define SENSORS_LM87_FAN2            32 /* R */
#define SENSORS_LM87_FAN1_MIN        41 /* RW */
#define SENSORS_LM87_FAN2_MIN        42 /* RW */
#define SENSORS_LM87_TEMP1           51 /* R */
#define SENSORS_LM87_TEMP2           52 /* R */
#define SENSORS_LM87_TEMP3           53 /* R */
#define SENSORS_LM87_TEMP1_HYST      54 /* RW */
#define SENSORS_LM87_TEMP1_OVER      55 /* RW */
#define SENSORS_LM87_TEMP2_HYST      56 /* RW */
#define SENSORS_LM87_TEMP2_OVER      57 /* RW */
#define SENSORS_LM87_TEMP3_HYST      58 /* RW */
#define SENSORS_LM87_TEMP3_OVER      59 /* RW */
#define SENSORS_LM87_VID             61 /* R */
#define SENSORS_LM87_VRM             62 /* RW */
#define SENSORS_LM87_FAN1_DIV        71 /* RW */
#define SENSORS_LM87_FAN2_DIV        72 /* RW */
#define SENSORS_LM87_ALARMS          81 /* R */
#define SENSORS_LM87_ANALOG_OUT      82 /* RW */

/* Myson MTP008 chips */

#define SENSORS_MTP008_PREFIX		"mtp008"

#define SENSORS_MTP008_IN0              1 /* R */
#define SENSORS_MTP008_IN1              2 /* R */
#define SENSORS_MTP008_IN2              3 /* R */
#define SENSORS_MTP008_IN3              4 /* R */
#define SENSORS_MTP008_IN4              5 /* R */
#define SENSORS_MTP008_IN5              6 /* R */
#define SENSORS_MTP008_IN6              7 /* R */
#define SENSORS_MTP008_IN0_MIN         11 /* RW */
#define SENSORS_MTP008_IN1_MIN         12 /* RW */
#define SENSORS_MTP008_IN2_MIN         13 /* RW */
#define SENSORS_MTP008_IN3_MIN         14 /* RW */
#define SENSORS_MTP008_IN4_MIN         15 /* RW */
#define SENSORS_MTP008_IN5_MIN         16 /* RW */
#define SENSORS_MTP008_IN6_MIN         17 /* RW */
#define SENSORS_MTP008_IN0_MAX         21 /* RW */
#define SENSORS_MTP008_IN1_MAX         22 /* RW */
#define SENSORS_MTP008_IN2_MAX         23 /* RW */
#define SENSORS_MTP008_IN3_MAX         24 /* RW */
#define SENSORS_MTP008_IN4_MAX         25 /* RW */
#define SENSORS_MTP008_IN5_MAX         26 /* RW */
#define SENSORS_MTP008_IN6_MAX         27 /* RW */
#define SENSORS_MTP008_FAN1            31 /* R */
#define SENSORS_MTP008_FAN2            32 /* R */
#define SENSORS_MTP008_FAN3            33 /* R */
#define SENSORS_MTP008_FAN1_MIN        41 /* RW */
#define SENSORS_MTP008_FAN2_MIN        42 /* RW */
#define SENSORS_MTP008_FAN3_MIN        43 /* RW */
#define SENSORS_MTP008_TEMP1           51 /* R */
#define SENSORS_MTP008_TEMP2           52 /* R */
#define SENSORS_MTP008_TEMP3           53 /* R */
#define SENSORS_MTP008_TEMP1_OVER      54 /* RW */
#define SENSORS_MTP008_TEMP1_HYST      55 /* RW */
#define SENSORS_MTP008_TEMP2_OVER      56 /* RW */
#define SENSORS_MTP008_TEMP2_HYST      57 /* RW */
#define SENSORS_MTP008_TEMP3_OVER      58 /* RW */
#define SENSORS_MTP008_TEMP3_HYST      59 /* RW */
#define SENSORS_MTP008_VID             61 /* R */
#define SENSORS_MTP008_FAN1_DIV        71 /* RW */
#define SENSORS_MTP008_FAN2_DIV        72 /* RW */
#define SENSORS_MTP008_FAN3_DIV        73 /* RW */
#define SENSORS_MTP008_ALARMS          81 /* R */
#define SENSORS_MTP008_BEEP            82 /* RW */

/* DS1621 chips. */

#define SENSORS_DS1621_PREFIX "ds1621"

#define SENSORS_DS1621_TEMP 51 /* R */
#define SENSORS_DS1621_TEMP_HYST 52 /* RW */
#define SENSORS_DS1621_TEMP_OVER 53 /* RW */
#define SENSORS_DS1621_ALARMS 81 /* R */
#define SENSORS_DS1621_ENABLE 82 /* RW */
#define SENSORS_DS1621_CONTINUOUS 83 /* RW */
#define SENSORS_DS1621_POLARITY 84 /* RW */

/* ADM1024 chip */

#define SENSORS_ADM1024_PREFIX "adm1024"

#define SENSORS_ADM1024_IN0 1 /* R */
#define SENSORS_ADM1024_IN1 2 /* R */
#define SENSORS_ADM1024_IN2 3 /* R */
#define SENSORS_ADM1024_IN3 4 /* R */
#define SENSORS_ADM1024_IN4 5 /* R */
#define SENSORS_ADM1024_IN5 6 /* R */
#define SENSORS_ADM1024_IN0_MIN 11 /* RW */
#define SENSORS_ADM1024_IN1_MIN 12 /* RW */
#define SENSORS_ADM1024_IN2_MIN 13 /* RW */
#define SENSORS_ADM1024_IN3_MIN 14 /* RW */
#define SENSORS_ADM1024_IN4_MIN 15 /* RW */
#define SENSORS_ADM1024_IN5_MIN 16 /* RW */
#define SENSORS_ADM1024_IN0_MAX 21 /* RW */
#define SENSORS_ADM1024_IN1_MAX 22 /* RW */
#define SENSORS_ADM1024_IN2_MAX 23 /* RW */
#define SENSORS_ADM1024_IN3_MAX 24 /* RW */
#define SENSORS_ADM1024_IN4_MAX 25 /* RW */
#define SENSORS_ADM1024_IN5_MAX 26 /* RW */
#define SENSORS_ADM1024_FAN1 31 /* R */
#define SENSORS_ADM1024_FAN2 32 /* R */
#define SENSORS_ADM1024_FAN1_MIN 41 /* RW */
#define SENSORS_ADM1024_FAN2_MIN 42 /* RW */
#define SENSORS_ADM1024_TEMP 51 /* R */
#define SENSORS_ADM1024_TEMP1 52 /* R */
#define SENSORS_ADM1024_TEMP2 53 /* R */
#define SENSORS_ADM1024_TEMP_HYST 61 /* RW */
#define SENSORS_ADM1024_TEMP_OVER 62 /* RW */
#define SENSORS_ADM1024_TEMP1_HYST 63 /* RW */
#define SENSORS_ADM1024_TEMP1_OVER 64 /* RW */
#define SENSORS_ADM1024_TEMP2_HYST 65 /* RW */
#define SENSORS_ADM1024_TEMP2_OVER 66 /* RW */
#define SENSORS_ADM1024_VID 71 /* R */
#define SENSORS_ADM1024_FAN1_DIV 81 /* RW */
#define SENSORS_ADM1024_FAN2_DIV 82 /* RW */
#define SENSORS_ADM1024_ALARMS 91 /* R */
#define SENSORS_ADM1024_ANALOG_OUT 92 /* RW */

/* IT87xx chips */

#define SENSORS_IT87_PREFIX "it87"
#define SENSORS_IT8712_PREFIX "it8712"
#define SENSORS_IT8716_PREFIX "it8716"
#define SENSORS_IT8718_PREFIX "it8718"
#define SENSORS_IT8720_PREFIX "it8720"

#define SENSORS_IT87_IN0 1 /* R */
#define SENSORS_IT87_IN1 2 /* R */
#define SENSORS_IT87_IN2 3 /* R */
#define SENSORS_IT87_IN3 4 /* R */
#define SENSORS_IT87_IN4 5 /* R */
#define SENSORS_IT87_IN5 6 /* R */
#define SENSORS_IT87_IN6 7 /* R */
#define SENSORS_IT87_IN7 8 /* R */
#define SENSORS_IT87_IN8 9 /* R */
#define SENSORS_IT87_IN0_MIN 11 /* RW */
#define SENSORS_IT87_IN1_MIN 12 /* RW */
#define SENSORS_IT87_IN2_MIN 13 /* RW */
#define SENSORS_IT87_IN3_MIN 14 /* RW */
#define SENSORS_IT87_IN4_MIN 15 /* RW */
#define SENSORS_IT87_IN5_MIN 16 /* RW */
#define SENSORS_IT87_IN6_MIN 17 /* RW */
#define SENSORS_IT87_IN7_MIN 18 /* RW */
#define SENSORS_IT87_IN0_MAX 21 /* RW */
#define SENSORS_IT87_IN1_MAX 22 /* RW */
#define SENSORS_IT87_IN2_MAX 23 /* RW */
#define SENSORS_IT87_IN3_MAX 24 /* RW */
#define SENSORS_IT87_IN4_MAX 25 /* RW */
#define SENSORS_IT87_IN5_MAX 26 /* RW */
#define SENSORS_IT87_IN6_MAX 27 /* RW */
#define SENSORS_IT87_IN7_MAX 28 /* RW */
#define SENSORS_IT87_FAN1 31 /* R */
#define SENSORS_IT87_FAN2 32 /* R */
#define SENSORS_IT87_FAN3 33 /* R */
#define SENSORS_IT87_FAN4 34 /* R */
#define SENSORS_IT87_FAN5 35 /* R */
#define SENSORS_IT87_FAN1_MIN 41 /* RW */
#define SENSORS_IT87_FAN2_MIN 42 /* RW */
#define SENSORS_IT87_FAN3_MIN 43 /* RW */
#define SENSORS_IT87_FAN4_MIN 44 /* RW */
#define SENSORS_IT87_FAN5_MIN 45 /* RW */
#define SENSORS_IT87_TEMP1 51 /* R */
#define SENSORS_IT87_TEMP2 52 /* R */
#define SENSORS_IT87_TEMP3 53 /* R */
#define SENSORS_IT87_TEMP1_LOW 54 /* RW */
#define SENSORS_IT87_TEMP2_LOW 55 /* RW */
#define SENSORS_IT87_TEMP3_LOW 56 /* RW */
#define SENSORS_IT87_TEMP1_HIGH 57 /* RW */
#define SENSORS_IT87_TEMP2_HIGH 58 /* RW */
#define SENSORS_IT87_TEMP3_HIGH 59 /* RW */
#define SENSORS_IT87_VID 61 /* R */
#define SENSORS_IT87_FAN1_DIV 71 /* RW */
#define SENSORS_IT87_FAN2_DIV 72 /* RW */
#define SENSORS_IT87_FAN3_DIV 73 /* R (fan3 different) */
#define SENSORS_IT87_ALARMS_FAN 81 /* R */
#define SENSORS_IT87_ALARMS_VIN 82 /* R */
#define SENSORS_IT87_ALARMS_TEMP 83 /* R */
#define SENSORS_IT87_ALARMS 84 /* R */
#define SENSORS_IT87_SENS1 91 /* RW */
#define SENSORS_IT87_SENS2 92 /* RW */
#define SENSORS_IT87_SENS3 93 /* RW */


/* fsc poseidon chip */

#define SENSORS_FSCPOS_PREFIX "fscpos"

#define SENSORS_FSCPOS_REV 1 /* R */
#define SENSORS_FSCPOS_EVENT 2 /* R */
#define SENSORS_FSCPOS_CONTROL 3 /* RW */
#define SENSORS_FSCPOS_FAN1 4 /* R */
#define SENSORS_FSCPOS_FAN2 5 /* R */
#define SENSORS_FSCPOS_FAN3 6 /* R */
#define SENSORS_FSCPOS_FAN1_MIN 7 /* RW */
#define SENSORS_FSCPOS_FAN2_MIN 8 /* RW */
#define SENSORS_FSCPOS_FAN3_MIN 9 /* RW */
#define SENSORS_FSCPOS_FAN1_STATE 10 /* RW */
#define SENSORS_FSCPOS_FAN2_STATE 11 /* RW */
#define SENSORS_FSCPOS_FAN3_STATE 12 /* RW */
#define SENSORS_FSCPOS_FAN1_RIPPLE 13 /* RW */
#define SENSORS_FSCPOS_FAN2_RIPPLE 14 /* RW */
#define SENSORS_FSCPOS_FAN3_RIPPLE 15 /* RW */
#define SENSORS_FSCPOS_TEMP1 16 /* R */
#define SENSORS_FSCPOS_TEMP2 17 /* R */
#define SENSORS_FSCPOS_TEMP3 18 /* R */
#define SENSORS_FSCPOS_TEMP1_STATE 19 /* RW */
#define SENSORS_FSCPOS_TEMP2_STATE 20 /* RW */
#define SENSORS_FSCPOS_TEMP3_STATE 21 /* RW */
#define SENSORS_FSCPOS_VOLTAGE1 22 /* R */
#define SENSORS_FSCPOS_VOLTAGE2 23 /* R */
#define SENSORS_FSCPOS_VOLTAGE3 24 /* R */
#define SENSORS_FSCPOS_WDOG_PRESET 25/* RW */
#define SENSORS_FSCPOS_WDOG_STATE 26/* RW */
#define SENSORS_FSCPOS_WDOG_CONTROL 27/* RW */

/* fsc scylla chip */

#define SENSORS_FSCSCY_PREFIX "fscscy"

#define SENSORS_FSCSCY_REV 1 /* R */
#define SENSORS_FSCSCY_EVENT 2 /* R */
#define SENSORS_FSCSCY_CONTROL 3 /* RW */
#define SENSORS_FSCSCY_FAN1 4 /* R */
#define SENSORS_FSCSCY_FAN2 5 /* R */
#define SENSORS_FSCSCY_FAN3 6 /* R */
#define SENSORS_FSCSCY_FAN4 7 /* R */
#define SENSORS_FSCSCY_FAN5 8 /* R */
#define SENSORS_FSCSCY_FAN6 9 /* R */
#define SENSORS_FSCSCY_FAN1_MIN 10 /* RW */
#define SENSORS_FSCSCY_FAN2_MIN 11 /* RW */
#define SENSORS_FSCSCY_FAN3_MIN 12 /* RW */
#define SENSORS_FSCSCY_FAN4_MIN 13 /* RW */
#define SENSORS_FSCSCY_FAN5_MIN 14 /* RW */
#define SENSORS_FSCSCY_FAN6_MIN 15 /* RW */
#define SENSORS_FSCSCY_FAN1_STATE 16 /* RW */
#define SENSORS_FSCSCY_FAN2_STATE 17 /* RW */
#define SENSORS_FSCSCY_FAN3_STATE 18 /* RW */
#define SENSORS_FSCSCY_FAN4_STATE 19 /* RW */
#define SENSORS_FSCSCY_FAN5_STATE 20 /* RW */
#define SENSORS_FSCSCY_FAN6_STATE 21 /* RW */
#define SENSORS_FSCSCY_FAN1_RIPPLE 22 /* RW */
#define SENSORS_FSCSCY_FAN2_RIPPLE 23 /* RW */
#define SENSORS_FSCSCY_FAN3_RIPPLE 24 /* RW */
#define SENSORS_FSCSCY_FAN4_RIPPLE 25 /* RW */
#define SENSORS_FSCSCY_FAN5_RIPPLE 26 /* RW */
#define SENSORS_FSCSCY_FAN6_RIPPLE 27 /* RW */
#define SENSORS_FSCSCY_TEMP1 28 /* R */
#define SENSORS_FSCSCY_TEMP2 29 /* R */
#define SENSORS_FSCSCY_TEMP3 30 /* R */
#define SENSORS_FSCSCY_TEMP4 31 /* R */
#define SENSORS_FSCSCY_TEMP1_STATE 32 /* RW */
#define SENSORS_FSCSCY_TEMP2_STATE 33 /* RW */
#define SENSORS_FSCSCY_TEMP3_STATE 34 /* RW */
#define SENSORS_FSCSCY_TEMP4_STATE 35 /* RW */
#define SENSORS_FSCSCY_TEMP1_LIM 36 /* R */
#define SENSORS_FSCSCY_TEMP2_LIM 37 /* R */
#define SENSORS_FSCSCY_TEMP3_LIM 38 /* R */
#define SENSORS_FSCSCY_TEMP4_LIM 39 /* R */
#define SENSORS_FSCSCY_TEMP1_MIN 40 /* R */
#define SENSORS_FSCSCY_TEMP2_MIN 41 /* R */
#define SENSORS_FSCSCY_TEMP3_MIN 42 /* R */
#define SENSORS_FSCSCY_TEMP4_MIN 43 /* R */
#define SENSORS_FSCSCY_TEMP1_MAX 44 /* R */
#define SENSORS_FSCSCY_TEMP2_MAX 45 /* R */
#define SENSORS_FSCSCY_TEMP3_MAX 46 /* R */
#define SENSORS_FSCSCY_TEMP4_MAX 47 /* R */
#define SENSORS_FSCSCY_VOLTAGE1  48/* R */
#define SENSORS_FSCSCY_VOLTAGE2  49/* R */
#define SENSORS_FSCSCY_VOLTAGE3  50/* R */
#define SENSORS_FSCSCY_WDOG_PRESET  51 /* RW */
#define SENSORS_FSCSCY_WDOG_STATE   52 /* RW */
#define SENSORS_FSCSCY_WDOG_CONTROL 52 /* RW */

/* fsc hermes chip */

#define SENSORS_FSCHER_PREFIX "fscher"

#define SENSORS_FSCHER_REV 1 /* R */
#define SENSORS_FSCHER_EVENT 2 /* R */
#define SENSORS_FSCHER_CONTROL 3 /* RW */
#define SENSORS_FSCHER_FAN1 4 /* R */
#define SENSORS_FSCHER_FAN2 5 /* R */
#define SENSORS_FSCHER_FAN3 6 /* R */
#define SENSORS_FSCHER_PWM1 7 /* RW */
#define SENSORS_FSCHER_PWM2 8 /* RW */
#define SENSORS_FSCHER_PWM3 9 /* RW */
#define SENSORS_FSCHER_FAN1_STATE 10 /* RW */
#define SENSORS_FSCHER_FAN2_STATE 11 /* RW */
#define SENSORS_FSCHER_FAN3_STATE 12 /* RW */
#define SENSORS_FSCHER_FAN1_RIPPLE 13 /* RW */
#define SENSORS_FSCHER_FAN2_RIPPLE 14 /* RW */
#define SENSORS_FSCHER_FAN3_RIPPLE 15 /* RW */
#define SENSORS_FSCHER_TEMP1 16 /* R */
#define SENSORS_FSCHER_TEMP2 17 /* R */
#define SENSORS_FSCHER_TEMP3 18 /* R */
#define SENSORS_FSCHER_TEMP1_STATE 19 /* RW */
#define SENSORS_FSCHER_TEMP2_STATE 20 /* RW */
#define SENSORS_FSCHER_TEMP3_STATE 21 /* RW */
#define SENSORS_FSCHER_VOLTAGE1 22 /* R */
#define SENSORS_FSCHER_VOLTAGE2 23 /* R */
#define SENSORS_FSCHER_VOLTAGE3 24 /* R */
#define SENSORS_FSCHER_WDOG_PRESET 25/* RW */
#define SENSORS_FSCHER_WDOG_STATE 26/* RW */
#define SENSORS_FSCHER_WDOG_CONTROL 27/* RW */

/* PCF8591 chip. */

#define SENSORS_PCF8591_PREFIX "pcf8591"

#define SENSORS_PCF8591_AIN_CONF 1 /* RW */
#define SENSORS_PCF8591_CH0 2 /* R */
#define SENSORS_PCF8591_CH1 3 /* R */
#define SENSORS_PCF8591_CH2 4 /* R */
#define SENSORS_PCF8591_CH3 5 /* R */
#define SENSORS_PCF8591_AOUT_ENABLE 6 /* RW */
#define SENSORS_PCF8591_AOUT 7 /* RW */


#define SENSORS_VT1211_PREFIX "vt1211"

#define SENSORS_VT1211_IN0 1 /* R */
#define SENSORS_VT1211_IN1 2 /* R */
#define SENSORS_VT1211_IN2 3 /* R */
#define SENSORS_VT1211_IN3 4 /* R */
#define SENSORS_VT1211_IN4 5 /* R */
#define SENSORS_VT1211_IN5 6 /* R */
#define SENSORS_VT1211_IN0_MIN 11 /* RW */
#define SENSORS_VT1211_IN1_MIN 12 /* RW */
#define SENSORS_VT1211_IN2_MIN 13 /* RW */
#define SENSORS_VT1211_IN3_MIN 14 /* RW */
#define SENSORS_VT1211_IN4_MIN 15 /* RW */
#define SENSORS_VT1211_IN5_MIN 16 /* RW */
#define SENSORS_VT1211_IN0_MAX 21 /* RW */
#define SENSORS_VT1211_IN1_MAX 22 /* RW */
#define SENSORS_VT1211_IN2_MAX 23 /* RW */
#define SENSORS_VT1211_IN3_MAX 24 /* RW */
#define SENSORS_VT1211_IN4_MAX 25 /* RW */
#define SENSORS_VT1211_IN5_MAX 26 /* RW */
#define SENSORS_VT1211_FAN1 31 /* R */
#define SENSORS_VT1211_FAN2 32 /* R */
#define SENSORS_VT1211_FAN1_MIN 41 /* RW */
#define SENSORS_VT1211_FAN2_MIN 42 /* RW */
#define SENSORS_VT1211_TEMP1 51 /* R */
#define SENSORS_VT1211_TEMP1_HYST 52 /* RW */
#define SENSORS_VT1211_TEMP1_OVER 53 /* RW */
#define SENSORS_VT1211_TEMP2 54 /* R */
#define SENSORS_VT1211_TEMP2_HYST 55 /* RW */
#define SENSORS_VT1211_TEMP2_OVER 56 /* RW */
#define SENSORS_VT1211_TEMP3 57 /* R */
#define SENSORS_VT1211_TEMP3_HYST 58 /* RW */
#define SENSORS_VT1211_TEMP3_OVER 59 /* RW */
#define SENSORS_VT1211_TEMP4 60 /* R */
#define SENSORS_VT1211_TEMP4_HYST 61 /* RW */
#define SENSORS_VT1211_TEMP4_OVER 62 /* RW */
#define SENSORS_VT1211_TEMP5 63 /* R */
#define SENSORS_VT1211_TEMP5_HYST 64 /* RW */
#define SENSORS_VT1211_TEMP5_OVER 65 /* RW */
#define SENSORS_VT1211_TEMP6 66 /* R */
#define SENSORS_VT1211_TEMP6_HYST 67 /* RW */
#define SENSORS_VT1211_TEMP6_OVER 68 /* RW */
#define SENSORS_VT1211_TEMP7 69 /* R */
#define SENSORS_VT1211_TEMP7_HYST 70 /* RW */
#define SENSORS_VT1211_TEMP7_OVER 71 /* RW */
#define SENSORS_VT1211_FAN1_DIV 75 /* RW */
#define SENSORS_VT1211_FAN2_DIV 76 /* RW */
#define SENSORS_VT1211_ALARMS 81 /* R */
#define SENSORS_VT1211_VID 82 /* R */
#define SENSORS_VT1211_VRM 83 /* RW */
#define SENSORS_VT1211_UCH 84 /* RW */

#define SENSORS_SMSC47M1_PREFIX "smsc47m1"
#define SENSORS_SMSC47M2_PREFIX "smsc47m2"

#define SENSORS_SMSC47M1_FAN1 31 /* R */
#define SENSORS_SMSC47M1_FAN2 32 /* R */
#define SENSORS_SMSC47M1_FAN3 33 /* R */
#define SENSORS_SMSC47M1_FAN1_MIN 41 /* RW */
#define SENSORS_SMSC47M1_FAN2_MIN 42 /* RW */
#define SENSORS_SMSC47M1_FAN3_MIN 43 /* RW */
#define SENSORS_SMSC47M1_FAN1_DIV 75 /* RW */
#define SENSORS_SMSC47M1_FAN2_DIV 76 /* RW */
#define SENSORS_SMSC47M1_FAN3_DIV 77 /* RW */
#define SENSORS_SMSC47M1_ALARMS 81 /* R */

#define SENSORS_SMSC47M192_PREFIX "smsc47m192"

#define SENSORS_SMSC47M192_IN(n)		(1 + (n))	/* R */
#define SENSORS_SMSC47M192_IN_MIN(n)		(21 + (n))	/* RW */
#define SENSORS_SMSC47M192_IN_MAX(n)		(41 + (n))	/* RW */
#define SENSORS_SMSC47M192_IN_ALARM(n)		(61 + (n))	/* R */
#define SENSORS_SMSC47M192_TEMP(n)		(100 + (n))	/* R */
#define SENSORS_SMSC47M192_TEMP_MAX(n)		(120 + (n))	/* RW */
#define SENSORS_SMSC47M192_TEMP_MIN(n)		(140 + (n))	/* RW */
#define SENSORS_SMSC47M192_TEMP_OFFSET(n)	(160 + (n))	/* RW */
#define SENSORS_SMSC47M192_TEMP_ALARM(n)	(180 + (n))	/* R */
#define SENSORS_SMSC47M192_TEMP_FAULT(n)	(200 + (n))	/* R */
#define SENSORS_SMSC47M192_VID			301		/* R */
#define SENSORS_SMSC47M192_VRM			302		/* RW */

#define SENSORS_PC87360_PREFIX "pc87360"
#define SENSORS_PC87363_PREFIX "pc87363"
#define SENSORS_PC87364_PREFIX "pc87364"
#define SENSORS_PC87365_PREFIX "pc87365"
#define SENSORS_PC87366_PREFIX "pc87366"

#define SENSORS_PC87360_ALARMS_IN	10	/* R */
#define SENSORS_PC87360_ALARMS_TEMP	11	/* R */

#define SENSORS_PC87360_FAN1		31	/* R */
#define SENSORS_PC87360_FAN2		32	/* R */
#define SENSORS_PC87360_FAN3		33	/* R */
#define SENSORS_PC87360_FAN1_MIN	41	/* RW */
#define SENSORS_PC87360_FAN2_MIN	42	/* RW */
#define SENSORS_PC87360_FAN3_MIN	43	/* RW */
#define SENSORS_PC87360_FAN1_DIV	71	/* R */
#define SENSORS_PC87360_FAN2_DIV	72	/* R */
#define SENSORS_PC87360_FAN3_DIV	73	/* R */
#define SENSORS_PC87360_FAN1_STATUS	81	/* R */
#define SENSORS_PC87360_FAN2_STATUS	82	/* R */
#define SENSORS_PC87360_FAN3_STATUS	83	/* R */

#define SENSORS_PC87360_IN0		90	/* R */
#define SENSORS_PC87360_IN1		91	/* R */
#define SENSORS_PC87360_IN2		92	/* R */
#define SENSORS_PC87360_IN3		93	/* R */
#define SENSORS_PC87360_IN4		94	/* R */
#define SENSORS_PC87360_IN5		95	/* R */
#define SENSORS_PC87360_IN6		96	/* R */
#define SENSORS_PC87360_IN7		97	/* R */
#define SENSORS_PC87360_IN8		98	/* R */
#define SENSORS_PC87360_IN9		99	/* R */
#define SENSORS_PC87360_IN10		100	/* R */
#define SENSORS_PC87360_IN0_MIN		110	/* RW */
#define SENSORS_PC87360_IN1_MIN		111	/* RW */
#define SENSORS_PC87360_IN2_MIN		112	/* RW */
#define SENSORS_PC87360_IN3_MIN		113	/* RW */
#define SENSORS_PC87360_IN4_MIN		114	/* RW */
#define SENSORS_PC87360_IN5_MIN		115	/* RW */
#define SENSORS_PC87360_IN6_MIN		116	/* RW */
#define SENSORS_PC87360_IN7_MIN		117	/* RW */
#define SENSORS_PC87360_IN8_MIN		118	/* RW */
#define SENSORS_PC87360_IN9_MIN		119	/* RW */
#define SENSORS_PC87360_IN10_MIN	120	/* RW */
#define SENSORS_PC87360_IN0_MAX		130	/* RW */
#define SENSORS_PC87360_IN1_MAX		131	/* RW */
#define SENSORS_PC87360_IN2_MAX		132	/* RW */
#define SENSORS_PC87360_IN3_MAX		133	/* RW */
#define SENSORS_PC87360_IN4_MAX		134	/* RW */
#define SENSORS_PC87360_IN5_MAX		135	/* RW */
#define SENSORS_PC87360_IN6_MAX		136	/* RW */
#define SENSORS_PC87360_IN7_MAX		137	/* RW */
#define SENSORS_PC87360_IN8_MAX		138	/* RW */
#define SENSORS_PC87360_IN9_MAX		139	/* RW */
#define SENSORS_PC87360_IN10_MAX	140	/* RW */
#define SENSORS_PC87360_IN0_STATUS	150	/* R */
#define SENSORS_PC87360_IN1_STATUS	151	/* R */
#define SENSORS_PC87360_IN2_STATUS	152	/* R */
#define SENSORS_PC87360_IN3_STATUS	153	/* R */
#define SENSORS_PC87360_IN4_STATUS	154	/* R */
#define SENSORS_PC87360_IN5_STATUS	155	/* R */
#define SENSORS_PC87360_IN6_STATUS	156	/* R */
#define SENSORS_PC87360_IN7_STATUS	157	/* R */
#define SENSORS_PC87360_IN8_STATUS	158	/* R */
#define SENSORS_PC87360_IN9_STATUS	159	/* R */
#define SENSORS_PC87360_IN10_STATUS	160	/* R */

#define SENSORS_PC87360_TEMP1		171	/* R */
#define SENSORS_PC87360_TEMP2		172	/* R */
#define SENSORS_PC87360_TEMP3		173	/* R */
#define SENSORS_PC87360_TEMP4		174	/* R */
#define SENSORS_PC87360_TEMP5		175	/* R */
#define SENSORS_PC87360_TEMP6		176	/* R */
#define SENSORS_PC87360_TEMP1_MIN	181	/* RW */
#define SENSORS_PC87360_TEMP2_MIN	182	/* RW */
#define SENSORS_PC87360_TEMP3_MIN	183	/* RW */
#define SENSORS_PC87360_TEMP4_MIN	184	/* RW */
#define SENSORS_PC87360_TEMP5_MIN	185	/* RW */
#define SENSORS_PC87360_TEMP6_MIN	186	/* RW */
#define SENSORS_PC87360_TEMP1_MAX	191	/* RW */
#define SENSORS_PC87360_TEMP2_MAX	192	/* RW */
#define SENSORS_PC87360_TEMP3_MAX	193	/* RW */
#define SENSORS_PC87360_TEMP4_MAX	194	/* RW */
#define SENSORS_PC87360_TEMP5_MAX	195	/* RW */
#define SENSORS_PC87360_TEMP6_MAX	196	/* RW */
#define SENSORS_PC87360_TEMP1_CRIT	201	/* RW */
#define SENSORS_PC87360_TEMP2_CRIT	202	/* RW */
#define SENSORS_PC87360_TEMP3_CRIT	203	/* RW */
#define SENSORS_PC87360_TEMP4_CRIT	204	/* RW */
#define SENSORS_PC87360_TEMP5_CRIT	205	/* RW */
#define SENSORS_PC87360_TEMP6_CRIT	206	/* RW */
#define SENSORS_PC87360_TEMP1_STATUS	211	/* R */
#define SENSORS_PC87360_TEMP2_STATUS	212	/* R */
#define SENSORS_PC87360_TEMP3_STATUS	213	/* R */
#define SENSORS_PC87360_TEMP4_STATUS	214	/* R */
#define SENSORS_PC87360_TEMP5_STATUS	215	/* R */
#define SENSORS_PC87360_TEMP6_STATUS	216	/* R */

#define SENSORS_PC87360_VID		240	/* R */
#define SENSORS_PC87360_VRM		241	/* RW */

#define SENSORS_PC87427_PREFIX "pc87427"

/* fan n from 1 to 8 */
#define SENSORS_PC87427_FAN(n)		(n)		/* R */
#define SENSORS_PC87427_FAN_MIN(n)	(16 + (n))	/* RW */
#define SENSORS_PC87427_FAN_ALARM(n)	(32 + (n))	/* R */
#define SENSORS_PC87427_FAN_FAULT(n)	(48 + (n))	/* R */

#define SENSORS_LM92_PREFIX "lm92"

#define SENSORS_LM92_TEMP_HIGH		1	/* RW */
#define SENSORS_LM92_TEMP_LOW		2	/* RW */
#define SENSORS_LM92_TEMP_CRIT		3	/* RW */
#define SENSORS_LM92_TEMP_HYST		4	/* RW */
#define SENSORS_LM92_TEMP			5	/* R */
#define SENSORS_LM92_ALARMS			6	/* R */

#define SENSORS_VT8231_PREFIX "vt8231"

#define SENSORS_VT8231_IN0 1 /* R */
#define SENSORS_VT8231_IN1 2 /* R */
#define SENSORS_VT8231_IN2 3 /* R */
#define SENSORS_VT8231_IN3 4 /* R */
#define SENSORS_VT8231_IN4 5 /* R */
#define SENSORS_VT8231_IN5 6 /* R */
#define SENSORS_VT8231_IN0_MIN 11 /* RW */
#define SENSORS_VT8231_IN1_MIN 12 /* RW */
#define SENSORS_VT8231_IN2_MIN 13 /* RW */
#define SENSORS_VT8231_IN3_MIN 14 /* RW */
#define SENSORS_VT8231_IN4_MIN 15 /* RW */
#define SENSORS_VT8231_IN5_MIN 16 /* RW */
#define SENSORS_VT8231_IN0_MAX 21 /* RW */
#define SENSORS_VT8231_IN1_MAX 22 /* RW */
#define SENSORS_VT8231_IN2_MAX 23 /* RW */
#define SENSORS_VT8231_IN3_MAX 24 /* RW */
#define SENSORS_VT8231_IN4_MAX 25 /* RW */
#define SENSORS_VT8231_IN5_MAX 26 /* RW */
#define SENSORS_VT8231_FAN1 31 /* R */
#define SENSORS_VT8231_FAN2 32 /* R */
#define SENSORS_VT8231_FAN1_MIN 41 /* RW */
#define SENSORS_VT8231_FAN2_MIN 42 /* RW */
#define SENSORS_VT8231_TEMP 51 /* R */
#define SENSORS_VT8231_TEMP_HYST 52 /* RW */
#define SENSORS_VT8231_TEMP_OVER 53 /* RW */
#define SENSORS_VT8231_TEMP2 54 /* R */
#define SENSORS_VT8231_TEMP2_HYST 55 /* RW */
#define SENSORS_VT8231_TEMP2_OVER 56 /* RW */
#define SENSORS_VT8231_TEMP3 57 /* R */
#define SENSORS_VT8231_TEMP3_HYST 58 /* RW */
#define SENSORS_VT8231_TEMP3_OVER 59 /* RW */
#define SENSORS_VT8231_TEMP4 60 /* R */
#define SENSORS_VT8231_TEMP4_HYST 61 /* RW */
#define SENSORS_VT8231_TEMP4_OVER 62 /* RW */
#define SENSORS_VT8231_TEMP5 63 /* R */
#define SENSORS_VT8231_TEMP5_HYST 64 /* RW */
#define SENSORS_VT8231_TEMP5_OVER 65 /* RW */
#define SENSORS_VT8231_TEMP6 66 /* R */
#define SENSORS_VT8231_TEMP6_HYST 67 /* RW */
#define SENSORS_VT8231_TEMP6_OVER 68 /* RW */
#define SENSORS_VT8231_FAN1_DIV 75 /* RW */
#define SENSORS_VT8231_FAN2_DIV 76 /* RW */
#define SENSORS_VT8231_ALARMS 81 /* R */
#define SENSORS_VT8231_VID 82 /* R */
#define SENSORS_VT8231_VRM 83 /* RW */
#define SENSORS_VT8231_UCH 84 /* RW */

#define SENSORS_BMC_PREFIX "bmc"

/* quantity of each sensor is unknown, so just define the
   first one of each and keep them 100 apart. */
#define SENSORS_BMC_ALARMS 1 /* R */
#define SENSORS_BMC_IN1 101 /* R */
#define SENSORS_BMC_IN1_MIN 201 /* RW */
#define SENSORS_BMC_IN1_MAX 301 /* RW */
#define SENSORS_BMC_FAN1 1001 /* R */
#define SENSORS_BMC_FAN1_MIN 1101 /* RW */
#define SENSORS_BMC_TEMP1 2001 /* R */
#define SENSORS_BMC_TEMP1_MIN 2101 /* RW */
#define SENSORS_BMC_TEMP1_MAX 2201 /* RW */
#define SENSORS_BMC_CURR1 3001 /* R */
#define SENSORS_BMC_CURR1_MIN 3101 /* RW */
#define SENSORS_BMC_CURR1_MAX 3201 /* RW */

#define SENSORS_LM93_PREFIX "lm93"

#define SENSORS_LM93_IN1		1011
#define SENSORS_LM93_IN1_MIN		1012
#define SENSORS_LM93_IN1_MAX		1013
#define SENSORS_LM93_IN2		1021
#define SENSORS_LM93_IN2_MIN		1022
#define SENSORS_LM93_IN2_MAX		1023
#define SENSORS_LM93_IN3		1031
#define SENSORS_LM93_IN3_MIN		1032
#define SENSORS_LM93_IN3_MAX		1033
#define SENSORS_LM93_IN4		1041
#define SENSORS_LM93_IN4_MIN		1042
#define SENSORS_LM93_IN4_MAX		1043
#define SENSORS_LM93_IN5		1051
#define SENSORS_LM93_IN5_MIN		1052
#define SENSORS_LM93_IN5_MAX		1053
#define SENSORS_LM93_IN6		1061
#define SENSORS_LM93_IN6_MIN		1062
#define SENSORS_LM93_IN6_MAX		1063
#define SENSORS_LM93_IN7		1071
#define SENSORS_LM93_IN7_MIN		1072
#define SENSORS_LM93_IN7_MAX		1073
#define SENSORS_LM93_IN8		1081
#define SENSORS_LM93_IN8_MIN		1082
#define SENSORS_LM93_IN8_MAX		1083
#define SENSORS_LM93_IN9		1091
#define SENSORS_LM93_IN9_MIN		1092
#define SENSORS_LM93_IN9_MAX		1093
#define SENSORS_LM93_IN10		1101
#define SENSORS_LM93_IN10_MIN		1102
#define SENSORS_LM93_IN10_MAX		1103
#define SENSORS_LM93_IN11		1111
#define SENSORS_LM93_IN11_MIN		1112
#define SENSORS_LM93_IN11_MAX		1113
#define SENSORS_LM93_IN12		1121
#define SENSORS_LM93_IN12_MIN		1122
#define SENSORS_LM93_IN12_MAX		1123
#define SENSORS_LM93_IN13		1131
#define SENSORS_LM93_IN13_MIN		1132
#define SENSORS_LM93_IN13_MAX		1133
#define SENSORS_LM93_IN14		1141
#define SENSORS_LM93_IN14_MIN		1142
#define SENSORS_LM93_IN14_MAX		1143
#define SENSORS_LM93_IN15		1151
#define SENSORS_LM93_IN15_MIN		1152
#define SENSORS_LM93_IN15_MAX		1153
#define SENSORS_LM93_IN16		1161
#define SENSORS_LM93_IN16_MIN		1162
#define SENSORS_LM93_IN16_MAX		1163
#define SENSORS_LM93_TEMP1		2011
#define SENSORS_LM93_TEMP1_MIN		2012
#define SENSORS_LM93_TEMP1_MAX		2013
#define SENSORS_LM93_TEMP2		2021
#define SENSORS_LM93_TEMP2_MIN		2022
#define SENSORS_LM93_TEMP2_MAX		2023
#define SENSORS_LM93_TEMP3		2031
#define SENSORS_LM93_TEMP3_MIN		2032
#define SENSORS_LM93_TEMP3_MAX		2033
#define SENSORS_LM93_FAN1		3011
#define SENSORS_LM93_FAN1_MIN		3012
#define SENSORS_LM93_FAN2		3021
#define SENSORS_LM93_FAN2_MIN		3022
#define SENSORS_LM93_FAN3		3031
#define SENSORS_LM93_FAN3_MIN		3032
#define SENSORS_LM93_FAN4		3041
#define SENSORS_LM93_FAN4_MIN		3042
#define SENSORS_LM93_VID1		4001
#define SENSORS_LM93_VID2		4002
#define SENSORS_LM93_ALARMS		5001

#define SENSORS_ASB100_PREFIX "asb100"

#define SENSORS_ASB100_IN0		0x01 /* R */
#define SENSORS_ASB100_IN1		0x02 /* R */
#define SENSORS_ASB100_IN2		0x03 /* R */
#define SENSORS_ASB100_IN3		0x04 /* R */
#define SENSORS_ASB100_IN4		0x05 /* R */
#define SENSORS_ASB100_IN5		0x06 /* R */
#define SENSORS_ASB100_IN6		0x07 /* R */
#define SENSORS_ASB100_IN0_MIN		0x11 /* RW */
#define SENSORS_ASB100_IN1_MIN		0x12 /* RW */
#define SENSORS_ASB100_IN2_MIN		0x13 /* RW */
#define SENSORS_ASB100_IN3_MIN		0x14 /* RW */
#define SENSORS_ASB100_IN4_MIN		0x15 /* RW */
#define SENSORS_ASB100_IN5_MIN		0x16 /* RW */
#define SENSORS_ASB100_IN6_MIN		0x17 /* RW */
#define SENSORS_ASB100_IN0_MAX		0x21 /* RW */
#define SENSORS_ASB100_IN1_MAX		0x22 /* RW */
#define SENSORS_ASB100_IN2_MAX		0x23 /* RW */
#define SENSORS_ASB100_IN3_MAX		0x24 /* RW */
#define SENSORS_ASB100_IN4_MAX		0x25 /* RW */
#define SENSORS_ASB100_IN5_MAX		0x26 /* RW */
#define SENSORS_ASB100_IN6_MAX		0x27 /* RW */
#define SENSORS_ASB100_FAN1		0x31 /* R */
#define SENSORS_ASB100_FAN2		0x32 /* R */
#define SENSORS_ASB100_FAN3		0x33 /* R */
#define SENSORS_ASB100_FAN1_MIN		0x41 /* RW */
#define SENSORS_ASB100_FAN2_MIN		0x42 /* RW */
#define SENSORS_ASB100_FAN3_MIN		0x43 /* RW */
#define SENSORS_ASB100_TEMP1		0x51 /* R */
#define SENSORS_ASB100_TEMP1_HYST	0x52 /* RW */
#define SENSORS_ASB100_TEMP1_OVER	0x53 /* RW */
#define SENSORS_ASB100_TEMP2		0x54 /* R */
#define SENSORS_ASB100_TEMP2_HYST	0x55 /* RW */
#define SENSORS_ASB100_TEMP2_OVER	0x56 /* RW */
#define SENSORS_ASB100_TEMP3		0x57 /* R */
#define SENSORS_ASB100_TEMP3_HYST	0x58 /* RW */
#define SENSORS_ASB100_TEMP3_OVER	0x59 /* RW */
#define SENSORS_ASB100_TEMP4		0x5a /* R */
#define SENSORS_ASB100_TEMP4_HYST	0x5b /* RW */
#define SENSORS_ASB100_TEMP4_OVER	0x5c /* RW */
#define SENSORS_ASB100_VID		0x61 /* R */
#define SENSORS_ASB100_VRM		0x62 /* RW */
#define SENSORS_ASB100_FAN1_DIV		0x71 /* RW */
#define SENSORS_ASB100_FAN2_DIV		0x72 /* RW */
#define SENSORS_ASB100_FAN3_DIV		0x73 /* RW */
#define SENSORS_ASB100_ALARMS		0x81 /* R */
#define SENSORS_ASB100_BEEP_ENABLE	0x82 /* RW */
#define SENSORS_ASB100_BEEPS		0x83 /* RW */

#define SENSORS_XEONTEMP_PREFIX "xeontemp"

#define SENSORS_XEONTEMP_REMOTE_TEMP 54 /* R */
#define SENSORS_XEONTEMP_REMOTE_TEMP_HYST 55 /* RW */
#define SENSORS_XEONTEMP_REMOTE_TEMP_OVER 56 /* RW */
#define SENSORS_XEONTEMP_ALARMS 81 /* R */

/* MAX1619 chip */

#define SENSORS_MAX1619_PREFIX "max1619"

#define SENSORS_MAX1619_LOCAL_TEMP      51      /* R */
#define SENSORS_MAX1619_REMOTE_TEMP     52      /* R */
#define SENSORS_MAX1619_REMOTE_LOW      53      /* RW */
#define SENSORS_MAX1619_REMOTE_HIGH     54      /* RW */
#define SENSORS_MAX1619_REMOTE_MAX      55      /* RW */
#define SENSORS_MAX1619_REMOTE_HYST     56      /* RW */
#define SENSORS_MAX1619_ALARMS          81      /* R */

/* MAX6650 / 1 chips */

#define SENSORS_MAX6650_PREFIX "max6650"

#define SENSORS_MAX6650_FAN1_TACH  1 /* R */
#define SENSORS_MAX6650_FAN2_TACH  2 /* R */
#define SENSORS_MAX6650_FAN3_TACH  3 /* R */
#define SENSORS_MAX6650_FAN4_TACH  4 /* R */
#define SENSORS_MAX6650_SPEED      5 /* RW */

/* SMSC47B397-NC chip */
#define SENSORS_SMSC47B397_PREFIX "smsc47b397"

#define SENSORS_SMSC47B397_TEMP1	0x01 /* R */
#define SENSORS_SMSC47B397_TEMP2	0x02 /* R */
#define SENSORS_SMSC47B397_TEMP3	0x03 /* R */
#define SENSORS_SMSC47B397_TEMP4	0x04 /* R */
#define SENSORS_SMSC47B397_FAN1		0x11 /* R */
#define SENSORS_SMSC47B397_FAN2		0x12 /* R */
#define SENSORS_SMSC47B397_FAN3		0x13 /* R */
#define SENSORS_SMSC47B397_FAN4		0x14 /* R */

/* Fintek F71805F/FG and F71872F/FG chips */
#define SENSORS_F71805F_PREFIX		"f71805f"
#define SENSORS_F71872F_PREFIX		"f71872f"

/* in n from 0 to 10 */
#define SENSORS_F71805F_IN(n)		(1 + (n))
#define SENSORS_F71805F_IN_MIN(n)	(16 + (n))
#define SENSORS_F71805F_IN_MAX(n)	(31 + (n))
/* fan n from 1 to 3 */
#define SENSORS_F71805F_FAN(n)		(50 + (n))
#define SENSORS_F71805F_FAN_MIN(n)	(60 + (n))
/* temp n from 1 to 3 */
#define SENSORS_F71805F_TEMP(n)		(80 + (n))
#define SENSORS_F71805F_TEMP_MAX(n)	(90 + (n))
#define SENSORS_F71805F_TEMP_HYST(n)	(100 + (n))
#define SENSORS_F71805F_TEMP_TYPE(n)	(110 + (n))
/* alarms */
#define SENSORS_F71805F_ALARMS_IN	200
#define SENSORS_F71805F_ALARMS_FAN	201
#define SENSORS_F71805F_ALARMS_TEMP	202

/* Abit uGuru chips */
#define SENSORS_ABITUGURU_PREFIX "abituguru"
#define SENSORS_ABITUGURU3_PREFIX "abituguru3"

/* in n from 0 to 10 */
#define SENSORS_ABITUGURU_IN(n)			(0x01 + (n)) /* R */
#define SENSORS_ABITUGURU_IN_MIN(n)		(0x11 + (n)) /* RW */
#define SENSORS_ABITUGURU_IN_MIN_ALARM(n)	(0x21 + (n)) /* R */
#define SENSORS_ABITUGURU_IN_MAX(n)		(0x31 + (n)) /* RW */
#define SENSORS_ABITUGURU_IN_MAX_ALARM(n)	(0x41 + (n)) /* R */
/* temp n from 1 to 7 */
#define SENSORS_ABITUGURU_TEMP(n)		(0x50 + (n)) /* R */
#define SENSORS_ABITUGURU_TEMP_ALARM(n)		(0x60 + (n)) /* R */
#define SENSORS_ABITUGURU_TEMP_MAX(n)		(0x70 + (n)) /* RW */
#define SENSORS_ABITUGURU_TEMP_CRIT(n)		(0x80 + (n)) /* RW */
/* fan n from 1 to 6 */
#define SENSORS_ABITUGURU_FAN(n)		(0x90 + (n)) /* R */
#define SENSORS_ABITUGURU_FAN_ALARM(n)		(0xA0 + (n)) /* R */
#define SENSORS_ABITUGURU_FAN_MIN(n)		(0xB0 + (n)) /* RW */

/* K8TEMP */
#define SENSORS_K8TEMP_PREFIX "k8temp"
#define SENSORS_K8TEMP_TEMP1	0x01 /* R */
#define SENSORS_K8TEMP_TEMP2	0x02 /* R */
#define SENSORS_K8TEMP_TEMP3	0x03 /* R */
#define SENSORS_K8TEMP_TEMP4	0x04 /* R */

/* coretemp */

#define SENSORS_CORETEMP_PREFIX "coretemp"
#define SENSORS_CORETEMP_TEMP1			0x01 /* R */
#define SENSORS_CORETEMP_TEMP1_CRIT		0x02 /* R */
#define SENSORS_CORETEMP_TEMP1_CRIT_ALARM	0x03 /* R */

/* DME1737 chips */

#define SENSORS_DME1737_PREFIX "dme1737"
#define SENSORS_SCH311X_PREFIX "sch311x"

/* in n from 0 to 6 */
#define SENSORS_DME1737_IN(n)			(0x01 + (n)) /* R */
#define SENSORS_DME1737_IN_MIN(n)		(0x11 + (n)) /* RW */
#define SENSORS_DME1737_IN_MAX(n)		(0x21 + (n)) /* RW */
#define SENSORS_DME1737_IN_ALARM(n)		(0x31 + (n)) /* R */

/* temp n from 1 to 3*/
#define SENSORS_DME1737_TEMP(n)			(0x41 + (n)) /* R */
#define SENSORS_DME1737_TEMP_MIN(n)		(0x51 + (n)) /* RW */
#define SENSORS_DME1737_TEMP_MAX(n)		(0x61 + (n)) /* RW */
#define SENSORS_DME1737_TEMP_ALARM(n)		(0x71 + (n)) /* R */
#define SENSORS_DME1737_TEMP_FAULT(n)		(0x81 + (n)) /* R */

/* fan n from 1 to 6 */
#define SENSORS_DME1737_FAN(n)			(0x91 + (n)) /* R */
#define SENSORS_DME1737_FAN_MIN(n)		(0xa1 + (n)) /* RW */
#define SENSORS_DME1737_FAN_ALARM(n)		(0xb1 + (n)) /* R */

/* pwm n from 1 to 3 and 5 to 6 */
#define SENSORS_DME1737_PWM(n)			(0xc1 + (n)) /* RW */
#define SENSORS_DME1737_PWM_ENABLE(n)		(0xd1 + (n)) /* RW */
#define SENSORS_DME1737_PWM_FREQ(n)		(0xe1 + (n)) /* RW */

#define SENSORS_DME1737_VID			(0xf0) /* R */
#define SENSORS_DME1737_VRM			(0xf1) /* RW */

/* applesmc */

#define SENSORS_APPLESMC_PREFIX "applesmc"

/* temp n from 0 to 11 */
#define SENSORS_APPLESMC_TEMP(n)		(0x01 + (n)) /* R */

/* fan n from 0 to 1 */
#define SENSORS_APPLESMC_FAN(n)			(0x21 + (n)) /* R */
#define SENSORS_APPLESMC_FAN_MIN(n)		(0x41 + (n)) /* R */
#define SENSORS_APPLESMC_FAN_MAX(n)		(0x61 + (n)) /* R */
#define SENSORS_APPLESMC_FAN_SAFE(n)		(0x81 + (n)) /* R */

/* Fintek F71882FG and F71883FG chips */
#define SENSORS_F71882FG_PREFIX		"f71882fg"

/* in n from 0 to 8 */
#define SENSORS_F71882FG_IN(n)			(1 + (n))
#define SENSORS_F71882FG_IN_MAX(n)		(16 + (n))
#define SENSORS_F71882FG_IN_ALARM(n)		(31 + (n))
/* fan n from 1 to 4 */
#define SENSORS_F71882FG_FAN(n)			(50 + (n))
#define SENSORS_F71882FG_FAN_ALARM(n)		(60 + (n))
/* temp n from 1 to 3 */
#define SENSORS_F71882FG_TEMP(n)		(80 + (n))
#define SENSORS_F71882FG_TEMP_MAX(n)		(90 + (n))
#define SENSORS_F71882FG_TEMP_MAX_HYST(n)	(100 + (n))
#define SENSORS_F71882FG_TEMP_CRIT(n)		(110 + (n))
#define SENSORS_F71882FG_TEMP_CRIT_HYST(n)	(120 + (n))
#define SENSORS_F71882FG_TEMP_ALARM(n)		(130 + (n))
#define SENSORS_F71882FG_TEMP_FAULT(n)		(140 + (n))
#define SENSORS_F71882FG_TEMP_TYPE(n)		(150 + (n))

/* Fujitsu Siemens Computers Heimdal and Heracles */
#define SENSORS_FSCHMD_PREFIX		"fschmd"
#define SENSORS_FSCHRC_PREFIX		"fschrc"

/* Note, we start with feature numbers of 60+ to not have any overlapping
   features with the old FSCPOS, FSCSCY and FSCHER feature lists, as the
   FSCHMD feature defines for features only exported by the new FSCHMD driver
   are reused in the feature lists of the FSCPOS, FSCSCY and FSCHER */
   
/* in n from 0 to 3 */
#define SENSORS_FSCHMD_IN(n)			(60 + (n))
/* fan n from 1 to 5 (4 for the hrc) */
#define SENSORS_FSCHMD_FAN(n)			(70 + (n))
#define SENSORS_FSCHMD_FAN_DIV(n)		(80 + (n))
#define SENSORS_FSCHMD_FAN_ALARM(n)		(90 + (n))
#define SENSORS_FSCHMD_FAN_FAULT(n)		(100 + (n))
/* temp n from 1 to 5 (3 for the hrc) */
#define SENSORS_FSCHMD_TEMP(n)			(110 + (n))
#define SENSORS_FSCHMD_TEMP_MAX(n)		(120 + (n))
#define SENSORS_FSCHMD_TEMP_ALARM(n)		(130 + (n))
#define SENSORS_FSCHMD_TEMP_FAULT(n)		(140 + (n))

#endif /* def LIB_SENSORS_CHIPS_H */
