/*
    lm93.h - Part of lm_sensors, Linux kernel modules for hardware monitoring

    Author/Maintainer: Mark M. Hoffman <mhoffman@lightlink.com>
	Copyright (c) 2004 Utilitek Systems, Inc.

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

#ifndef LM93_H
#define LM93_H

/* LM93 REGISTER ADDRESSES */

/* miscellaneous */
#define LM93_REG_MFR_ID			0x3e
#define LM93_REG_VER			0x3f
#define LM93_REG_STATUS_CONTROL		0xe2
#define LM93_REG_CONFIG			0xe3
#define LM93_REG_SLEEP_CONTROL		0xe4

/* alarm values start here */
#define LM93_REG_HOST_ERROR_1		0x48

/* voltage inputs: in1-in16 (nr => 0-15) */
#define LM93_REG_IN(nr)			(0x56 + (nr))
#define LM93_REG_IN_MIN(nr)		(0x90 + (nr) * 2)
#define LM93_REG_IN_MAX(nr)		(0x91 + (nr) * 2)

/* temperature inputs: temp1-temp4 (nr => 0-3) */
#define LM93_REG_TEMP(nr)		(0x50 + (nr))
#define LM93_REG_TEMP_MIN(nr)		(0x78 + (nr) * 2)
#define LM93_REG_TEMP_MAX(nr)		(0x79 + (nr) * 2)

/* temp[1-4]_auto_boost (nr => 0-3) */
#define LM93_REG_BOOST(nr)		(0x80 + (nr))

/* #PROCHOT inputs: prochot1-prochot2 (nr => 0-1) */
#define LM93_REG_PROCHOT_CUR(nr)	(0x67 + (nr) * 2)
#define LM93_REG_PROCHOT_AVG(nr)	(0x68 + (nr) * 2)
#define LM93_REG_PROCHOT_MAX(nr)	(0xb0 + (nr))

/* fan tach inputs: fan1-fan4 (nr => 0-3) */
#define LM93_REG_FAN(nr)		(0x6e + (nr) * 2)
#define LM93_REG_FAN_MIN(nr)		(0xb4 + (nr) * 2)

/* pwm outputs: pwm1-pwm2 (nr => 0-1, reg => 0-3) */
#define LM93_REG_PWM_CTL(nr,reg)	(0xc8 + (reg) + (nr) * 4)
#define LM93_PWM_CTL1	0
#define LM93_PWM_CTL2	1
#define LM93_PWM_CTL3	2
#define LM93_PWM_CTL4	3

/* GPIO input state */
#define LM93_REG_GPI			0x6b

/* vid inputs: vid1-vid2 (nr => 0-1) */
#define LM93_REG_VID(nr)		(0x6c + (nr))

/* vccp1 & vccp2: VID relative inputs (nr => 0-1) */
#define LM93_REG_VCCP_LIMIT_OFF(nr)	(0xb2 + (nr))

/* temp[1-4]_auto_boost_hyst */
#define LM93_REG_BOOST_HYST_12		0xc0
#define LM93_REG_BOOST_HYST_34		0xc1
#define LM93_REG_BOOST_HYST(nr)		(0xc0 + (nr)/2)

/* temp[1-4]_auto_pwm_[min|hyst] */
#define LM93_REG_PWM_MIN_HYST_12	0xc3
#define LM93_REG_PWM_MIN_HYST_34	0xc4
#define LM93_REG_PWM_MIN_HYST(nr)	(0xc3 + (nr)/2)

/* prochot_override & prochot_interval */
#define LM93_REG_PROCHOT_OVERRIDE	0xc6
#define LM93_REG_PROCHOT_INTERVAL	0xc7

/* temp[1-4]_auto_base (nr => 0-3) */
#define LM93_REG_TEMP_BASE(nr)		(0xd0 + (nr))

/* temp[1-4]_auto_offsets (step => 0-11) */
#define LM93_REG_TEMP_OFFSET(step)	(0xd4 + (step))

/* #PROCHOT & #VRDHOT PWM ramp control */
#define LM93_REG_PWM_RAMP_CTL		0xbf

/* miscellaneous */
#define LM93_REG_SFC1		0xbc
#define LM93_REG_SFC2		0xbd
#define LM93_REG_GPI_VID_CTL	0xbe
#define LM93_REG_SF_TACH_TO_PWM	0xe0

/* error masks */
#define LM93_REG_GPI_ERR_MASK	0xec
#define LM93_REG_MISC_ERR_MASK	0xed

/* LM93 REGISTER VALUES */
#define LM93_MFR_ID		0x73
#define LM93_MFR_ID_PROTOTYPE	0x72

#endif /* !defined LM93_H */

