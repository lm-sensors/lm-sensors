/*
 *  pc87360.c - Part of lm_sensors, Linux kernel modules
 *              for hardware monitoring
 *  Copyright (C) 2004 Jean Delvare <khali@linux-fr.org> 
 *
 *  Copied from smsc47m1.c:
 *  Copyright (C) 2002 Mark D. Studebaker <mdsxyz123@yahoo.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Supports the following chips:
 *
 *  Chip	#vin	#fan	#pwm	#temp	devid
 *  PC87360	-	2	2	-	0xE1
 *  PC87363	-	2	2	-	0xE8
 *  PC87364	-	3	3	-	0xE4
 *  PC87365	11	3	3	2	0xE5
 *  PC87366	11	3	3	3	0xE9
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/i2c.h>
#include <linux/i2c-proc.h>
#include <linux/init.h>
#include <asm/io.h>
#include "version.h"

static unsigned short normal_i2c[] = { SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { SENSORS_I2C_END };
static unsigned int normal_isa[] = { 0x0000, SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };
static u8 devid;

SENSORS_INSMOD_5(pc87360, pc87363, pc87364, pc87365, pc87366);

/* modified from kernel/include/traps.c */
#define REG	0x2e	/* The register to read/write */
#define DEV	0x07	/* Register: Logical device select */
#define VAL	0x2f	/* The value to read/write */
#define FSCM	0x09	/* The device with the fan registers in it */
#define DEVID	0x20	/* Register: Device ID */

static inline void superio_outb(int reg, int val)
{
	outb(reg, REG);
	outb(val, VAL);
}

static inline int superio_inb(int reg)
{
	outb(reg, REG);
	return inb(VAL);
}

static inline void superio_select(void)
{
	outb(DEV, REG);
	outb(FSCM, VAL);
}

static inline void superio_exit(void)
{
	outb(0x02, REG);
	outb(0x02, VAL);
}

/*
 * The PC87360 (device id 0xE1) and PC87363 (device id 0xE8) monitor and
 * control two fans.
 * The PC87364 (device id 0xE4), PC87365 (device id 0xE5) and PC87366
 * (device id 0xE9) monitor and control three fans.
 */
#define PC87360_DEVID_MATCH(id)	((id) == 0xE1 || (id) == 0xE8 || \
				 (id) == 0xE4 || (id) == 0xE5 || \
				 (id) == 0xE9)

#define PC87360_BASE_REG	0x60
#define PC87360_ACTIVATE_REG	0x30

#define PC87360_EXTENT		0x10

/* nr has to be 0 or 1 (PC87360/87363) or 2 (PC87364/87365/87366) */
#define PC87360_REG_PRESCALE(nr)	(0x00 + 2 * (nr))
#define PC87360_REG_PWM(nr)		(0x01 + 2 * (nr))
#define PC87360_REG_FAN_MIN(nr)		(0x06 + 3 * (nr))
#define PC87360_REG_FAN(nr)		(0x07 + 3 * (nr))
#define PC87360_REG_FAN_STATUS(nr)	(0x08 + 3 * (nr))

#define FAN_FROM_REG(val,div)	((val)==0?-1:(val)==255?0: \
				 960000/((val)*(div)))
#define FAN_DIV_FROM_REG(val)	(1 << ((val >> 5) & 0x03))
#define PWM_FROM_REG(val)	(val)
#define ALARM_FROM_REG(val)	((val) & 0x07)

struct pc87360_data {
	struct semaphore lock;
	int sysctl_id;

	struct semaphore update_lock;
	char valid;		/* !=0 if following fields are valid */
	unsigned long last_updated;	/* In jiffies */

	u8 fannr;

	u8 fan[3];		/* Register value */
	u8 fan_min[3];		/* Register value */
	u8 fan_status[3];	/* Register value */
	u8 pwm[3];		/* Register value */
};


static int pc87360_attach_adapter(struct i2c_adapter *adapter);
static int pc87360_detect(struct i2c_adapter *adapter, int address,
			  unsigned short flags, int kind);
static int pc87360_detach_client(struct i2c_client *client);

static int pc87360_read_value(struct i2c_client *client, u8 register);
#if 0
static int pc87360_write_value(struct i2c_client *client, u8 register,
			       u8 value);
#endif
static void pc87360_update_client(struct i2c_client *client);
static void pc87360_init_client(struct i2c_client *client);
static int pc87360_find(int *address, u8 *devid);


