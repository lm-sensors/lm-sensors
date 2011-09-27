/*
    maxilife.c - Part of lm_sensors, Linux kernel modules for hardware
                 monitoring
    Copyright (c) 1999-2000 Fons Rademakers <Fons.Rademakers@cern.ch> 

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

/* The is the driver for the HP MaxiLife Health monitoring system
   as used in the line of HP Kayak Workstation PC's.
   
   The driver supports the following MaxiLife firmware versions:
   
   0) HP KAYAK XU/XAs (Dual Pentium II Slot 1, Deschutes/Klamath)
   1) HP KAYAK XU (Dual Xeon [Slot 2] 400/450 Mhz)
   2) HP KAYAK XA (Pentium II Slot 1, monoprocessor)
   
   Currently firmware auto detection is not implemented. To use the
   driver load it with the correct option for you Kayak. For example:
   
   insmod maxilife.o maxi_version=0 | 1 | 2
   
   maxi_version=0 is the default
   
   This version of MaxiLife is called MaxiLife'98 and has been
   succeeded by MaxiLife'99, see below.
   
   The new version of the driver also supports MaxiLife NBA (New BIOS
   Architecture). This new MaxiLife controller provides a much cleaner
   machine independent abstraction layer to the MaxiLife controller.
   Instead of accessing directly registers (different for each revision)
   one now accesses the sensors via unique mailbox tokens that do not
   change between revisions. Also the quantities are already in physical
   units (degrees, rpms, voltages, etc.) and don't need special conversion
   formulas. This new MaxiLife is available on the new 2000 machines,
   like the Kayak XU800 and XM600. This hardware is also autodetected.
*/

static const char *version_str = "2.00 29/2/2000 Fons Rademakers";


#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/i2c-proc.h>
#include <linux/init.h>
#include "version.h"

MODULE_LICENSE("GPL");

#undef AUTODETECT		/* try to autodetect MaxiLife version */
/*#define AUTODETECT*/
#define NOWRITE			/* don't allow writing to MaxiLife registers */

#ifdef AUTODETECT
#include <linux/vmalloc.h>
#include <linux/ctype.h>
#endif

/* Addresses to scan */
static unsigned short normal_i2c[] = { SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { 0x10, 0x14, SENSORS_I2C_END };
static unsigned int normal_isa[] = { SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/* Insmod parameters */
SENSORS_INSMOD_1(maxilife);

/* Macro definitions */
#define LOW(MyWord) ((u8) (MyWord))
#define HIGH(MyWord) ((u8) (((u16)(MyWord) >> 8) & 0xFF))

/*----------------- MaxiLife'98 registers and conversion formulas ------------*/
#define MAXI_REG_TEMP(nr)      (0x60 + (nr))

#define MAXI_REG_FAN(nr)       (0x65 + (nr))
#define MAXI_REG_FAN_MIN(nr)   ((nr)==0 ? 0xb3 : (nr)==1 ? 0xb3 : 0xab)
#define MAXI_REG_FAN_MINAS(nr) ((nr)==0 ? 0xb3 : (nr)==1 ? 0xab : 0xb3)
#define MAXI_REG_FAN_SPEED(nr) ((nr)==0 ? 0xe4 : (nr)==1 ? 0xe5 : 0xe9)

#define MAXI_REG_PLL           0xb9
#define MAXI_REG_PLL_MIN       0xba
#define MAXI_REG_PLL_MAX       0xbb

#define MAXI_REG_VID(nr)       ((nr)==0 ? 0xd1 : (nr)==1 ? 0xd9 : \
                                (nr)==2 ? 0xd4 : 0xc5)
#define MAXI_REG_VID_MIN(nr)   MAXI_REG_VID(nr)+1
#define MAXI_REG_VID_MAX(nr)   MAXI_REG_VID(nr)+2

#define MAXI_REG_DIAG_RT1      0x2c
#define MAXI_REG_DIAG_RT2      0x2d

#define MAXI_REG_BIOS_CTRL     0x2a

/* Conversions. Rounding and limit checking is only done on the TO_REG
   variants. Note that you should be a bit careful with which arguments
   these macros are called: arguments may be evaluated more than once.
   Fixing this is just not worth it. */

			       /* 0xfe: fan off, 0xff: stopped (alarm) */
			       /* 19531 / val * 60 == 1171860 / val */
#define FAN_FROM_REG(val)      ((val)==0xfe ? 0 : (val)==0xff ? -1 : \
                                (val)==0x00 ? -1 : (1171860 / (val)))

static inline u8 FAN_TO_REG(long rpm)
{
	if (rpm == 0)
		return 255;
	rpm = SENSORS_LIMIT(rpm, 1, 1000000);
	return SENSORS_LIMIT((1171860 + rpm / 2) / (rpm), 1, 254);
}

#define TEMP_FROM_REG(val)     ((val) * 5)
#define TEMP_TO_REG(val)       (SENSORS_LIMIT((val+2) / 5),0,0xff)
#define PLL_FROM_REG(val)      (((val) * 1000) / 32)
#define PLL_TO_REG(val)        (SENSORS_LIMIT((((val) * 32 + 500) / 1000),\
                                              0,0xff))
#define VID_FROM_REG(val)      ((val) ? (((val) * 27390) / 256) + 3208 : 0)
#define VID_TO_REG(val)        (SENSORS_LIMIT((((val) - 3208) * 256) / 27390, \
                                              0,255))
#define ALARMS_FROM_REG(val)   (val)

/*----------------- MaxiLife'99 mailbox and token definitions ----------------*/
/* MaxiLife mailbox data register map */
#define MAXI_REG_MBX_STATUS    0x5a
#define MAXI_REG_MBX_CMD       0x5b
#define MAXI_REG_MBX_TOKEN_H   0x5c
#define MAXI_REG_MBX_TOKEN_L   0x5d
#define MAXI_REG_MBX_DATA      0x60

/* Mailbox status register definition */
#define MAXI_STAT_IDLE         0xff
#define MAXI_STAT_OK           0x00
#define MAXI_STAT_BUSY         0x0b
/* other values not used */

/* Mailbox command register opcodes */
#define MAXI_CMD_READ          0x02
#define MAXI_CMD_WRITE         0x03
/* other values not used */

/* MaxiLife NBA Hardware monitoring tokens */

