/*
    gl518sm.c - Part of lm_sensors, Linux kernel modules for hardware
                monitoring
    Copyright (c) 1998, 1999  Frodo Looijaard <frodol@dds.nl>,
                              Kyösti Mälkki <kmalkki@cc.hut.fi>

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

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/i2c-proc.h>
#include <linux/init.h>
#ifdef __SMP__
#include <linux/smp_lock.h>
#endif
#include "version.h"

MODULE_LICENSE("GPL");

/* Addresses to scan */
static unsigned short normal_i2c[] = { 0x2c, 0x2d, SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { SENSORS_I2C_END };
static unsigned int normal_isa[] = { SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/* Insmod parameters */
SENSORS_INSMOD_2(gl518sm_r00, gl518sm_r80);

/* Defining this will enable debug messages for the voltage iteration
   code used with rev 0 ICs */
#undef DEBUG_VIN

/* Many GL518 constants specified below */

/* The GL518 registers */
#define GL518_REG_CHIP_ID 0x00
#define GL518_REG_REVISION 0x01
#define GL518_REG_VENDOR_ID 0x02
#define GL518_REG_CONF 0x03
#define GL518_REG_TEMP 0x04
#define GL518_REG_TEMP_OVER 0x05
#define GL518_REG_TEMP_HYST 0x06
#define GL518_REG_FAN_COUNT 0x07
#define GL518_REG_FAN_LIMIT 0x08
#define GL518_REG_VIN1_LIMIT 0x09
#define GL518_REG_VIN2_LIMIT 0x0a
#define GL518_REG_VIN3_LIMIT 0x0b
#define GL518_REG_VDD_LIMIT 0x0c
#define GL518_REG_VIN3 0x0d
#define GL518_REG_MISC 0x0f
#define GL518_REG_ALARM 0x10
#define GL518_REG_MASK 0x11
#define GL518_REG_INT 0x12
#define GL518_REG_VIN2 0x13
#define GL518_REG_VIN1 0x14
#define GL518_REG_VDD 0x15


/* Conversions. Rounding and limit checking is only done on the TO_REG
   variants. Note that you should be a bit careful with which arguments
   these macros are called: arguments may be evaluated more than once.
   Fixing this is just not worth it. */

#define TEMP_TO_REG(val) (SENSORS_LIMIT(((((val)<0?(val)-5:(val)+5) / 10)+119),\
                                        0,255))
#define TEMP_FROM_REG(val) (((val) - 119) * 10)

static inline u8 FAN_TO_REG(long rpm, int div)
{
	if (rpm == 0)
		return 255;
	rpm = SENSORS_LIMIT(rpm, 1, 1000000);
	return SENSORS_LIMIT((960000 + rpm * div / 2) / (rpm * div), 1,
			     254);
}

#define FAN_FROM_REG(val,div) \
 ( (val)==0 ? 0 : (val)==255 ? 0 : (960000/((val)*(div))) )

#define IN_TO_REG(val) (SENSORS_LIMIT((((val)*10+8)/19),0,255))
#define IN_FROM_REG(val) (((val)*19)/10)

#define VDD_TO_REG(val) (SENSORS_LIMIT((((val)*10+11)/23),0,255))
#define VDD_FROM_REG(val) (((val)*23)/10)

#define DIV_TO_REG(val) ((val)==8?3:(val)==4?2:(val)==1?0:1)
#define DIV_FROM_REG(val) (1 << (val))

#define ALARMS_FROM_REG(val) val

#define BEEP_ENABLE_TO_REG(val) ((val)?0:1)
#define BEEP_ENABLE_FROM_REG(val) ((val)?0:1)

#define BEEPS_TO_REG(val) ((val) & 0x7f)
#define BEEPS_FROM_REG(val) ((val) & 0x7f)

/* Each client has this additional data */
struct gl518_data {
	struct i2c_client client;
	int sysctl_id;
	enum chips type;

	struct semaphore update_lock;

	int iterate_lock;
	int quit_thread;
	struct task_struct *thread;
	wait_queue_head_t wq;
	char valid;		/* !=0 if following fields are valid */
	unsigned long last_updated;	/* In jiffies */
	unsigned long last_updated_v00;
	/* In jiffies (used only by rev00 chips) */