static void pc87360_fan(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void pc87360_fan_status(struct i2c_client *client, int operation,
			       int ctl_name, int *nrels_mag, long *results);
static void pc87360_fan_div(struct i2c_client *client, int operation,
			    int ctl_name, int *nrels_mag, long *results);
static void pc87360_pwm(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);

static int pc87360_id = 0;

static struct i2c_driver pc87360_driver = {
	.owner		= THIS_MODULE,
	.name		= "PC8736x hardware monitor",
	.flags		= I2C_DF_NOTIFY,
	.attach_adapter	= pc87360_attach_adapter,
	.detach_client	= pc87360_detach_client,
};

/* -- SENSORS SYSCTL START -- */

#define PC87360_SYSCTL_FAN1		1101 /* Rotations/min */
#define PC87360_SYSCTL_FAN2		1102
#define PC87360_SYSCTL_FAN3		1103 /* not for PC87360/PC87363 */
#define PC87360_SYSCTL_FAN_DIV		1201 /* 1, 2, 4 or 8 */
#define PC87360_SYSCTL_FAN1_STATUS	1301 /* bit field */
#define PC87360_SYSCTL_FAN2_STATUS	1302
#define PC87360_SYSCTL_FAN3_STATUS	1303 /* not for PC87360/PC87363 */
#define PC87360_SYSCTL_PWM1		1401 /* 0-255 */
#define PC87360_SYSCTL_PWM2		1402
#define PC87360_SYSCTL_PWM3		1403 /* not for PC87360/PC87363 */

#define PC87360_ALARM_FAN_READY		0x01
#define PC87360_ALARM_FAN_LOW		0x02
#define PC87360_ALARM_FAN_OVERFLOW	0x04

/* -- SENSORS SYSCTL END -- */

static ctl_table pc87360_dir_table_template[] = { /* PC87363 too */
	{PC87360_SYSCTL_FAN1, "fan1", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87360_fan},
	{PC87360_SYSCTL_FAN2, "fan2", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87360_fan},
	{PC87360_SYSCTL_FAN_DIV, "fan_div", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87360_fan_div},
	{PC87360_SYSCTL_FAN1_STATUS, "fan1_status", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87360_fan_status},
	{PC87360_SYSCTL_FAN2_STATUS, "fan2_status", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87360_fan_status},
	{PC87360_SYSCTL_PWM1, "pwm1", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87360_pwm},
	{PC87360_SYSCTL_PWM2, "pwm2", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87360_pwm},
	{0}
};

static ctl_table pc87364_dir_table_template[] = { /* PC87365 and PC87366 too */
	{PC87360_SYSCTL_FAN1, "fan1", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87360_fan},
	{PC87360_SYSCTL_FAN2, "fan2", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87360_fan},
	{PC87360_SYSCTL_FAN3, "fan3", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87360_fan},
	{PC87360_SYSCTL_FAN_DIV, "fan_div", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87360_fan_div},
	{PC87360_SYSCTL_FAN1_STATUS, "fan1_status", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87360_fan_status},
	{PC87360_SYSCTL_FAN2_STATUS, "fan2_status", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87360_fan_status},
	{PC87360_SYSCTL_FAN3_STATUS, "fan3_status", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87360_fan_status},
	{PC87360_SYSCTL_PWM1, "pwm1", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87360_pwm},
	{PC87360_SYSCTL_PWM2, "pwm2", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87360_pwm},
	{PC87360_SYSCTL_PWM3, "pwm3", NULL, 0, 0444, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &pc87360_pwm},
	{0}
};

static int pc87360_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_detect(adapter, &addr_data, pc87360_detect);
}

static int pc87360_find(int *address, u8 *devid)
{
	u16 val;

	/* no superio_enter */
	val = superio_inb(DEVID);
	if (!PC87360_DEVID_MATCH(val)) {
		superio_exit();
		return -ENODEV;
	}
	*devid = val;

	superio_select();

	val = superio_inb(PC87360_ACTIVATE_REG);
	if (!(val & 0x01)) {
		printk("pc87360.o: device not activated\n");
		superio_exit();
		return -ENODEV;
	}

	val = (superio_inb(PC87360_BASE_REG) << 8)
	    | superio_inb(PC87360_BASE_REG + 1);
	*address = val & ~(PC87360_EXTENT - 1);
	if (*address == 0) {
		printk("pc87360.o: base address not set\n");
		superio_exit();
		return -ENODEV;
	}

	superio_exit();
	return 0;
}

int pc87360_detect(struct i2c_adapter *adapter, int address,
		   unsigned short flags, int kind)
{
	int i;
	struct i2c_client *new_client;
	struct pc87360_data *data;
	int err = 0;
	const char *type_name = "pc87360";
	const char *client_name = "PC8736x chip";

	if (!i2c_is_isa_adapter(adapter)) {
		return 0;
	}

	if (check_region(address, PC87360_EXTENT)) {
		printk("pc87360.o: region 0x%x already in use!\n", address);
		return -ENODEV;
	}

	if (!(new_client = kmalloc(sizeof(struct i2c_client) +
				   sizeof(struct pc87360_data),
				   GFP_KERNEL))) {
		return -ENOMEM;
	}

	data = (struct pc87360_data *) (new_client + 1);
	new_client->addr = address;
	init_MUTEX(&data->lock);
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &pc87360_driver;
	new_client->flags = 0;

	data->fannr = 2;

	switch (devid) {
		case 0xe4:
		case 0xe5:
		case 0xe9:
			data->fannr = 3;
	}

	request_region(address, PC87360_EXTENT, "pc87360");
	strcpy(new_client->name, client_name);

	new_client->id = pc87360_id++;
	data->valid = 0;
	init_MUTEX(&data->update_lock);

	if ((err = i2c_attach_client(new_client)))
		goto ERROR1;

	if ((i = i2c_register_entry((struct i2c_client *) new_client,
				    type_name,
				    (data->fannr == 3) ?
				    pc87364_dir_table_template :
				    pc87360_dir_table_template)) < 0) {
		err = i;
		goto ERROR2;
	}
	data->sysctl_id = i;

	pc87360_init_client(new_client);
	return 0;

      ERROR2:
	i2c_detach_client(new_client);
      ERROR1:
	release_region(address, PC87360_EXTENT);
	kfree(new_client);
	return err;
}