/* Alarm tokens (0x1xxx) */
#define MAXI_TOK_ALARM(nr)    (0x1000 + (nr))
#define MAXI_TOK_ALARM_EVENT   0x1000
#define MAXI_TOK_ALARM_FAN     0x1001
#define MAXI_TOK_ALARM_TEMP    0x1002
#define MAXI_TOK_ALARM_VID     0x1003	/* voltages */
#define MAXI_TOK_ALARM_AVID    0x1004	/* additional voltages */
#define MAXI_TOK_ALARM_PWR     0x1101	/* power supply glitch */

/* Fan status tokens (0x20xx) */
#define MAXI_TOK_FAN(nr)      (0x2000 + (nr))
#define MAXI_TOK_FAN_CPU       0x2000
#define MAXI_TOK_FAN_PCI       0x2001
#define MAXI_TOK_FAN_HDD       0x2002	/* hard disk bay fan */
#define MAXI_TOK_FAN_SINK      0x2003	/* heatsink */

/* Temperature status tokens (0x21xx) */
#define MAXI_TOK_TEMP(nr)     (0x2100 + (nr))
#define MAXI_TOK_TEMP_CPU1     0x2100
#define MAXI_TOK_TEMP_CPU2     0x2101
#define MAXI_TOK_TEMP_PCI      0x2102	/* PCI/ambient temp */
#define MAXI_TOK_TEMP_HDD      0x2103	/* hard disk bay temp */
#define MAXI_TOK_TEMP_MEM      0x2104	/* mother board temp */
#define MAXI_TOK_TEMP_CPU      0x2105	/* CPU reference temp */

/* Voltage status tokens (0x22xx) */
#define MAXI_TOK_VID(nr)      (0x2200 + (nr))
#define MAXI_TOK_VID_12        0x2200	/* +12 volt */
#define MAXI_TOK_VID_CPU1      0x2201	/* cpu 1 voltage */
#define MAXI_TOK_VID_CPU2      0x2202	/* cpu 2 voltage */
#define MAXI_TOK_VID_L2        0x2203	/* level 2 cache voltage */
#define MAXI_TOK_VID_M12       0x2204	/* -12 volt */

/* Additive voltage status tokens (0x23xx) */
#define MAXI_TOK_AVID(nr)     (0x2300 + (nr))
#define MAXI_TOK_AVID_15       0x2300	/* 1.5 volt */
#define MAXI_TOK_AVID_18       0x2301	/* 1.8 volt */
#define MAXI_TOK_AVID_25       0x2302	/* 2.5 volt */
#define MAXI_TOK_AVID_33       0x2303	/* 3.3 volt */
#define MAXI_TOK_AVID_5        0x2304	/* 5 volt */
#define MAXI_TOK_AVID_M5       0x2305	/* -5 volt */
#define MAXI_TOK_AVID_BAT      0x2306	/* battery voltage */

/* Threshold tokens (0x3xxx) */
#define MAXI_TOK_MIN(token)    ((token) + 0x1000)
#define MAXI_TOK_MAX(token)    ((token) + 0x1800)

/* LCD Panel (0x4xxx) */
#define MAXI_TOK_LCD(nr)      (0x4000 + (nr))
#define MAXI_TOK_LCD_LINE1     0x4000
#define MAXI_TOK_LCD_LINE2     0x4001
#define MAXI_TOK_LCD_LINE3     0x4002
#define MAXI_TOK_LCD_LINE4     0x4003

			       /* 0xfe: fan off, 0xff: stopped (alarm) */
			       /* or not available */
#define FAN99_FROM_REG(val)    ((val)==0xfe ? 0 : (val)==0xff ? -1 : ((val)*39))

			       /* when no CPU2 temp is 127 (0x7f) */
#define TEMP99_FROM_REG(val)   ((val)==0x7f ? -1 : (val)==0xff ? -1 : (val))

#define VID99_FROM_REG(nr,val) ((val)==0xff ? 0 : \
                                (nr)==1 ? ((val) * 608) : \
                                (nr)==2 ? ((val) * 160) : \
                                (nr)==3 ? ((val) * 160) : \
                                (nr)==4 ? (val) /* no formula spcified */ : \
                                (nr)==5 ? ((val) * 823 - 149140) : 0)


/* The following product codenames apply:
     Cristal/Geronimo: HP KAYAK XU/XAs
                       (Dual Pentium II Slot 1, Deschutes/Klamath)
     Cognac: HP KAYAK XU (Dual Xeon [Slot 2] 400/450 Mhz)
     Ashaki: HP KAYAK XA (Pentium II Slot 1, monoprocessor)
     NBA:    New BIOS Architecture, Kayak XU800, XM600, ... */

enum maxi_type { cristal, cognac, ashaki, nba };
enum sensor_type { fan, temp, vid, pll, lcd, alarm };

/* For each registered MaxiLife controller, we need to keep some data in
   memory. It is dynamically allocated, at the same time when a new MaxiLife
   client is allocated. We assume MaxiLife will only be present on the
   SMBus and not on the ISA bus. */
struct maxi_data {
	struct i2c_client client;
	int sysctl_id;
	enum maxi_type type;

	struct semaphore update_lock;
	char valid;		/* !=0 if following fields are valid */
	unsigned long last_updated;	/* In jiffies */

	u8 fan[4];		/* Register value */
	u8 fan_min[4];		/* Register value */
	u8 fan_speed[4];	/* Register value */
	u8 fan_div[4];		/* Static value */
	u8 temp[6];		/* Register value */
	u8 temp_max[6];		/* Static value */
	u8 temp_hyst[6];	/* Static value */
	u8 pll;			/* Register value */
	u8 pll_min;		/* Register value */
	u8 pll_max;		/* register value */
	u8 vid[5];		/* Register value */
	u8 vid_min[5];		/* Register value */
	u8 vid_max[5];		/* Register value */
	u8 lcd[4][17];		/* Four LCD lines */
	u16 alarms;		/* Register encoding, combined */
};


static int maxi_attach_adapter(struct i2c_adapter *adapter);
static int maxi_detect(struct i2c_adapter *adapter, int address,
		       unsigned short flags, int kind);
static int maxi_detach_client(struct i2c_client *client);

static int maxi_read_value(struct i2c_client *client, u8 reg);
static int maxi_read_token(struct i2c_client *client, u16 token);
#ifndef NOWRITE
static int maxi_write_value(struct i2c_client *client, u8 reg,
			    u8 value);