	u8 voltage[4];		/* Register values; [0] = VDD */
	u8 voltage_min[4];	/* Register values; [0] = VDD */
	u8 voltage_max[4];	/* Register values; [0] = VDD */
	u8 iter_voltage[4];	/* Register values; [0] = VDD */
	u8 fan[2];
	u8 fan_min[2];
	u8 temp;		/* Register values */
	u8 temp_over;		/* Register values */
	u8 temp_hyst;		/* Register values */
	u8 alarms, beeps;	/* Register value */
	u8 alarm_mask;		/* Register value */
	u8 fan_div[2];		/* Register encoding, shifted right */
	u8 beep_enable;		/* Boolean */
	u8 iterate;		/* Voltage iteration mode */
};

static int gl518_attach_adapter(struct i2c_adapter *adapter);
static int gl518_detect(struct i2c_adapter *adapter, int address,
			unsigned short flags, int kind);
static void gl518_init_client(struct i2c_client *client);
static int gl518_detach_client(struct i2c_client *client);

static int gl518_read_value(struct i2c_client *client, u8 reg);
static int gl518_write_value(struct i2c_client *client, u8 reg, u16 value);
static void gl518_update_client(struct i2c_client *client);

static void gl518_update_client_rev00(struct i2c_client *client);
static void gl518_update_iterate(struct i2c_client *client);

static void gl518_vin(struct i2c_client *client, int operation,
		      int ctl_name, int *nrels_mag, long *results);
static void gl518_fan(struct i2c_client *client, int operation,
		      int ctl_name, int *nrels_mag, long *results);
static void gl518_temp(struct i2c_client *client, int operation,
		       int ctl_name, int *nrels_mag, long *results);
static void gl518_fan_div(struct i2c_client *client, int operation,
			  int ctl_name, int *nrels_mag, long *results);
static void gl518_alarms(struct i2c_client *client, int operation,
			 int ctl_name, int *nrels_mag, long *results);
static void gl518_beep(struct i2c_client *client, int operation,
		       int ctl_name, int *nrels_mag, long *results);
static void gl518_fan1off(struct i2c_client *client, int operation,
			  int ctl_name, int *nrels_mag, long *results);
static void gl518_iterate(struct i2c_client *client, int operation,
			  int ctl_name, int *nrels_mag, long *results);

/* This is the driver that will be inserted */
static struct i2c_driver gl518_driver = {
	.name		= "GL518SM sensor chip driver",
	.id		= I2C_DRIVERID_GL518,
	.flags		= I2C_DF_NOTIFY,
	.attach_adapter	= gl518_attach_adapter,
	.detach_client	= gl518_detach_client,
};

/* -- SENSORS SYSCTL START -- */

#define GL518_SYSCTL_VDD  1000	/* Volts * 100 */
#define GL518_SYSCTL_VIN1 1001
#define GL518_SYSCTL_VIN2 1002
#define GL518_SYSCTL_VIN3 1003
#define GL518_SYSCTL_FAN1 1101	/* RPM */
#define GL518_SYSCTL_FAN2 1102
#define GL518_SYSCTL_TEMP 1200	/* Degrees Celcius * 10 */
#define GL518_SYSCTL_FAN_DIV 2000	/* 1, 2, 4 or 8 */
#define GL518_SYSCTL_ALARMS 2001	/* bitvector */
#define GL518_SYSCTL_BEEP 2002	/* bitvector */
#define GL518_SYSCTL_FAN1OFF 2003
#define GL518_SYSCTL_ITERATE 2004

#define GL518_ALARM_VDD 0x01
#define GL518_ALARM_VIN1 0x02
#define GL518_ALARM_VIN2 0x04
#define GL518_ALARM_VIN3 0x08
#define GL518_ALARM_TEMP 0x10
#define GL518_ALARM_FAN1 0x20
#define GL518_ALARM_FAN2 0x40

/* -- SENSORS SYSCTL END -- */

/* These files are created for each detected GL518. This is just a template;
   though at first sight, you might think we could use a statically
   allocated list, we need some way to get back to the parent - which
   is done through one of the 'extra' fields which are initialized
   when a new copy is allocated. */
