/*
    vt1211.c - Part of lm_sensors, Linux kernel modules
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

/* Supports VIA VT1211 Super I/O sensors via ISA (LPC) accesses only. */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/i2c.h>
#include <linux/i2c-proc.h>
#include <linux/init.h>
#include <asm/io.h>
#include "version.h"
#include "sensors_vid.h"

static int force_addr = 0;
MODULE_PARM(force_addr, "i");
MODULE_PARM_DESC(force_addr,
		 "Initialize the base address of the sensors");

static unsigned short normal_i2c[] = { SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { SENSORS_I2C_END };
static unsigned int normal_isa[] = { 0x0000, SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

SENSORS_INSMOD_1(vt1211);

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

#define VT1211_DEVID 0x3c
#define VT1211_ACT_REG 0x30
#define VT1211_BASE_REG 0x60

#define VT1211_EXTENT 0x80

/* pwm numbered 1-2 */
#define VT1211_REG_PWM(nr) (0x5f + (nr))
#define VT1211_REG_PWM_CTL 0x51

/* The VT1211 registers */
/* We define the sensors as follows. Somewhat convoluted to minimize
   changes from via686a.
	Sensor		Voltage Mode	Temp Mode
	--------	------------	---------
	Reading 1			temp1   Intel thermal diode
	Reading 3			temp2   VT1211 internal thermal diode
	UCH1/Reading2	in0		temp3
	UCH2		in1		temp4
	UCH3		in2		temp5
	UCH4		in3		temp6
	UCH5		in4		temp7
	3.3V		in5
*/

/* ins numbered 0-5 */
#define VT1211_REG_IN_MAX(nr) ((nr)==0 ? 0x3d : 0x29 + ((nr) * 2))
#define VT1211_REG_IN_MIN(nr) ((nr)==0 ? 0x3e : 0x2a + ((nr) * 2))
#define VT1211_REG_IN(nr)     (0x21 + (nr))

/* fans numbered 1-2 */
#define VT1211_REG_FAN_MIN(nr) (0x3a + (nr))
#define VT1211_REG_FAN(nr)     (0x28 + (nr))

static const u8 regtemp[] = { 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25 };
static const u8 regover[] = { 0x39, 0x1d, 0x3d, 0x2b, 0x2d, 0x2f, 0x31 };
static const u8 reghyst[] = { 0x3a, 0x1e, 0x3e, 0x2c, 0x2e, 0x30, 0x32 };

/* temps numbered 1-7 */
#define VT1211_REG_TEMP(nr)		(regtemp[(nr) - 1])
#define VT1211_REG_TEMP_OVER(nr)	(regover[(nr) - 1])
#define VT1211_REG_TEMP_HYST(nr)	(reghyst[(nr) - 1])
#define VT1211_REG_TEMP_LOW1	0x4b	/* bits 7-6 */
#define VT1211_REG_TEMP_LOW3	0x49	/* bits 5-4 */
#define VT1211_REG_TEMP_LOW47	0x4d

#define VT1211_REG_CONFIG 0x40
#define VT1211_REG_ALARM1 0x41
#define VT1211_REG_ALARM2 0x42
#define VT1211_REG_VID    0x45
#define VT1211_REG_FANDIV 0x47
#define VT1211_REG_UCH_CONFIG 0x4a
#define VT1211_REG_TEMP1_CONFIG 0x4b
#define VT1211_REG_TEMP2_CONFIG 0x4c

/* temps 1-7; voltages 0-5 */
#define ISTEMP(i, ch_config) ((i) <= 2 ? 1 : \
			      ((ch_config) >> ((i)-1)) & 1)
#define ISVOLT(i, ch_config) ((i) > 4 ? 1 : !(((ch_config) >> ((i)+2)) & 0x01))

#define DIV_FROM_REG(val) (1 << (val))
#define DIV_TO_REG(val) ((val)==8?3:(val)==4?2:(val)==1?0:1)
#define PWM_FROM_REG(val) (val)
#define PWM_TO_REG(val) SENSORS_LIMIT((val), 0, 255)

/* temp1 (i = 1) is an Intel thermal diode which is scaled in user space.
   temp2 (i = 2) is the internal temp diode so it's scaled in the driver
   according to some measurements taken on an EPIA M10000.
   temp3-7 are thermistor based so the driver returns the voltage measured at
   the pin (range 0V - 2.2V). */
#define TEMP_FROM_REG(i, reg)	((i) == 1 ? (reg) * 10 : \
				 (i) == 2 ? (reg) < 51 ? 0 : \
				 ((reg) - 51) * 10 : \
				 ((253 - (reg)) * 2200 + 105) / 210)
