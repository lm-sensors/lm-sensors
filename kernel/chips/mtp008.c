/*
   mtp008.c - Part of lm_sensors, Linux kernel modules for hardware
   monitoring
   Copyright (c) 2001  Kris Van Hees <aedil@alchar.org>

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
#include <linux/init.h>

#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,2,18)) || \
    (LINUX_VERSION_CODE == KERNEL_VERSION(2,3,0))
#define init_MUTEX(s) do { *(s) = MUTEX; } while(0)
#endif

#ifndef THIS_MODULE
#define THIS_MODULE NULL
#endif

/* Addresses to scan */
static unsigned short normal_i2c[] = {SENSORS_I2C_END};
static unsigned short normal_i2c_range[] = {0x2c, 0x2e, SENSORS_I2C_END};
static unsigned int normal_isa[] = {SENSORS_ISA_END};
static unsigned int normal_isa_range[] = {SENSORS_ISA_END};

/* Insmod parameters */
SENSORS_INSMOD_1(mtp008);

/* The MTP008 registers */
/*      in0 .. in6 */
#define MTP008_REG_IN(nr)		(0x20 + (nr))
#define MTP008_REG_IN_MAX(nr)		(0x2b + (nr) * 2)
#define MTP008_REG_IN_MIN(nr)		(0x2c + (nr) * 2)

/*      temp1 */
#define MTP008_REG_TEMP			0x27
#define MTP008_REG_TEMP_MAX		0x39
#define MTP008_REG_TEMP_MIN		0x3a

/*      fan1 .. fan3 */
#define MTP008_REG_FAN(nr)		(0x27 + (nr))
#define MTP008_REG_FAN_MIN(nr)		(0x3a + (nr))

#define MTP008_REG_CONFIG		0x40
#define MTP008_REG_INT_STAT1		0x41
#define MTP008_REG_INT_STAT2		0x42

#define MTP008_REG_SMI_MASK1		0x43
#define MTP008_REG_SMI_MASK2		0x44

#define MTP008_REG_NMI_MASK1		0x45
#define MTP008_REG_NMI_MASK2		0x46

#define MTP008_REG_VID_FANDIV		0x47

#define MTP008_REG_I2C_ADDR		0x48

#define MTP008_REG_RESET_VID4		0x49

#define MTP008_REG_OVT_PROP		0x50

#define MTP008_REG_BEEP_CTRL1		0x51
#define MTP008_REG_BEEP_CTRL2		0x52

/*      pwm1 .. pwm3  nr range 1-3 */
#define MTP008_REG_PWM_CTRL(nr)		(0x52 + (nr))

#define MTP008_REG_PIN_CTRL1		0x56
#define MTP008_REG_PIN_CTRL2		0x57

#define MTP008_REG_CHIPID		0x58

/*
 * Pin control register configuration constants.
 */
#define MTP008_CFG_VT1_PII		0x08
#define MTP008_CFG_VT2_AIN		0x00
#define MTP008_CFG_VT2_VT		0x03
#define MTP008_CFG_VT2_PII		0x04
#define MTP008_CFG_VT2_MASK		0x06
#define MTP008_CFG_VT3_VT		0x01

/* sensor pin types */
#define VOLTAGE		1
#define THERMISTOR	2
#define PIIDIODE	3

/*
 * Conversion routines and macros.  Rounding and limit checking is only done on
 * the TO_REG variants.
 *
 * Note that IN values are expressed as 100 times the actual voltage to avoid
 * having to use floating point values.  As such, IN values are between 0 and
 * 409 (0V to 4.096V).
 */
#define IN_TO_REG(val)		(SENSORS_LIMIT((((val) * 10 + 8) / 16), 0, 255))
#define IN_FROM_REG(val)	(((val) * 16) / 10)

/*
 * The fan cotation count (as stored in the register) is calculated using the
 * following formula:
 *      count = (22.5K * 60) / (rpm * div) = 1350000 / (rpm * div)
 * and the rpm is therefore:
 *      rpm = 1350000 / (count * div)
 */
extern inline u8 FAN_TO_REG(long rpm, int div)
{
	if (rpm == 0)
		return 255;

	rpm = SENSORS_LIMIT(rpm, 1, 1000000);

	return SENSORS_LIMIT(
		 (1350000 + rpm * div / 2) / (rpm * div),
		 1, 254
	       );
}

#define FAN_FROM_REG(val, div)	((val) == 0 ? -1			      \
					    : (val) == 255 ? 0		      \
							   : 1350000 /	      \
							     ((val) * (div))  \
				)

/*
 * Temperatures are stored as two's complement values of the Celsius value.  It
 * actually uses 10 times the Celsius value to avoid using floating point
 * values.
 */
#define TEMP_TO_REG(val)	(					      \
				 (val) < 0				      \
				    ? SENSORS_LIMIT(((val) - 5) / 10, 0, 255) \
				    : SENSORS_LIMIT(((val) + 5) / 10, 0, 255) \
				)
#define TEMP_FROM_REG(val)	(					      \
				 (					      \
				  (val) > 0x80 ? (val) - 0x100		      \
					       : (val)			      \
				 ) * 10					      \
				)

/*
 * VCORE voltage:
 *      0x00 to 0x0f    = 2.05 to 1.30 (0.05 per unit)
 *      0x10 to 0x1e    = 3.50 to 2.10 (0.10 per unit)
 *      0x1f            = No CPU
 */