static ctl_table gl518_dir_table_template[] = {
	{GL518_SYSCTL_VIN1, "in1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &gl518_vin},
	{GL518_SYSCTL_VIN2, "in2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &gl518_vin},
	{GL518_SYSCTL_VIN3, "in3", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &gl518_vin},
	{GL518_SYSCTL_VDD, "in0", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &gl518_vin},
	{GL518_SYSCTL_FAN1, "fan1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &gl518_fan},
	{GL518_SYSCTL_FAN2, "fan2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &gl518_fan},
	{GL518_SYSCTL_TEMP, "temp", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &gl518_temp},
	{GL518_SYSCTL_FAN_DIV, "fan_div", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &gl518_fan_div},
	{GL518_SYSCTL_ALARMS, "alarms", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &gl518_alarms},
	{GL518_SYSCTL_BEEP, "beep", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &gl518_beep},
	{GL518_SYSCTL_FAN1OFF, "fan1off", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &gl518_fan1off},
	{GL518_SYSCTL_ITERATE, "iterate", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &gl518_iterate},
	{0}
};

/* I choose here for semi-static GL518SM allocation. Complete dynamic
   allocation could also be used; the code needed for this would probably
   take more memory than the datastructure takes now. */
#define MAX_GL518_NR 4
static struct i2c_client *gl518_list[MAX_GL518_NR];

static int gl518_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_detect(adapter, &addr_data, gl518_detect);
}

static int gl518_detect(struct i2c_adapter *adapter, int address,
			unsigned short flags, int kind)
{
	int i;
	struct i2c_client *new_client;
	struct gl518_data *data;
	int err = 0;
	const char *type_name = "";
	const char *client_name = "";

	/* Make sure we aren't probing the ISA bus!! This is just a safety check
	   at this moment; i2c_detect really won't call us. */
#ifdef DEBUG
	if (i2c_is_isa_adapter(adapter)) {
		printk
		    ("gl518sm.o: gl518_detect called for an ISA bus adapter?!?\n");
		return 0;
	}
#endif

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA |
				     I2C_FUNC_SMBUS_WORD_DATA))
		    goto ERROR0;

	/* OK. For now, we presume we have a valid client. We now create the
	   client structure, even though we cannot fill it completely yet.
	   But it allows us to access gl518_{read,write}_value. */

	if (!(data = kmalloc(sizeof(struct gl518_data), GFP_KERNEL))) {
		err = -ENOMEM;
		goto ERROR0;
	}

	new_client = &data->client;
	new_client->addr = address;
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &gl518_driver;
	new_client->flags = 0;

	/* Now, we do the remaining detection. */

	if (kind < 0) {
		if (
		    (gl518_read_value(new_client, GL518_REG_CHIP_ID) !=
		     0x80)
		    || (gl518_read_value(new_client, GL518_REG_CONF) &
			0x80)) goto ERROR1;
	}

	/* Determine the chip type. */
	if (kind <= 0) {
		i = gl518_read_value(new_client, GL518_REG_REVISION);
		if (i == 0x00)
			kind = gl518sm_r00;
		else if (i == 0x80)
			kind = gl518sm_r80;
		else {
			if (kind == 0)
				printk
				    ("gl518sm.o: Ignoring 'force' parameter for unknown chip at "
				     "adapter %d, address 0x%02x\n",
				     i2c_adapter_id(adapter), address);
			goto ERROR1;
		}
	}

	type_name = "gl518sm";
	if (kind == gl518sm_r00) {
		client_name = "GL518SM Revision 0x00 chip";
	} else if (kind == gl518sm_r80) {
		client_name = "GL518SM Revision 0x80 chip";
	} else {
#ifdef DEBUG
		printk("gl518sm.o: Internal error: unknown kind (%d)?!?",
		       kind);
#endif
		goto ERROR1;
	}

	/* Fill in the remaining client fields and put it into the global list */
	strcpy(new_client->name, client_name);
	data->type = kind;

	for (i = 0; i < MAX_GL518_NR; i++)
		if (!gl518_list[i])
			break;
	if (i == MAX_GL518_NR) {
		printk
		    ("gl518sm.o: No empty slots left, recompile and heighten "
		     "MAX_GL518_NR!\n");
		err = -ENOMEM;
		goto ERROR2;
	}
	gl518_list[i] = new_client;
	data->valid = 0;
	init_MUTEX(&data->update_lock);

	/* Tell the I2C layer a new client has arrived */
	if ((err = i2c_attach_client(new_client)))
		goto ERROR3;

	/* Register a new directory entry with module sensors */
	if ((i = i2c_register_entry((struct i2c_client *) new_client,
					type_name,
					gl518_dir_table_template,
					THIS_MODULE)) < 0) {
		err = i;
		goto ERROR4;
	}
	data->sysctl_id = i;

	/* Initialize the GL518SM chip */
	if (kind == gl518sm_r00)
		data->iterate = 0;
	else
		data->iterate = 3;
	data->iterate_lock = 0;
	data->quit_thread = 0;
	data->thread = NULL;
	data->alarm_mask = 0xff;
	data->voltage[0]=data->voltage[1]=data->voltage[2]=0;
	gl518_init_client((struct i2c_client *) new_client);
	return 0;

/* OK, this is not exactly good programming practice, usually. But it is
   very code-efficient in this case. */

      ERROR4:
	i2c_detach_client(new_client);
      ERROR3:
	for (i = 0; i < MAX_GL518_NR; i++)
		if (new_client == gl518_list[i])
			gl518_list[i] = NULL;
      ERROR2:
      ERROR1:
	kfree(data);
      ERROR0:
	return err;
}


/* Called when we have found a new GL518SM. It should set limits, etc. */
static void gl518_init_client(struct i2c_client *client)
{
	/* Power-on defaults (bit 7=1) */
	gl518_write_value(client, GL518_REG_CONF, 0x80);

	/* No noisy output (bit 2=1), Comparator mode (bit 3=0), two fans (bit4=0),
	   standby mode (bit6=0) */
	gl518_write_value(client, GL518_REG_CONF, 0x04);

	/* Never interrupts */
	gl518_write_value(client, GL518_REG_MASK, 0x00);

	/* Clear status register (bit 5=1), start (bit6=1) */
	gl518_write_value(client, GL518_REG_CONF, 0x24);
	gl518_write_value(client, GL518_REG_CONF, 0x44);
}

static int gl518_detach_client(struct i2c_client *client)
{
	int err, i;
	struct gl518_data *data = client->data;

	i2c_deregister_entry(((struct gl518_data *) (client->data))->
				 sysctl_id);

	if ((err = i2c_detach_client(client))) {
		printk
		    ("gl518sm.o: Client deregistration failed, client not detached.\n");
		return err;
	}

	for (i = 0; i < MAX_GL518_NR; i++)
		if (client == gl518_list[i])
			break;
	if ((i == MAX_GL518_NR)) {
		printk("gl518sm.o: Client to detach not found.\n");
		return -ENOENT;
	}
	gl518_list[i] = NULL;

	if (data->thread) {
		data->quit_thread = 1;
		wake_up_interruptible(&data->wq);
	}

	kfree(client->data);

	return 0;
}


/* Registers 0x07 to 0x0c are word-sized, others are byte-sized 
   GL518 uses a high-byte first convention, which is exactly opposite to
   the usual practice. */
static int gl518_read_value(struct i2c_client *client, u8 reg)
{
	if ((reg >= 0x07) && (reg <= 0x0c))
		return swab16(i2c_smbus_read_word_data(client, reg));
	else
		return i2c_smbus_read_byte_data(client, reg);
}

/* Registers 0x07 to 0x0c are word-sized, others are byte-sized 
   GL518 uses a high-byte first convention, which is exactly opposite to
   the usual practice. */
static int gl518_write_value(struct i2c_client *client, u8 reg, u16 value)
{
	if ((reg >= 0x07) && (reg <= 0x0c))
		return i2c_smbus_write_word_data(client, reg, swab16(value));
	else
		return i2c_smbus_write_byte_data(client, reg, value);
}

static void gl518_update_client(struct i2c_client *client)
{
	struct gl518_data *data = client->data;
	int val;

	down(&data->update_lock);

	if ((jiffies - data->last_updated > HZ + HZ / 2) ||
	    (jiffies < data->last_updated) || !data->valid) {

#ifdef DEBUG
		printk("Starting gl518 update\n");
#endif

		data->alarms = gl518_read_value(client, GL518_REG_INT);
		data->beeps = gl518_read_value(client, GL518_REG_ALARM);

		val = gl518_read_value(client, GL518_REG_VDD_LIMIT);
		data->voltage_min[0] = val & 0xff;
		data->voltage_max[0] = (val >> 8) & 0xff;
		val = gl518_read_value(client, GL518_REG_VIN1_LIMIT);
		data->voltage_min[1] = val & 0xff;
		data->voltage_max[1] = (val >> 8) & 0xff;
		val = gl518_read_value(client, GL518_REG_VIN2_LIMIT);
		data->voltage_min[2] = val & 0xff;
		data->voltage_max[2] = (val >> 8) & 0xff;
		val = gl518_read_value(client, GL518_REG_VIN3_LIMIT);
		data->voltage_min[3] = val & 0xff;
		data->voltage_max[3] = (val >> 8) & 0xff;

		val = gl518_read_value(client, GL518_REG_FAN_COUNT);
		data->fan[0] = (val >> 8) & 0xff;
		data->fan[1] = val & 0xff;

		val = gl518_read_value(client, GL518_REG_FAN_LIMIT);
		data->fan_min[0] = (val >> 8) & 0xff;
		data->fan_min[1] = val & 0xff;

		data->temp = gl518_read_value(client, GL518_REG_TEMP);
		data->temp_over =
		    gl518_read_value(client, GL518_REG_TEMP_OVER);
		data->temp_hyst =
		    gl518_read_value(client, GL518_REG_TEMP_HYST);

		val = gl518_read_value(client, GL518_REG_MISC);
		data->fan_div[0] = (val >> 6) & 0x03;
		data->fan_div[1] = (val >> 4) & 0x03;

		data->alarms &= data->alarm_mask;

		val = gl518_read_value(client, GL518_REG_CONF);
		data->beep_enable = (val >> 2) & 1;

#ifndef DEBUG_VIN
		if (data->type != gl518sm_r00) {
			data->voltage[0] =
			    gl518_read_value(client, GL518_REG_VDD);
			data->voltage[1] =
			    gl518_read_value(client, GL518_REG_VIN1);
			data->voltage[2] =
			    gl518_read_value(client, GL518_REG_VIN2);
			data->voltage[3] =
			    gl518_read_value(client, GL518_REG_VIN3);
		} else
			gl518_update_client_rev00(client);
#else
		gl518_update_client_rev00(client);
#endif

		data->last_updated = jiffies;
		data->valid = 1;
	}

	up(&data->update_lock);
}

/* Here we decide how to run the iteration code.
   When called, we trigger the iteration and report the last
   measured voltage. No delay for user apps */
static void gl518_update_client_rev00(struct i2c_client *client)
{
	struct gl518_data *data = client->data;
	int i;

	if (data->iterate == 1) {	/* 10 sec delay */
		/* as that update is slow, we consider the data valid for 30 seconds */
		if (
		    ((jiffies - data->last_updated_v00 > 30 * HZ)
		     || (data->alarms & 7)
		     || (!data->valid)) && (!data->iterate_lock)) {
			data->iterate_lock = 1;
			gl518_update_iterate(client);
			data->iterate_lock = 0;
		}
		for (i = 0; i < 4; i++)
			data->voltage[i] = data->iter_voltage[i];
	} else if (data->iterate == 2) {	/* show results of last iteration */
		for (i = 0; i < 4; i++)
			data->voltage[i] = data->iter_voltage[i];
		wake_up_interruptible(&data->wq);
	} else {		/* no iteration */
		data->voltage[3] =
		    gl518_read_value(client, GL518_REG_VIN3);
	}
}

static int gl518_update_thread(void *c)
{
	struct i2c_client *client = c;
	struct gl518_data *data = client->data;

#ifdef __SMP__
	lock_kernel();
#endif
	exit_mm(current);
	current->session = 1;
	current->pgrp = 1;
	sigfillset(&current->blocked);
	current->fs->umask = 0;
	strcpy(current->comm, "gl518sm");

	init_waitqueue_head(&(data->wq));
	data->thread = current;

#ifdef __SMP__
	unlock_kernel();
#endif

	for (;;) {
		if (!data->iterate_lock) {
			data->iterate_lock = 1;
			gl518_update_iterate(client);
			data->iterate_lock = 0;
		}

		if ((data->quit_thread) || signal_pending(current))
			break;
		interruptible_sleep_on(&data->wq);
	}

	data->thread = NULL;
	data->quit_thread = 0;
	return 0;
}

/* This updates vdd, vin1, vin2 values by doing slow and multiple
   comparisons for the GL518SM rev 00 that lacks support for direct
   reading of these values.   Values are kept in iter_voltage   */

static void gl518_update_iterate(struct i2c_client *client)
{
	struct gl518_data *data = client->data;
	int i, j, loop_more = 1, min[3], max[3], delta[3];
	int alarm, beeps, irqs;

#define VIN_REG(c) c==0?GL518_REG_VDD_LIMIT:\
                   c==1?GL518_REG_VIN1_LIMIT:\
                   GL518_REG_VIN2_LIMIT

	/* disable beeps & irqs for vin0-2 */
	beeps = gl518_read_value(client, GL518_REG_ALARM);
	irqs = gl518_read_value(client, GL518_REG_MASK);
	gl518_write_value(client, GL518_REG_ALARM, beeps & ~0x7);
	gl518_write_value(client, GL518_REG_MASK, irqs & ~0x7);

	alarm = data->alarms;

	for (i = 0; i < 3; i++) {
		if (alarm & (1 << i)) {
			min[i] = 0;
			max[i] = 127;
		} else {
			min[i] = data->voltage_min[i];
			max[i] =
			    (data->voltage_max[i] +
			     data->voltage_min[i]) / 2;
		}
		delta[i] = (max[i] - min[i]) / 2;
	}

	for (j = 0; (j < 10 && loop_more); j++) {

		for (i = 0; i < 3; i++)
			gl518_write_value(client, VIN_REG(i),
					  max[i] << 8 | min[i]);

		if ((data->thread) &&
		    ((data->quit_thread) || signal_pending(current)))
			goto finish;

		/* we wait now 1.5 seconds before comparing */
		current->state = TASK_INTERRUPTIBLE;
		schedule_timeout(HZ + HZ / 2);

		alarm = gl518_read_value(client, GL518_REG_INT);

#ifdef DEBUG_VIN
		printk("gl518sm: iteration %2d: %4d%c %4d%c %4d%c\n", j,
		       max[0], (alarm & 1) ? '!' : ' ',
		       max[1], (alarm & 2) ? '!' : ' ',
		       max[2], (alarm & 4) ? '!' : ' ');
#endif

		for (loop_more = 0, i = 0; i < 3; i++) {
			if (alarm & (1 << i))
				max[i] += delta[i];
			else
				max[i] -= delta[i];

			if (delta[i])
				loop_more++;
			delta[i] >>= 1;
		}

	}

	for (i = 0; i < 3; i++)
		if (alarm & (1 << i))
			max[i]++;

#ifdef DEBUG_VIN
	printk("gl518sm:    final   :%5d %5d %5d\n", max[0], max[1],
	       max[2]);
	printk("gl518sm:    meter   :%5d %5d %5d\n", data->voltage[0],
	       data->voltage[1], data->voltage[2]);
#endif

	/* update values, including vin3 */
	for (i = 0; i < 3; i++) {
		data->iter_voltage[i] = max[i];
	}
	data->iter_voltage[3] = gl518_read_value(client, GL518_REG_VIN3);
	data->last_updated_v00 = jiffies;

      finish:

	/* reset values */
	for (i = 0; i < 3; i++) {
		gl518_write_value(client, VIN_REG(i),
				  data->voltage_max[i] << 8 | data->
				  voltage_min[i]);
	}

	gl518_write_value(client, GL518_REG_ALARM, beeps);
	gl518_write_value(client, GL518_REG_MASK, irqs);

#undef VIN_REG
}

void gl518_temp(struct i2c_client *client, int operation, int ctl_name,
		int *nrels_mag, long *results)
{
	struct gl518_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 1;
	else if (operation == SENSORS_PROC_REAL_READ) {
		gl518_update_client(client);
		results[0] = TEMP_FROM_REG(data->temp_over);
		results[1] = TEMP_FROM_REG(data->temp_hyst);
		results[2] = TEMP_FROM_REG(data->temp);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->temp_over = TEMP_TO_REG(results[0]);
			gl518_write_value(client, GL518_REG_TEMP_OVER,
					  data->temp_over);
		}
		if (*nrels_mag >= 2) {
			data->temp_hyst = TEMP_TO_REG(results[1]);
			gl518_write_value(client, GL518_REG_TEMP_HYST,
					  data->temp_hyst);
		}
	}
}

