/*
    smsc47m1.c - Part of lm_sensors, Linux kernel modules
                for hardware monitoring
                
    Copyright (c) 2002 Mark D. Studebaker <mdsxyz123@yahoo.com>

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
#include <linux/ioport.h>
#include <linux/i2c.h>
#include <linux/i2c-proc.h>
#include <linux/init.h>
#include <asm/io.h>
#include "version.h"

static int force_addr = 0;
MODULE_PARM(force_addr, "i");
MODULE_PARM_DESC(force_addr,
		 "Initialize the base address of the sensors");

static unsigned short normal_i2c[] = { SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { SENSORS_I2C_END };
static unsigned int normal_isa[] = { 0x0000, SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

SENSORS_INSMOD_1(smsc47m1);

/* modified from kernel/include/traps.c */
#define	REG	0x2e	/* The register to read/write */
#define	DEV	0x07	/* Register: Logical device select */
#define	VAL	0x2f	/* The value to read/write */
#define PME	0x0a	/* The device with the fan registers in it */
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
	outb(0x55, REG);
}

static inline void
superio_exit(void)
{
	outb(0xAA, REG);
}

/*
 * SMSC LPC47M10x (device id 0x59), LPC47M14x (device id 0x5F) and
 * LPC47B27x (device id 0x51) have fan control.
 * The 47M15x and 47M192 chips "with hardware monitoring block"
 * can do much more besides (device id 0x60).
 */
#define SMSC_DEVID_MATCH(id) ((id) == 0x51 || (id) == 0x59 || (id) == 0x5F || (id) == 0x60)

#define SMSC_ACT_REG 0x30
#define SMSC_BASE_REG 0x60

#define SMSC_EXTENT 0x80

#define SMSC47M1_REG_ALARM1 0x04
#define SMSC47M1_REG_TPIN2 0x33
#define SMSC47M1_REG_TPIN1 0x34
#define SMSC47M1_REG_PPIN(nr) (0x37 - (nr))
#define SMSC47M1_REG_PWM(nr) (0x55 + (nr))
#define SMSC47M1_REG_FANDIV 0x58
#define SMSC47M1_REG_FAN(nr) (0x58 + (nr))
#define SMSC47M1_REG_FAN_MIN(nr) (0x5a + (nr))

static inline u8 MIN_TO_REG(long rpm, int div)
{
	if (rpm == 0)
		return 0;
	rpm = SENSORS_LIMIT(rpm, 1, 1000000);
	return SENSORS_LIMIT(192 - ((983040 + rpm * div / 2) / (rpm * div)),
			     0, 191);
}

#define MIN_FROM_REG(val,div) ((val)>=192?0: \
                                983040/((192-(val))*(div)))
#define FAN_FROM_REG(val,div,preload) ((val)==0?-1:(val)==255?0: \
                                983040/(((val)-preload)*(div)))
#define DIV_FROM_REG(val) (1 << (val))
#define DIV_TO_REG(val) ((val)==8?3:(val)==4?2:(val)==1?0:1)
/* reg is 6 middle bits; /proc is 8 bits */
#define PWM_FROM_REG(val) (((val) << 1) & 0xfc)
#define PWM_TO_REG(val)   (((SENSORS_LIMIT((val), 0, 255)) >> 1) & 0x7e)

struct smsc47m1_data {
	struct i2c_client client;
	struct semaphore lock;
	int sysctl_id;

	struct semaphore update_lock;
	char valid;		/* !=0 if following fields are valid */
	unsigned long last_updated;	/* In jiffies */

	u8 fan[2];		/* Register value */
	u8 fan_min[2];		/* Register value */
	u8 fan_div[2];		/* Register encoding, shifted right */
	u8 alarms;		/* Register encoding */
	u8 pwm[2];		/* Register value (bit 7 is enable) */
};


static int smsc47m1_attach_adapter(struct i2c_adapter *adapter);
static int smsc47m1_detect(struct i2c_adapter *adapter, int address,
			  unsigned short flags, int kind);
static int smsc47m1_detach_client(struct i2c_client *client);

