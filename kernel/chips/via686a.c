/*
    via686a.c - Part of lm_sensors, Linux kernel modules
                for hardware monitoring
                
    Copyright (c) 1998 - 2001  Frodo Looijaard <frodol@dds.nl>,
                        Kyösti Mälkki <kmalkki@cc.hut.fi>,
			Mark Studebaker <mdsxyz123@yahoo.com>,
			and Bob Dougherty <bobd@stanford.edu>
    (Some conversion-factor data were contributed by Jonathan Teh Soon Yew 
    <j.teh@iname.com> and Alex van Kaam <darkside@chello.nl>.)

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

/*
    Supports the Via VT82C686A and VT82C686B south bridges.
    Reports either as a 686A.
    See doc/chips/via686a for details.
    Warning - only supports a single device.
*/
#include <linux/version.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/ioport.h>
#include <linux/sysctl.h>
#include <linux/pci.h>
#include <asm/errno.h>
#include <asm/io.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include "version.h"
#include "sensors.h"
#include <linux/init.h>

#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif

#ifndef PCI_DEVICE_ID_VIA_82C686_4
#define PCI_DEVICE_ID_VIA_82C686_4 0x3057
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,2,18)) || \
    (LINUX_VERSION_CODE == KERNEL_VERSION(2,3,0))
#define init_MUTEX(s) do { *(s) = MUTEX; } while(0)
#endif

#ifndef THIS_MODULE
#define THIS_MODULE NULL
#endif

/* If force_addr is set to anything different from 0, we forcibly enable
   the device at the given address. */
static int force_addr = 0;
MODULE_PARM(force_addr, "i");
MODULE_PARM_DESC(force_addr,
		 "Initialize the base address of the sensors");

/* Addresses to scan.
   Note that we can't determine the ISA address until we have initialized
   our module */