void gl518_vin(struct i2c_client *client, int operation, int ctl_name,
	       int *nrels_mag, long *results)
{
	struct gl518_data *data = client->data;
	int nr = ctl_name - GL518_SYSCTL_VDD;
	int regnr, old = 0;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 2;
	else if (operation == SENSORS_PROC_REAL_READ) {
		gl518_update_client(client);
		results[0] = nr ? IN_FROM_REG(data->voltage_min[nr]) :
		    VDD_FROM_REG(data->voltage_min[nr]);
		results[1] = nr ? IN_FROM_REG(data->voltage_max[nr]) :
		    VDD_FROM_REG(data->voltage_max[nr]);
		results[2] = nr ? IN_FROM_REG(data->voltage[nr]) :
		    VDD_FROM_REG(data->voltage[nr]);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		regnr =
		    nr == 0 ? GL518_REG_VDD_LIMIT : nr ==
		    1 ? GL518_REG_VIN1_LIMIT : nr ==
		    2 ? GL518_REG_VIN2_LIMIT : GL518_REG_VIN3_LIMIT;
		if (*nrels_mag == 1)
			old = gl518_read_value(client, regnr) & 0xff00;
		if (*nrels_mag >= 2) {
			data->voltage_max[nr] =
			    nr ? IN_TO_REG(results[1]) :
			    VDD_TO_REG(results[1]);
			old = data->voltage_max[nr] << 8;
		}
		if (*nrels_mag >= 1) {
			data->voltage_min[nr] =
			    nr ? IN_TO_REG(results[0]) :
			    VDD_TO_REG(results[0]);
			old |= data->voltage_min[nr];
			gl518_write_value(client, regnr, old);
		}
	}
}