static int smsc47m1_read_value(struct i2c_client *client, u8 register);
static int smsc47m1_write_value(struct i2c_client *client, u8 register,
			       u8 value);
static void smsc47m1_update_client(struct i2c_client *client);
static void smsc47m1_init_client(struct i2c_client *client);
static int smsc47m1_find(int *address);


static void smsc47m1_fan(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void smsc47m1_alarms(struct i2c_client *client, int operation,
			   int ctl_name, int *nrels_mag, long *results);
static void smsc47m1_fan_div(struct i2c_client *client, int operation,
			    int ctl_name, int *nrels_mag, long *results);
static void smsc47m1_pwm(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);

static struct i2c_driver smsc47m1_driver = {
	.name		= "SMSC 47M1xx fan monitor",
	.id		= I2C_DRIVERID_SMSC47M1,
	.flags		= I2C_DF_NOTIFY,
	.attach_adapter	= smsc47m1_attach_adapter,
	.detach_client	= smsc47m1_detach_client,
};

/* -- SENSORS SYSCTL START -- */
#define SMSC47M1_SYSCTL_FAN1 1101   /* Rotations/min */
#define SMSC47M1_SYSCTL_FAN2 1102
#define SMSC47M1_SYSCTL_PWM1 1401
#define SMSC47M1_SYSCTL_PWM2 1402
#define SMSC47M1_SYSCTL_FAN_DIV 2000        /* 1, 2, 4 or 8 */
#define SMSC47M1_SYSCTL_ALARMS 2004    /* bitvector */

#define SMSC47M1_ALARM_FAN1 0x0001
#define SMSC47M1_ALARM_FAN2 0x0002

/* -- SENSORS SYSCTL END -- */

static ctl_table smsc47m1_dir_table_template[] = {
	{SMSC47M1_SYSCTL_FAN1, "fan1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &smsc47m1_fan},
	{SMSC47M1_SYSCTL_FAN2, "fan2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &smsc47m1_fan},
	{SMSC47M1_SYSCTL_FAN_DIV, "fan_div", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &smsc47m1_fan_div},
	{SMSC47M1_SYSCTL_ALARMS, "alarms", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &smsc47m1_alarms},
	{SMSC47M1_SYSCTL_PWM1, "pwm1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &smsc47m1_pwm},
	{SMSC47M1_SYSCTL_PWM2, "pwm2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &smsc47m1_pwm},
	{0}
};

static int smsc47m1_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_detect(adapter, &addr_data, smsc47m1_detect);
}

static int smsc47m1_find(int *address)
{
	u16 val;

	superio_enter();
	val= superio_inb(DEVID);
	if (!SMSC_DEVID_MATCH(val)) {
		superio_exit();
		return -ENODEV;
	}

	superio_select();
	val = (superio_inb(SMSC_BASE_REG) << 8) |
	       superio_inb(SMSC_BASE_REG + 1);
	*address = val & ~(SMSC_EXTENT - 1);
	if (*address == 0 && force_addr == 0) {
		printk("smsc47m1.o: base address not set - use force_addr=0xaddr\n");
		superio_exit();
		return -ENODEV;
	}
	if (force_addr)
		*address = force_addr;	/* so detect will get called */

	superio_exit();
	return 0;
}

int smsc47m1_detect(struct i2c_adapter *adapter, int address,
		   unsigned short flags, int kind)
{
	int i;
	struct i2c_client *new_client;
	struct smsc47m1_data *data;
	int err = 0;
	const char *type_name = "smsc47m1";
	const char *client_name = "47M1xx chip";

	if (!i2c_is_isa_adapter(adapter)) {
		return 0;
	}

	if(force_addr)
		address = force_addr & ~(SMSC_EXTENT - 1);
	if (check_region(address, SMSC_EXTENT)) {
		printk("smsc47m1.o: region 0x%x already in use!\n", address);
		return -ENODEV;
	}
	if(force_addr) {
		printk("smsc47m1.o: forcing ISA address 0x%04X\n", address);
		superio_enter();
		superio_select();
		superio_outb(SMSC_BASE_REG, address >> 8);
		superio_outb(SMSC_BASE_REG+1, address & 0xff);
		superio_exit();
	}

	if (!(data = kmalloc(sizeof(struct smsc47m1_data), GFP_KERNEL))) {
		return -ENOMEM;
	}

	new_client = &data->client;
	new_client->addr = address;
	init_MUTEX(&data->lock);
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &smsc47m1_driver;
	new_client->flags = 0;

	request_region(address, SMSC_EXTENT, "smsc47m1-fans");
	strcpy(new_client->name, client_name);
	data->valid = 0;
	init_MUTEX(&data->update_lock);

	if ((err = i2c_attach_client(new_client)))
		goto ERROR3;

	if ((i = i2c_register_entry((struct i2c_client *) new_client,
					type_name,
					smsc47m1_dir_table_template,
					THIS_MODULE)) < 0) {
		err = i;
		goto ERROR4;
	}
	data->sysctl_id = i;

	smsc47m1_init_client(new_client);
	return 0;

      ERROR4:
	i2c_detach_client(new_client);
      ERROR3:
	release_region(address, SMSC_EXTENT);
	kfree(data);
	return err;
}

static int smsc47m1_detach_client(struct i2c_client *client)
{
	int err;

	i2c_deregister_entry(((struct smsc47m1_data *) (client->data))->
				 sysctl_id);

	if ((err = i2c_detach_client(client))) {
		printk
		    ("smsc47m1.o: Client deregistration failed, client not detached.\n");
		return err;
	}

	release_region(client->addr, SMSC_EXTENT);
	kfree(client->data);

	return 0;
}

static int smsc47m1_read_value(struct i2c_client *client, u8 reg)
{
	int res;

	down(&(((struct smsc47m1_data *) (client->data))->lock));
	res = inb_p(client->addr + reg);
	up(&(((struct smsc47m1_data *) (client->data))->lock));
	return res;
}

static int smsc47m1_write_value(struct i2c_client *client, u8 reg, u8 value)
{
	down(&(((struct smsc47m1_data *) (client->data))->lock));
	outb_p(value, client->addr + reg);
	up(&(((struct smsc47m1_data *) (client->data))->lock));
	return 0;
}

static void smsc47m1_init_client(struct i2c_client *client)
{
	/* configure pins for tach function */
	smsc47m1_write_value(client, SMSC47M1_REG_TPIN1, 0x05);
	smsc47m1_write_value(client, SMSC47M1_REG_TPIN2, 0x05);
}

static void smsc47m1_update_client(struct i2c_client *client)
{
	struct smsc47m1_data *data = client->data;
	int i;

	down(&data->update_lock);

	if ((jiffies - data->last_updated > HZ + HZ / 2) ||
	    (jiffies < data->last_updated) || !data->valid) {
		for (i = 1; i <= 2; i++) {
			data->fan[i - 1] =
			    smsc47m1_read_value(client, SMSC47M1_REG_FAN(i));
			data->fan_min[i - 1] =
			    smsc47m1_read_value(client, SMSC47M1_REG_FAN_MIN(i));
			data->pwm[i - 1] =
			    smsc47m1_read_value(client, SMSC47M1_REG_PWM(i));
		}

		i = smsc47m1_read_value(client, SMSC47M1_REG_FANDIV);
		data->fan_div[0] = (i >> 4) & 0x03;
		data->fan_div[1] = i >> 6;
		data->alarms =
		        smsc47m1_read_value(client, SMSC47M1_REG_ALARM1) >> 6;
		if(data->alarms)
			smsc47m1_write_value(client, SMSC47M1_REG_ALARM1, 0xc0);
		data->last_updated = jiffies;
		data->valid = 1;
	}

	up(&data->update_lock);
}


void smsc47m1_fan(struct i2c_client *client, int operation, int ctl_name,
		 int *nrels_mag, long *results)
{
	struct smsc47m1_data *data = client->data;
	int nr = ctl_name - SMSC47M1_SYSCTL_FAN1 + 1;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		smsc47m1_update_client(client);
		results[0] = MIN_FROM_REG(data->fan_min[nr - 1],
					  DIV_FROM_REG(data->fan_div[nr - 1]));
		results[1] = FAN_FROM_REG(data->fan[nr - 1],
					  DIV_FROM_REG(data->fan_div[nr - 1]),
		                          data->fan_min[nr - 1]);
		*nrels_mag = 2;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->fan_min[nr - 1] = MIN_TO_REG(results[0],
							   DIV_FROM_REG
							   (data->
							    fan_div[nr-1]));
			smsc47m1_write_value(client, SMSC47M1_REG_FAN_MIN(nr),
					    data->fan_min[nr - 1]);
		}
	}
}


void smsc47m1_alarms(struct i2c_client *client, int operation, int ctl_name,
		    int *nrels_mag, long *results)
{
	struct smsc47m1_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		smsc47m1_update_client(client);
		results[0] = data->alarms;
		*nrels_mag = 1;
	}
}

void smsc47m1_fan_div(struct i2c_client *client, int operation,
		     int ctl_name, int *nrels_mag, long *results)
{
	struct smsc47m1_data *data = client->data;
	int old;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		smsc47m1_update_client(client);
		results[0] = DIV_FROM_REG(data->fan_div[0]);
		results[1] = DIV_FROM_REG(data->fan_div[1]);
		*nrels_mag = 2;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		old = smsc47m1_read_value(client, SMSC47M1_REG_FANDIV);
		if (*nrels_mag >= 2) {
			data->fan_div[1] = DIV_TO_REG(results[1]);
			old = (old & 0x3f) | (data->fan_div[1] << 6);
		}
		if (*nrels_mag >= 1) {
			data->fan_div[0] = DIV_TO_REG(results[0]);
			old = (old & 0xcf) | (data->fan_div[0] << 4);
			smsc47m1_write_value(client, SMSC47M1_REG_FANDIV, old);
		}
	}
}