static unsigned short normal_i2c[] = { SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { SENSORS_I2C_END };
static unsigned int normal_isa[] = { 0x0000, SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/* Insmod parameters */
SENSORS_INSMOD_1(via686a);

/*
   The Via 686a southbridge has a LM78-like chip integrated on the same IC.
   This driver is a customized copy of lm78.c
*/

/* Many VIA686A constants specified below */

/* Length of ISA address segment */
#define VIA686A_EXTENT 0x80
#define VIA686A_BASE_REG 0x70
#define VIA686A_ENABLE_REG 0x74

/* The VIA686A registers */
/* ins numbered 0-4 */
#define VIA686A_REG_IN_MAX(nr) (0x2b + ((nr) * 2))
#define VIA686A_REG_IN_MIN(nr) (0x2c + ((nr) * 2))
#define VIA686A_REG_IN(nr)     (0x22 + (nr))

/* fans numbered 1-2 */
#define VIA686A_REG_FAN_MIN(nr) (0x3a + (nr))
#define VIA686A_REG_FAN(nr)     (0x28 + (nr))

// the following values are as speced by VIA:
static const u8 regtemp[] = { 0x20, 0x21, 0x1f };
static const u8 regover[] = { 0x39, 0x3d, 0x1d };
static const u8 reghyst[] = { 0x3a, 0x3e, 0x1e };

/* temps numbered 1-3 */
#define VIA686A_REG_TEMP(nr)		(regtemp[(nr) - 1])
#define VIA686A_REG_TEMP_OVER(nr)	(regover[(nr) - 1])
#define VIA686A_REG_TEMP_HYST(nr)	(reghyst[(nr) - 1])
#define VIA686A_REG_TEMP_LOW1	0x4b	// bits 7-6
#define VIA686A_REG_TEMP_LOW23	0x49	// 2 = bits 5-4, 3 = bits 7-6

#define VIA686A_REG_ALARM1 0x41
#define VIA686A_REG_ALARM2 0x42
#define VIA686A_REG_FANDIV 0x47
#define VIA686A_REG_CONFIG 0x40
// The following register sets temp interrupt mode (bits 1-0 for temp1, 
// 3-2 for temp2, 5-4 for temp3).  Modes are:
//    00 interrupt stays as long as value is out-of-range
//    01 interrupt is cleared once register is read (default)
//    10 comparator mode- like 00, but ignores hysteresis
//    11 same as 00
#define VIA686A_REG_TEMP_MODE 0x4b
// We'll just assume that you want to set all 3 simulataneously:
#define VIA686A_TEMP_MODE_MASK 0x3F
#define VIA686A_TEMP_MODE_CONTINUOUS (0x00)

/* Conversions. Rounding and limit checking is only done on the TO_REG
   variants. */

/********* VOLTAGE CONVERSIONS (Bob Dougherty) ********/
// From HWMon.cpp (Copyright 1998-2000 Jonathan Teh Soon Yew):
// voltagefactor[0]=1.25/2628; (2628/1.25=2102.4)   // Vccp
// voltagefactor[1]=1.25/2628; (2628/1.25=2102.4)   // +2.5V
// voltagefactor[2]=1.67/2628; (2628/1.67=1573.7)   // +3.3V
// voltagefactor[3]=2.6/2628;  (2628/2.60=1010.8)   // +5V
// voltagefactor[4]=6.3/2628;  (2628/6.30=417.14)   // +12V
// in[i]=(data[i+2]*25.0+133)*voltagefactor[i];
// That is:
// volts = (25*regVal+133)*factor
// regVal = (volts/factor-133)/25
// (These conversions were contributed by Jonathan Teh Soon Yew 
// <j.teh@iname.com>)
// 
// These get us close, but they don't completely agree with what my BIOS 
// says- they are all a bit low.  But, it all we have to go on...
extern inline u8 IN_TO_REG(long val, int inNum)
{
	// to avoid floating point, we multiply everything by 100.
	// val is guaranteed to be positive, so we can achieve the effect of 
	// rounding by (...*10+5)/10.  Note that the *10 is hidden in the 
	// /250 (which should really be /2500).
	// At the end, we need to /100 because we *100 everything and we need
	// to /10 because of the rounding thing, so we /1000.  
	if (inNum <= 1)
		return (u8)
		    SENSORS_LIMIT(((val * 210240 - 13300) / 250 + 5) / 1000, 
				  0, 255);
	else if (inNum == 2)
		return (u8)
		    SENSORS_LIMIT(((val * 157370 - 13300) / 250 + 5) / 1000, 
				  0, 255);
	else if (inNum == 3)
		return (u8)
		    SENSORS_LIMIT(((val * 101080 - 13300) / 250 + 5) / 1000, 
				  0, 255);
	else
		return (u8) SENSORS_LIMIT(((val * 41714 - 13300) / 250 + 5)
					  / 1000, 0, 255);
}

extern inline long IN_FROM_REG(u8 val, int inNum)
{
	// to avoid floating point, we multiply everything by 100.
	// val is guaranteed to be positive, so we can achieve the effect of
	// rounding by adding 0.5.  Or, to avoid fp math, we do (...*10+5)/10.
	// We need to scale with *100 anyway, so no need to /100 at the end.
	if (inNum <= 1)
		return (long) (((250000 * val + 13300) / 210240 * 10 + 5) /10);
	else if (inNum == 2)
		return (long) (((250000 * val + 13300) / 157370 * 10 + 5) /10);
	else if (inNum == 3)
		return (long) (((250000 * val + 13300) / 101080 * 10 + 5) /10);
	else
		return (long) (((250000 * val + 13300) / 41714 * 10 + 5) /10);
}

/********* FAN RPM CONVERSIONS ********/
// Higher register values = slower fans (the fan's strobe gates a counter).
// But this chip saturates back at 0, not at 255 like all the other chips.
// So, 0 means 0 RPM
extern inline u8 FAN_TO_REG(long rpm, int div)
{
	if (rpm == 0)
		return 0;
	rpm = SENSORS_LIMIT(rpm, 1, 1000000);
	return SENSORS_LIMIT((1350000 + rpm * div / 2) / (rpm * div), 1, 255);
}

#define FAN_FROM_REG(val,div) ((val)==0?0:(val)==255?0:1350000/((val)*(div)))

/******** TEMP CONVERSIONS (Bob Dougherty) *********/
// linear fits from HWMon.cpp (Copyright 1998-2000 Jonathan Teh Soon Yew)
//      if(temp<169)
//              return double(temp)*0.427-32.08;
//      else if(temp>=169 && temp<=202)
//              return double(temp)*0.582-58.16;
//      else
//              return double(temp)*0.924-127.33;
//
// A fifth-order polynomial fits the unofficial data (provided by Alex van 
// Kaam <darkside@chello.nl>) a bit better.  It also give more reasonable 
// numbers on my machine (ie. they agree with what my BIOS tells me).  
// Here's the fifth-order fit to the 8-bit data:
// temp = 1.625093e-10*val^5 - 1.001632e-07*val^4 + 2.457653e-05*val^3 - 
//        2.967619e-03*val^2 + 2.175144e-01*val - 7.090067e+0.
//
// (2000-10-25- RFD: thanks to Uwe Andersen <uandersen@mayah.com> for 
// finding my typos in this formula!)
//
// Alas, none of the elegant function-fit solutions will work because we 
// aren't allowed to use floating point in the kernel and doing it with 
// integers doesn't rpovide enough precision.  So we'll do boring old 
// look-up table stuff.  The unofficial data (see below) have effectively 
// 7-bit resolution (they are rounded to the nearest degree).  I'm assuming 
// that the transfer function of the device is monotonic and smooth, so a 
// smooth function fit to the data will allow us to get better precision.  
// I used the 5th-order poly fit described above and solved for
// VIA register values 0-255.  I *10 before rounding, so we get tenth-degree 
// precision.  (I could have done all 1024 values for our 10-bit readings, 
// but the function is very linear in the useful range (0-80 deg C), so 
// we'll just use linear interpolation for 10-bit readings.)  So, tempLUT 
// is the temp at via register values 0-255:
static const long tempLUT[] =
    { -709, -688, -667, -646, -627, -607, -589, -570, -553, -536, -519,
	    -503, -487, -471, -456, -442, -428, -414, -400, -387, -375,
	    -362, -350, -339, -327, -316, -305, -295, -285, -275, -265,
	    -255, -246, -237, -229, -220, -212, -204, -196, -188, -180,
	    -173, -166, -159, -152, -145, -139, -132, -126, -120, -114,
	    -108, -102, -96, -91, -85, -80, -74, -69, -64, -59, -54, -49,
	    -44, -39, -34, -29, -25, -20, -15, -11, -6, -2, 3, 7, 12, 16,
	    20, 25, 29, 33, 37, 42, 46, 50, 54, 59, 63, 67, 71, 75, 79, 84,
	    88, 92, 96, 100, 104, 109, 113, 117, 121, 125, 130, 134, 138,
	    142, 146, 151, 155, 159, 163, 168, 172, 176, 181, 185, 189,
	    193, 198, 202, 206, 211, 215, 219, 224, 228, 232, 237, 241,
	    245, 250, 254, 259, 263, 267, 272, 276, 281, 285, 290, 294,
	    299, 303, 307, 312, 316, 321, 325, 330, 334, 339, 344, 348,
	    353, 357, 362, 366, 371, 376, 380, 385, 390, 395, 399, 404,
	    409, 414, 419, 423, 428, 433, 438, 443, 449, 454, 459, 464,
	    469, 475, 480, 486, 491, 497, 502, 508, 514, 520, 526, 532,
	    538, 544, 551, 557, 564, 571, 578, 584, 592, 599, 606, 614,
	    621, 629, 637, 645, 654, 662, 671, 680, 689, 698, 708, 718,
	    728, 738, 749, 759, 770, 782, 793, 805, 818, 830, 843, 856,
	    870, 883, 898, 912, 927, 943, 958, 975, 991, 1008, 1026, 1044,
	    1062, 1081, 1101, 1121, 1141, 1162, 1184, 1206, 1229, 1252,
	    1276, 1301, 1326, 1352, 1378, 1406, 1434, 1462
};

/* the original LUT values from Alex van Kaam <darkside@chello.nl> 
   (for via register values 12-240):
{-50,-49,-47,-45,-43,-41,-39,-38,-37,-35,-34,-33,-32,-31,
-30,-29,-28,-27,-26,-25,-24,-24,-23,-22,-21,-20,-20,-19,-18,-17,-17,-16,-15,
-15,-14,-14,-13,-12,-12,-11,-11,-10,-9,-9,-8,-8,-7,-7,-6,-6,-5,-5,-4,-4,-3,
-3,-2,-2,-1,-1,0,0,1,1,1,3,3,3,4,4,4,5,5,5,6,6,7,7,8,8,9,9,9,10,10,11,11,12,
12,12,13,13,13,14,14,15,15,16,16,16,17,17,18,18,19,19,20,20,21,21,21,22,22,
22,23,23,24,24,25,25,26,26,26,27,27,27,28,28,29,29,30,30,30,31,31,32,32,33,
33,34,34,35,35,35,36,36,37,37,38,38,39,39,40,40,41,41,42,42,43,43,44,44,45,
45,46,46,47,48,48,49,49,50,51,51,52,52,53,53,54,55,55,56,57,57,58,59,59,60,
61,62,62,63,64,65,66,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,83,84,
85,86,88,89,91,92,94,96,97,99,101,103,105,107,109,110};
*/

// Here's the reverse LUT.  I got it by doing a 6-th order poly fit (needed
// an extra term for a good fit to these inverse data!) and then 
// solving for each temp value from -50 to 110 (the useable range for 
// this chip).  Here's the fit: 
// viaRegVal = -1.160370e-10*val^6 +3.193693e-08*val^5 - 1.464447e-06*val^4 
// - 2.525453e-04*val^3 + 1.424593e-02*val^2 + 2.148941e+00*val +7.275808e+01)
// Note that n=161:
static const u8 viaLUT[] =
    { 12, 12, 13, 14, 14, 15, 16, 16, 17, 18, 18, 19, 20, 20, 21, 22, 23,
	    23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 35, 36, 37, 39, 40,
	    41, 43, 45, 46, 48, 49, 51, 53, 55, 57, 59, 60, 62, 64, 66,
	    69, 71, 73, 75, 77, 79, 82, 84, 86, 88, 91, 93, 95, 98, 100,
	    103, 105, 107, 110, 112, 115, 117, 119, 122, 124, 126, 129,
	    131, 134, 136, 138, 140, 143, 145, 147, 150, 152, 154, 156,
	    158, 160, 162, 164, 166, 168, 170, 172, 174, 176, 178, 180,
	    182, 183, 185, 187, 188, 190, 192, 193, 195, 196, 198, 199,
	    200, 202, 203, 205, 206, 207, 208, 209, 210, 211, 212, 213,
	    214, 215, 216, 217, 218, 219, 220, 221, 222, 222, 223, 224,
	    225, 226, 226, 227, 228, 228, 229, 230, 230, 231, 232, 232,
	    233, 233, 234, 235, 235, 236, 236, 237, 237, 238, 238, 239,
	    239, 240
};

/* Converting temps to (8-bit) hyst and over registers */
// No interpolation here.  Just check the limits and go.
// The +5 effectively rounds off properly and the +50 is because 
// the temps start at -50
extern inline u8 TEMP_TO_REG(long val)
{
	return (u8)
	    SENSORS_LIMIT(viaLUT[((val <= -500) ? 0 : (val >= 1100) ? 160 : 
				  ((val + 5) / 10 + 50))], 0, 255);
}

/* for 8-bit temperature hyst and over registers */
// The temp values are already *10, so we don't need to do that.
// But we _will_ round these off to the nearest degree with (...*10+5)/10
#define TEMP_FROM_REG(val) ((tempLUT[(val)]*10+5)/10)

/* for 10-bit temperature readings */
// You might _think_ this is too long to inline, but's it's really only
// called once...
extern inline long TEMP_FROM_REG10(u16 val)
{
	// the temp values are already *10, so we don't need to do that.
	long temp;
	u16 eightBits = val >> 2;
	u16 twoBits = val & 3;

	// handle the extremes first (they won't interpolate well! ;-)
	if (val == 0)
		return (long) tempLUT[0];
	if (val == 1023)
		return (long) tempLUT[255];

	if (twoBits == 0)
		return (long) tempLUT[eightBits];
	else {
		// do some interpolation by multipying the lower and upper
		// bounds by 25, 50 or 75, then /100.
		temp = ((25 * (4 - twoBits)) * tempLUT[eightBits]
			+ (25 * twoBits) * tempLUT[eightBits + 1]);
		// increase the magnitude by 50 to achieve rounding.
		if (temp > 0)
			temp += 50;
		else
			temp -= 50;
		return (temp / 100);
	}
}

#define ALARMS_FROM_REG(val) (val)

#define DIV_FROM_REG(val) (1 << (val))
#define DIV_TO_REG(val) ((val)==8?3:(val)==4?2:(val)==1?0:1)

/* Initial limits */
#define VIA686A_INIT_IN_0 200
#define VIA686A_INIT_IN_1 250
#define VIA686A_INIT_IN_2 330
#define VIA686A_INIT_IN_3 500
#define VIA686A_INIT_IN_4 1200

#define VIA686A_INIT_IN_PERCENTAGE 10

#define VIA686A_INIT_IN_MIN_0 (VIA686A_INIT_IN_0 - VIA686A_INIT_IN_0 \
        * VIA686A_INIT_IN_PERCENTAGE / 100)
#define VIA686A_INIT_IN_MAX_0 (VIA686A_INIT_IN_0 + VIA686A_INIT_IN_0 \
        * VIA686A_INIT_IN_PERCENTAGE / 100)
