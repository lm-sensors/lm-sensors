/*
    w83627hf.c - Part of lm_sensors, Linux kernel modules for hardware
                monitoring
    Copyright (c) 1998 - 2002  Frodo Looijaard <frodol@dds.nl>,
    Philip Edelbrock <phil@netroedge.com>,
    and Mark Studebaker <mdsxyz123@yahoo.com>

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
    Supports following chips:

    Chip	#vin	#fanin	#pwm	#temp	wchipid	vendid	i2c	ISA
    w83627hf	9	3	2	3	0x20	0x5ca3	no	yes(LPC)
    w83697hf	8	2	2	2	0x60	0x5ca3	no	yes(LPC)

    For other winbond chips, and for i2c support in the above chips,
    use w83627hf.c.
*/

#include <linux/version.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/ioport.h>
#include <linux/sysctl.h>
#include <asm/errno.h>
#include <asm/io.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include "version.h"
#include "sensors.h"
#include "sensors_vid.h"
#include <linux/init.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,2,18)) || \
    (LINUX_VERSION_CODE == KERNEL_VERSION(2,3,0))
#define init_MUTEX(s) do { *(s) = MUTEX; } while(0)
#endif

#ifndef THIS_MODULE
#define THIS_MODULE NULL
#endif

static int force_addr;
MODULE_PARM(force_addr, "i");
MODULE_PARM_DESC(force_addr,
		 "Initialize the base address of the sensors");
static int force_i2c = 0x29;
MODULE_PARM(force_i2c, "i");
MODULE_PARM_DESC(force_i2c,
		 "Initialize the i2c address of the sensors");

/* Addresses to scan */
static unsigned short normal_i2c[] = { SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { SENSORS_I2C_END };
static unsigned int normal_isa[] = { 0, SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/* Insmod parameters */
SENSORS_INSMOD_2(w83627hf, w83697hf);

static int init = 1;
MODULE_PARM(init, "i");
MODULE_PARM_DESC(init, "Set to zero to bypass chip initialization");

/* modified from kernel/include/traps.c */
#define	REG	0x2e	/* The register to read/write */
#define	DEV	0x07	/* Register: Logical device select */
#define	VAL	0x2f	/* The value to read/write */
#define PME	0x0b	/* The device with the hardware monitor */
#define	DEVID	0x20	/* Register: Device ID */

static inline void
superio_outb(int reg, int val)
{
	outb(reg, REG);
	outb(val, VAL);
}

static inline int
superio_inb(int reg)
{
	outb(reg, REG);
	return inb(VAL);
}

static inline void
superio_select(void)
{
	outb(DEV, REG);
	outb(PME, VAL);
}

static inline void
superio_enter(void)
{
	outb(0x87, REG);
	outb(0x87, REG);
}

static inline void
superio_exit(void)
{
	outb(0xAA, REG);
}

#define W627_DEVID 0x52
#define W697_DEVID 0x60
#define WINB_ACT_REG 0x30
#define WINB_BASE_REG 0x60
/* Constants specified below */

/* Length of ISA address segment */
#define WINB_EXTENT 8

/* Where are the ISA address/data registers relative to the base address */
#define W83781D_ADDR_REG_OFFSET 5
#define W83781D_DATA_REG_OFFSET 6

/* The W83781D registers */
/* The W83782D registers for nr=7,8 are in bank 5 */
#define W83781D_REG_IN_MAX(nr) ((nr < 7) ? (0x2b + (nr) * 2) : \
					   (0x554 + (((nr) - 7) * 2)))
#define W83781D_REG_IN_MIN(nr) ((nr < 7) ? (0x2c + (nr) * 2) : \
					   (0x555 + (((nr) - 7) * 2)))
#define W83781D_REG_IN(nr)     ((nr < 7) ? (0x20 + (nr)) : \
					   (0x550 + (nr) - 7))

#define W83781D_REG_FAN_MIN(nr) (0x3a + (nr))
#define W83781D_REG_FAN(nr) (0x27 + (nr))

#define W83781D_REG_TEMP2 0x0150
#define W83781D_REG_TEMP3 0x0250
#define W83781D_REG_TEMP2_HYST 0x153
#define W83781D_REG_TEMP3_HYST 0x253
#define W83781D_REG_TEMP2_CONFIG 0x152
#define W83781D_REG_TEMP3_CONFIG 0x252
#define W83781D_REG_TEMP2_OVER 0x155
#define W83781D_REG_TEMP3_OVER 0x255

#define W83781D_REG_TEMP 0x27
#define W83781D_REG_TEMP_OVER 0x39
#define W83781D_REG_TEMP_HYST 0x3A
#define W83781D_REG_BANK 0x4E

#define W83781D_REG_CONFIG 0x40
#define W83781D_REG_ALARM1 0x41
#define W83781D_REG_ALARM2 0x42
#define W83781D_REG_ALARM3 0x450	/* not on W83781D */

#define W83781D_REG_IRQ 0x4C
#define W83781D_REG_BEEP_CONFIG 0x4D
#define W83781D_REG_BEEP_INTS1 0x56
#define W83781D_REG_BEEP_INTS2 0x57
#define W83781D_REG_BEEP_INTS3 0x453	/* not on W83781D */

#define W83781D_REG_VID_FANDIV 0x47

#define W83781D_REG_CHIPID 0x49
#define W83781D_REG_WCHIPID 0x58
#define W83781D_REG_CHIPMAN 0x4F
#define W83781D_REG_PIN 0x4B

/* 782D/783S only */
#define W83781D_REG_VBAT 0x5D

/* PWM 782D (1-4) and 783S (1-2) only */
#define W83781D_REG_PWM1 0x5B	/* 782d and 783s/627hf datasheets disagree */
				/* on which is which; */
#define W83781D_REG_PWM2 0x5A	/* We follow the 782d convention here, */
				/* However 782d is probably wrong. */
#define W83781D_REG_PWM3 0x5E
#define W83781D_REG_PWM4 0x5F
#define W83781D_REG_PWMCLK12 0x5C
#define W83781D_REG_PWMCLK34 0x45C
static const u8 regpwm[] = { W83781D_REG_PWM1, W83781D_REG_PWM2,
	W83781D_REG_PWM3, W83781D_REG_PWM4
};
#define W83781D_REG_PWM(nr) (regpwm[(nr) - 1])

#define W83781D_REG_I2C_ADDR 0x48
#define W83781D_REG_I2C_SUBADDR 0x4A

/* The following are undocumented in the data sheets however we
   received the information in an email from Winbond tech support */
/* Sensor selection - not on 781d */
#define W83781D_REG_SCFG1 0x5D
static const u8 BIT_SCFG1[] = { 0x02, 0x04, 0x08 };
#define W83781D_REG_SCFG2 0x59
static const u8 BIT_SCFG2[] = { 0x10, 0x20, 0x40 };
#define W83781D_DEFAULT_BETA 3435

/* RT Table registers */
#define W83781D_REG_RT_IDX 0x50
#define W83781D_REG_RT_VAL 0x51