#define VID_FROM_REG(val)	((val) == 0x1f				      \
					 ? 0				      \
					 : (val) < 0x10 ? 205 - (val) * 5     \
							: 510 - (val) * 10)

/*
 * Fan divider.
 */
#define DIV_FROM_REG(val)	(1 << (val))
#define DIV_TO_REG(val)		((val) == 8 ? 3				      \
					    : (val) == 4 ? 2		      \
							 : (val) == 2 ? 1     \
								      : 0)

/*
 * Alarms (interrupt status).
 */
#define ALARMS_FROM_REG(val)	(val)

/*
 * Beep controls.
 */
#define BEEPS_FROM_REG(val)	(val)
#define BEEPS_TO_REG(val)	(val)

/*
 * PWM control. nr range 1 to 3
 */
#define PWM_FROM_REG(val)	(val)
#define PWM_TO_REG(val)		(val)
#define PWMENABLE_FROM_REG(nr, val)	(((val) >> ((nr) + 3)) & 1)

/* Initial limits */
#define MTP008_INIT_IN_0	(vid)				/* VCore 1 */
#define MTP008_INIT_IN_1	330				/* +3.3V */
#define MTP008_INIT_IN_2	(1200 * 10 / 38)		/* +12V */
#define MTP008_INIT_IN_3	(vid)				/* VCore 2 */
#define MTP008_INIT_IN_5	((11861 + 7 * (-1200)) / 36)	/* -12V */
#define MTP008_INIT_IN_6	150				/* Vtt */

#define MTP008_INIT_IN_PCT	10

#define MTP008_INIT_IN_MIN_0	(MTP008_INIT_IN_0 -			      \
				 MTP008_INIT_IN_0 * MTP008_INIT_IN_PCT / 100)
#define MTP008_INIT_IN_MAX_0	(MTP008_INIT_IN_0 +			      \
				 MTP008_INIT_IN_0 * MTP008_INIT_IN_PCT / 100)
#define MTP008_INIT_IN_MIN_1	(MTP008_INIT_IN_1 -			      \
				 MTP008_INIT_IN_1 * MTP008_INIT_IN_PCT / 100)
#define MTP008_INIT_IN_MAX_1	(MTP008_INIT_IN_1 +			      \
				 MTP008_INIT_IN_1 * MTP008_INIT_IN_PCT / 100)
#define MTP008_INIT_IN_MIN_2	(MTP008_INIT_IN_2 -			      \
				 MTP008_INIT_IN_2 * MTP008_INIT_IN_PCT / 100)
#define MTP008_INIT_IN_MAX_2	(MTP008_INIT_IN_2 +			      \
				 MTP008_INIT_IN_2 * MTP008_INIT_IN_PCT / 100)
#define MTP008_INIT_IN_MIN_3	(MTP008_INIT_IN_3 -			      \
				 MTP008_INIT_IN_3 * MTP008_INIT_IN_PCT / 100)
#define MTP008_INIT_IN_MAX_3	(MTP008_INIT_IN_3 +			      \
				 MTP008_INIT_IN_3 * MTP008_INIT_IN_PCT / 100)

#define MTP008_INIT_IN_MIN_5	(MTP008_INIT_IN_5 -			      \
				 MTP008_INIT_IN_5 * MTP008_INIT_IN_PCT / 100)
#define MTP008_INIT_IN_MAX_5	(MTP008_INIT_IN_5 +			      \
				 MTP008_INIT_IN_5 * MTP008_INIT_IN_PCT / 100)
#define MTP008_INIT_IN_MIN_6	(MTP008_INIT_IN_6 -			      \
				 MTP008_INIT_IN_6 * MTP008_INIT_IN_PCT / 100)
#define MTP008_INIT_IN_MAX_6	(MTP008_INIT_IN_6 +			      \
				 MTP008_INIT_IN_6 * MTP008_INIT_IN_PCT / 100)

#define MTP008_INIT_FAN_MIN_1	3000
#define MTP008_INIT_FAN_MIN_2	3000
#define MTP008_INIT_FAN_MIN_3	3000

#define MTP008_INIT_TEMP_OVER	700			/* 70 Celsius */
#define MTP008_INIT_TEMP_HYST	500			/* 50 Celsius */
#define MTP008_INIT_TEMP2_OVER	700			/* 70 Celsius */
#define MTP008_INIT_TEMP2_HYST	500			/* 50 Celsius */

#ifdef MODULE
extern int init_module(void);
extern int cleanup_module(void);
#endif				/* MODULE */

/*
 * For each registered MTP008, we need to keep some data in memory.  The
 * structure itself is dynamically allocated, at the same time when a new
 * mtp008 client is allocated.
 */
struct mtp008_data {
	int sysctl_id;
	enum chips type;

	struct semaphore update_lock;
	char valid;				/* !=0 if fields are valid */
	unsigned long last_updated;		/* In jiffies */

	u8 in[7];				/* Register value */
	u8 in_max[7];				/* Register value */
	u8 in_min[7];				/* Register value */
	u8 temp;				/* Register value */
	u8 temp_max;				/* Register value */
	u8 temp_min;				/* Register value */
	u8 fan[3];				/* Register value */
	u8 fan_min[3];				/* Register value */
	u8 vid;					/* Register encoding */
	u8 fan_div[3];				/* Register encoding */
	u16 alarms;				/* Register encoding */
	u16 beeps;				/* Register encoding */
	u8 pwm[4];				/* Register value */
	u8 sens[3];				/* 1 = Analog input,
						   2 = Thermistor,
						   3 = PII/Celeron diode */
	u8 pwmenable;				/* Register 0x57 value */
};