#define VIA686A_INIT_IN_MIN_1 (VIA686A_INIT_IN_1 - VIA686A_INIT_IN_1 \
        * VIA686A_INIT_IN_PERCENTAGE / 100)
#define VIA686A_INIT_IN_MAX_1 (VIA686A_INIT_IN_1 + VIA686A_INIT_IN_1 \
        * VIA686A_INIT_IN_PERCENTAGE / 100)
#define VIA686A_INIT_IN_MIN_2 (VIA686A_INIT_IN_2 - VIA686A_INIT_IN_2 \
        * VIA686A_INIT_IN_PERCENTAGE / 100)
#define VIA686A_INIT_IN_MAX_2 (VIA686A_INIT_IN_2 + VIA686A_INIT_IN_2 \
        * VIA686A_INIT_IN_PERCENTAGE / 100)
#define VIA686A_INIT_IN_MIN_3 (VIA686A_INIT_IN_3 - VIA686A_INIT_IN_3 \
        * VIA686A_INIT_IN_PERCENTAGE / 100)
#define VIA686A_INIT_IN_MAX_3 (VIA686A_INIT_IN_3 + VIA686A_INIT_IN_3 \
        * VIA686A_INIT_IN_PERCENTAGE / 100)
#define VIA686A_INIT_IN_MIN_4 (VIA686A_INIT_IN_4 - VIA686A_INIT_IN_4 \
        * VIA686A_INIT_IN_PERCENTAGE / 100)