void gl518_fan(struct i2c_client *client, int operation, int ctl_name,
	       int *nrels_mag, long *results)
{
	struct gl518_data *data = client->data;
	int nr = ctl_name - GL518_SYSCTL_FAN1;
	int old;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		gl518_update_client(client);
		results[0] = FAN_FROM_REG(data->fan_min[nr],
					  DIV_FROM_REG(data->fan_div[nr]));
		results[1] =
		    FAN_FROM_REG(data->fan[nr],
				 DIV_FROM_REG(data->fan_div[nr]));
		*nrels_mag = 2;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->fan_min[nr] = FAN_TO_REG(results[0],
						       DIV_FROM_REG(data->
								    fan_div
								    [nr]));
			old =
			    gl518_read_value(client, GL518_REG_FAN_LIMIT);

			if (nr == 0) {
				old =
				    (old & 0x00ff) | (data->
						      fan_min[0] << 8);
				if (results[0] == 0)
					data->alarm_mask &= ~0x20;
				else
					data->alarm_mask |= 0x20;
			} else {
				old = (old & 0xff00) | data->fan_min[1];
				if (results[0] == 0)
					data->alarm_mask &= ~0x40;
				else
					data->alarm_mask |= 0x40;
			}
			gl518_write_value(client, GL518_REG_FAN_LIMIT,
					  old);
		}
	}
}