#ifdef MODULE
static int __init sensors_mtp008_init(void);
#else
extern int __init sensors_mtp008_init(void);
#endif
static int __init mtp008_cleanup(void);

static int mtp008_attach_adapter(struct i2c_adapter *adapter);
static int mtp008_detect(struct i2c_adapter *adapter, int address,
			 unsigned short flags, int kind);
static int mtp008_detach_client(struct i2c_client *client);
static int mtp008_command(struct i2c_client *client, unsigned int cmd,
			  void *arg);
static void mtp008_inc_use(struct i2c_client *client);
static void mtp008_dec_use(struct i2c_client *client);

static int mtp008_read_value(struct i2c_client *client, u8 register);
static int mtp008_write_value(struct i2c_client *client, u8 register, u8 value);
static void mtp008_update_client(struct i2c_client *client);
static void mtp008_init_client(struct i2c_client *client);

static void mtp008_in(struct i2c_client *client, int operation,
		      int ctl_name, int *nrels_mag, long *results);
static void mtp008_fan(struct i2c_client *client, int operation,
		       int ctl_name, int *nrels_mag, long *results);
static void mtp008_temp(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void mtp008_temp_add(struct i2c_client *client, int operation,
			    int ctl_name, int *nrels_mag, long *results);
static void mtp008_vid(struct i2c_client *client, int operation,
		       int ctl_name, int *nrels_mag, long *results);
static void mtp008_fan_div(struct i2c_client *client, int operation,
			   int ctl_name, int *nrels_mag, long *results);
static void mtp008_alarms(struct i2c_client *client, int operation,
			  int ctl_name, int *nrels_mag, long *results);
static void mtp008_beep(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void mtp008_pwm(struct i2c_client *client, int operation,
		       int ctl_name, int *nrels_mag, long *results);
static void mtp008_sens(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void mtp008_getsensortype(struct mtp008_data *data, u8 inp);

static int mtp008_id = 0;

static struct i2c_driver mtp008_driver =
{
    /* name */			"MTP008 sensor driver",
    /* id */			I2C_DRIVERID_MTP008,
    /* flags */			I2C_DF_NOTIFY,
    /* attach_adapter */	&mtp008_attach_adapter,
    /* detach_client */		&mtp008_detach_client,
    /* command */		&mtp008_command,
    /* inc_use */		&mtp008_inc_use,
    /* dec_use */		&mtp008_dec_use
};

/* Used by mtp008_init/cleanup */
static int __initdata mtp008_initialized = 0;

/* The /proc/sys entries */
/* These files are created for each detected chip. This is just a template;
   though at first sight, you might think we could use a statically
   allocated list, we need some way to get back to the parent - which
   is done through one of the 'extra' fields which are initialized 
   when a new copy is allocated. */

static ctl_table mtp008_dir_table_template[] =
{
	{MTP008_SYSCTL_IN0, "in0", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &mtp008_in},
	{MTP008_SYSCTL_IN1, "in1", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &mtp008_in},
	{MTP008_SYSCTL_IN2, "in2", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &mtp008_in},
	{MTP008_SYSCTL_IN3, "in3", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &mtp008_in},
	{MTP008_SYSCTL_IN4, "in4", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &mtp008_in},
	{MTP008_SYSCTL_IN5, "in5", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &mtp008_in},
	{MTP008_SYSCTL_IN6, "in6", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &mtp008_in},
	{MTP008_SYSCTL_FAN1, "fan1", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &mtp008_fan},
	{MTP008_SYSCTL_FAN2, "fan2", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &mtp008_fan},
	{MTP008_SYSCTL_FAN3, "fan3", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &mtp008_fan},
	{MTP008_SYSCTL_TEMP1, "temp1", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &mtp008_temp},
	{MTP008_SYSCTL_TEMP2, "temp2", NULL, 0, 0644, NULL,
       &i2c_proc_real, &i2c_sysctl_real, NULL, &mtp008_temp_add},
	{MTP008_SYSCTL_TEMP3, "temp3", NULL, 0, 0644, NULL,
       &i2c_proc_real, &i2c_sysctl_real, NULL, &mtp008_temp_add},
	{MTP008_SYSCTL_VID, "vid", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &mtp008_vid},
	{MTP008_SYSCTL_FAN_DIV, "fan_div", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &mtp008_fan_div},
	{MTP008_SYSCTL_ALARMS, "alarms", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &mtp008_alarms},
	{MTP008_SYSCTL_BEEP, "beep", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &mtp008_beep},
	{MTP008_SYSCTL_PWM1, "pwm1", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &mtp008_pwm},
	{MTP008_SYSCTL_PWM2, "pwm2", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &mtp008_pwm},
	{MTP008_SYSCTL_PWM3, "pwm3", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &mtp008_pwm},
	{MTP008_SYSCTL_SENS1, "sensor1", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &mtp008_sens},
	{MTP008_SYSCTL_SENS2, "sensor2", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &mtp008_sens},
	{MTP008_SYSCTL_SENS3, "sensor3", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &mtp008_sens},
	{0}
};

/* This function is called when:
 * mtp008_driver is inserted (when this module is loaded), for each available
 * adapter when a new adapter is inserted (and mtp008_driver is still present)
 */
int mtp008_attach_adapter(struct i2c_adapter *adapter)
{
	struct i2c_client_address_data mtp008_addr_data;

	mtp008_addr_data.normal_i2c = addr_data.normal_i2c;
	mtp008_addr_data.normal_i2c_range = addr_data.normal_i2c_range;
	mtp008_addr_data.probe = addr_data.probe;
	mtp008_addr_data.probe_range = addr_data.probe_range;
	mtp008_addr_data.ignore = addr_data.ignore;
	mtp008_addr_data.ignore_range = addr_data.ignore_range;
	mtp008_addr_data.force = addr_data.forces->force;

	return i2c_probe(adapter, &mtp008_addr_data, mtp008_detect);
}

int mtp008_detect(struct i2c_adapter *adapter, int address,
		  unsigned short flags, int kind)
{
	const char *type_name = "";
	const char *client_name = "";
	int is_isa, err, sysid;
	struct i2c_client *new_client;
	struct mtp008_data *data;

	err = 0;

	is_isa = i2c_is_isa_adapter(adapter);
	if (is_isa ||
	    !i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		goto ERROR0;

	/*
	 * We presume we have a valid client.  We now create the client
	 * structure, even though we cannot fill it completely yet.  But it
	 * allows us to use mtp008_(read|write)_value().
	 */
	if (!(new_client = kmalloc(sizeof(struct i2c_client) +
				   sizeof(struct mtp008_data),
				   GFP_KERNEL))) {
		err = -ENOMEM;
		goto ERROR0;
	}
	data = (struct mtp008_data *) (new_client + 1);
	new_client->addr = address;
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &mtp008_driver;
	new_client->flags = 0;

	/*
	 * Remaining detection.
	 */
	if (kind < 0) {
		if (mtp008_read_value(new_client, MTP008_REG_CHIPID) != 0xac)
			goto ERROR1;
	}
	/*
	 * Fill in the remaining client fields and put it into the global list.
	 */
	type_name = "mtp008";
	client_name = "MTP008 chip";
	strcpy(new_client->name, client_name);
	data->type = kind;

	new_client->id = mtp008_id++;
	data->valid = 0;
	init_MUTEX(&data->update_lock);

	/*
	 * Tell the I2C layer that a new client has arrived.
	 */
	if ((err = i2c_attach_client(new_client)))
		goto ERROR1;

	/*
	 * Register a new directory entry with the sensors module.
	 */
	if ((sysid = i2c_register_entry(new_client, type_name,
					    mtp008_dir_table_template,
					    THIS_MODULE)) < 0) {
		err = sysid;
		goto ERROR2;
	}
	data->sysctl_id = sysid;

	/*
	 * Initialize the MTP008 chip.
	 */
	mtp008_init_client(new_client);

	return 0;

	/*
	 * Error handling.  Bad programming practise but very code efficient.
	 */
      ERROR2:
	i2c_detach_client(new_client);
      ERROR1:
	kfree(new_client);

      ERROR0:
	return err;
}

int mtp008_detach_client(struct i2c_client *client)
{
	int err;

	i2c_deregister_entry(
		((struct mtp008_data *) (client->data))->sysctl_id);

	if ((err = i2c_detach_client(client))) {
		printk("mtp008.o: Deregistration failed, "
		       "client not detached.\n");
		return err;
	}
	kfree(client);

	return 0;
}

/* No commands defined yet */
int mtp008_command(struct i2c_client *client, unsigned int cmd, void *arg)
{
	return 0;
}

void mtp008_inc_use(struct i2c_client *client)
{
#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif
}

void mtp008_dec_use(struct i2c_client *client)
{
#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
}

int mtp008_read_value(struct i2c_client *client, u8 reg)
{
	return i2c_smbus_read_byte_data(client, reg) & 0xff;
}

int mtp008_write_value(struct i2c_client *client, u8 reg, u8 value)
{
	return i2c_smbus_write_byte_data(client, reg, value);
}

/* Called when we have found a new MTP008. It should set limits, etc. */
void mtp008_init_client(struct i2c_client *client)
{
	int vid;
	u8 save1, save2;
	struct mtp008_data *data;

	data = client->data;

	/*
	 * Initialize the Myson MTP008 hardware monitoring chip.
	 * Save the pin settings that the BIOS hopefully set.
	 */
	save1 = mtp008_read_value(client, MTP008_REG_PIN_CTRL1);
	save2 = mtp008_read_value(client, MTP008_REG_PIN_CTRL2);
	mtp008_write_value(client, MTP008_REG_CONFIG,
	     (mtp008_read_value(client, MTP008_REG_CONFIG) & 0x7f) | 0x80);
	mtp008_write_value(client, MTP008_REG_PIN_CTRL1, save1);
	mtp008_write_value(client, MTP008_REG_PIN_CTRL2, save2);

	mtp008_getsensortype(data, save2);

	/*
	 * Retrieve the VID setting (needed for the default limits).
	 */
	vid = mtp008_read_value(client, MTP008_REG_VID_FANDIV) & 0x0f;
	vid |= (mtp008_read_value(client, MTP008_REG_RESET_VID4) & 0x01) << 4;
	vid = VID_FROM_REG(vid);

	/*
	 * Set the default limits.
	 *
	 * Setting temp sensors is done as follows:
	 *
	 *          Register 0x57: 0 0 0 0 x x x x
	 *                                 | \ / +-- AIN5/VT3
	 *                                 |  +----- AIN4/VT2/PII
	 *                                 +-------- VT1/PII
	 */

	mtp008_write_value(client, MTP008_REG_IN_MAX(0),
			   IN_TO_REG(MTP008_INIT_IN_MAX_0));
	mtp008_write_value(client, MTP008_REG_IN_MIN(0),
			   IN_TO_REG(MTP008_INIT_IN_MIN_0));
	mtp008_write_value(client, MTP008_REG_IN_MAX(1),
			   IN_TO_REG(MTP008_INIT_IN_MAX_1));
	mtp008_write_value(client, MTP008_REG_IN_MIN(1),
			   IN_TO_REG(MTP008_INIT_IN_MIN_1));
	mtp008_write_value(client, MTP008_REG_IN_MAX(2),
			   IN_TO_REG(MTP008_INIT_IN_MAX_2));
	mtp008_write_value(client, MTP008_REG_IN_MIN(2),
			   IN_TO_REG(MTP008_INIT_IN_MIN_2));
	mtp008_write_value(client, MTP008_REG_IN_MAX(3),
			   IN_TO_REG(MTP008_INIT_IN_MAX_3));
	mtp008_write_value(client, MTP008_REG_IN_MIN(3),
			   IN_TO_REG(MTP008_INIT_IN_MIN_3));

	mtp008_write_value(client, MTP008_REG_IN_MAX(5),
			   IN_TO_REG(MTP008_INIT_IN_MAX_5));
	mtp008_write_value(client, MTP008_REG_IN_MIN(5),
			   IN_TO_REG(MTP008_INIT_IN_MIN_5));
	mtp008_write_value(client, MTP008_REG_IN_MAX(6),
			   IN_TO_REG(MTP008_INIT_IN_MAX_6));
	mtp008_write_value(client, MTP008_REG_IN_MIN(6),
			   IN_TO_REG(MTP008_INIT_IN_MIN_6));

	mtp008_write_value(client, MTP008_REG_TEMP_MAX,
			   TEMP_TO_REG(MTP008_INIT_TEMP_OVER));
	mtp008_write_value(client, MTP008_REG_TEMP_MIN,
			   TEMP_TO_REG(MTP008_INIT_TEMP_HYST));
	mtp008_write_value(client, MTP008_REG_IN_MAX(4),
			   TEMP_TO_REG(MTP008_INIT_TEMP2_OVER));
	mtp008_write_value(client, MTP008_REG_IN_MIN(4),
			   TEMP_TO_REG(MTP008_INIT_TEMP2_HYST));

	mtp008_write_value(client, MTP008_REG_FAN_MIN(1),
			   FAN_TO_REG(MTP008_INIT_FAN_MIN_1, 2));
	mtp008_write_value(client, MTP008_REG_FAN_MIN(2),
			   FAN_TO_REG(MTP008_INIT_FAN_MIN_2, 2));
	mtp008_write_value(client, MTP008_REG_FAN_MIN(3),
			   FAN_TO_REG(MTP008_INIT_FAN_MIN_3, 2));

	/*
	 * Start monitoring.
	 */
	mtp008_write_value(
		client, MTP008_REG_CONFIG,
		(mtp008_read_value(client, MTP008_REG_CONFIG) & 0xf7) | 0x01
	);
}

void mtp008_update_client(struct i2c_client *client)
{
	int i;
	u8 inp;
	struct mtp008_data *data;

	data = client->data;

	down(&data->update_lock);

	if ((jiffies - data->last_updated > HZ + HZ / 2) ||
	    (jiffies < data->last_updated) || !data->valid) {
#ifdef DEBUG
		printk("Starting MTP008 update\n");
#endif

		/*
		 * Read in the analog inputs.  We're reading AIN4 and AIN5 as
		 * regular analog inputs, even though they may have been
		 * configured as temperature readings instead.  Interpretation
		 * of these values is done elsewhere.
		 */
		for (i = 0; i < 7; i++) {
			data->in[i] =
				mtp008_read_value(client, MTP008_REG_IN(i));
			data->in_max[i] =
				mtp008_read_value(client, MTP008_REG_IN_MAX(i));
			data->in_min[i] =
				mtp008_read_value(client, MTP008_REG_IN_MIN(i));
		}

		/*
		 * Read the temperature sensor.
		 */
		data->temp = mtp008_read_value(client, MTP008_REG_TEMP);
		data->temp_max = mtp008_read_value(client, MTP008_REG_TEMP_MAX);
		data->temp_min = mtp008_read_value(client, MTP008_REG_TEMP_MIN);

		/*
		 * Read the first 2 fan dividers and the VID setting.  Read the
		 * third fan divider from a different register.
		 */
		inp = mtp008_read_value(client, MTP008_REG_VID_FANDIV);
		data->vid = inp & 0x0f;
		data->vid |= (mtp008_read_value(client,
				     MTP008_REG_RESET_VID4) & 0x01) << 4;

		data->fan_div[0] = (inp >> 4) & 0x03;
		data->fan_div[1] = inp >> 6;
		data->fan_div[2] =
			mtp008_read_value(client, MTP008_REG_PIN_CTRL1) >> 6;

		/*
		 * Read the interrupt status registers.
		 */
		data->alarms =
			(mtp008_read_value(client,
					   MTP008_REG_INT_STAT1) & 0xdf) |
			(mtp008_read_value(client,
					   MTP008_REG_INT_STAT2) & 0x0f) << 8;

		/*
		 * Read the beep control registers.
		 */
		data->beeps =
			(mtp008_read_value(client,
					   MTP008_REG_BEEP_CTRL1) & 0xdf) |
			(mtp008_read_value(client,
					   MTP008_REG_BEEP_CTRL2) & 0x8f) << 8;

		/*
		 * Read the sensor configuration.
		 */
		inp = mtp008_read_value(client, MTP008_REG_PIN_CTRL2);
		mtp008_getsensortype(data, inp);

		/*
		 * Read the PWM registers if enabled.
		 */
		for (i = 1; i <= 3; i++)
		{
			if(PWMENABLE_FROM_REG(i, inp))
				data->pwm[i-1] = mtp008_read_value(client,
						  MTP008_REG_PWM_CTRL(i));
			else
				data->pwm[i-1] = 255;
		}

		/*
		 * Read the fan sensors. Skip 3 if PWM3 enabled.
		 */
		for (i = 1; i <= 3; i++) {
			if(i == 3 && PWMENABLE_FROM_REG(3, inp)) {
				data->fan[2] = 0;
				data->fan_min[2] = 0;
			} else {
				data->fan[i-1] = mtp008_read_value(client,
					  MTP008_REG_FAN(i));
				data->fan_min[i-1] = mtp008_read_value(client,
					  MTP008_REG_FAN_MIN(i));
			}
		}

		data->last_updated = jiffies;
		data->valid = 1;
	}
	up(&data->update_lock);
}

void mtp008_getsensortype(struct mtp008_data *data, u8 inp)
{
	inp &= 0x0f;
	data->sens[0] = (inp >> 3) + 2;			/* 2 or 3 */
	data->sens[1] = ((inp >> 1) & 0x03) + 1;	/* 1, 2 or 3 */
	data->sens[2] = (inp & 0x01) + 1;		/* 1 or 2 */
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
void mtp008_in(struct i2c_client *client, int operation, int ctl_name,
	       int *nrels_mag, long *results)
{
	int nr;
	struct mtp008_data *data;

	nr = ctl_name - MTP008_SYSCTL_IN0;
	data = client->data;

	switch (operation) {
	case SENSORS_PROC_REAL_INFO:
		*nrels_mag = 2;

		break;
	case SENSORS_PROC_REAL_READ:
		mtp008_update_client(client);

		if((nr != 4 && nr != 5) || data->sens[nr - 3] == VOLTAGE) {
			results[0] = IN_FROM_REG(data->in_min[nr]);
			results[1] = IN_FROM_REG(data->in_max[nr]);
			results[2] = IN_FROM_REG(data->in[nr]);
		} else {
			results[0] = 0;
			results[1] = 0;
			results[2] = 0;
		}

		*nrels_mag = 3;

		break;
	case SENSORS_PROC_REAL_WRITE:
		if((nr != 4 && nr != 5) || data->sens[nr - 3] == VOLTAGE) {
			if (*nrels_mag >= 1) {
				data->in_min[nr] = IN_TO_REG(results[0]);
				mtp008_write_value(client, MTP008_REG_IN_MIN(nr),
						   data->in_min[nr]);
			}
			if (*nrels_mag >= 2) {
				data->in_max[nr] = IN_TO_REG(results[1]);
				mtp008_write_value(client, MTP008_REG_IN_MAX(nr),
						   data->in_max[nr]);
			}
		}
	}
}

void mtp008_fan(struct i2c_client *client, int operation, int ctl_name,
		int *nrels_mag, long *results)
{
	int nr;
	struct mtp008_data *data;

	nr = ctl_name - MTP008_SYSCTL_FAN1;
	data = client->data;

	switch (operation) {
	case SENSORS_PROC_REAL_INFO:
		*nrels_mag = 0;

		break;
	case SENSORS_PROC_REAL_READ:
		mtp008_update_client(client);

		results[0] = FAN_FROM_REG(data->fan_min[nr],
					DIV_FROM_REG(data->fan_div[nr]));
		results[1] = FAN_FROM_REG(data->fan[nr],
					DIV_FROM_REG(data->fan_div[nr]));

		*nrels_mag = 2;

		break;
	case SENSORS_PROC_REAL_WRITE:
		if (*nrels_mag >= 1) {
			data->fan_min[nr] =
			    FAN_TO_REG(results[0],
				       DIV_FROM_REG(data->fan_div[nr]));
			mtp008_write_value(client, MTP008_REG_FAN_MIN(nr + 1),
					   data->fan_min[nr]);
		}
	}
}

void mtp008_temp(struct i2c_client *client, int operation, int ctl_name,
		 int *nrels_mag, long *results)
{
	struct mtp008_data *data;

	data = client->data;

	switch (operation) {
	case SENSORS_PROC_REAL_INFO:
		*nrels_mag = 1;

		break;
	case SENSORS_PROC_REAL_READ:
		mtp008_update_client(client);

		results[0] = TEMP_FROM_REG(data->temp_max);
		results[1] = TEMP_FROM_REG(data->temp_min);
		results[2] = TEMP_FROM_REG(data->temp);
		*nrels_mag = 3;

		break;
	case SENSORS_PROC_REAL_WRITE:
		if (*nrels_mag >= 1) {
			data->temp_max = TEMP_TO_REG(results[0]);
			mtp008_write_value(client, MTP008_REG_TEMP_MAX,
					   data->temp_max);
		}
		if (*nrels_mag >= 2) {
			data->temp_min = TEMP_TO_REG(results[1]);
			mtp008_write_value(client, MTP008_REG_TEMP_MIN,
					   data->temp_min);
		}
	}
}

void mtp008_temp_add(struct i2c_client *client, int operation, int ctl_name,
		     int *nrels_mag, long *results)
{
	int nr;
	struct mtp008_data *data;

	nr = 3 + ctl_name - MTP008_SYSCTL_TEMP1;	/* AIN4 or AIN5 */
	data = client->data;

	switch (operation) {
	case SENSORS_PROC_REAL_INFO:
		*nrels_mag = 1;

		break;
	case SENSORS_PROC_REAL_READ:
		mtp008_update_client(client);

		if(data->sens[nr - 3] != VOLTAGE) {
			results[0] = TEMP_FROM_REG(data->in_max[nr]);
			results[1] = TEMP_FROM_REG(data->in_min[nr]);
			results[2] = TEMP_FROM_REG(data->in[nr]);
		} else {
			results[0] = 0;
			results[1] = 0;
			results[2] = 0;
		}
		*nrels_mag = 3;

		break;
	case SENSORS_PROC_REAL_WRITE:
		if(data->sens[nr - 3] != VOLTAGE) {
			if (*nrels_mag >= 1) {
				data->in_max[nr] = TEMP_TO_REG(results[0]);
				mtp008_write_value(client, MTP008_REG_TEMP_MAX,
						   data->in_max[nr]);
			}
			if (*nrels_mag >= 2) {
				data->in_min[nr] = TEMP_TO_REG(results[1]);
				mtp008_write_value(client, MTP008_REG_TEMP_MIN,
						   data->in_min[nr]);
			}
		}
	}
}

void mtp008_vid(struct i2c_client *client, int operation, int ctl_name,
		int *nrels_mag, long *results)
{
	struct mtp008_data *data;

	data = client->data;

	switch (operation) {
	case SENSORS_PROC_REAL_INFO:
		*nrels_mag = 2;

		break;
	case SENSORS_PROC_REAL_READ:
		mtp008_update_client(client);

		results[0] = VID_FROM_REG(data->vid);

		*nrels_mag = 1;
	}
}

void mtp008_fan_div(struct i2c_client *client, int operation,
		    int ctl_name, int *nrels_mag, long *results)
{
	struct mtp008_data *data;
	u8 val;

	data = client->data;

	switch (operation) {
	case SENSORS_PROC_REAL_INFO:
		*nrels_mag = 0;

		break;
	case SENSORS_PROC_REAL_READ:
		mtp008_update_client(client);

		results[0] = DIV_FROM_REG(data->fan_div[0]);
		results[1] = DIV_FROM_REG(data->fan_div[1]);
		results[2] = DIV_FROM_REG(data->fan_div[2]);

		*nrels_mag = 3;

		break;
	case SENSORS_PROC_REAL_WRITE:
		if (*nrels_mag >= 3) {
			data->fan_div[2] = DIV_TO_REG(results[2]);
			val = mtp008_read_value(client, MTP008_REG_PIN_CTRL1);
			val = (val & 0x3f) | (data->fan_div[2] & 0x03) << 6;
			mtp008_write_value(client, MTP008_REG_PIN_CTRL1, val);
		}
		if (*nrels_mag >= 1) {
			val = mtp008_read_value(client, MTP008_REG_VID_FANDIV);
			if (*nrels_mag >= 2) {
				data->fan_div[1] = DIV_TO_REG(results[1]);
				val = (val & 0x3f) |
				      (data->fan_div[1] & 0x03) << 6;
			}
			data->fan_div[0] = DIV_TO_REG(results[0]);
			val = (val & 0xcf) | (data->fan_div[0] & 0x03) << 4;
			mtp008_write_value(client, MTP008_REG_VID_FANDIV, val);
		}
	}
}

void mtp008_alarms(struct i2c_client *client, int operation, int ctl_name,
		   int *nrels_mag, long *results)
{
	struct mtp008_data *data;

	data = client->data;

	switch (operation) {
	case SENSORS_PROC_REAL_INFO:
		*nrels_mag = 0;

		break;
	case SENSORS_PROC_REAL_READ:
		mtp008_update_client(client);

		results[0] = ALARMS_FROM_REG(data->alarms);

		*nrels_mag = 1;
	}
}

void mtp008_beep(struct i2c_client *client, int operation, int ctl_name,
		 int *nrels_mag, long *results)
{
	struct mtp008_data *data;

	data = client->data;

	switch (operation) {
	case SENSORS_PROC_REAL_INFO:
		*nrels_mag = 0;

		break;
	case SENSORS_PROC_REAL_READ:
		mtp008_update_client(client);

		results[0] = BEEPS_FROM_REG(data->beeps);

		*nrels_mag = 1;

		break;
	case SENSORS_PROC_REAL_WRITE:
		if (*nrels_mag >= 1) {
			data->beeps = BEEPS_TO_REG(results[0]) & 0xdf8f;

			mtp008_write_value(client, MTP008_REG_BEEP_CTRL1,
					   data->beeps & 0xff);
			mtp008_write_value(client, MTP008_REG_BEEP_CTRL2,
					   data->beeps >> 8);
		}
	}
}

void mtp008_pwm(struct i2c_client *client, int operation, int ctl_name,
		int *nrels_mag, long *results)
{
	int nr;
	struct mtp008_data *data;

	nr = ctl_name - MTP008_SYSCTL_PWM1;
	data = client->data;

	switch (operation) {
	case SENSORS_PROC_REAL_INFO:
		*nrels_mag = 0;

		break;
	case SENSORS_PROC_REAL_READ:
		mtp008_update_client(client);

		results[0] = PWM_FROM_REG(data->pwm[nr]);

		*nrels_mag = 1;

		break;
	case SENSORS_PROC_REAL_WRITE:
		if (*nrels_mag >= 1) {
			data->pwm[nr] = PWM_TO_REG(results[0]);
			mtp008_write_value(client, MTP008_REG_PWM_CTRL(nr),
					   data->pwm[nr]);
		}
	}
}

void mtp008_sens(struct i2c_client *client, int operation, int ctl_name,
		 int *nrels_mag, long *results)
{
	const char *opts = "";
	int nr;
	u8 tmp;
	struct mtp008_data *data;

	nr = 1 + ctl_name - MTP008_SYSCTL_SENS1;
	data = client->data;

	switch (operation) {
	case SENSORS_PROC_REAL_INFO:
		*nrels_mag = 0;

		break;
	case SENSORS_PROC_REAL_READ:
		results[0] = data->sens[nr - 1];

		*nrels_mag = 1;

		break;
	case SENSORS_PROC_REAL_WRITE:
		if (*nrels_mag >= 1) {
			tmp = mtp008_read_value(client, MTP008_REG_PIN_CTRL2);

			switch (nr) {
			case 1:	/* VT or PII */
				opts = "2 or 3";

				switch (results[0]) {
				case THERMISTOR:
					mtp008_write_value(
						client, MTP008_REG_PIN_CTRL2,
						tmp & ~MTP008_CFG_VT1_PII);
					data->sens[0] = 2;
					return;
				case PIIDIODE:
					mtp008_write_value(
						client, MTP008_REG_PIN_CTRL2,
						tmp | MTP008_CFG_VT1_PII);
					data->sens[0] = 3;
					return;
				}

				break;
			case 2:	/* AIN, VT or PII */
				tmp &= ~MTP008_CFG_VT2_MASK;
				opts = "1, 2 or 3";

				switch (results[0]) {
				case VOLTAGE:
					mtp008_write_value(
						client, MTP008_REG_PIN_CTRL2,
						tmp | MTP008_CFG_VT2_AIN);
					data->sens[1] = 1;
					return;
				case THERMISTOR:
					mtp008_write_value(
						client, MTP008_REG_PIN_CTRL2,
						tmp | MTP008_CFG_VT2_VT);
					data->sens[1] = 2;
					return;
				case PIIDIODE:
					mtp008_write_value(
						client, MTP008_REG_PIN_CTRL2,
						tmp | MTP008_CFG_VT2_PII);
					data->sens[1] = 3;
					return;
				}

				break;
			case 3:	/* AIN or VT */
				opts = "1 or 2";

				switch (results[0]) {
				case VOLTAGE:
					mtp008_write_value(
						client, MTP008_REG_PIN_CTRL2,
						tmp & ~MTP008_CFG_VT3_VT);
					data->sens[2] = 1;
					return;
				case THERMISTOR:
					mtp008_write_value(
						client, MTP008_REG_PIN_CTRL2,
						tmp | MTP008_CFG_VT3_VT);
					data->sens[2] = 2;
					return;
				}

				break;
			}

			printk("mtp008.o: Invalid sensor type %ld "
			       "for sensor %d; must be %s.\n",
			       results[0], nr, opts);
		}
	}
}

int __init sensors_mtp008_init(void)
{
	int res;

	printk("mtp008.o version %s (%s)\n", LM_VERSION, LM_DATE);
	mtp008_initialized = 0;

	if ((res = i2c_add_driver(&mtp008_driver))) {
		printk("mtp008.o: Driver registration failed, "
		       "module not inserted.\n");
		mtp008_cleanup();
		return res;
	}
	mtp008_initialized++;

	return 0;
}

int __init mtp008_cleanup(void)
{
	int res;

	if (mtp008_initialized >= 1) {
		if ((res = i2c_del_driver(&mtp008_driver))) {
			printk("mtp008.o: Driver deregistration failed, "
			       "module not removed.\n");
			return res;
		}
		mtp008_initialized--;
	}
	return 0;
}

EXPORT_NO_SYMBOLS;

#ifdef MODULE

MODULE_AUTHOR("Frodo Looijaard <frodol@dds.nl>, "
	      "Philip Edelbrock <phil@netroedge.com>, "
	      "and Kris Van Hees <aedil@alchar.org>");
MODULE_DESCRIPTION("MTP008 driver");

int init_module(void)
{
	return sensors_mtp008_init();
}

int cleanup_module(void)
{
	return mtp008_cleanup();
}

#endif				/* MODULE */