#define VIA686A_INIT_IN_MAX_4 (VIA686A_INIT_IN_4 + VIA686A_INIT_IN_4 \
        * VIA686A_INIT_IN_PERCENTAGE / 100)

#define VIA686A_INIT_FAN_MIN	3000

#define VIA686A_INIT_TEMP_OVER 600
#define VIA686A_INIT_TEMP_HYST 500

#ifdef MODULE
extern int init_module(void);
extern int cleanup_module(void);
#endif				/* MODULE */

/* For the VIA686A, we need to keep some data in memory. That
   data is pointed to by via686a_list[NR]->data. The structure itself is
   dynamically allocated, at the same time when a new via686a client is
   allocated. */
struct via686a_data {
	struct semaphore lock;
	int sysctl_id;

	struct semaphore update_lock;
	char valid;		/* !=0 if following fields are valid */
	unsigned long last_updated;	/* In jiffies */

	u8 in[5];		/* Register value */
	u8 in_max[5];		/* Register value */
	u8 in_min[5];		/* Register value */
	u8 fan[2];		/* Register value */
	u8 fan_min[2];		/* Register value */
	u16 temp[3];		/* Register value 10 bit */
	u8 temp_over[3];	/* Register value */
	u8 temp_hyst[3];	/* Register value */
	u8 fan_div[2];		/* Register encoding, shifted right */
	u16 alarms;		/* Register encoding, combined */
};