/* Conversions. Rounding and limit checking is only done on the TO_REG
   variants. Note that you should be a bit careful with which arguments
   these macros are called: arguments may be evaluated more than once.
   Fixing this is just not worth it. */
#define IN_TO_REG(val)  (SENSORS_LIMIT((((val) * 10 + 8)/16),0,255))
#define IN_FROM_REG(val) (((val) * 16) / 10)

extern inline u8 FAN_TO_REG(long rpm, int div)
{
	if (rpm == 0)
		return 255;
	rpm = SENSORS_LIMIT(rpm, 1, 1000000);
	return SENSORS_LIMIT((1350000 + rpm * div / 2) / (rpm * div), 1,
			     254);
}

#define FAN_FROM_REG(val,div) ((val)==0?-1:(val)==255?0:1350000/((val)*(div)))

#define TEMP_TO_REG(val) (SENSORS_LIMIT(((val)<0?(((val)-5)/10):\
                                                 ((val)+5)/10),0,255))
#define TEMP_FROM_REG(val) (((val)>0x80?(val)-0x100:(val))*10)

#define TEMP_ADD_TO_REG(val)   (SENSORS_LIMIT(((((val) + 2) / 5) << 7),\
                                              0,0xffff))
#define TEMP_ADD_FROM_REG(val) (((val) >> 7) * 5)

#define PWM_TO_REG(val) (SENSORS_LIMIT((val),0,255))
#define BEEPS_TO_REG(val) ((val) & 0xffffff)

#define BEEP_ENABLE_TO_REG(val)   ((val)?1:0)
#define BEEP_ENABLE_FROM_REG(val) ((val)?1:0)

#define DIV_FROM_REG(val) (1 << (val))

extern inline u8 DIV_TO_REG(long val)
{
	int i;
	val = SENSORS_LIMIT(val, 1, 128) >> 1;
	for (i = 0; i < 6; i++) {
		if (val == 0)
			break;
		val >>= 1;
	}
	return ((u8) i);
}

/* Initial limits */
#define W83781D_INIT_IN_0 (vid==3500?280:vid/10)
#define W83781D_INIT_IN_1 (vid==3500?280:vid/10)
#define W83781D_INIT_IN_2 330
#define W83781D_INIT_IN_3 (((500)   * 100)/168)
#define W83781D_INIT_IN_4 (((1200)  * 10)/38)
#define W83781D_INIT_IN_5 (((-1200) * -604)/2100)
#define W83781D_INIT_IN_6 (((-500)  * -604)/909)
#define W83781D_INIT_IN_7 (((500)   * 100)/168)
#define W83781D_INIT_IN_8 300
/* Initial limits for 782d/783s negative voltages */
/* Note level shift. Change min/max below if you change these. */
#define W83782D_INIT_IN_5 ((((-1200) + 1491) * 100)/514)
#define W83782D_INIT_IN_6 ((( (-500)  + 771) * 100)/314)

#define W83781D_INIT_IN_PERCENTAGE 10

#define W83781D_INIT_IN_MIN_0 \
        (W83781D_INIT_IN_0 - W83781D_INIT_IN_0 * W83781D_INIT_IN_PERCENTAGE \
         / 100)
#define W83781D_INIT_IN_MAX_0 \
        (W83781D_INIT_IN_0 + W83781D_INIT_IN_0 * W83781D_INIT_IN_PERCENTAGE \
         / 100)
#define W83781D_INIT_IN_MIN_1 \
        (W83781D_INIT_IN_1 - W83781D_INIT_IN_1 * W83781D_INIT_IN_PERCENTAGE \
         / 100)
#define W83781D_INIT_IN_MAX_1 \
        (W83781D_INIT_IN_1 + W83781D_INIT_IN_1 * W83781D_INIT_IN_PERCENTAGE \
         / 100)
#define W83781D_INIT_IN_MIN_2 \
        (W83781D_INIT_IN_2 - W83781D_INIT_IN_2 * W83781D_INIT_IN_PERCENTAGE \
         / 100)
#define W83781D_INIT_IN_MAX_2 \
        (W83781D_INIT_IN_2 + W83781D_INIT_IN_2 * W83781D_INIT_IN_PERCENTAGE \
         / 100)
#define W83781D_INIT_IN_MIN_3 \
        (W83781D_INIT_IN_3 - W83781D_INIT_IN_3 * W83781D_INIT_IN_PERCENTAGE \
         / 100)
#define W83781D_INIT_IN_MAX_3 \
        (W83781D_INIT_IN_3 + W83781D_INIT_IN_3 * W83781D_INIT_IN_PERCENTAGE \
         / 100)
#define W83781D_INIT_IN_MIN_4 \
        (W83781D_INIT_IN_4 - W83781D_INIT_IN_4 * W83781D_INIT_IN_PERCENTAGE \
         / 100)
#define W83781D_INIT_IN_MAX_4 \
        (W83781D_INIT_IN_4 + W83781D_INIT_IN_4 * W83781D_INIT_IN_PERCENTAGE \
         / 100)
#define W83781D_INIT_IN_MIN_5 \
        (W83781D_INIT_IN_5 - W83781D_INIT_IN_5 * W83781D_INIT_IN_PERCENTAGE \
         / 100)
#define W83781D_INIT_IN_MAX_5 \
        (W83781D_INIT_IN_5 + W83781D_INIT_IN_5 * W83781D_INIT_IN_PERCENTAGE \
         / 100)
#define W83781D_INIT_IN_MIN_6 \
        (W83781D_INIT_IN_6 - W83781D_INIT_IN_6 * W83781D_INIT_IN_PERCENTAGE \
         / 100)
#define W83781D_INIT_IN_MAX_6 \
        (W83781D_INIT_IN_6 + W83781D_INIT_IN_6 * W83781D_INIT_IN_PERCENTAGE \
         / 100)
#define W83781D_INIT_IN_MIN_7 \
        (W83781D_INIT_IN_7 - W83781D_INIT_IN_7 * W83781D_INIT_IN_PERCENTAGE \
         / 100)
#define W83781D_INIT_IN_MAX_7 \
        (W83781D_INIT_IN_7 + W83781D_INIT_IN_7 * W83781D_INIT_IN_PERCENTAGE \
         / 100)
#define W83781D_INIT_IN_MIN_8 \
        (W83781D_INIT_IN_8 - W83781D_INIT_IN_8 * W83781D_INIT_IN_PERCENTAGE \
         / 100)
#define W83781D_INIT_IN_MAX_8 \
        (W83781D_INIT_IN_8 + W83781D_INIT_IN_8 * W83781D_INIT_IN_PERCENTAGE \
         / 100)
/* Initial limits for 782d/783s negative voltages */
/* These aren't direct multiples because of level shift */
/* Beware going negative - check */
#define W83782D_INIT_IN_MIN_5_TMP \
        (((-1200 * (100 + W83781D_INIT_IN_PERCENTAGE)) + (1491 * 100))/514)
#define W83782D_INIT_IN_MIN_5 \
        ((W83782D_INIT_IN_MIN_5_TMP > 0) ? W83782D_INIT_IN_MIN_5_TMP : 0)
