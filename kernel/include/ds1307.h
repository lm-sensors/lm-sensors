#ifndef DS1307_H
#define DS1307_H

/*
 * linux/include/linux/ds1307.h
 *
 * Author: Abraham van der Merwe <abraham@2d3d.co.za>
 *
 * Linux support for the Dallas Semiconductor DS1307 Serial Real-Time
 * Clock.
 *
 * Based on code from the lm-sensors project which is available
 * at http://www.lm-sensors.nu/ and Russell King's PCF8583 Real-Time
 * Clock driver (linux/drivers/acorn/char/pcf8583.c).
 *
 * This source code is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */

#include <linux/types.h>

/* practically the same as struct rtc_time, but without tm_yday and tm_isdst */
struct ds1307_date {
	int tm_sec;
	int tm_min;
	int tm_hour;
	int tm_mday;
	int tm_mon;
	int tm_year;
	int tm_wday;
};

struct ds1307_memory {
	u8 offset;		/* 0 - 55 */
	u8 buf[56];		/* data */
	u8 length;		/* offset + length <= 50 */
};

/* size of the rtc non-volatile ram */
#define DS1307_SIZE	56

/* the following frequencies are supported */
#define DS1307_FREQ_1HZ		1
#define DS1307_FREQ_4KHZ	4096
#define DS1307_FREQ_8KHZ	8192
#define DS1307_FREQ_32KHZ	32768

#define DS1307_GET_DATE	_IOR ('d',0,struct ds1307_date *)
#define DS1307_SET_DATE	_IOW ('d',1,struct ds1307_date *)
#define DS1307_IRQ_ON	_IO  ('d',2)
#define DS1307_IRQ_OFF	_IO  ('d',3)
#define DS1307_GET_FREQ	_IOR ('d',4,u16 *)
#define DS1307_SET_FREQ	_IOW ('d',5,u16 *)
#define DS1307_READ		_IOR ('d',6,struct ds1307_memory *)
#define DS1307_WRITE	_IOW ('d',7,struct ds1307_memory *)
#define DS1307_ENABLE	_IO  ('d',8)

#endif	/* #ifdef DS1307_H */