void gl518_alarms(struct i2c_client *client, int operation, int ctl_name,
		  int *nrels_mag, long *results)
{
	struct gl518_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		gl518_update_client(client);
		results[0] = ALARMS_FROM_REG(data->alarms);
		*nrels_mag = 1;
	}
}

void gl518_beep(struct i2c_client *client, int operation, int ctl_name,
		int *nrels_mag, long *results)
{
	struct gl518_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		gl518_update_client(client);
		results[0] = BEEP_ENABLE_FROM_REG(data->beep_enable);
		results[1] = BEEPS_FROM_REG(data->beeps);
		*nrels_mag = 2;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->beep_enable = BEEP_ENABLE_TO_REG(results[0]);
			gl518_write_value(client, GL518_REG_CONF,
					  (gl518_read_value(client,
							    GL518_REG_CONF)
					   & 0xfb) | (data->
						      beep_enable << 2));
		}
		if (*nrels_mag >= 2) {
			data->beeps =
			    BEEPS_TO_REG(results[1]) & data->alarm_mask;
			gl518_write_value(client, GL518_REG_ALARM,
					  data->beeps);
		}
	}
}


void gl518_fan_div(struct i2c_client *client, int operation, int ctl_name,
		   int *nrels_mag, long *results)
{
	struct gl518_data *data = client->data;
	int old;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		gl518_update_client(client);
		results[0] = DIV_FROM_REG(data->fan_div[0]);
		results[1] = DIV_FROM_REG(data->fan_div[1]);
		*nrels_mag = 2;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		old = gl518_read_value(client, GL518_REG_MISC);
		if (*nrels_mag >= 2) {
			data->fan_div[1] = DIV_TO_REG(results[1]);
			old = (old & 0xcf) | (data->fan_div[1] << 4);
		}
		if (*nrels_mag >= 1) {
			data->fan_div[0] = DIV_TO_REG(results[0]);
			old = (old & 0x3f) | (data->fan_div[0] << 6);
		}
		gl518_write_value(client, GL518_REG_MISC, old);
	}
}