#define W83782D_INIT_IN_MAX_5 \
        (((-1200 * (100 - W83781D_INIT_IN_PERCENTAGE)) + (1491 * 100))/514)
#define W83782D_INIT_IN_MIN_6_TMP \
        ((( -500 * (100 + W83781D_INIT_IN_PERCENTAGE)) +  (771 * 100))/314)
#define W83782D_INIT_IN_MIN_6 \
        ((W83782D_INIT_IN_MIN_6_TMP > 0) ? W83782D_INIT_IN_MIN_6_TMP : 0)
#define W83782D_INIT_IN_MAX_6 \
        ((( -500 * (100 - W83781D_INIT_IN_PERCENTAGE)) +  (771 * 100))/314)

#define W83781D_INIT_FAN_MIN_1 3000
#define W83781D_INIT_FAN_MIN_2 3000
#define W83781D_INIT_FAN_MIN_3 3000

#define W83781D_INIT_TEMP_OVER 600
#define W83781D_INIT_TEMP_HYST 1270	/* must be 127 for ALARM to work */
#define W83781D_INIT_TEMP2_OVER 600
#define W83781D_INIT_TEMP2_HYST 500
#define W83781D_INIT_TEMP3_OVER 600
#define W83781D_INIT_TEMP3_HYST 500

#ifdef MODULE
extern int init_module(void);
extern int cleanup_module(void);
#endif				/* MODULE */

/* There are some complications in a module like this. First off, W83781D chips
   may be both present on the SMBus and the ISA bus, and we have to handle
   those cases separately at some places. Second, there might be several
   W83781D chips available (well, actually, that is probably never done; but
   it is a clean illustration of how to handle a case like that). Finally,
   a specific chip may be attached to *both* ISA and SMBus, and we would
   not like to detect it double. Fortunately, in the case of the W83781D at
   least, a register tells us what SMBus address we are on, so that helps
   a bit - except if there could be more than one SMBus. Groan. No solution
   for this yet. */

/* This module may seem overly long and complicated. In fact, it is not so
   bad. Quite a lot of bookkeeping is done. A real driver can often cut
   some corners. */

/* For each registered W83781D, we need to keep some data in memory. That
   data is pointed to by w83627hf_list[NR]->data. The structure itself is
   dynamically allocated, at the same time when a new w83627hf client is
   allocated. */
struct w83627hf_data {
	struct semaphore lock;
	int sysctl_id;
	enum chips type;

	struct semaphore update_lock;
	char valid;		/* !=0 if following fields are valid */
	unsigned long last_updated;	/* In jiffies */

	struct i2c_client *lm75;	/* for secondary I2C addresses */
	/* pointer to array of 2 subclients */

	u8 in[9];		/* Register value - 8 & 9 for 782D only */
	u8 in_max[9];		/* Register value - 8 & 9 for 782D only */
	u8 in_min[9];		/* Register value - 8 & 9 for 782D only */
	u8 fan[3];		/* Register value */
	u8 fan_min[3];		/* Register value */
	u8 temp;
	u8 temp_over;		/* Register value */
	u8 temp_hyst;		/* Register value */
	u16 temp_add[2];	/* Register value */
	u16 temp_add_over[2];	/* Register value */
	u16 temp_add_hyst[2];	/* Register value */
	u8 fan_div[3];		/* Register encoding, shifted right */
	u8 vid;			/* Register encoding, combined */
	u32 alarms;		/* Register encoding, combined */
	u32 beeps;		/* Register encoding, combined */
	u8 beep_enable;		/* Boolean */
	u8 pwm[4];		/* Register value */
	u16 sens[3];		/* 782D/783S only.
				   1 = pentium diode; 2 = 3904 diode;
				   3000-5000 = thermistor beta.
				   Default = 3435. 
				   Other Betas unimplemented */
	u8 vrm;
};


#ifdef MODULE
static
#else
extern
#endif
int __init sensors_w83627hf_init(void);
static int __init w83627hf_cleanup(void);

static int w83627hf_attach_adapter(struct i2c_adapter *adapter);
static int w83627hf_detect(struct i2c_adapter *adapter, int address,
			  unsigned short flags, int kind);
static int w83627hf_detach_client(struct i2c_client *client);
static int w83627hf_command(struct i2c_client *client, unsigned int cmd,
			   void *arg);
static void w83627hf_inc_use(struct i2c_client *client);
static void w83627hf_dec_use(struct i2c_client *client);

static int w83627hf_read_value(struct i2c_client *client, u16 register);
static int w83627hf_write_value(struct i2c_client *client, u16 register,
			       u16 value);
static void w83627hf_update_client(struct i2c_client *client);
static void w83627hf_init_client(struct i2c_client *client);


static void w83627hf_in(struct i2c_client *client, int operation,
		       int ctl_name, int *nrels_mag, long *results);