#endif
static int maxi_write_token_loop(struct i2c_client *client, u16 token,
				 u8 len, u8 * values);

static void maxi_update_client(struct i2c_client *client);
static void maxi99_update_client(struct i2c_client *client,
				 enum sensor_type sensor, int which);
static void maxi_init_client(struct i2c_client *client);

static void maxi_fan(struct i2c_client *client, int operation,
		     int ctl_name, int *nrels_mag, long *results);
static void maxi99_fan(struct i2c_client *client, int operation,
		       int ctl_name, int *nrels_mag, long *results);
static void maxi_temp(struct i2c_client *client, int operation,
		      int ctl_name, int *nrels_mag, long *results);
static void maxi99_temp(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void maxi_pll(struct i2c_client *client, int operation,
		     int ctl_name, int *nrels_mag, long *results);
static void maxi_vid(struct i2c_client *client, int operation,
		     int ctl_name, int *nrels_mag, long *results);
static void maxi99_vid(struct i2c_client *client, int operation,
		       int ctl_name, int *nrels_mag, long *results);
static void maxi_lcd(struct i2c_client *client, int operation,
		     int ctl_name, int *nrels_mag, long *results);
static void maxi_alarms(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);

/* The driver. I choose to use type i2c_driver, as at is identical to
   the smbus_driver. */
static struct i2c_driver maxi_driver = {
	.name		= "HP MaxiLife driver",
	.id		= I2C_DRIVERID_MAXILIFE,
	.flags		= I2C_DF_NOTIFY,
	.attach_adapter	= maxi_attach_adapter,
	.detach_client	= maxi_detach_client,
};

/* Default firmware version. Use module option "maxi_version"
   to set desired version. Auto detect is not yet working */
static int maxi_version = cristal;

/* The /proc/sys entries */

/* -- SENSORS SYSCTL START -- */
#define MAXI_SYSCTL_FAN1   1101	/* Rotations/min */
#define MAXI_SYSCTL_FAN2   1102	/* Rotations/min */
#define MAXI_SYSCTL_FAN3   1103	/* Rotations/min */
#define MAXI_SYSCTL_FAN4   1104	/* Rotations/min */
#define MAXI_SYSCTL_TEMP1  1201	/* Degrees Celsius */
#define MAXI_SYSCTL_TEMP2  1202	/* Degrees Celsius */
#define MAXI_SYSCTL_TEMP3  1203	/* Degrees Celsius */
#define MAXI_SYSCTL_TEMP4  1204	/* Degrees Celsius */
#define MAXI_SYSCTL_TEMP5  1205	/* Degrees Celsius */
#define MAXI_SYSCTL_TEMP6  1206	/* Degrees Celsius */
#define MAXI_SYSCTL_PLL    1301	/* MHz */
#define MAXI_SYSCTL_VID1   1401	/* Volts / 6.337, for nba just Volts */
#define MAXI_SYSCTL_VID2   1402	/* Volts */
#define MAXI_SYSCTL_VID3   1403	/* Volts */
#define MAXI_SYSCTL_VID4   1404	/* Volts */
#define MAXI_SYSCTL_VID5   1405	/* Volts */
#define MAXI_SYSCTL_LCD1   1501	/* Line 1 of LCD */
#define MAXI_SYSCTL_LCD2   1502	/* Line 2 of LCD */
#define MAXI_SYSCTL_LCD3   1503	/* Line 3 of LCD */
#define MAXI_SYSCTL_LCD4   1504	/* Line 4 of LCD */
#define MAXI_SYSCTL_ALARMS 2001	/* Bitvector (see below) */

#define MAXI_ALARM_VID4      0x0001
#define MAXI_ALARM_TEMP2     0x0002
#define MAXI_ALARM_VID1      0x0004
#define MAXI_ALARM_VID2      0x0008
#define MAXI_ALARM_VID3      0x0010
#define MAXI_ALARM_PLL       0x0080
#define MAXI_ALARM_TEMP4     0x0100
#define MAXI_ALARM_TEMP5     0x0200
#define MAXI_ALARM_FAN1      0x1000
#define MAXI_ALARM_FAN2      0x2000
#define MAXI_ALARM_FAN3      0x4000

#define MAXI_ALARM_FAN       0x0100	/* To be used with  MaxiLife'99 */
#define MAXI_ALARM_VID       0x0200	/* The MSB specifies which sensor */
#define MAXI_ALARM_TEMP      0x0400	/* in the alarm group failed, i.e.: */
#define MAXI_ALARM_VADD      0x0800	/* 0x0402 = TEMP2 failed = CPU2 temp */

/* -- SENSORS SYSCTL END -- */

/* These files are created for each detected MaxiLife processor.
   This is just a template; though at first sight, you might think we
   could use a statically allocated list, we need some way to get back
   to the parent - which is done through one of the 'extra' fields 
   which are initialized when a new copy is allocated. */
static ctl_table maxi_dir_table_template[] = {
	{MAXI_SYSCTL_FAN1, "fan1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &maxi_fan},
	{MAXI_SYSCTL_FAN2, "fan2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &maxi_fan},
	{MAXI_SYSCTL_FAN3, "fan3", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &maxi_fan},
	{MAXI_SYSCTL_FAN4, "fan4", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &maxi_fan},
	{MAXI_SYSCTL_TEMP1, "temp1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &maxi_temp},
	{MAXI_SYSCTL_TEMP2, "temp2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &maxi_temp},
	{MAXI_SYSCTL_TEMP3, "temp3", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &maxi_temp},
	{MAXI_SYSCTL_TEMP4, "temp4", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &maxi_temp},
	{MAXI_SYSCTL_TEMP5, "temp5", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &maxi_temp},
	{MAXI_SYSCTL_TEMP6, "temp6", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &maxi_temp},
	{MAXI_SYSCTL_PLL, "pll", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &maxi_pll},
	{MAXI_SYSCTL_VID1, "vid1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &maxi_vid},
	{MAXI_SYSCTL_VID2, "vid2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &maxi_vid},
	{MAXI_SYSCTL_VID3, "vid3", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &maxi_vid},
	{MAXI_SYSCTL_VID4, "vid4", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &maxi_vid},
	{MAXI_SYSCTL_VID5, "vid5", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &maxi_vid},
	{MAXI_SYSCTL_LCD1, "lcd1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &maxi_lcd},
	{MAXI_SYSCTL_LCD2, "lcd2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &maxi_lcd},
	{MAXI_SYSCTL_LCD3, "lcd3", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &maxi_lcd},
	{MAXI_SYSCTL_LCD4, "lcd4", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &maxi_lcd},
	{MAXI_SYSCTL_ALARMS, "alarms", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &maxi_alarms},
	{0}
};

/* This function is called when:
    - maxi_driver is inserted (when this module is loaded), for each
      available adapter
    - when a new adapter is inserted (and maxi_driver is still present) */
static int maxi_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_detect(adapter, &addr_data, maxi_detect);
}

/* This function is called by i2c_detect */
int maxi_detect(struct i2c_adapter *adapter, int address,
		unsigned short flags, int kind)
{
	struct i2c_client *new_client;
	struct maxi_data *data;
	enum maxi_type type = 0;
	int i, j, err = 0;
	const char *type_name = NULL, *client_name = NULL;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		goto ERROR0;

	/* OK. For now, we presume we have a valid client. We now create the
	   client structure, even though we cannot fill it completely yet.
	   But it allows us to access maxi_{read,write}_value. */
	if (!(data = kmalloc(sizeof(struct maxi_data), GFP_KERNEL))) {
		err = -ENOMEM;
		goto ERROR0;
	}

	/* Fill the new client structure with data */
	new_client = &data->client;
	new_client->addr = address;
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &maxi_driver;
	new_client->flags = 0;

	/* Now we do the remaining detection. */
	if (kind < 0) {
		if (i2c_smbus_read_byte_data
		    (new_client, MAXI_REG_MBX_STATUS) < 0)
			goto ERROR2;
	}

	/* Determine the chip type - only one kind supported */
	if (kind <= 0)
		kind = maxilife;

	if (kind == maxilife) {
		/* Detect if the machine has a MaxiLife NBA controller.
		   The right way to perform this check is to do a read/modify/write
		   on register MbxStatus (5A):
		   - Read 5A (value 0 for non-NBA firmware, FF (MbxIdle on NBA-firmware)
		   - Write 55 on 5A, then read back 5A
		   Non-NBA firmware: value is 55 (reg 5A is a standard writable reg)
		   NBA firmaware: value is FF (write-protect on MbxStatus active) */
		int stat;
		i2c_smbus_write_byte_data(new_client, MAXI_REG_MBX_STATUS,
					  0x55);
		stat =
		    i2c_smbus_read_byte_data(new_client,
					     MAXI_REG_MBX_STATUS);

		/*if (stat == MAXI_STAT_IDLE || stat == MAXI_STAT_OK) */
		if (stat != 0x55)
			maxi_version = nba;
#ifdef AUTODETECT
		else {
			/* The right way to get the platform info is to read the firmware
			   revision from serial EEPROM (addr=0x54), at offset 0x0045.
			   This is a string as:
			   "CG 00.04" -> Cristal [XU] / Geronimo [XAs]
			   "CO 00.03" -> Cognac [XU]
			   "AS 00.01" -> Ashaki [XA] */
#if 0
			int biosctl;
			biosctl =
			    i2c_smbus_read_byte_data(new_client,
						     MAXI_REG_BIOS_CTRL);
			i2c_smbus_write_byte_data(new_client,
						  MAXI_REG_BIOS_CTRL,
						  biosctl | 4);
			err = eeprom_read_byte_data(adapter, 0x54, 0x45);
			i2c_smbus_write_byte_data(new_client,
						  MAXI_REG_BIOS_CTRL,
						  biosctl);
#endif
			int i;
			char *biosmem, *bm;
			bm = biosmem = ioremap(0xe0000, 0x20000);
			if (biosmem) {
				printk("begin of bios search\n");
				for (i = 0; i < 0x20000; i++) {
					if (*bm == 'C') {
						char *s = bm;
						while (s && isprint(*s)) {
							printk("%c", *s);
							s++;
						}
						printk("\n");
						if (!strncmp
						    (bm, "CG 00.04", 8)) {
							maxi_version =
							    cristal;
							printk
							    ("maxilife: found MaxiLife Rev CG 00.04\n");
							break;
						}
						if (!strncmp
						    (bm, "CO 00.03", 8)) {
							maxi_version =
							    cognac;
							printk
							    ("maxilife: found MaxiLife Rev CO 00.03\n");
							break;
						}
					}
					if (*bm == 'A' && *(bm + 1) == 'S') {
						char *s = bm;
						while (s && isprint(*s)) {
							printk("%c", *s);
							s++;
						}
						printk("\n");
						if (!strncmp
						    (bm, "AS 00.01", 8)) {
							maxi_version =
							    ashaki;
							printk
							    ("maxilife: found MaxiLife Rev AS 00.01\n");
							break;
						}
					}
					bm++;
				}
				printk("end of bios search\n");
			} else
				printk("could not map bios memory\n");
		}
#endif

		if (maxi_version == cristal) {
			type = cristal;
			type_name = "maxilife-cg";
			client_name = "HP MaxiLife Rev CG 00.04";
			printk
			    ("maxilife: HP KAYAK XU/XAs (Dual Pentium II Slot 1)\n");
		} else if (maxi_version == cognac) {
			type = cognac;
			type_name = "maxilife-co";
			client_name = "HP MaxiLife Rev CO 00.03";
			printk
			    ("maxilife: HP KAYAK XU (Dual Xeon Slot 2 400/450 Mhz)\n");
		} else if (maxi_version == ashaki) {
			type = ashaki;
			type_name = "maxilife-as";
			client_name = "HP MaxiLife Rev AS 00.01";
			printk
			    ("maxilife: HP KAYAK XA (Pentium II Slot 1, monoprocessor)\n");
		} else if (maxi_version == nba) {
			type = nba;
			type_name = "maxilife-nba";
			client_name = "HP MaxiLife NBA";
			printk("maxilife: HP KAYAK XU800/XM600\n");
		} else {
#ifdef AUTODETECT
			printk
			    ("maxilife: Warning: probed non-maxilife chip?!? (%x)\n",
			     err);
#else
			printk
			    ("maxilife: Error: specified wrong maxi_version (%d)\n",
			     maxi_version);
#endif
			goto ERROR2;
		}
	}

	/* Fill in the remaining client fields and put it into the global list */
	strcpy(new_client->name, client_name);
	((struct maxi_data *) (new_client->data))->type = type;

	for (i = 0; i < 4; i++)
		for (j = 0; j < 17; j++)
			    ((struct maxi_data *) (new_client->data))->
			    lcd[i][j] = (u8) 0;

	data->valid = 0;
	init_MUTEX(&data->update_lock);

	/* Tell i2c-core that a new client has arrived */
	if ((err = i2c_attach_client(new_client)))
		goto ERROR2;

	/* Register a new directory entry with module sensors */
	if ((err = i2c_register_entry(new_client, type_name,
					  maxi_dir_table_template,
					  THIS_MODULE)) < 0)
		goto ERROR4;
	data->sysctl_id = err;

	/* Initialize the MaxiLife chip */
	maxi_init_client(new_client);
	return 0;

	/* OK, this is not exactly good programming practice, usually.
	   But it is very code-efficient in this case. */
      ERROR4:
	i2c_detach_client(new_client);
      ERROR2:
	kfree(data);
      ERROR0:
	return err;
}

/* This function is called whenever a client should be removed:
    - maxi_driver is removed (when this module is unloaded)
    - when an adapter is removed which has a maxi client (and maxi_driver
      is still present). */
static int maxi_detach_client(struct i2c_client *client)
{
	int err;

	i2c_deregister_entry(((struct maxi_data *) (client->data))->
				 sysctl_id);

	if ((err = i2c_detach_client(client))) {
		printk
		    ("maxilife: Client deregistration failed, client not detached.\n");
		return err;
	}
	kfree(client->data);
	return 0;
}

/* Read byte from specified register (-1 in case of error, value otherwise). */
static int maxi_read_value(struct i2c_client *client, u8 reg)
{
	return i2c_smbus_read_byte_data(client, reg);
}

/* Read the byte value for a MaxiLife token (-1 in case of error, value otherwise */
static int maxi_read_token(struct i2c_client *client, u16 token)
{
	u8 lowToken, highToken;
	int error, value;

	lowToken = LOW(token);
	highToken = HIGH(token);

	/* Set mailbox status register to idle state. */
	error =
	    i2c_smbus_write_byte_data(client, MAXI_REG_MBX_STATUS,
				      MAXI_STAT_IDLE);
	if (error < 0)
		return error;

	/* Check for mailbox idle state. */
	error = i2c_smbus_read_byte_data(client, MAXI_REG_MBX_STATUS);
	if (error != MAXI_STAT_IDLE)
		return -1;

	/* Write the most significant byte of the token we want to read. */
	error =
	    i2c_smbus_write_byte_data(client, MAXI_REG_MBX_TOKEN_H,
				      highToken);
	if (error < 0)
		return error;

	/* Write the least significant byte of the token we want to read. */
	error =
	    i2c_smbus_write_byte_data(client, MAXI_REG_MBX_TOKEN_L,
				      lowToken);
	if (error < 0)
		return error;

	/* Write the read token opcode to the mailbox. */
	error =
	    i2c_smbus_write_byte_data(client, MAXI_REG_MBX_CMD,
				      MAXI_CMD_READ);
	if (error < 0)
		return error;

	/* Check for transaction completion */
	do {
		error =
		    i2c_smbus_read_byte_data(client, MAXI_REG_MBX_STATUS);
	} while (error == MAXI_STAT_BUSY);
	if (error != MAXI_STAT_OK)
		return -1;

	/* Read the value of the token. */
	value = i2c_smbus_read_byte_data(client, MAXI_REG_MBX_DATA);
	if (value == -1)
		return -1;

	/* set mailbox status to idle to complete transaction. */
	error =
	    i2c_smbus_write_byte_data(client, MAXI_REG_MBX_STATUS,
				      MAXI_STAT_IDLE);
	if (error < 0)
		return error;

	return value;
}

#ifndef NOWRITE
/* Write byte to specified register (-1 in case of error, 0 otherwise). */
static int maxi_write_value(struct i2c_client *client, u8 reg, u8 value)
{
	return i2c_smbus_write_byte_data(client, reg, value);
}
#endif

/* Write a set of len byte values to MaxiLife token (-1 in case of error, 0 otherwise). */
int maxi_write_token_loop(struct i2c_client *client, u16 token, u8 len,
			  u8 * values)
{
	u8 lowToken, highToken, bCounter;
	int error;

	lowToken = LOW(token);
	highToken = HIGH(token);

	/* Set mailbox status register to idle state. */
	error =
	    i2c_smbus_write_byte_data(client, MAXI_REG_MBX_STATUS,
				      MAXI_STAT_IDLE);
	if (error < 0)
		return error;

	/* Check for mailbox idle state. */
	error = i2c_smbus_read_byte_data(client, MAXI_REG_MBX_STATUS);
	if (error != MAXI_STAT_IDLE)
		return -1;

	for (bCounter = 0; (bCounter < len && bCounter < 32); bCounter++) {
		error =
		    i2c_smbus_write_byte_data(client,
					      (u8) (MAXI_REG_MBX_DATA +
						    bCounter),
					      values[bCounter]);
		if (error < 0)
			return error;
	}

	/* Write the most significant byte of the token we want to read. */
	error =
	    i2c_smbus_write_byte_data(client, MAXI_REG_MBX_TOKEN_H,
				      highToken);
	if (error < 0)
		return error;

	/* Write the least significant byte of the token we want to read. */
	error =
	    i2c_smbus_write_byte_data(client, MAXI_REG_MBX_TOKEN_L,
				      lowToken);
	if (error < 0)
		return error;

	/* Write the write token opcode to the mailbox. */
	error =
	    i2c_smbus_write_byte_data(client, MAXI_REG_MBX_CMD,
				      MAXI_CMD_WRITE);
	if (error < 0)
		return error;

	/* Check for transaction completion */
	do {
		error =
		    i2c_smbus_read_byte_data(client, MAXI_REG_MBX_STATUS);
	} while (error == MAXI_STAT_BUSY);
	if (error != MAXI_STAT_OK)
		return -1;

	/* set mailbox status to idle to complete transaction. */
	return i2c_smbus_write_byte_data(client, MAXI_REG_MBX_STATUS,
					 MAXI_STAT_IDLE);
}

/* Called when we have found a new MaxiLife. */
static void maxi_init_client(struct i2c_client *client)
{
	struct maxi_data *data = client->data;

	if (data->type == nba) {
		strcpy(data->lcd[2], " Linux MaxiLife");
		maxi_write_token_loop(client, MAXI_TOK_LCD(2),
				      strlen(data->lcd[2]) + 1,
				      data->lcd[2]);
	}
}

static void maxi_update_client(struct i2c_client *client)
{
	struct maxi_data *data = client->data;
	int i;

	if (data->type == nba) {
		printk
		    ("maxi_update_client should never be called by nba\n");
		return;
	}

	down(&data->update_lock);

	if ((jiffies - data->last_updated > HZ + HZ / 2) ||
	    (jiffies < data->last_updated) || !data->valid) {

#ifdef DEBUG
		printk("maxilife: Starting MaxiLife update\n");
#endif
		for (i = 0; i < 5; i++)
			data->temp[i] =
			    maxi_read_value(client, MAXI_REG_TEMP(i));
		switch (data->type) {
		case cristal:
			data->temp[0] = 0;	/* not valid */
			data->temp_max[0] = 0;
			data->temp_hyst[0] = 0;
			data->temp_max[1] = 110;	/* max PCI slot temp */
			data->temp_hyst[1] = 100;
			data->temp_max[2] = 120;	/* max BX chipset temp */
			data->temp_hyst[2] = 110;
			data->temp_max[3] = 100;	/* max HDD temp */
			data->temp_hyst[3] = 90;
			data->temp_max[4] = 120;	/* max CPU temp */
			data->temp_hyst[4] = 110;
			break;

		case cognac:
			data->temp_max[0] = 120;	/* max CPU1 temp */
			data->temp_hyst[0] = 110;
			data->temp_max[1] = 110;	/* max PCI slot temp */
			data->temp_hyst[1] = 100;
			data->temp_max[2] = 120;	/* max CPU2 temp */
			data->temp_hyst[2] = 110;
			data->temp_max[3] = 100;	/* max HDD temp */
			data->temp_hyst[3] = 90;
			data->temp_max[4] = 120;	/* max reference CPU temp */
			data->temp_hyst[4] = 110;
			break;

		case ashaki:
			data->temp[0] = 0;	/* not valid */
			data->temp_max[0] = 0;
			data->temp_hyst[0] = 0;
			data->temp_max[1] = 110;	/* max PCI slot temp */
			data->temp_hyst[1] = 100;
			data->temp[2] = 0;	/* not valid */
			data->temp_max[2] = 0;
			data->temp_hyst[2] = 0;
			data->temp_max[3] = 100;	/* max HDD temp */
			data->temp_hyst[3] = 90;
			data->temp_max[4] = 120;	/* max CPU temp */
			data->temp_hyst[4] = 110;
			break;

		default:
			printk("maxilife: Unknown MaxiLife chip\n");
		}
		data->temp[5] = 0;	/* only used by MaxiLife'99 */
		data->temp_max[5] = 0;
		data->temp_hyst[5] = 0;

		for (i = 0; i < 3; i++) {
			data->fan[i] =
			    maxi_read_value(client, MAXI_REG_FAN(i));
			data->fan_speed[i] =
			    maxi_read_value(client, MAXI_REG_FAN_SPEED(i));
			data->fan_div[i] = 4;
			if (data->type == ashaki)
				data->fan_min[i] =
				    maxi_read_value(client,
						    MAXI_REG_FAN_MINAS(i));
			else
				data->fan_min[i] =
				    maxi_read_value(client,
						    MAXI_REG_FAN_MIN(i));
		}
		data->fan[3] = 0xff;	/* only used by MaxiLife'99 */
		data->fan_speed[3] = 0;
		data->fan_div[3] = 4;	/* avoid possible /0 */
		data->fan_min[3] = 0;

		data->pll = maxi_read_value(client, MAXI_REG_PLL);
		data->pll_min = maxi_read_value(client, MAXI_REG_PLL_MIN);
		data->pll_max = maxi_read_value(client, MAXI_REG_PLL_MAX);

		for (i = 0; i < 4; i++) {
			data->vid[i] =
			    maxi_read_value(client, MAXI_REG_VID(i));
			data->vid_min[i] =
			    maxi_read_value(client, MAXI_REG_VID_MIN(i));
			data->vid_max[i] =
			    maxi_read_value(client, MAXI_REG_VID_MAX(i));
		}
		switch (data->type) {
		case cristal:
			data->vid[3] = 0;	/* no voltage cache L2 */
			data->vid_min[3] = 0;
			data->vid_max[3] = 0;
			break;

		case cognac:
			break;

		case ashaki:
			data->vid[1] = 0;	/* no voltage CPU 2 */
			data->vid_min[1] = 0;
			data->vid_max[1] = 0;
			data->vid[3] = 0;	/* no voltage cache L2 */
			data->vid_min[3] = 0;
			data->vid_max[3] = 0;
			break;

		default:
			printk("maxilife: Unknown MaxiLife chip\n");
		}
		data->vid[4] = 0;	/* only used by MaxliLife'99 */
		data->vid_min[4] = 0;
		data->vid_max[4] = 0;

		data->alarms = maxi_read_value(client, MAXI_REG_DIAG_RT1) +
		    (maxi_read_value(client, MAXI_REG_DIAG_RT2) << 8);

		data->last_updated = jiffies;
		data->valid = 1;
	}

	up(&data->update_lock);
}

void maxi99_update_client(struct i2c_client *client,
			  enum sensor_type sensor, int which)
{
	static unsigned long last_updated[6][6];	/* sensor, which */
	struct maxi_data *data = client->data;

	down(&data->update_lock);

	/*maxi_write_token_loop(client, MAXI_TOK_LCD_LINE3, 13, "Linux 2.2.13"); */

	if ((jiffies - last_updated[sensor][which] > 2 * HZ) ||
	    (jiffies < last_updated[sensor][which]
	     || !last_updated[sensor][which])) {

		int tmp, i;

		switch (sensor) {
		case fan:
			for (i = 0; i < 4; i++) {
				if (i == which) {
					tmp =
					    maxi_read_token(client,
							    MAXI_TOK_FAN
							    (i));
					data->fan[i] =
					    maxi_read_token(client,
							    MAXI_TOK_FAN
							    (i));
					data->fan_speed[i] =
					    maxi_read_token(client,
							    MAXI_TOK_MAX
							    (MAXI_TOK_FAN
							     (i)));
					data->fan_div[i] = 1;
					data->fan_min[i] = 0;
				}
			}
			break;

		case temp:
			for (i = 0; i < 6; i++) {
				if (i == which) {
					data->temp[i] =
					    maxi_read_token(client,
							    MAXI_TOK_TEMP
							    (i));
					data->temp_max[i] =
					    maxi_read_token(client,
							    MAXI_TOK_MAX
							    (MAXI_TOK_TEMP
							     (i)));
					data->temp_hyst[i] =
					    data->temp_max[i] - 5;
				}
			}
			break;

		case vid:
			for (i = 0; i < 5; i++) {
				if (i == which) {
					data->vid[i] =
					    maxi_read_token(client,
							    MAXI_TOK_VID
							    (i));
					data->vid_min[i] =
					    maxi_read_token(client,
							    MAXI_TOK_MIN
							    (MAXI_TOK_VID
							     (i)));
					data->vid_max[i] =
					    maxi_read_token(client,
							    MAXI_TOK_MAX
							    (MAXI_TOK_VID
							     (i)));
				}
			}
			break;

		case pll:
			data->pll = 0;
			data->pll_min = 0;
			data->pll_max = 0;
			break;

		case alarm:
			data->alarms =
			    (maxi_read_token(client, MAXI_TOK_ALARM_EVENT)
			     << 8);
			if (data->alarms)
				data->alarms +=
				    data->alarms ==
				    (1 << 8) ? maxi_read_token(client,
							       MAXI_TOK_ALARM_FAN)
				    : data->alarms ==
				    (2 << 8) ? maxi_read_token(client,
							       MAXI_TOK_ALARM_VID)
				    : data->alarms ==
				    (4 << 8) ? maxi_read_token(client,
							       MAXI_TOK_ALARM_TEMP)
				    : data->alarms ==
				    (8 << 8) ? maxi_read_token(client,
							       MAXI_TOK_ALARM_FAN)
				    : 0;
			break;

		default:
			printk("maxilife: Unknown sensor type\n");
		}

		last_updated[sensor][which] = jiffies;
		data->valid = 1;
	}

	up(&data->update_lock);
}

/* The next few functions are the call-back functions of the /proc/sys and
   sysctl files. Which function is used is defined in the ctl_table in
   the extra1 field.
   Each function must return the magnitude (power of 10 to divide the data
   with) if it is called with operation==SENSORS_PROC_REAL_INFO. It must
   put a maximum of *nrels elements in results reflecting the data of this
   file, and set *nrels to the number it actually put in it, if operation==
   SENSORS_PROC_REAL_READ. Finally, it must get upto *nrels elements from
   results and write them to the chip, if operations==SENSORS_PROC_REAL_WRITE.
   Note that on SENSORS_PROC_REAL_READ, I do not check whether results is
   large enough (by checking the incoming value of *nrels). This is not very
   good practice, but as long as you put less than about 5 values in results,
   you can assume it is large enough. */
void maxi_fan(struct i2c_client *client, int operation, int ctl_name,
	      int *nrels_mag, long *results)
{
	struct maxi_data *data = client->data;
	int nr;

	if (data->type == nba) {
		maxi99_fan(client, operation, ctl_name, nrels_mag,
			   results);
		return;
	}

	nr = ctl_name - MAXI_SYSCTL_FAN1 + 1;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		maxi_update_client(client);
		results[0] = FAN_FROM_REG(data->fan_min[nr - 1]);
		results[1] = data->fan_div[nr - 1];
		results[2] = FAN_FROM_REG(data->fan[nr - 1]);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
#ifndef NOWRITE
		if (*nrels_mag >= 1) {
			data->fan_min[nr - 1] = FAN_TO_REG(results[0]);
			maxi_write_value(client, MAXI_REG_FAN_MIN(nr),
					 data->fan_min[nr - 1]);
		}
#endif
	}
}

void maxi99_fan(struct i2c_client *client, int operation, int ctl_name,
		int *nrels_mag, long *results)
{
	struct maxi_data *data = client->data;
	int nr;

	nr = ctl_name - MAXI_SYSCTL_FAN1 + 1;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		maxi99_update_client(client, fan, nr - 1);
		results[0] = FAN99_FROM_REG(data->fan_min[nr - 1]);	/* min rpm */
		results[1] = data->fan_div[nr - 1];	/* divisor */
		results[2] = FAN99_FROM_REG(data->fan[nr - 1]);	/* rpm */
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
#ifndef NOWRITE
		/* still to do */
		if (*nrels_mag >= 1) {
			data->fan_min[nr - 1] = FAN_TO_REG(results[0]);
			maxi_write_value(client, MAXI_REG_FAN_MIN(nr),
					 data->fan_min[nr - 1]);
		}
#endif
	}
}

void maxi_temp(struct i2c_client *client, int operation, int ctl_name,
	       int *nrels_mag, long *results)
{
	struct maxi_data *data = client->data;
	int nr;

	if (data->type == nba) {
		maxi99_temp(client, operation, ctl_name, nrels_mag,
			    results);
		return;
	}

	nr = ctl_name - MAXI_SYSCTL_TEMP1 + 1;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 1;
	else if (operation == SENSORS_PROC_REAL_READ) {
		maxi_update_client(client);
		results[0] = TEMP_FROM_REG(data->temp_max[nr - 1]);
		results[1] = TEMP_FROM_REG(data->temp_hyst[nr - 1]);
		results[2] = TEMP_FROM_REG(data->temp[nr - 1]);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		/* temperature range can not be changed */
	}
}

void maxi99_temp(struct i2c_client *client, int operation, int ctl_name,
		 int *nrels_mag, long *results)
{
	struct maxi_data *data = client->data;
	int nr;

	nr = ctl_name - MAXI_SYSCTL_TEMP1 + 1;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		maxi99_update_client(client, temp, nr - 1);
		results[0] = TEMP99_FROM_REG(data->temp_max[nr - 1]);
		results[1] = TEMP99_FROM_REG(data->temp_hyst[nr - 1]);
		results[2] = TEMP99_FROM_REG(data->temp[nr - 1]);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		/* temperature range can not be changed */
	}
}

void maxi_pll(struct i2c_client *client, int operation, int ctl_name,
	      int *nrels_mag, long *results)
{
	struct maxi_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 2;
	else if (operation == SENSORS_PROC_REAL_READ) {
		if (data->type == nba)
			maxi99_update_client(client, pll, 0);
		else
			maxi_update_client(client);
		results[0] = PLL_FROM_REG(data->pll_min);
		results[1] = PLL_FROM_REG(data->pll_max);
		results[2] = PLL_FROM_REG(data->pll);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
#ifndef NOWRITE
		if (*nrels_mag >= 1) {
			data->pll_min = PLL_TO_REG(results[0]);
			maxi_write_value(client, MAXI_REG_PLL_MIN,
					 data->pll_min);
		}
		if (*nrels_mag >= 2) {
			data->pll_max = PLL_TO_REG(results[1]);
			maxi_write_value(client, MAXI_REG_PLL_MAX,
					 data->pll_max);
		}
#endif
	}
}

void maxi_vid(struct i2c_client *client, int operation, int ctl_name,
	      int *nrels_mag, long *results)
{
	struct maxi_data *data = client->data;
	int nr;

	if (data->type == nba) {
		maxi99_vid(client, operation, ctl_name, nrels_mag,
			   results);
		return;
	}

	nr = ctl_name - MAXI_SYSCTL_VID1 + 1;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 4;
	else if (operation == SENSORS_PROC_REAL_READ) {
		maxi_update_client(client);
		results[0] = VID_FROM_REG(data->vid_min[nr - 1]);
		results[1] = VID_FROM_REG(data->vid_max[nr - 1]);
		results[2] = VID_FROM_REG(data->vid[nr - 1]);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
#ifndef NOWRITE
		if (*nrels_mag >= 1) {
			data->vid_min[nr - 1] = VID_TO_REG(results[0]);
			maxi_write_value(client, MAXI_REG_VID_MIN(nr),
					 data->vid_min[nr - 1]);
		}
		if (*nrels_mag >= 2) {
			data->vid_max[nr - 1] = VID_TO_REG(results[1]);
			maxi_write_value(client, MAXI_REG_VID_MAX(nr),
					 data->vid_max[nr - 1]);
		}
#endif
	}
}

void maxi99_vid(struct i2c_client *client, int operation, int ctl_name,
		int *nrels_mag, long *results)
{
	struct maxi_data *data = client->data;
	int nr = ctl_name - MAXI_SYSCTL_VID1 + 1;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 4;
	else if (operation == SENSORS_PROC_REAL_READ) {
		maxi99_update_client(client, vid, nr - 1);
		results[0] = VID99_FROM_REG(nr, data->vid_min[nr - 1]);
		results[1] = VID99_FROM_REG(nr, data->vid_max[nr - 1]);
		results[2] = VID99_FROM_REG(nr, data->vid[nr - 1]);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
#ifndef NOWRITE
		/* still to do */
		if (*nrels_mag >= 1) {
			data->vid_min[nr - 1] = VID_TO_REG(results[0]);
			maxi_write_value(client, MAXI_REG_VID_MIN(nr),
					 data->vid_min[nr - 1]);
		}
		if (*nrels_mag >= 2) {
			data->vid_max[nr - 1] = VID_TO_REG(results[1]);
			maxi_write_value(client, MAXI_REG_VID_MAX(nr),
					 data->vid_max[nr - 1]);
		}
#endif
	}
}

void maxi_lcd(struct i2c_client *client, int operation, int ctl_name,
	      int *nrels_mag, long *results)
{
	/* Allows writing and reading from LCD display */

	struct maxi_data *data = client->data;
	int nr;

	if (data->type != nba)
		return;

	nr = ctl_name - MAXI_SYSCTL_LCD1 + 1;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		results[0] = *((long *) &data->lcd[nr - 1][0]);
		results[1] = *((long *) &data->lcd[nr - 1][4]);
		results[2] = *((long *) &data->lcd[nr - 1][8]);
		results[3] = *((long *) &data->lcd[nr - 1][12]);
		*nrels_mag = 4;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		/* 
		   Writing a string to line 3 of the LCD can be done like:
		   echo -n "Linux MaxiLife" | od -A n -l > \
		   /proc/sys/dev/sensors/maxilife-nba-i2c-0-14/lcd3
		 */
		if (*nrels_mag >= 1)
			*((long *) &data->lcd[nr - 1][0]) = results[0];
		if (*nrels_mag >= 2)
			*((long *) &data->lcd[nr - 1][4]) = results[1];
		if (*nrels_mag >= 3)
			*((long *) &data->lcd[nr - 1][8]) = results[2];
		if (*nrels_mag >= 4)
			*((long *) &data->lcd[nr - 1][12]) = results[3];
		maxi_write_token_loop(client, MAXI_TOK_LCD(nr - 1),
				      strlen(data->lcd[nr - 1]) + 1,
				      data->lcd[nr - 1]);
#if 0
		if (*nrels_mag >= 1)
			printk("nr=%d, result[0] = %.4s\n", nr,
			       (char *) &results[0]);
		if (*nrels_mag >= 2)
			printk("nr=%d, result[1] = %.4s\n", nr,
			       (char *) &results[1]);
		if (*nrels_mag >= 3)
			printk("nr=%d, result[2] = %.4s\n", nr,
			       (char *) &results[2]);
		if (*nrels_mag >= 4)
			printk("nr=%d, result[3] = %.4s\n", nr,
			       (char *) &results[3]);
#endif
	}

}

void maxi_alarms(struct i2c_client *client, int operation, int ctl_name,
		 int *nrels_mag, long *results)
{
	struct maxi_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		if (data->type == nba)
			maxi99_update_client(client, alarm, 0);
		else
			maxi_update_client(client);
		results[0] = ALARMS_FROM_REG(data->alarms);
		*nrels_mag = 1;
	}
}

static int __init sm_maxilife_init(void)
{
	printk("maxilife: Version %s (lm_sensors %s (%s))\n", version_str,
	       LM_VERSION, LM_DATE);
	return i2c_add_driver(&maxi_driver);
}

static void __exit sm_maxilife_exit(void)
{
	i2c_del_driver(&maxi_driver);
}



MODULE_AUTHOR("Fons Rademakers <Fons.Rademakers@cern.ch>");
MODULE_DESCRIPTION("HP MaxiLife driver");
MODULE_PARM(maxi_version, "i");
MODULE_PARM_DESC(maxi_version, "MaxiLife firmware version");

module_init(sm_maxilife_init);
module_exit(sm_maxilife_exit);