void gl518_fan1off(struct i2c_client *client, int operation, int ctl_name,
		   int *nrels_mag, long *results)
{
	int old;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		results[0] =
		    ((gl518_read_value(client, GL518_REG_MISC) & 0x08) !=
		     0);
		results[1] =
		    ((gl518_read_value(client, GL518_REG_CONF) & 0x10) !=
		     0);
		*nrels_mag = 2;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			old =
			    gl518_read_value(client,
					     GL518_REG_MISC) & 0xf7;
			if (results[0])
				old |= 0x08;
			gl518_write_value(client, GL518_REG_MISC, old);
		}
		if (*nrels_mag >= 2) {
			old =
			    gl518_read_value(client,
					     GL518_REG_CONF) & 0xef;
			if (results[1])
				old |= 0x10;
			gl518_write_value(client, GL518_REG_CONF, old);
		}
	}
}

void gl518_iterate(struct i2c_client *client, int operation, int ctl_name,
		   int *nrels_mag, long *results)
{
	struct gl518_data *data = client->data;
	int i;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		results[0] = data->iterate;
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE &&
	           data->type == gl518sm_r00 ) {
		if ((*nrels_mag >= 1) && (data->iterate != results[0])) {
			data->iterate = results[0];
			for (i = 0; i < 4; i++) {
				data->voltage[i] = 0;
				data->iter_voltage[i] = 0;
			}
			data->valid = 0;

			if ((data->iterate != 2) && (data->thread)) {
				data->quit_thread = 1;
				wake_up_interruptible(&data->wq);
			} else if ((data->iterate == 2) && (!data->thread)) {
				init_waitqueue_head(&(data->wq));
				kernel_thread(gl518_update_thread,
					      (void *) client, 0);
			}
		}
	}
}

static int __init sm_gl518sm_init(void)
{
	printk("gl518sm.o version %s (%s)\n", LM_VERSION, LM_DATE);
	return i2c_add_driver(&gl518_driver);
}

static void __exit sm_gl518sm_exit(void)
{
	i2c_del_driver(&gl518_driver);
}



MODULE_AUTHOR
    ("Frodo Looijaard <frodol@dds.nl> and Kyösti Mälkki <kmalkki@cc.hut.fi>");
MODULE_DESCRIPTION("GL518SM driver");

module_init(sm_gl518sm_init);
module_exit(sm_gl518sm_exit);