static struct pci_dev *s_bridge;	/* pointer to the (only) via686a */

#ifdef MODULE
static
#else
extern
#endif
int __init sensors_via686a_init(void);
static int __init via686a_cleanup(void);

static int via686a_attach_adapter(struct i2c_adapter *adapter);
static int via686a_detect(struct i2c_adapter *adapter, int address,
			  unsigned short flags, int kind);
static int via686a_detach_client(struct i2c_client *client);
static int via686a_command(struct i2c_client *client, unsigned int cmd,
			   void *arg);
static void via686a_inc_use(struct i2c_client *client);
static void via686a_dec_use(struct i2c_client *client);

static int via686a_read_value(struct i2c_client *client, u8 register);
static void via686a_write_value(struct i2c_client *client, u8 register,
				u8 value);
static void via686a_update_client(struct i2c_client *client);
static void via686a_init_client(struct i2c_client *client);
static int via686a_find(int *address);


static void via686a_in(struct i2c_client *client, int operation,
		       int ctl_name, int *nrels_mag, long *results);
static void via686a_fan(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void via686a_temp(struct i2c_client *client, int operation,
			 int ctl_name, int *nrels_mag, long *results);
static void via686a_alarms(struct i2c_client *client, int operation,
			   int ctl_name, int *nrels_mag, long *results);
static void via686a_fan_div(struct i2c_client *client, int operation,
			    int ctl_name, int *nrels_mag, long *results);

static int via686a_id = 0;

/* The driver. I choose to use type i2c_driver, as at is identical to both
   smbus_driver and isa_driver, and clients could be of either kind */
static struct i2c_driver via686a_driver = {
	/* name */ "VIA 686A",
	/* id */ I2C_DRIVERID_VIA686A,
	/* flags */ I2C_DF_NOTIFY,
	/* attach_adapter */ &via686a_attach_adapter,
	/* detach_client */ &via686a_detach_client,
	/* command */ &via686a_command,
	/* inc_use */ &via686a_inc_use,
	/* dec_use */ &via686a_dec_use
};

/* Used by via686a_init/cleanup */
static int __initdata via686a_initialized = 0;

/* The /proc/sys entries */
/* These files are created for each detected VIA686A. This is just a template;
   though at first sight, you might think we could use a statically
   allocated list, we need some way to get back to the parent - which
   is done through one of the 'extra' fields which are initialized 
   when a new copy is allocated. */
static ctl_table via686a_dir_table_template[] = {
	{VIA686A_SYSCTL_IN0, "in0", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &via686a_in},
	{VIA686A_SYSCTL_IN1, "in1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &via686a_in},
	{VIA686A_SYSCTL_IN2, "in2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &via686a_in},
	{VIA686A_SYSCTL_IN3, "in3", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &via686a_in},
	{VIA686A_SYSCTL_IN4, "in4", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &via686a_in},
	{VIA686A_SYSCTL_FAN1, "fan1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &via686a_fan},
	{VIA686A_SYSCTL_FAN2, "fan2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &via686a_fan},
	{VIA686A_SYSCTL_TEMP, "temp1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &via686a_temp},
	{VIA686A_SYSCTL_TEMP2, "temp2", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &via686a_temp},
	{VIA686A_SYSCTL_TEMP3, "temp3", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &via686a_temp},
	{VIA686A_SYSCTL_FAN_DIV, "fan_div", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &via686a_fan_div},
	{VIA686A_SYSCTL_ALARMS, "alarms", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &via686a_alarms},
	{0}
};

static inline int via686a_read_value(struct i2c_client *client, u8 reg)
{
	return (inb_p(client->addr + reg));
}

static inline void via686a_write_value(struct i2c_client *client, u8 reg,
				       u8 value)
{
	outb_p(value, client->addr + reg);
}

/* This is called when the module is loaded */
int via686a_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_detect(adapter, &addr_data, via686a_detect);
}

/* Locate chip and get correct base address */
int via686a_find(int *address)
{
	u16 val;

	if (!pci_present())
		return -ENODEV;

	if (!(s_bridge = pci_find_device(PCI_VENDOR_ID_VIA,
					 PCI_DEVICE_ID_VIA_82C686_4,
					 NULL))) return -ENODEV;

	if (PCIBIOS_SUCCESSFUL !=
	    pci_read_config_word(s_bridge, VIA686A_BASE_REG, &val))
		return -ENODEV;
	*address = val & ~(VIA686A_EXTENT - 1);
	if (*address == 0 && force_addr == 0) {
		printk("via686a.o: base address not set - upgrade BIOS or use force_addr=0xaddr\n");
		return -ENODEV;
	}
	if (force_addr)
		*address = force_addr;	/* so detect will get called */

	return 0;
}

int via686a_detect(struct i2c_adapter *adapter, int address,
		   unsigned short flags, int kind)
{
	int i;
	struct i2c_client *new_client;
	struct via686a_data *data;
	int err = 0;
	const char *type_name = "via686a";
	u16 val;

	/* Make sure we are probing the ISA bus!!  */
	if (!i2c_is_isa_adapter(adapter)) {
		printk
		("via686a.o: via686a_detect called for an I2C bus adapter?!?\n");
		return 0;
	}

	if(force_addr)
		address = force_addr & ~(VIA686A_EXTENT - 1);
	if (check_region(address, VIA686A_EXTENT)) {
		printk("via686a.o: region 0x%x already in use!\n",
		       address);
		return -ENODEV;
	}

	if(force_addr) {
		printk("via686a.o: forcing ISA address 0x%04X\n", address);
		if (PCIBIOS_SUCCESSFUL !=
		    pci_write_config_word(s_bridge, VIA686A_BASE_REG, address))
			return -ENODEV;
	}
	if (PCIBIOS_SUCCESSFUL !=
	    pci_read_config_word(s_bridge, VIA686A_ENABLE_REG, &val))
		return -ENODEV;
	if (!(val & 0x0001)) {
		printk("via686a.o: enabling sensors\n");
		if (PCIBIOS_SUCCESSFUL !=
		    pci_write_config_word(s_bridge, VIA686A_ENABLE_REG,
		                      val | 0x0001))
			return -ENODEV;
	}

	if (!(new_client = kmalloc(sizeof(struct i2c_client) +
				   sizeof(struct via686a_data),
				   GFP_KERNEL))) {
		err = -ENOMEM;
		goto ERROR0;
	}

	data = (struct via686a_data *) (new_client + 1);
	new_client->addr = address;
	init_MUTEX(&data->lock);
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &via686a_driver;
	new_client->flags = 0;

	/* Reserve the ISA region */
	request_region(address, VIA686A_EXTENT, "via686a-sensors");

	/* Fill in the remaining client fields and put into the global list */
	strcpy(new_client->name, "Via 686A Integrated Sensors");

	new_client->id = via686a_id++;
	data->valid = 0;
	init_MUTEX(&data->update_lock);

	/* Tell the I2C layer a new client has arrived */
	if ((err = i2c_attach_client(new_client)))
		goto ERROR3;

	/* Register a new directory entry with module sensors */
	if ((i = i2c_register_entry((struct i2c_client *) new_client,
					type_name,
					via686a_dir_table_template,
					THIS_MODULE)) < 0) {
		err = i;
		goto ERROR4;
	}
	data->sysctl_id = i;

	/* Initialize the VIA686A chip */
	via686a_init_client(new_client);
	return 0;

      ERROR4:
	i2c_detach_client(new_client);
      ERROR3:
	release_region(address, VIA686A_EXTENT);
	kfree(new_client);
      ERROR0:
	return err;
}

int via686a_detach_client(struct i2c_client *client)
{
	int err;

	i2c_deregister_entry(((struct via686a_data *) 
				  (client->data))->sysctl_id);

	if ((err = i2c_detach_client(client))) {
		printk
		("via686a.o: Client deregistration failed, client not detached.\n");
		return err;
	}

	release_region(client->addr, VIA686A_EXTENT);
	kfree(client);

	return 0;
}

/* No commands defined yet */
int via686a_command(struct i2c_client *client, unsigned int cmd, void *arg)
{
	return 0;
}

void via686a_inc_use(struct i2c_client *client)
{
	MOD_INC_USE_COUNT;
}

void via686a_dec_use(struct i2c_client *client)
{
	MOD_DEC_USE_COUNT;
}

/* Called when we have found a new VIA686A. Set limits, etc. */
void via686a_init_client(struct i2c_client *client)
{
	int i;

	/* Reset the device */
	via686a_write_value(client, VIA686A_REG_CONFIG, 0x80);

	/* Have to wait for reset to complete or else the following
	   initializations won't work reliably. The delay was arrived at
	   empirically, the datasheet doesn't tell you.
	   Waiting for the reset bit to clear doesn't work, it
	   clears in about 2-4 udelays and that isn't nearly enough. */
	udelay(50);

	via686a_write_value(client, VIA686A_REG_IN_MIN(0),
			    IN_TO_REG(VIA686A_INIT_IN_MIN_0, 0));
	via686a_write_value(client, VIA686A_REG_IN_MAX(0),
			    IN_TO_REG(VIA686A_INIT_IN_MAX_0, 0));
	via686a_write_value(client, VIA686A_REG_IN_MIN(1),
			    IN_TO_REG(VIA686A_INIT_IN_MIN_1, 1));
	via686a_write_value(client, VIA686A_REG_IN_MAX(1),
			    IN_TO_REG(VIA686A_INIT_IN_MAX_1, 1));
	via686a_write_value(client, VIA686A_REG_IN_MIN(2),
			    IN_TO_REG(VIA686A_INIT_IN_MIN_2, 2));
	via686a_write_value(client, VIA686A_REG_IN_MAX(2),
			    IN_TO_REG(VIA686A_INIT_IN_MAX_2, 2));
	via686a_write_value(client, VIA686A_REG_IN_MIN(3),
			    IN_TO_REG(VIA686A_INIT_IN_MIN_3, 3));
	via686a_write_value(client, VIA686A_REG_IN_MAX(3),
			    IN_TO_REG(VIA686A_INIT_IN_MAX_3, 3));
	via686a_write_value(client, VIA686A_REG_IN_MIN(4),
			    IN_TO_REG(VIA686A_INIT_IN_MIN_4, 4));
	via686a_write_value(client, VIA686A_REG_IN_MAX(4),
			    IN_TO_REG(VIA686A_INIT_IN_MAX_4, 4));
	via686a_write_value(client, VIA686A_REG_FAN_MIN(1),
			    FAN_TO_REG(VIA686A_INIT_FAN_MIN, 2));
	via686a_write_value(client, VIA686A_REG_FAN_MIN(2),
			    FAN_TO_REG(VIA686A_INIT_FAN_MIN, 2));
	for (i = 1; i <= 3; i++) {
		via686a_write_value(client, VIA686A_REG_TEMP_OVER(i),
				    TEMP_TO_REG(VIA686A_INIT_TEMP_OVER));
		via686a_write_value(client, VIA686A_REG_TEMP_HYST(i),
				    TEMP_TO_REG(VIA686A_INIT_TEMP_HYST));
	}

	/* Start monitoring */
	via686a_write_value(client, VIA686A_REG_CONFIG, 0x01);

	/* Cofigure temp interrupt mode for continuous-interrupt operation */
	via686a_write_value(client, VIA686A_REG_TEMP_MODE, 
			    via686a_read_value(client, VIA686A_REG_TEMP_MODE) &
			    !(VIA686A_TEMP_MODE_MASK | VIA686A_TEMP_MODE_CONTINUOUS));
}

void via686a_update_client(struct i2c_client *client)
{
	struct via686a_data *data = client->data;
	int i;

	down(&data->update_lock);

	if ((jiffies - data->last_updated > HZ + HZ / 2) ||
	    (jiffies < data->last_updated) || !data->valid) {

		for (i = 0; i <= 4; i++) {
			data->in[i] =
			    via686a_read_value(client, VIA686A_REG_IN(i));
			data->in_min[i] = via686a_read_value(client,
							     VIA686A_REG_IN_MIN
							     (i));
			data->in_max[i] =
			    via686a_read_value(client, VIA686A_REG_IN_MAX(i));
		}
		for (i = 1; i <= 2; i++) {
			data->fan[i - 1] =
			    via686a_read_value(client, VIA686A_REG_FAN(i));
			data->fan_min[i - 1] = via686a_read_value(client,
						     VIA686A_REG_FAN_MIN(i));
		}
		for (i = 1; i <= 3; i++) {
			data->temp[i - 1] = via686a_read_value(client,
						 VIA686A_REG_TEMP(i)) << 2;
			data->temp_over[i - 1] =
			    via686a_read_value(client,
					       VIA686A_REG_TEMP_OVER(i));
			data->temp_hyst[i - 1] =
			    via686a_read_value(client,
					       VIA686A_REG_TEMP_HYST(i));
		}
		/* add in lower 2 bits 
		   temp1 uses bits 7-6 of VIA686A_REG_TEMP_LOW1
		   temp2 uses bits 5-4 of VIA686A_REG_TEMP_LOW23
		   temp3 uses bits 7-6 of VIA686A_REG_TEMP_LOW23
		 */
		data->temp[0] |= (via686a_read_value(client,
						     VIA686A_REG_TEMP_LOW1)
				  & 0xc0) >> 6;
		data->temp[1] |=
		    (via686a_read_value(client, VIA686A_REG_TEMP_LOW23) &
		     0x30) >> 4;
		data->temp[2] |=
		    (via686a_read_value(client, VIA686A_REG_TEMP_LOW23) &
		     0xc0) >> 6;

		i = via686a_read_value(client, VIA686A_REG_FANDIV);
		data->fan_div[0] = (i >> 4) & 0x03;
		data->fan_div[1] = i >> 6;
		data->alarms =
		    via686a_read_value(client,
				       VIA686A_REG_ALARM1) |
		    (via686a_read_value(client, VIA686A_REG_ALARM2) << 8);
		data->last_updated = jiffies;
		data->valid = 1;
	}

	up(&data->update_lock);
}


/* The next few functions are the call-back functions of the /proc/sys and
   sysctl files. Which function is used is defined in the ctl_table in
   the extra1 field.
   Each function must return the magnitude (power of 10 to divide the date
   with) if it is called with operation==SENSORS_PROC_REAL_INFO. It must
   put a maximum of *nrels elements in results reflecting the data of this
   file, and set *nrels to the number it actually put in it, if operation==
   SENSORS_PROC_REAL_READ. Finally, it must get upto *nrels elements from
   results and write them to the chip, if operations==SENSORS_PROC_REAL_WRITE.
   Note that on SENSORS_PROC_REAL_READ, I do not check whether results is
   large enough (by checking the incoming value of *nrels). This is not very
   good practice, but as long as you put less than about 5 values in results,
   you can assume it is large enough. */
void via686a_in(struct i2c_client *client, int operation, int ctl_name,
		int *nrels_mag, long *results)
{
	struct via686a_data *data = client->data;
	int nr = ctl_name - VIA686A_SYSCTL_IN0;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 2;
	else if (operation == SENSORS_PROC_REAL_READ) {
		via686a_update_client(client);
		results[0] = IN_FROM_REG(data->in_min[nr], nr);
		results[1] = IN_FROM_REG(data->in_max[nr], nr);
		results[2] = IN_FROM_REG(data->in[nr], nr);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->in_min[nr] = IN_TO_REG(results[0], nr);
			via686a_write_value(client, VIA686A_REG_IN_MIN(nr),
					    data->in_min[nr]);
		}
		if (*nrels_mag >= 2) {
			data->in_max[nr] = IN_TO_REG(results[1], nr);
			via686a_write_value(client, VIA686A_REG_IN_MAX(nr),
					    data->in_max[nr]);
		}
	}
}

void via686a_fan(struct i2c_client *client, int operation, int ctl_name,
		 int *nrels_mag, long *results)
{
	struct via686a_data *data = client->data;
	int nr = ctl_name - VIA686A_SYSCTL_FAN1 + 1;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		via686a_update_client(client);
		results[0] = FAN_FROM_REG(data->fan_min[nr - 1],
					  DIV_FROM_REG(data->fan_div
						       [nr - 1]));
		results[1] =
		    FAN_FROM_REG(data->fan[nr - 1],
				 DIV_FROM_REG(data->fan_div[nr - 1]));
		*nrels_mag = 2;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->fan_min[nr - 1] = FAN_TO_REG(results[0], 
							   DIV_FROM_REG(data->
							      fan_div[nr -1]));
			via686a_write_value(client,
					    VIA686A_REG_FAN_MIN(nr),
					    data->fan_min[nr - 1]);
		}
	}
}

void via686a_temp(struct i2c_client *client, int operation, int ctl_name,
		  int *nrels_mag, long *results)
{
	struct via686a_data *data = client->data;
	int nr = ctl_name - VIA686A_SYSCTL_TEMP;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 1;
	else if (operation == SENSORS_PROC_REAL_READ) {
		via686a_update_client(client);
		results[0] = TEMP_FROM_REG(data->temp_over[nr]);
		results[1] = TEMP_FROM_REG(data->temp_hyst[nr]);
		results[2] = TEMP_FROM_REG10(data->temp[nr]);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->temp_over[nr] = TEMP_TO_REG(results[0]);
			via686a_write_value(client,
					    VIA686A_REG_TEMP_OVER(nr + 1),
					    data->temp_over[nr]);
		}
		if (*nrels_mag >= 2) {
			data->temp_hyst[nr] = TEMP_TO_REG(results[1]);
			via686a_write_value(client,
					    VIA686A_REG_TEMP_HYST(nr + 1),
					    data->temp_hyst[nr]);
		}
	}
}

void via686a_alarms(struct i2c_client *client, int operation, int ctl_name,
		    int *nrels_mag, long *results)
{
	struct via686a_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		via686a_update_client(client);
		results[0] = ALARMS_FROM_REG(data->alarms);
		*nrels_mag = 1;
	}
}