void smsc47m1_pwm(struct i2c_client *client, int operation, int ctl_name,
		 int *nrels_mag, long *results)
{
	struct smsc47m1_data *data = client->data;
	int nr = 1 + ctl_name - SMSC47M1_SYSCTL_PWM1;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		smsc47m1_update_client(client);
		results[0] = PWM_FROM_REG(data->pwm[nr - 1]);
		results[1] = data->pwm[nr - 1] & 0x01;
		*nrels_mag = 2;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->pwm[nr - 1] &= 0x81;
			data->pwm[nr - 1] |= PWM_TO_REG(results[0]);
			if (*nrels_mag >= 2) {
				if(results[1] && (data->pwm[nr-1] & 0x01)) {
					/* enable PWM */
/* hope BIOS did it already
					smsc47m1_write_value(client,
					          SMSC47M1_REG_PPIN(nr), 0x04);
*/
					data->pwm[nr - 1] &= 0xfe;
				} else if((!results[1]) && (!(data->pwm[nr-1] & 0x01))) {
					/* disable PWM */
					data->pwm[nr - 1] |= 0x01;
				}
			}
			smsc47m1_write_value(client, SMSC47M1_REG_PWM(nr),
					     data->pwm[nr - 1]);
		}
	}
}

static int __init sm_smsc47m1_init(void)
{
	int addr;

	printk("smsc47m1.o version %s (%s)\n", LM_VERSION, LM_DATE);

	if (smsc47m1_find(&addr)) {
		printk("smsc47m1.o: SMSC 47M1xx not detected, module not inserted.\n");
		return -ENODEV;
	}
	normal_isa[0] = addr;

	return i2c_add_driver(&smsc47m1_driver);
}

static void __exit sm_smsc47m1_exit(void)
{
	i2c_del_driver(&smsc47m1_driver);
}



MODULE_AUTHOR("Mark D. Studebaker <mdsxyz123@yahoo.com>");
MODULE_DESCRIPTION("SMSC 47M1xx Fan sensors");
MODULE_LICENSE("GPL");

module_init(sm_smsc47m1_init);
module_exit(sm_smsc47m1_exit);