/* for 10-bit temp values */
#define TEMP_FROM_REG10(i, reg)	((i) == 1 ? (reg) * 10 / 4 : \
				 (i) == 2 ? (reg) < 204 ? 0 : \
				 ((reg) - 204) * 10 / 4 : \
				 ((1012 - (reg)) * 550 + 105) / 210)
#define TEMP_TO_REG(i, val)	SENSORS_LIMIT( \
				 ((i) == 1 ? ((val) + 5) / 10 : \
				  (i) == 2 ? ((val) + 5) / 10 + 51 : \
				  253 - ((val) * 210 + 1100) / 2200), 0, 255)

/* in5 (i = 5) is special. It's the internal 3.3V so it's scaled in the
   driver according to the VT1211 BIOS porting guide */
#define IN_FROM_REG(i, reg)	((reg) < 3 ? 0 : (i) == 5 ? \
				 (((reg) - 3) * 15882 + 4790) / 9580 : \
				 (((reg) - 3) * 10000 + 4790) / 9580)
#define IN_TO_REG(i, val)	(SENSORS_LIMIT((i) == 5 ? \
				 ((val) * 9580 + 7941) / 15882 + 3 : \
				 ((val) * 9580 + 5000) / 10000 + 3, 0, 255))

/********* FAN RPM CONVERSIONS ********/
/* But this chip saturates back at 0, not at 255 like all the other chips.
   So, 0 means 0 RPM */
static inline u8 FAN_TO_REG(long rpm, int div)
{
	if (rpm == 0)
		return 0;
	rpm = SENSORS_LIMIT(rpm, 1, 1000000);
	return SENSORS_LIMIT((1310720 + rpm * div / 2) / (rpm * div), 1, 255);
}

#define MIN_TO_REG(a,b) FAN_TO_REG(a,b)
#define FAN_FROM_REG(val,div) ((val)==0?0:(val)==255?0:1310720/((val)*(div)))

struct vt1211_data {
	struct i2c_client client;
	struct semaphore lock;
	int sysctl_id;

	struct semaphore update_lock;
	char valid;		/* !=0 if following fields are valid */
	unsigned long last_updated;	/* In jiffies */

	u8 in[6];		/* Register value */
	u8 in_max[6];		/* Register value */
	u8 in_min[6];		/* Register value */
	u16 temp[7];		/* Register value 10 bit */
	u8 temp_over[7];	/* Register value */
	u8 temp_hyst[7];	/* Register value */
	u8 fan[2];		/* Register value */
	u8 fan_min[2];		/* Register value */
	u8 fan_div[2];		/* Register encoding, shifted right */
	u16 alarms;		/* Register encoding */
	u8 pwm[2];		/* Register value */
	u8 pwm_ctl;		/* Register value */
	u8 vid;			/* Register encoding */
	u8 vrm;
	u8 uch_config;
};

static int vt1211_attach_adapter(struct i2c_adapter *adapter);
static int vt1211_detect(struct i2c_adapter *adapter, int address,
			  unsigned short flags, int kind);
static int vt1211_detach_client(struct i2c_client *client);

static inline int vt_rdval(struct i2c_client *client, u8 reg);
static inline void vt1211_write_value(struct i2c_client *client, u8 reg,
			       u8 value);