void via686a_fan_div(struct i2c_client *client, int operation,
		     int ctl_name, int *nrels_mag, long *results)
{
	struct via686a_data *data = client->data;
	int old;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		via686a_update_client(client);
		results[0] = DIV_FROM_REG(data->fan_div[0]);
		results[1] = DIV_FROM_REG(data->fan_div[1]);
		*nrels_mag = 2;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		old = via686a_read_value(client, VIA686A_REG_FANDIV);
		if (*nrels_mag >= 2) {
			data->fan_div[1] = DIV_TO_REG(results[1]);
			old = (old & 0x3f) | (data->fan_div[1] << 6);
		}
		if (*nrels_mag >= 1) {
			data->fan_div[0] = DIV_TO_REG(results[0]);
			old = (old & 0xcf) | (data->fan_div[0] << 4);
			via686a_write_value(client, VIA686A_REG_FANDIV,
					    old);
		}
	}
}

int __init sensors_via686a_init(void)
{
	int res, addr;

	printk("via686a.o version %s (%s)\n", LM_VERSION, LM_DATE);
	via686a_initialized = 0;

	if (via686a_find(&addr)) {
		printk("via686a.o: No Via 686A sensors found.\n");
		return -ENODEV;
	}
	normal_isa[0] = addr;

	if ((res = i2c_add_driver(&via686a_driver))) {
		printk("via686a.o: Driver registration failed.\n");
		via686a_cleanup();
		return res;
	}
	via686a_initialized++;
	return 0;
}

int __init via686a_cleanup(void)
{
	int res;

	if (via686a_initialized >= 1) {
		if ((res = i2c_del_driver(&via686a_driver))) {
			printk
			    ("via686a.o: Driver deregistration failed.\n");
			return res;
		}
		via686a_initialized--;
	}
	return 0;
}

EXPORT_NO_SYMBOLS;

#ifdef MODULE

MODULE_AUTHOR
    ("Kyösti Mälkki <kmalkki@cc.hut.fi>, Mark Studebaker <mdsxyz123@yahoo.com>, Bob Dougherty <bobd@stanford.edu>");
MODULE_DESCRIPTION("VIA 686A Sensor device");

int init_module(void)
{
	return sensors_via686a_init();
}

int cleanup_module(void)
{
	return via686a_cleanup();
}

#endif				/* MODULE */