static void w83627hf_fan(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void w83627hf_temp(struct i2c_client *client, int operation,
			 int ctl_name, int *nrels_mag, long *results);
static void w83627hf_temp_add(struct i2c_client *client, int operation,
			     int ctl_name, int *nrels_mag, long *results);
static void w83627hf_vid(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void w83627hf_vrm(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void w83627hf_alarms(struct i2c_client *client, int operation,
			   int ctl_name, int *nrels_mag, long *results);
static void w83627hf_beep(struct i2c_client *client, int operation,
			 int ctl_name, int *nrels_mag, long *results);
static void w83627hf_fan_div(struct i2c_client *client, int operation,
			    int ctl_name, int *nrels_mag, long *results);
static void w83627hf_pwm(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void w83627hf_sens(struct i2c_client *client, int operation,
			 int ctl_name, int *nrels_mag, long *results);

static int w83627hf_id = 0;

static struct i2c_driver w83627hf_driver = {
	/* name */ "W83781D sensor driver",
	/* id */ I2C_DRIVERID_W83781D,
	/* flags */ I2C_DF_NOTIFY,
	/* attach_adapter */ &w83627hf_attach_adapter,
	/* detach_client */ &w83627hf_detach_client,
	/* command */ &w83627hf_command,
	/* inc_use */ &w83627hf_inc_use,
	/* dec_use */ &w83627hf_dec_use
};

/* Used by w83627hf_init/cleanup */
static int __initdata w83627hf_initialized = 0;

/* The /proc/sys entries */
/* These files are created for each detected chip. This is just a template;
   though at first sight, you might think we could use a statically
   allocated list, we need some way to get back to the parent - which
   is done through one of the 'extra' fields which are initialized 
   when a new copy is allocated. */


/* without pwm3-4 */
static ctl_table w83782d_isa_dir_table_template[] = {
	{W83781D_SYSCTL_IN0, "in0", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_in},
	{W83781D_SYSCTL_IN1, "in1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_in},
	{W83781D_SYSCTL_IN2, "in2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_in},
	{W83781D_SYSCTL_IN3, "in3", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_in},
	{W83781D_SYSCTL_IN4, "in4", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_in},
	{W83781D_SYSCTL_IN5, "in5", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_in},
	{W83781D_SYSCTL_IN6, "in6", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_in},
	{W83781D_SYSCTL_IN7, "in7", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_in},
	{W83781D_SYSCTL_IN8, "in8", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_in},
	{W83781D_SYSCTL_FAN1, "fan1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_fan},
	{W83781D_SYSCTL_FAN2, "fan2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_fan},
	{W83781D_SYSCTL_FAN3, "fan3", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_fan},
	{W83781D_SYSCTL_TEMP1, "temp1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_temp},
	{W83781D_SYSCTL_TEMP2, "temp2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_temp_add},
	{W83781D_SYSCTL_TEMP3, "temp3", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_temp_add},
	{W83781D_SYSCTL_VID, "vid", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_vid},
	{W83781D_SYSCTL_VRM, "vrm", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_vrm},
	{W83781D_SYSCTL_FAN_DIV, "fan_div", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_fan_div},
	{W83781D_SYSCTL_ALARMS, "alarms", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_alarms},
	{W83781D_SYSCTL_BEEP, "beep", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_beep},
	{W83781D_SYSCTL_PWM1, "pwm1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_pwm},
	{W83781D_SYSCTL_PWM2, "pwm2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_pwm},
	{W83781D_SYSCTL_SENS1, "sensor1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_sens},
	{W83781D_SYSCTL_SENS2, "sensor2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_sens},
	{W83781D_SYSCTL_SENS3, "sensor3", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_sens},
	{0}
};

/* similar to w83782d but no fan3, no vid */
static ctl_table w83697hf_dir_table_template[] = {
	{W83781D_SYSCTL_IN0, "in0", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_in},
	/* no in1 to maintain compatibility with 781d and 782d. */
	{W83781D_SYSCTL_IN2, "in2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_in},
	{W83781D_SYSCTL_IN3, "in3", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_in},
	{W83781D_SYSCTL_IN4, "in4", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_in},
	{W83781D_SYSCTL_IN5, "in5", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_in},
	{W83781D_SYSCTL_IN6, "in6", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_in},
	{W83781D_SYSCTL_IN7, "in7", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_in},
	{W83781D_SYSCTL_IN8, "in8", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_in},
	{W83781D_SYSCTL_FAN1, "fan1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_fan},
	{W83781D_SYSCTL_FAN2, "fan2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_fan},
	{W83781D_SYSCTL_TEMP1, "temp1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_temp},
	{W83781D_SYSCTL_TEMP2, "temp2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_temp_add},
	{W83781D_SYSCTL_FAN_DIV, "fan_div", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_fan_div},
	{W83781D_SYSCTL_ALARMS, "alarms", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_alarms},
	{W83781D_SYSCTL_BEEP, "beep", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_beep},
	{W83781D_SYSCTL_PWM1, "pwm1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_pwm},
	{W83781D_SYSCTL_PWM2, "pwm2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_pwm},
	{W83781D_SYSCTL_SENS1, "sensor1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_sens},
	{W83781D_SYSCTL_SENS2, "sensor2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &w83627hf_sens},
	{0}
};


/* This function is called when:
     * w83627hf_driver is inserted (when this module is loaded), for each
       available adapter
     * when a new adapter is inserted (and w83627hf_driver is still present) */
int w83627hf_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_detect(adapter, &addr_data, w83627hf_detect);
}

int w83627hf_find(int *address)
{
	u16 val;

	superio_enter();
	val= superio_inb(DEVID);
	if(val != W627_DEVID && val != W697_DEVID) {
		superio_exit();
		return -ENODEV;
	}

	superio_select();
	val = (superio_inb(WINB_BASE_REG) << 8) |
	       superio_inb(WINB_BASE_REG + 1);
	*address = val & ~(WINB_EXTENT - 1);
	if (*address == 0 && force_addr == 0) {
		printk("vt1211.o: base address not set - use force_addr=0xaddr\n");
		superio_exit();
		return -ENODEV;
	}
	if (force_addr)
		*address = force_addr;	/* so detect will get called */

	superio_exit();
	return 0;
}

int w83627hf_detect(struct i2c_adapter *adapter, int address,
		   unsigned short flags, int kind)
{
	int i, val, id;
	struct i2c_client *new_client;
	struct w83627hf_data *data;
	int err = 0;
	const char *type_name = "";
	const char *client_name = "";
	enum vendor { winbond, asus } vendid;

	if (!i2c_is_isa_adapter(adapter))
		return 0;

	if(force_addr)
		address = force_addr & ~(WINB_EXTENT - 1);
	if (check_region(address, WINB_EXTENT)) {
		printk("vt1211.o: region 0x%x already in use!\n", address);
		return -ENODEV;
	}
	if(force_addr) {
		printk("vt1211.o: forcing ISA address 0x%04X\n", address);
		superio_enter();
		superio_select();
		superio_outb(WINB_BASE_REG, address >> 8);
		superio_outb(WINB_BASE_REG+1, address & 0xff);
		superio_exit();
	}

	superio_enter();
	val= superio_inb(DEVID);
	if(val == W627_DEVID)
		kind = w83627hf;
	else if(val == W697_DEVID)
		kind = w83697hf;
	superio_select();
	if((val = 0x01 & superio_inb(WINB_ACT_REG)) == 0)
		superio_outb(WINB_ACT_REG, 1);
	superio_exit();

	/* OK. For now, we presume we have a valid client. We now create the
	   client structure, even though we cannot fill it completely yet.
	   But it allows us to access w83627hf_{read,write}_value. */

	if (!(new_client = kmalloc(sizeof(struct i2c_client) +
				   sizeof(struct w83627hf_data),
				   GFP_KERNEL))) {
		err = -ENOMEM;
		goto ERROR0;
	}

	data = (struct w83627hf_data *) (new_client + 1);
	new_client->addr = address;
	init_MUTEX(&data->lock);
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &w83627hf_driver;
	new_client->flags = 0;


	if (kind == w83627hf) {
		type_name = "w83627hf";
		client_name = "W83627HF chip";
	} else if (kind == w83697hf) {
		type_name = "w83697hf";
		client_name = "W83697HF chip";
	} else {
		goto ERROR1;
	}

	request_region(address, WINB_EXTENT, type_name);

	/* Fill in the remaining client fields and put it into the global list */
	strcpy(new_client->name, client_name);
	data->type = kind;
	new_client->id = w83627hf_id++;
	data->valid = 0;
	init_MUTEX(&data->update_lock);

	/* Tell the I2C layer a new client has arrived */
	if ((err = i2c_attach_client(new_client)))
		goto ERROR3;

	data->lm75 = NULL;

	/* Register a new directory entry with module sensors */
	if ((i = i2c_register_entry(new_client,
				type_name,
				(kind == w83697hf) ?
				   w83697hf_dir_table_template :
				   w83782d_isa_dir_table_template ,
				THIS_MODULE)) < 0) {
		err = i;
		goto ERROR7;
	}
	data->sysctl_id = i;

	/* Initialize the chip */
	w83627hf_init_client(new_client);
	return 0;

/* OK, this is not exactly good programming practice, usually. But it is
   very code-efficient in this case. */

      ERROR7:
      ERROR6:
      ERROR5:
      ERROR4:
	i2c_detach_client(new_client);
      ERROR3:
	release_region(address, WINB_EXTENT);
      ERROR1:
	kfree(new_client);
      ERROR0:
	return err;
}

int w83627hf_detach_client(struct i2c_client *client)
{
	int err;

	i2c_deregister_entry(((struct w83627hf_data *) (client->data))->
				 sysctl_id);

	if ((err = i2c_detach_client(client))) {
		printk
		    (KERN_ERR "w83627hf.o: Client deregistration failed, client not detached.\n");
		return err;
	}

	release_region(client->addr, WINB_EXTENT);
	kfree(client);

	return 0;
}

/* No commands defined yet */
int w83627hf_command(struct i2c_client *client, unsigned int cmd, void *arg)
{
	return 0;
}

void w83627hf_inc_use(struct i2c_client *client)
{
#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif
}

void w83627hf_dec_use(struct i2c_client *client)
{
#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
}

/*
   ISA access must always be locked explicitly! 
   We ignore the W83781D BUSY flag at this moment - it could lead to deadlocks,
   would slow down the W83781D access and should not be necessary. 
   There are some ugly typecasts here, but the good news is - they should
   nowhere else be necessary! */
int w83627hf_read_value(struct i2c_client *client, u16 reg)
{
	int res, word_sized, bank;
	struct i2c_client *cl;

	down(&(((struct w83627hf_data *) (client->data))->lock));
	word_sized = (((reg & 0xff00) == 0x100)
		      || ((reg & 0xff00) == 0x200))
	    && (((reg & 0x00ff) == 0x50)
		|| ((reg & 0x00ff) == 0x53)
		|| ((reg & 0x00ff) == 0x55));
	if (reg & 0xff00) {
		outb_p(W83781D_REG_BANK,
		       client->addr + W83781D_ADDR_REG_OFFSET);
		outb_p(reg >> 8,
		       client->addr + W83781D_DATA_REG_OFFSET);
	}
	outb_p(reg & 0xff, client->addr + W83781D_ADDR_REG_OFFSET);
	res = inb_p(client->addr + W83781D_DATA_REG_OFFSET);
	if (word_sized) {
		outb_p((reg & 0xff) + 1,
		       client->addr + W83781D_ADDR_REG_OFFSET);
		res =
		    (res << 8) + inb_p(client->addr +
				       W83781D_DATA_REG_OFFSET);
	}
	if (reg & 0xff00) {
		outb_p(W83781D_REG_BANK,
		       client->addr + W83781D_ADDR_REG_OFFSET);
		outb_p(0, client->addr + W83781D_DATA_REG_OFFSET);
	}
	up(&(((struct w83627hf_data *) (client->data))->lock));
	return res;
}

int w83627hf_write_value(struct i2c_client *client, u16 reg, u16 value)
{
	int word_sized, bank;
	struct i2c_client *cl;

	down(&(((struct w83627hf_data *) (client->data))->lock));
	word_sized = (((reg & 0xff00) == 0x100)
		      || ((reg & 0xff00) == 0x200))
	    && (((reg & 0x00ff) == 0x53)
		|| ((reg & 0x00ff) == 0x55));
	if (reg & 0xff00) {
		outb_p(W83781D_REG_BANK,
		       client->addr + W83781D_ADDR_REG_OFFSET);
		outb_p(reg >> 8,
		       client->addr + W83781D_DATA_REG_OFFSET);
	}
	outb_p(reg & 0xff, client->addr + W83781D_ADDR_REG_OFFSET);
	if (word_sized) {
		outb_p(value >> 8,
		       client->addr + W83781D_DATA_REG_OFFSET);
		outb_p((reg & 0xff) + 1,
		       client->addr + W83781D_ADDR_REG_OFFSET);
	}
	outb_p(value & 0xff,
	       client->addr + W83781D_DATA_REG_OFFSET);
	if (reg & 0xff00) {
		outb_p(W83781D_REG_BANK,
		       client->addr + W83781D_ADDR_REG_OFFSET);
		outb_p(0, client->addr + W83781D_DATA_REG_OFFSET);
	}
	up(&(((struct w83627hf_data *) (client->data))->lock));
	return 0;
}

/* Called when we have found a new W83781D. It should set limits, etc. */
void w83627hf_init_client(struct i2c_client *client)
{
	struct w83627hf_data *data = client->data;
	int vid = 0, i;
	int type = data->type;
	u8 tmp;

	if(init) {
		/* save this register */
		i = w83627hf_read_value(client, W83781D_REG_BEEP_CONFIG);
		/* Reset all except Watchdog values and last conversion values
		   This sets fan-divs to 2, among others */
		w83627hf_write_value(client, W83781D_REG_CONFIG, 0x80);
		/* Restore the register and disable power-on abnormal beep.
		   This saves FAN 1/2/3 input/output values set by BIOS. */
		w83627hf_write_value(client, W83781D_REG_BEEP_CONFIG, i | 0x80);
		/* Disable master beep-enable (reset turns it on).
		   Individual beeps should be reset to off but for some reason
		   disabling this bit helps some people not get beeped */
		w83627hf_write_value(client, W83781D_REG_BEEP_INTS2, 0);
	}

	/* Minimize conflicts with other winbond i2c-only clients...  */
	/* disable i2c subclients... how to disable main i2c client?? */
	/* force i2c address to relatively uncommon address */
	w83627hf_write_value(client, W83781D_REG_I2C_SUBADDR, 0x89);
	w83627hf_write_value(client, W83781D_REG_I2C_ADDR, force_i2c);

	if (type != w83697hf) {
		vid = w83627hf_read_value(client, W83781D_REG_VID_FANDIV) & 0x0f;
		vid |=
		    (w83627hf_read_value(client, W83781D_REG_CHIPID) & 0x01) << 4;
		data->vrm = DEFAULT_VRM;
		vid = vid_from_reg(vid, data->vrm);
	}

	tmp = w83627hf_read_value(client, W83781D_REG_SCFG1);
	for (i = 1; i <= 3; i++) {
		if (!(tmp & BIT_SCFG1[i - 1])) {
			data->sens[i - 1] = W83781D_DEFAULT_BETA;
		} else {
			if (w83627hf_read_value
			    (client,
			     W83781D_REG_SCFG2) & BIT_SCFG2[i - 1])
				data->sens[i - 1] = 1;
			else
				data->sens[i - 1] = 2;
		}
		if ((type == w83697hf) && (i == 2))
			break;
	}

	if(init) {
		w83627hf_write_value(client, W83781D_REG_IN_MIN(0),
				    IN_TO_REG(W83781D_INIT_IN_MIN_0));
		w83627hf_write_value(client, W83781D_REG_IN_MAX(0),
				    IN_TO_REG(W83781D_INIT_IN_MAX_0));
		if (type != w83697hf) {
			w83627hf_write_value(client, W83781D_REG_IN_MIN(1),
					    IN_TO_REG(W83781D_INIT_IN_MIN_1));
			w83627hf_write_value(client, W83781D_REG_IN_MAX(1),
					    IN_TO_REG(W83781D_INIT_IN_MAX_1));
		}

		w83627hf_write_value(client, W83781D_REG_IN_MIN(2),
				    IN_TO_REG(W83781D_INIT_IN_MIN_2));
		w83627hf_write_value(client, W83781D_REG_IN_MAX(2),
				    IN_TO_REG(W83781D_INIT_IN_MAX_2));
		w83627hf_write_value(client, W83781D_REG_IN_MIN(3),
				    IN_TO_REG(W83781D_INIT_IN_MIN_3));
		w83627hf_write_value(client, W83781D_REG_IN_MAX(3),
				    IN_TO_REG(W83781D_INIT_IN_MAX_3));
		w83627hf_write_value(client, W83781D_REG_IN_MIN(4),
				    IN_TO_REG(W83781D_INIT_IN_MIN_4));
		w83627hf_write_value(client, W83781D_REG_IN_MAX(4),
				    IN_TO_REG(W83781D_INIT_IN_MAX_4));
		w83627hf_write_value(client, W83781D_REG_IN_MIN(5),
				    IN_TO_REG(W83782D_INIT_IN_MIN_5));
		w83627hf_write_value(client, W83781D_REG_IN_MAX(5),
				    IN_TO_REG(W83782D_INIT_IN_MAX_5));
		w83627hf_write_value(client, W83781D_REG_IN_MIN(6),
				    IN_TO_REG(W83782D_INIT_IN_MIN_6));
		w83627hf_write_value(client, W83781D_REG_IN_MAX(6),
				    IN_TO_REG(W83782D_INIT_IN_MAX_6));
		w83627hf_write_value(client, W83781D_REG_IN_MIN(7),
				    IN_TO_REG(W83781D_INIT_IN_MIN_7));
		w83627hf_write_value(client, W83781D_REG_IN_MAX(7),
				    IN_TO_REG(W83781D_INIT_IN_MAX_7));
		w83627hf_write_value(client, W83781D_REG_IN_MIN(8),
				    IN_TO_REG(W83781D_INIT_IN_MIN_8));
		w83627hf_write_value(client, W83781D_REG_IN_MAX(8),
				    IN_TO_REG(W83781D_INIT_IN_MAX_8));
		w83627hf_write_value(client, W83781D_REG_VBAT,
		    (w83627hf_read_value(client, W83781D_REG_VBAT) | 0x01));
		w83627hf_write_value(client, W83781D_REG_FAN_MIN(1),
				    FAN_TO_REG(W83781D_INIT_FAN_MIN_1, 2));
		w83627hf_write_value(client, W83781D_REG_FAN_MIN(2),
				    FAN_TO_REG(W83781D_INIT_FAN_MIN_2, 2));
		if (type != w83697hf) {
			w83627hf_write_value(client, W83781D_REG_FAN_MIN(3),
				    FAN_TO_REG(W83781D_INIT_FAN_MIN_3, 2));
		}

		w83627hf_write_value(client, W83781D_REG_TEMP_OVER,
				    TEMP_TO_REG(W83781D_INIT_TEMP_OVER));
		w83627hf_write_value(client, W83781D_REG_TEMP_HYST,
				    TEMP_TO_REG(W83781D_INIT_TEMP_HYST));

		w83627hf_write_value(client, W83781D_REG_TEMP2_OVER,
				    TEMP_ADD_TO_REG
				    (W83781D_INIT_TEMP2_OVER));
		w83627hf_write_value(client, W83781D_REG_TEMP2_HYST,
				    TEMP_ADD_TO_REG
				    (W83781D_INIT_TEMP2_HYST));
		w83627hf_write_value(client, W83781D_REG_TEMP2_CONFIG, 0x00);

		if (type != w83697hf) {
			w83627hf_write_value(client, W83781D_REG_TEMP3_OVER,
					    TEMP_ADD_TO_REG
					    (W83781D_INIT_TEMP3_OVER));
			w83627hf_write_value(client, W83781D_REG_TEMP3_HYST,
					    TEMP_ADD_TO_REG
					    (W83781D_INIT_TEMP3_HYST));
		}
		if (type != w83697hf) {
			w83627hf_write_value(client, W83781D_REG_TEMP3_CONFIG,
					    0x00);
		}
		/* enable PWM2 control (can't hurt since PWM reg
	           should have been reset to 0xff) */
		w83627hf_write_value(client, W83781D_REG_PWMCLK12, 0x19);
		/* enable comparator mode for temp2 and temp3 so
	           alarm indication will work correctly */
		w83627hf_write_value(client, W83781D_REG_IRQ, 0x41);
	}

	/* Start monitoring */
	w83627hf_write_value(client, W83781D_REG_CONFIG,
			    (w83627hf_read_value(client,
						W83781D_REG_CONFIG) & 0xf7)
			    | 0x01);
}

void w83627hf_update_client(struct i2c_client *client)
{
	struct w83627hf_data *data = client->data;
	int i;

	down(&data->update_lock);

	if ((jiffies - data->last_updated > HZ + HZ / 2) ||
	    (jiffies < data->last_updated) || !data->valid) {
		for (i = 0; i <= 8; i++) {
			if ((data->type == w83697hf)
			    && (i == 1))
				continue;	/* 783S has no in1 */
			data->in[i] =
			    w83627hf_read_value(client, W83781D_REG_IN(i));
			data->in_min[i] =
			    w83627hf_read_value(client,
					       W83781D_REG_IN_MIN(i));
			data->in_max[i] =
			    w83627hf_read_value(client,
					       W83781D_REG_IN_MAX(i));
			if ((data->type != w83697hf)
			    && (data->type != w83627hf) && (i == 6))
				break;
		}
		for (i = 1; i <= 3; i++) {
			data->fan[i - 1] =
			    w83627hf_read_value(client, W83781D_REG_FAN(i));
			data->fan_min[i - 1] =
			    w83627hf_read_value(client,
					       W83781D_REG_FAN_MIN(i));
		}
		for (i = 1; i <= 2; i++) {
			data->pwm[i - 1] =
			    w83627hf_read_value(client,
					       W83781D_REG_PWM(i));
		}

		data->temp = w83627hf_read_value(client, W83781D_REG_TEMP);
		data->temp_over =
		    w83627hf_read_value(client, W83781D_REG_TEMP_OVER);
		data->temp_hyst =
		    w83627hf_read_value(client, W83781D_REG_TEMP_HYST);
		data->temp_add[0] =
		    w83627hf_read_value(client, W83781D_REG_TEMP2);
		data->temp_add_over[0] =
		    w83627hf_read_value(client, W83781D_REG_TEMP2_OVER);
		data->temp_add_hyst[0] =
		    w83627hf_read_value(client, W83781D_REG_TEMP2_HYST);
		if (data->type != w83697hf) {
			data->temp_add[1] =
			    w83627hf_read_value(client, W83781D_REG_TEMP3);
			data->temp_add_over[1] =
			    w83627hf_read_value(client, W83781D_REG_TEMP3_OVER);
			data->temp_add_hyst[1] =
			    w83627hf_read_value(client, W83781D_REG_TEMP3_HYST);
		}
		i = w83627hf_read_value(client, W83781D_REG_VID_FANDIV);
		if (data->type != w83697hf) {
			data->vid = i & 0x0f;
			data->vid |=
			    (w83627hf_read_value(client, W83781D_REG_CHIPID) & 0x01)
			    << 4;
		}
		data->fan_div[0] = (i >> 4) & 0x03;
		data->fan_div[1] = (i >> 6) & 0x03;
		if (data->type != w83697hf) {
			data->fan_div[2] = (w83627hf_read_value(client,
					       W83781D_REG_PIN) >> 6) & 0x03;
		}
		i = w83627hf_read_value(client, W83781D_REG_VBAT);
		data->fan_div[0] |= (i >> 3) & 0x04;
		data->fan_div[1] |= (i >> 4) & 0x04;
		if (data->type != w83697hf)
			data->fan_div[2] |= (i >> 5) & 0x04;
		data->alarms =
		    w83627hf_read_value(client,
				       W83781D_REG_ALARM1) +
		    (w83627hf_read_value(client, W83781D_REG_ALARM2) << 8);
		if (data->type == w83627hf) {
			data->alarms |=
			    w83627hf_read_value(client,
					       W83781D_REG_ALARM3) << 16;
		}
		i = w83627hf_read_value(client, W83781D_REG_BEEP_INTS2);
		data->beep_enable = i >> 7;
		data->beeps = ((i & 0x7f) << 8) +
		    w83627hf_read_value(client, W83781D_REG_BEEP_INTS1);
			data->beeps |=
			    w83627hf_read_value(client,
					       W83781D_REG_BEEP_INTS3) << 16;
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
void w83627hf_in(struct i2c_client *client, int operation, int ctl_name,
		int *nrels_mag, long *results)
{
	struct w83627hf_data *data = client->data;
	int nr = ctl_name - W83781D_SYSCTL_IN0;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 2;
	else if (operation == SENSORS_PROC_REAL_READ) {
		w83627hf_update_client(client);
		results[0] = IN_FROM_REG(data->in_min[nr]);
		results[1] = IN_FROM_REG(data->in_max[nr]);
		results[2] = IN_FROM_REG(data->in[nr]);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->in_min[nr] = IN_TO_REG(results[0]);
			w83627hf_write_value(client, W83781D_REG_IN_MIN(nr),
					    data->in_min[nr]);
		}
		if (*nrels_mag >= 2) {
			data->in_max[nr] = IN_TO_REG(results[1]);
			w83627hf_write_value(client, W83781D_REG_IN_MAX(nr),
					    data->in_max[nr]);
		}
	}
}

void w83627hf_fan(struct i2c_client *client, int operation, int ctl_name,
		 int *nrels_mag, long *results)
{
	struct w83627hf_data *data = client->data;
	int nr = ctl_name - W83781D_SYSCTL_FAN1 + 1;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		w83627hf_update_client(client);
		results[0] = FAN_FROM_REG(data->fan_min[nr - 1],
				  DIV_FROM_REG(data->fan_div[nr - 1]));
		results[1] = FAN_FROM_REG(data->fan[nr - 1],
			          DIV_FROM_REG(data->fan_div[nr - 1]));
		*nrels_mag = 2;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->fan_min[nr - 1] =
			     FAN_TO_REG(results[0],
			            DIV_FROM_REG(data->fan_div[nr-1]));
			w83627hf_write_value(client,
					    W83781D_REG_FAN_MIN(nr),
					    data->fan_min[nr - 1]);
		}
	}
}

void w83627hf_temp(struct i2c_client *client, int operation, int ctl_name,
		  int *nrels_mag, long *results)
{
	struct w83627hf_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 1;
	else if (operation == SENSORS_PROC_REAL_READ) {
		w83627hf_update_client(client);
		results[0] = TEMP_FROM_REG(data->temp_over);
		results[1] = TEMP_FROM_REG(data->temp_hyst);
		results[2] = TEMP_FROM_REG(data->temp);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->temp_over = TEMP_TO_REG(results[0]);
			w83627hf_write_value(client, W83781D_REG_TEMP_OVER,
					    data->temp_over);
		}
		if (*nrels_mag >= 2) {
			data->temp_hyst = TEMP_TO_REG(results[1]);
			w83627hf_write_value(client, W83781D_REG_TEMP_HYST,
					    data->temp_hyst);
		}
	}
}

void w83627hf_temp_add(struct i2c_client *client, int operation,
		      int ctl_name, int *nrels_mag, long *results)
{
	struct w83627hf_data *data = client->data;
	int nr = ctl_name - W83781D_SYSCTL_TEMP2;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 1;
	else if (operation == SENSORS_PROC_REAL_READ) {
		w83627hf_update_client(client);
			results[0] =
			    TEMP_ADD_FROM_REG(data->temp_add_over[nr]);
			results[1] =
			    TEMP_ADD_FROM_REG(data->temp_add_hyst[nr]);
			results[2] = TEMP_ADD_FROM_REG(data->temp_add[nr]);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
				data->temp_add_over[nr] =
				    TEMP_ADD_TO_REG(results[0]);
			w83627hf_write_value(client,
					    nr ? W83781D_REG_TEMP3_OVER :
					    W83781D_REG_TEMP2_OVER,
					    data->temp_add_over[nr]);
		}
		if (*nrels_mag >= 2) {
				data->temp_add_hyst[nr] =
				    TEMP_ADD_TO_REG(results[1]);
			w83627hf_write_value(client,
					    nr ? W83781D_REG_TEMP3_HYST :
					    W83781D_REG_TEMP2_HYST,
					    data->temp_add_hyst[nr]);
		}
	}
}


void w83627hf_vid(struct i2c_client *client, int operation, int ctl_name,
		 int *nrels_mag, long *results)
{
	struct w83627hf_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 3;
	else if (operation == SENSORS_PROC_REAL_READ) {
		w83627hf_update_client(client);
		results[0] = vid_from_reg(data->vid, data->vrm);
		*nrels_mag = 1;
	}
}

void w83627hf_vrm(struct i2c_client *client, int operation, int ctl_name,
		 int *nrels_mag, long *results)
{
	struct w83627hf_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 1;
	else if (operation == SENSORS_PROC_REAL_READ) {
		results[0] = data->vrm;
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1)
			data->vrm = results[0];
	}
}

void w83627hf_alarms(struct i2c_client *client, int operation, int ctl_name,
		    int *nrels_mag, long *results)
{
	struct w83627hf_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		w83627hf_update_client(client);
		results[0] = data->alarms;
		*nrels_mag = 1;
	}
}

void w83627hf_beep(struct i2c_client *client, int operation, int ctl_name,
		  int *nrels_mag, long *results)
{
	struct w83627hf_data *data = client->data;
	int val;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		w83627hf_update_client(client);
		results[0] = BEEP_ENABLE_FROM_REG(data->beep_enable);
		results[1] = data->beeps;
		*nrels_mag = 2;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 2) {
			data->beeps = BEEPS_TO_REG(results[1]);
			w83627hf_write_value(client, W83781D_REG_BEEP_INTS1,
					    data->beeps & 0xff);
				w83627hf_write_value(client,
						    W83781D_REG_BEEP_INTS3,
						    ((data-> beeps) >> 16) &
						      0xff);
			val = (data->beeps >> 8) & 0x7f;
		} else if (*nrels_mag >= 1)
			val =
			    w83627hf_read_value(client,
					       W83781D_REG_BEEP_INTS2) &
			    0x7f;
		if (*nrels_mag >= 1) {
			data->beep_enable = BEEP_ENABLE_TO_REG(results[0]);
			w83627hf_write_value(client, W83781D_REG_BEEP_INTS2,
					    val | data->beep_enable << 7);
		}
	}
}

/* w83697hf only has two fans */
void w83627hf_fan_div(struct i2c_client *client, int operation,
		     int ctl_name, int *nrels_mag, long *results)
{
	struct w83627hf_data *data = client->data;
	int old, old2, old3 = 0;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		w83627hf_update_client(client);
		results[0] = DIV_FROM_REG(data->fan_div[0]);
		results[1] = DIV_FROM_REG(data->fan_div[1]);
		if (data->type == w83697hf) {
			*nrels_mag = 2;
		} else {
			results[2] = DIV_FROM_REG(data->fan_div[2]);
			*nrels_mag = 3;
		}
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		old = w83627hf_read_value(client, W83781D_REG_VID_FANDIV);
		/* w83627hf and as99127f don't have extended divisor bits */
			old3 =
			    w83627hf_read_value(client, W83781D_REG_VBAT);
		if (*nrels_mag >= 3 && data->type != w83697hf) {
			data->fan_div[2] =
			    DIV_TO_REG(results[2]);
			old2 = w83627hf_read_value(client, W83781D_REG_PIN);
			old2 =
			    (old2 & 0x3f) | ((data->fan_div[2] & 0x03) << 6);
			w83627hf_write_value(client, W83781D_REG_PIN, old2);
				old3 =
				    (old3 & 0x7f) |
				    ((data->fan_div[2] & 0x04) << 5);
		}
		if (*nrels_mag >= 2) {
			data->fan_div[1] =
			    DIV_TO_REG(results[1]);
			old =
			    (old & 0x3f) | ((data->fan_div[1] & 0x03) << 6);
				old3 =
				    (old3 & 0xbf) |
				    ((data->fan_div[1] & 0x04) << 4);
		}
		if (*nrels_mag >= 1) {
			data->fan_div[0] =
			    DIV_TO_REG(results[0]);
			old =
			    (old & 0xcf) | ((data->fan_div[0] & 0x03) << 4);
			w83627hf_write_value(client, W83781D_REG_VID_FANDIV,
					    old);
				old3 =
				    (old3 & 0xdf) |
				    ((data->fan_div[0] & 0x04) << 3);
				w83627hf_write_value(client,
						    W83781D_REG_VBAT,
						    old3);
		}
	}
}

void w83627hf_pwm(struct i2c_client *client, int operation, int ctl_name,
		 int *nrels_mag, long *results)
{
	struct w83627hf_data *data = client->data;
	int nr = 1 + ctl_name - W83781D_SYSCTL_PWM1;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		w83627hf_update_client(client);
		results[0] = data->pwm[nr - 1];
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->pwm[nr - 1] = PWM_TO_REG(results[0]);
			w83627hf_write_value(client, W83781D_REG_PWM(nr),
					    data->pwm[nr - 1]);
		}
	}
}

void w83627hf_sens(struct i2c_client *client, int operation, int ctl_name,
		  int *nrels_mag, long *results)
{
	struct w83627hf_data *data = client->data;
	int nr = 1 + ctl_name - W83781D_SYSCTL_SENS1;
	u8 tmp;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		results[0] = data->sens[nr - 1];
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			switch (results[0]) {
			case 1:	/* PII/Celeron diode */
				tmp = w83627hf_read_value(client,
						       W83781D_REG_SCFG1);
				w83627hf_write_value(client,
						    W83781D_REG_SCFG1,
						    tmp | BIT_SCFG1[nr -
								    1]);
				tmp = w83627hf_read_value(client,
						       W83781D_REG_SCFG2);
				w83627hf_write_value(client,
						    W83781D_REG_SCFG2,
						    tmp | BIT_SCFG2[nr -
								    1]);
				data->sens[nr - 1] = results[0];
				break;
			case 2:	/* 3904 */
				tmp = w83627hf_read_value(client,
						       W83781D_REG_SCFG1);
				w83627hf_write_value(client,
						    W83781D_REG_SCFG1,
						    tmp | BIT_SCFG1[nr -
								    1]);
				tmp = w83627hf_read_value(client,
						       W83781D_REG_SCFG2);
				w83627hf_write_value(client,
						    W83781D_REG_SCFG2,
						    tmp & ~BIT_SCFG2[nr -
								     1]);
				data->sens[nr - 1] = results[0];
				break;
			case W83781D_DEFAULT_BETA:	/* thermistor */
				tmp = w83627hf_read_value(client,
						       W83781D_REG_SCFG1);
				w83627hf_write_value(client,
						    W83781D_REG_SCFG1,
						    tmp & ~BIT_SCFG1[nr -
								     1]);
				data->sens[nr - 1] = results[0];
				break;
			default:
				printk
				    (KERN_ERR "w83627hf.o: Invalid sensor type %ld; must be 1, 2, or %d\n",
				     results[0], W83781D_DEFAULT_BETA);
				break;
			}
		}
	}
}

int __init sensors_w83627hf_init(void)
{
	int res, addr;

	printk(KERN_INFO "w83627hf.o version %s (%s)\n", LM_VERSION, LM_DATE);
	if (w83627hf_find(&addr)) {
		printk("w83627hf.o: W83627/697 not detected, module not inserted.\n");
		return -ENODEV;
	}
	normal_isa[0] = addr;

	w83627hf_initialized = 0;

	if ((res = i2c_add_driver(&w83627hf_driver))) {
		printk
		    (KERN_ERR "w83627hf.o: Driver registration failed, module not inserted.\n");
		w83627hf_cleanup();
		return res;
	}
	w83627hf_initialized++;
	return 0;
}

int __init w83627hf_cleanup(void)
{
	int res;

	if (w83627hf_initialized >= 1) {
		if ((res = i2c_del_driver(&w83627hf_driver))) {
			return res;
		}
		w83627hf_initialized--;
	}
	return 0;
}

EXPORT_NO_SYMBOLS;

#ifdef MODULE

MODULE_AUTHOR("Frodo Looijaard <frodol@dds.nl>, "
	      "Philip Edelbrock <phil@netroedge.com>, "
	      "and Mark Studebaker <mdsxyz123@yahoo.com>");
MODULE_DESCRIPTION("W83627HF driver");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif


int init_module(void)
{
	return sensors_w83627hf_init();
}

int cleanup_module(void)
{
	return w83627hf_cleanup();
}

#endif				/* MODULE */