static void vt1211_update_client(struct i2c_client *client);
static void vt1211_init_client(struct i2c_client *client);


static void vt1211_fan(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void vt1211_alarms(struct i2c_client *client, int operation,
			   int ctl_name, int *nrels_mag, long *results);
static void vt1211_fan_div(struct i2c_client *client, int operation,
			    int ctl_name, int *nrels_mag, long *results);
static void vt1211_in(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void vt1211_pwm(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void vt1211_vid(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void vt1211_vrm(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void vt1211_uch(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void vt1211_temp(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);

static struct i2c_driver vt1211_driver = {
	.name		= "VT1211 sensors driver",
	.id		= I2C_DRIVERID_VT1211,
	.flags		= I2C_DF_NOTIFY,
	.attach_adapter	= vt1211_attach_adapter,
	.detach_client	= vt1211_detach_client,
};

/* -- SENSORS SYSCTL START -- */
#define VT1211_SYSCTL_IN0 1000
#define VT1211_SYSCTL_IN1 1001
#define VT1211_SYSCTL_IN2 1002
#define VT1211_SYSCTL_IN3 1003
#define VT1211_SYSCTL_IN4 1004
#define VT1211_SYSCTL_IN5 1005
#define VT1211_SYSCTL_FAN1 1101
#define VT1211_SYSCTL_FAN2 1102
#define VT1211_SYSCTL_TEMP1 1200
#define VT1211_SYSCTL_TEMP2 1201
#define VT1211_SYSCTL_TEMP3 1202
#define VT1211_SYSCTL_TEMP4 1203
#define VT1211_SYSCTL_TEMP5 1204
#define VT1211_SYSCTL_TEMP6 1205
#define VT1211_SYSCTL_TEMP7 1206
#define VT1211_SYSCTL_VID	1300
#define VT1211_SYSCTL_PWM1	1401
#define VT1211_SYSCTL_PWM2	1402
#define VT1211_SYSCTL_VRM	1600
#define VT1211_SYSCTL_UCH	1700
#define VT1211_SYSCTL_FAN_DIV 2000
#define VT1211_SYSCTL_ALARMS 2001

#define VT1211_ALARM_IN1 0x01
#define VT1211_ALARM_IN2 0x02
#define VT1211_ALARM_IN5 0x04
#define VT1211_ALARM_IN3 0x08
#define VT1211_ALARM_TEMP1 0x10
#define VT1211_ALARM_FAN1 0x40
#define VT1211_ALARM_FAN2 0x80
#define VT1211_ALARM_IN4 0x100
#define VT1211_ALARM_TEMP3 0x800
#define VT1211_ALARM_CHAS 0x1000
#define VT1211_ALARM_TEMP2 0x8000
/* duplicates */
#define VT1211_ALARM_IN0 VT1211_ALARM_TEMP3
#define VT1211_ALARM_TEMP4 VT1211_ALARM_IN1
#define VT1211_ALARM_TEMP5 VT1211_ALARM_IN2
#define VT1211_ALARM_TEMP6 VT1211_ALARM_IN3
#define VT1211_ALARM_TEMP7 VT1211_ALARM_IN4

/* -- SENSORS SYSCTL END -- */

static ctl_table vt1211_dir_table_template[] = {
	{VT1211_SYSCTL_IN0, "in0", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &vt1211_in},
	{VT1211_SYSCTL_IN1, "in1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &vt1211_in},
	{VT1211_SYSCTL_IN2, "in2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &vt1211_in},
	{VT1211_SYSCTL_IN3, "in3", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &vt1211_in},
	{VT1211_SYSCTL_IN4, "in4", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &vt1211_in},
	{VT1211_SYSCTL_IN5, "in5", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &vt1211_in},
	{VT1211_SYSCTL_TEMP1, "temp1", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &vt1211_temp},
	{VT1211_SYSCTL_TEMP2, "temp2", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &vt1211_temp},
	{VT1211_SYSCTL_TEMP3, "temp3", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &vt1211_temp},
	{VT1211_SYSCTL_TEMP4, "temp4", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &vt1211_temp},
	{VT1211_SYSCTL_TEMP5, "temp5", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &vt1211_temp},
	{VT1211_SYSCTL_TEMP6, "temp6", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &vt1211_temp},
	{VT1211_SYSCTL_TEMP7, "temp7", NULL, 0, 0644, NULL,
	 &i2c_proc_real, &i2c_sysctl_real, NULL, &vt1211_temp},
	{VT1211_SYSCTL_FAN1, "fan1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &vt1211_fan},
	{VT1211_SYSCTL_FAN2, "fan2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &vt1211_fan},
	{VT1211_SYSCTL_FAN_DIV, "fan_div", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &vt1211_fan_div},
	{VT1211_SYSCTL_ALARMS, "alarms", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &vt1211_alarms},
	{VT1211_SYSCTL_PWM1, "pwm1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &vt1211_pwm},
	{VT1211_SYSCTL_PWM2, "pwm2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &vt1211_pwm},
	{VT1211_SYSCTL_VID, "vid", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &vt1211_vid},
	{VT1211_SYSCTL_VRM, "vrm", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &vt1211_vrm},
	{VT1211_SYSCTL_UCH, "uch_config", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &vt1211_uch},
	{0}
};

static int vt1211_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_detect(adapter, &addr_data, vt1211_detect);
}

static int __init vt1211_find(int *address)
{
	u16 val;

	superio_enter();
	val= superio_inb(DEVID);
	if(VT1211_DEVID != val) {
		superio_exit();
		return -ENODEV;
	}

	superio_select();
	val = (superio_inb(VT1211_BASE_REG) << 8) |
	       superio_inb(VT1211_BASE_REG + 1);
	*address = val & ~(VT1211_EXTENT - 1);
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

int vt1211_detect(struct i2c_adapter *adapter, int address,
		   unsigned short flags, int kind)
{
	int i;
	struct i2c_client *new_client;
	struct vt1211_data *data;
	int err = 0;
	u8 val;
	const char *type_name = "vt1211";
	const char *client_name = "VT1211 chip";

	if (!i2c_is_isa_adapter(adapter)) {
		return 0;
	}

	if(force_addr)
		address = force_addr & ~(VT1211_EXTENT - 1);
	if (check_region(address, VT1211_EXTENT)) {
		printk("vt1211.o: region 0x%x already in use!\n", address);
		return -ENODEV;
	}
	if(force_addr) {
		printk("vt1211.o: forcing ISA address 0x%04X\n", address);
		superio_enter();
		superio_select();
		superio_outb(VT1211_BASE_REG, address >> 8);
		superio_outb(VT1211_BASE_REG+1, address & 0xff);
		superio_exit();
	}

	superio_enter();
	superio_select();
	if((val = 0x01 & superio_inb(VT1211_ACT_REG)) == 0)
		superio_outb(VT1211_ACT_REG, 1);
	superio_exit();

	if (!(data = kmalloc(sizeof(struct vt1211_data), GFP_KERNEL))) {
		return -ENOMEM;
	}

	new_client = &data->client;
	new_client->addr = address;
	init_MUTEX(&data->lock);
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &vt1211_driver;
	new_client->flags = 0;

	request_region(address, VT1211_EXTENT, "vt1211-sensors");
	strcpy(new_client->name, client_name);
	data->valid = 0;
	init_MUTEX(&data->update_lock);

	if ((err = i2c_attach_client(new_client)))
		goto ERROR3;

	if ((i = i2c_register_entry((struct i2c_client *) new_client,
					type_name,
					vt1211_dir_table_template,
					THIS_MODULE)) < 0) {
		err = i;
		goto ERROR4;
	}
	data->sysctl_id = i;

	vt1211_init_client(new_client);
	return 0;

      ERROR4:
	i2c_detach_client(new_client);
      ERROR3:
	release_region(address, VT1211_EXTENT);
	kfree(data);
	return err;
}

static int vt1211_detach_client(struct i2c_client *client)
{
	int err;

	i2c_deregister_entry(((struct vt1211_data *) (client->data))->
				 sysctl_id);

	if ((err = i2c_detach_client(client))) {
		printk
		    ("vt1211.o: Client deregistration failed, client not detached.\n");
		return err;
	}

	release_region(client->addr, VT1211_EXTENT);
	kfree(client->data);

	return 0;
}

static inline int vt_rdval(struct i2c_client *client, u8 reg)
{
	return (inb_p(client->addr + reg));
}

static inline void vt1211_write_value(struct i2c_client *client, u8 reg, u8 value)
{
	outb_p(value, client->addr + reg);
}

static void vt1211_init_client(struct i2c_client *client)
{
	struct vt1211_data *data = client->data;

	data->vrm = 91;
	/* set "default" interrupt mode for alarms, which isn't the default */
	vt1211_write_value(client, VT1211_REG_TEMP1_CONFIG, 0);
	vt1211_write_value(client, VT1211_REG_TEMP2_CONFIG, 0);
}

static void vt1211_update_client(struct i2c_client *client)
{
	struct vt1211_data *data = client->data;
	int i, j;

	down(&data->update_lock);

	if ((jiffies - data->last_updated > HZ + HZ / 2) ||
	    (jiffies < data->last_updated) || !data->valid) {
		data->uch_config = vt_rdval(client, VT1211_REG_UCH_CONFIG);
		for (i = 0; i <= 5; i++) {
			if(ISVOLT(i, data->uch_config)) {
				data->in[i] = vt_rdval(client, VT1211_REG_IN(i));
				data->in_min[i] = vt_rdval(client,
				                        VT1211_REG_IN_MIN(i));
				data->in_max[i] = vt_rdval(client,
				                        VT1211_REG_IN_MAX(i));
			} else {
				data->in[i] = 0;
				data->in_min[i] = 0;
				data->in_max[i] = 0;
			}
		}
		for (i = 1; i <= 2; i++) {
			data->fan[i - 1] = vt_rdval(client, VT1211_REG_FAN(i));
			data->fan_min[i - 1] = vt_rdval(client,
						     VT1211_REG_FAN_MIN(i));
		}
		for (i = 1; i <= 7; i++) {
			if(ISTEMP(i, data->uch_config)) {
				data->temp[i - 1] = vt_rdval(client,
					             VT1211_REG_TEMP(i)) << 2;
				switch(i) {
					case 1:
						j = (vt_rdval(client,
						  VT1211_REG_TEMP_LOW1) >>
							6) & 3;
						break;
					case 2:
						j = 0;
						break;
					case 3:
						j = (vt_rdval(client,
						  VT1211_REG_TEMP_LOW3) >>
							4) & 3;
						break;
					default:
						j = (vt_rdval(client,
						  VT1211_REG_TEMP_LOW47) >>
						            ((i-4)*2)) & 0x03;	
				}
				data->temp[i - 1] |= j;
				data->temp_over[i - 1] = vt_rdval(client,
					              VT1211_REG_TEMP_OVER(i));
				data->temp_hyst[i - 1] = vt_rdval(client,
					              VT1211_REG_TEMP_HYST(i));
			} else {
				data->temp[i - 1] = 0;
				data->temp_over[i - 1] = 0;
				data->temp_hyst[i - 1] = 0;
			}
		}

		for (i = 1; i <= 2; i++) {
			data->fan[i - 1] = vt_rdval(client, VT1211_REG_FAN(i));
			data->fan_min[i - 1] = vt_rdval(client,
			                                VT1211_REG_FAN_MIN(i));
			data->pwm[i - 1] = vt_rdval(client, VT1211_REG_PWM(i));
		}

		data->pwm_ctl = vt_rdval(client, VT1211_REG_PWM_CTL);
		i = vt_rdval(client, VT1211_REG_FANDIV);
		data->fan_div[0] = (i >> 4) & 0x03;
		data->fan_div[1] = i >> 6;
		data->alarms = vt_rdval(client, VT1211_REG_ALARM1) |
		                    (vt_rdval(client, VT1211_REG_ALARM2) << 8);
		data->vid= vt_rdval(client, VT1211_REG_VID) & 0x1f;
		data->last_updated = jiffies;
		data->valid = 1;
	}

	up(&data->update_lock);
}


void vt1211_in(struct i2c_client *client, int operation, int ctl_name,
		int *nrels_mag, long *results)
{
	struct vt1211_data *data = client->data;
	int nr = ctl_name - VT1211_SYSCTL_IN0;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 2;
	else if (operation == SENSORS_PROC_REAL_READ) {
		vt1211_update_client(client);
		results[0] = IN_FROM_REG(nr, data->in_min[nr]);
		results[1] = IN_FROM_REG(nr, data->in_max[nr]);
		results[2] = IN_FROM_REG(nr, data->in[nr]);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->in_min[nr] = IN_TO_REG(nr, results[0]);
			vt1211_write_value(client, VT1211_REG_IN_MIN(nr),
					    data->in_min[nr]);
		}
		if (*nrels_mag >= 2) {
			data->in_max[nr] = IN_TO_REG(nr, results[1]);
			vt1211_write_value(client, VT1211_REG_IN_MAX(nr),
					    data->in_max[nr]);
		}
	}
}

void vt1211_fan(struct i2c_client *client, int operation, int ctl_name,
		 int *nrels_mag, long *results)
{
	struct vt1211_data *data = client->data;
	int nr = ctl_name - VT1211_SYSCTL_FAN1 + 1;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		vt1211_update_client(client);
		results[0] = FAN_FROM_REG(data->fan_min[nr - 1],
					  DIV_FROM_REG(data->fan_div
						       [nr - 1]));
		results[1] = FAN_FROM_REG(data->fan[nr - 1],
				 DIV_FROM_REG(data->fan_div[nr - 1]));
		*nrels_mag = 2;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->fan_min[nr - 1] = MIN_TO_REG(results[0],
							   DIV_FROM_REG
							   (data->
							    fan_div[nr-1]));
			vt1211_write_value(client, VT1211_REG_FAN_MIN(nr),
					    data->fan_min[nr - 1]);
		}
	}
}


void vt1211_temp(struct i2c_client *client, int operation, int ctl_name,
		  int *nrels_mag, long *results)
{
	struct vt1211_data *data = client->data;
	int nr = ctl_name - VT1211_SYSCTL_TEMP1;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = nr < 2 ? 1 : 3;
	else if (operation == SENSORS_PROC_REAL_READ) {
		vt1211_update_client(client);
		results[0] = TEMP_FROM_REG(nr + 1, data->temp_over[nr]);
		results[1] = TEMP_FROM_REG(nr + 1, data->temp_hyst[nr]);
		results[2] = TEMP_FROM_REG10(nr + 1, data->temp[nr]);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->temp_over[nr] = TEMP_TO_REG(nr + 1, results[0]);
			vt1211_write_value(client,
					    VT1211_REG_TEMP_OVER(nr + 1),
					    data->temp_over[nr]);
		}
		if (*nrels_mag >= 2) {
			data->temp_hyst[nr] = TEMP_TO_REG(nr + 1, results[1]);
			vt1211_write_value(client,
					    VT1211_REG_TEMP_HYST(nr + 1),
					    data->temp_hyst[nr]);
		}
	}
}

void vt1211_alarms(struct i2c_client *client, int operation, int ctl_name,
		    int *nrels_mag, long *results)
{
	struct vt1211_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		vt1211_update_client(client);
		results[0] = data->alarms;
		*nrels_mag = 1;
	}
}

void vt1211_fan_div(struct i2c_client *client, int operation,
		     int ctl_name, int *nrels_mag, long *results)
{
	struct vt1211_data *data = client->data;
	int old;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		vt1211_update_client(client);
		results[0] = DIV_FROM_REG(data->fan_div[0]);
		results[1] = DIV_FROM_REG(data->fan_div[1]);
		*nrels_mag = 2;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		old = vt_rdval(client, VT1211_REG_FANDIV);
		if (*nrels_mag >= 2) {
			data->fan_div[1] = DIV_TO_REG(results[1]);
			old = (old & 0x3f) | (data->fan_div[1] << 6);
		}
		if (*nrels_mag >= 1) {
			data->fan_div[0] = DIV_TO_REG(results[0]);
			old = (old & 0xcf) | (data->fan_div[0] << 4);
			vt1211_write_value(client, VT1211_REG_FANDIV, old);
		}
	}
}

void vt1211_pwm(struct i2c_client *client, int operation, int ctl_name,
		 int *nrels_mag, long *results)
{
	struct vt1211_data *data = client->data;
	int nr = 1 + ctl_name - VT1211_SYSCTL_PWM1;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		vt1211_update_client(client);
		results[0] = PWM_FROM_REG(data->pwm[nr - 1]);
		results[1] = (data->pwm_ctl >> (3 + (4 * (nr - 1)))) & 1;
		*nrels_mag = 2;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->pwm[nr - 1] = PWM_TO_REG(results[0]);
			if (*nrels_mag >= 2) {
				if(results[1]) {
					data->pwm_ctl |=
					          (0x08 << (4 * (nr - 1)));
					vt1211_write_value(client,
					                   VT1211_REG_PWM_CTL, 
				                           data->pwm_ctl);
				} else {
					data->pwm_ctl &=
					        ~ (0x08 << (4 * (nr - 1)));
					vt1211_write_value(client,
					                   VT1211_REG_PWM_CTL, 
				                           data->pwm_ctl);
				}
			}
			vt1211_write_value(client, VT1211_REG_PWM(nr),
					    data->pwm[nr - 1]);
		}
	}
}

void vt1211_vid(struct i2c_client *client, int operation, int ctl_name,
		 int *nrels_mag, long *results)
{
	struct vt1211_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 3;
	else if (operation == SENSORS_PROC_REAL_READ) {
		vt1211_update_client(client);
		results[0] = vid_from_reg(data->vid, data->vrm);
		*nrels_mag = 1;
	}
}

void vt1211_vrm(struct i2c_client *client, int operation, int ctl_name,
		 int *nrels_mag, long *results)
{
	struct vt1211_data *data = client->data;
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

void vt1211_uch(struct i2c_client *client, int operation, int ctl_name,
		 int *nrels_mag, long *results)
{
	struct vt1211_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		results[0] = data->uch_config & 0x7c;
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->uch_config = (data->uch_config & 0x83)|(results[0] & 0x7c);
			vt1211_write_value(client, VT1211_REG_UCH_CONFIG,
			                   data->uch_config);
		}
	}
}

static int __init sm_vt1211_init(void)
{
	int addr;

	printk("vt1211.o version %s (%s)\n", LM_VERSION, LM_DATE);

	if (vt1211_find(&addr)) {
		printk("vt1211.o: VT1211 not detected, module not inserted.\n");
		return -ENODEV;
	}
	normal_isa[0] = addr;

	return i2c_add_driver(&vt1211_driver);
}

static void __exit sm_vt1211_exit(void)
{
	i2c_del_driver(&vt1211_driver);
}



MODULE_AUTHOR("Mark D. Studebaker <mdsxyz123@yahoo.com>");
MODULE_DESCRIPTION("VT1211 sensors");
MODULE_LICENSE("GPL");

module_init(sm_vt1211_init);
module_exit(sm_vt1211_exit);
