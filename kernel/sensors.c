/*
    sensors.c - Part of lm_sensors, Linux kernel modules for hardware
                monitoring
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

/* Not configurable as a module */

#include <linux/init.h>

#ifdef CONFIG_SENSORS_ADM1021
extern int sensors_adm1021_init(void);
#endif
#ifdef CONFIG_SENSORS_ADM1025
extern int sensors_adm1025_init(void);
#endif
#ifdef CONFIG_SENSORS_ADM9240
extern int sensors_adm9240_init(void);
#endif
#ifdef CONFIG_SENSORS_BT869
extern int sensors_bt869_init(void);
#endif
#ifdef CONFIG_SENSORS_DDCMON
extern int sensors_ddcmon_init(void);
#endif
#ifdef CONFIG_SENSORS_DS1621
extern int sensors_ds1621_init(void);
#endif
#ifdef CONFIG_SENSORS_GL518SM
extern int sensors_gl518sm_init(void);
#endif
#ifdef CONFIG_SENSORS_GL520SM
extern int sensors_gl520_init(void);
#endif
#ifdef CONFIG_SENSORS_LM75
extern int sensors_lm75_init(void);
#endif
#ifdef CONFIG_SENSORS_LM78
extern int sensors_lm78_init(void);
#endif
#ifdef CONFIG_SENSORS_LM80
extern int sensors_lm80_init(void);
#endif
#ifdef CONFIG_SENSORS_LM87
extern int sensors_lm87_init(void);
#endif
#ifdef CONFIG_SENSORS_MTP008
extern int sensors_mtp008_init(void);
#endif
#ifdef CONFIG_SENSORS_SIS5595
extern int sensors_sis5595_init(void);
#endif
#ifdef CONFIG_SENSORS_THMC50
extern int sensors_thmc50_init(void);
#endif
#ifdef CONFIG_SENSORS_VIA686A
extern int sensors_via686a_init(void);
#endif
#ifdef CONFIG_SENSORS_W83781D
extern int sensors_w83781d_init(void);
#endif
#ifdef CONFIG_SENSORS_EEPROM
extern int sensors_eeprom_init(void);
#endif
#ifdef CONFIG_SENSORS_LTC1710
extern int sensors_ltc1710_init(void);
#endif
#ifdef CONFIG_SENSORS_IT87
extern int sensors_it87_init(void);
#endif

int __init sensors_init_all(void)
{
#ifdef CONFIG_SENSORS_ADM1021
	sensors_adm1021_init();
#endif
#ifdef CONFIG_SENSORS_ADM1025
	sensors_adm1025_init();
#endif
#ifdef CONFIG_SENSORS_ADM9240
	sensors_adm9240_init();
#endif
#ifdef CONFIG_SENSORS_BT869
	sensors_bt869_init();
#endif
#ifdef CONFIG_SENSORS_DDCMON
	sensors_ddcmon_init();
#endif
#ifdef CONFIG_SENSORS_DS1621
	sensors_ds1621_init();
#endif
#ifdef CONFIG_SENSORS_GL518SM
	sensors_gl518sm_init();
#endif
#ifdef CONFIG_SENSORS_GL520SM
	sensors_gl520_init();
#endif
#ifdef CONFIG_SENSORS_LM75
	sensors_lm75_init();
#endif
#ifdef CONFIG_SENSORS_LM78
	sensors_lm78_init();
#endif
#ifdef CONFIG_SENSORS_LM80
	sensors_lm80_init();
#endif
#ifdef CONFIG_SENSORS_LM87
	sensors_lm87_init();
#endif
#ifdef CONFIG_SENSORS_MTP008
	sensors_mtp008_init();
#endif
#ifdef CONFIG_SENSORS_SIS5595
	sensors_sis5595_init();
#endif
#ifdef CONFIG_SENSORS_THMC50
	sensors_thmc50_init();
#endif
#ifdef CONFIG_SENSORS_VIA686A
	sensors_via686a_init();
#endif
#ifdef CONFIG_SENSORS_W83781D
	sensors_w83781d_init();
#endif
#ifdef CONFIG_SENSORS_EEPROM
	sensors_eeprom_init();
#endif
#ifdef CONFIG_SENSORS_LTC1710
	sensors_ltc1710_init();
#endif
#ifdef CONFIG_SENSORS_IT87
	sensors_it87_init();
#endif
	return 0;
}