static int pc87360_detach_client(struct i2c_client *client)
{
	int err;

	i2c_deregister_entry(((struct pc87360_data *) (client->data))->sysctl_id);

	if ((err = i2c_detach_client(client))) {
		printk("pc87360.o: Client deregistration failed, "
		       "client not detached.\n");
		return err;
	}

	release_region(client->addr, PC87360_EXTENT);
	kfree(client);

	return 0;
}

static int pc87360_read_value(struct i2c_client *client, u8 reg)
{
	int res;

	res = inb_p(client->addr + reg);
	return res;
}

#if 0
static int pc87360_write_value(struct i2c_client *client, u8 reg, u8 value)
{
	outb_p(value, client->addr + reg);
	return 0;
}
#endif

static void pc87360_init_client(struct i2c_client *client)
{
	/* nothing yet, read only driver */
}

static void pc87360_update_client(struct i2c_client *client)
{
	struct pc87360_data *data = client->data;
	int i;

	down(&data->update_lock);

	if ((jiffies - data->last_updated > HZ + HZ / 2) ||
	    (jiffies < data->last_updated) || !data->valid) {
		for (i = 0; i < data->fannr; i++) {
			data->fan[i] = pc87360_read_value(client, PC87360_REG_FAN(i));
			data->fan_min[i] = pc87360_read_value(client, PC87360_REG_FAN_MIN(i));
			data->fan_status[i] = pc87360_read_value(client, PC87360_REG_FAN_STATUS(i));
			data->pwm[i] = pc87360_read_value(client, PC87360_REG_PWM(i));
		}

		data->last_updated = jiffies;
		data->valid = 1;
	}

	up(&data->update_lock);
}


void pc87360_fan(struct i2c_client *client, int operation, int ctl_name,
		 int *nrels_mag, long *results)
{
	struct pc87360_data *data = client->data;
	int nr = ctl_name - PC87360_SYSCTL_FAN1;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		pc87360_update_client(client);
		results[0] = FAN_FROM_REG(data->fan_min[nr],
					  FAN_DIV_FROM_REG(data->fan_status[nr]));
		results[1] = FAN_FROM_REG(data->fan[nr],
					  FAN_DIV_FROM_REG(data->fan_status[nr]));
		*nrels_mag = 2;
	}
}


void pc87360_fan_div(struct i2c_client *client, int operation,
		     int ctl_name, int *nrels_mag, long *results)
{
	struct pc87360_data *data = client->data;
	int i;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		pc87360_update_client(client);
		for (i = 0; i < data->fannr; i++) {
			results[i] = FAN_DIV_FROM_REG(data->fan_status[i]);
		}
		*nrels_mag = data->fannr;
	}
}

void pc87360_pwm(struct i2c_client *client, int operation, int ctl_name,
		 int *nrels_mag, long *results)
{
	struct pc87360_data *data = client->data;
	int nr = ctl_name - PC87360_SYSCTL_PWM1;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		pc87360_update_client(client);
		results[0] = PWM_FROM_REG(data->pwm[nr]);
		*nrels_mag = 1;
	}
}

void pc87360_fan_status(struct i2c_client *client, int operation, int ctl_name,
			int *nrels_mag, long *results)
{
	struct pc87360_data *data = client->data;
	int nr = ctl_name - PC87360_SYSCTL_FAN1_STATUS;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		pc87360_update_client(client);
		results[0] = ALARM_FROM_REG(data->pwm[nr]);
		*nrels_mag = 1;
	}
}

static int __init pc87360_init(void)
{
	int addr;

	printk("pc87360.o version %s (%s)\n", LM_VERSION, LM_DATE);

	if (pc87360_find(&addr, &devid)) {
		printk("pc87360.o: PC8736x not detected, module not inserted.\n");
		return -ENODEV;
	}
	normal_isa[0] = addr;

	return i2c_add_driver(&pc87360_driver);
}

static void __exit pc87360_exit(void)
{
	i2c_del_driver(&pc87360_driver);
}


MODULE_AUTHOR("Jean Delvare <khali@linux-fr.org>");
MODULE_DESCRIPTION("PC8736x hardware monitor");
MODULE_LICENSE("GPL");

module_init(pc87360_init);
module_exit(pc87360_exit);
