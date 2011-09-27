/*
    it87.c - Part of lm_sensors, Linux kernel modules for hardware
             monitoring.

    Supports: IT8705F  Super I/O chip w/LPC interface
              IT8712F  Super I/O chip w/LPC interface & SMBus
              SiS950   A clone of the IT8705F

    Copyright (c) 2001 Chris Gauthron
    Largely inspired by lm78.c of the same package

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
    djg@pdp8.net David Gesswein 7/18/01
    Modified to fix bug with not all alarms enabled.
    Added ability to read battery voltage and select temperature sensor
    type at module load time.
*/

/*
    michael.hufer@gmx.de Michael Hufer 09/07/03
    Modified configure (enable/disable) chip reset at module load time.
    Added ability to read and set fan pwm registers and the smart
    guardian (sg) features of the chip.
*/

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/i2c.h>
#include <linux/i2c-proc.h>
#include <linux/init.h>
#include <asm/io.h>
#include "version.h"

MODULE_LICENSE("GPL");

/* Addresses to scan */
static unsigned short normal_i2c[] = { SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { 0x28, 0x2f, SENSORS_I2C_END };
static unsigned int normal_isa[] = { 0x0290, SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/* Insmod parameters */
SENSORS_INSMOD_2(it87, it8712);


#define	REG	0x2e	/* The register to read/write */
#define	DEV	0x07	/* Register: Logical device select */
#define	VAL	0x2f	/* The value to read/write */
#define PME	0x04	/* The device with the fan registers in it */
#define	DEVID	0x20	/* Register: Device ID */
#define	DEVREV	0x22	/* Register: Device Revision */

static inline int
superio_inb(int reg)
{
	outb(reg, REG);
	return inb(VAL);
}

static int superio_inw(int reg)
{
	int val;
	outb(reg++, REG);
	val = inb(VAL) << 8;
	outb(reg, REG);
	val |= inb(VAL);
	return val;
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
	outb(0x01, REG);
	outb(0x55, REG);
	outb(0x55, REG);
}

static inline void
superio_exit(void)
{
	outb(0x02, REG);
	outb(0x02, VAL);
}

#define IT87_DEVID_MATCH(id) ((id) == 0x8712 || (id) == 0x8705)

#define IT87_ACT_REG  0x30
#define IT87_BASE_REG 0x60

/* Update battery voltage after every reading if true */
static int update_vbat = 0;

/* Reset the registers on init */
static int reset = 0;

/* Many IT87 constants specified below */

/* Length of ISA address segment */
#define IT87_EXTENT 8

/* Where are the ISA address/data registers relative to the base address */
#define IT87_ADDR_REG_OFFSET 5
#define IT87_DATA_REG_OFFSET 6

/*----- The IT87 registers -----*/

#define IT87_REG_CONFIG        0x00

#define IT87_REG_ALARM1        0x01
#define IT87_REG_ALARM2        0x02
#define IT87_REG_ALARM3        0x03

#define IT87_REG_VID           0x0a
#define IT87_REG_FAN_DIV       0x0b

#define IT87_REG_FAN(nr)       (0x0c + (nr))
#define IT87_REG_FAN_MIN(nr)   (0x0f + (nr))
#define IT87_REG_FAN_CTRL      0x13

/* pwm and smart guardian registers */

#define IT87_REG_FAN_ONOFF     0x14
#define IT87_REG_PWM(nr)       (0x14 + (nr))
#define IT87_REG_SG_TL_OFF(nr) (0x58 + (nr)*8)
#define IT87_REG_SG_TL_LOW(nr) (0x59 + (nr)*8)
#define IT87_REG_SG_TL_MED(nr) (0x5a + (nr)*8)
#define IT87_REG_SG_TL_HI(nr)  (0x5b + (nr)*8)
#define IT87_REG_SG_TL_OVR(nr) (0x5c + (nr)*8)
#define IT87_REG_SG_PWM_LOW(nr) (0x5d + (nr)*8)
#define IT87_REG_SG_PWM_MED(nr) (0x5e + (nr)*8)
#define IT87_REG_SG_PWM_HI(nr)  (0x5f + (nr)*8)

/* Monitors: 9 voltage (0 to 7, battery), 3 temp (1 to 3), 3 fan (1 to 3) */

#define IT87_REG_VIN(nr)       (0x20 + (nr))
#define IT87_REG_TEMP(nr)      (0x28 + (nr))

#define IT87_REG_VIN_MAX(nr)   (0x30 + (nr) * 2)
#define IT87_REG_VIN_MIN(nr)   (0x31 + (nr) * 2)
#define IT87_REG_TEMP_HIGH(nr) (0x3e + (nr) * 2)
#define IT87_REG_TEMP_LOW(nr)  (0x3f + (nr) * 2)

#define IT87_REG_I2C_ADDR      0x48

#define IT87_REG_VIN_ENABLE    0x50
#define IT87_REG_TEMP_ENABLE   0x51

#define IT87_REG_CHIPID        0x58
#define IT87_REG_CHIPID2       0x5b /* IT8712F only */

/* sensor pin types */
#define UNUSED		0
#define THERMISTOR	2
#define PIIDIODE	3

/* Conversions. Limit checking is only done on the TO_REG 
   variants. Note that you should be a bit careful with which arguments
   these macros are called: arguments may be evaluated more than once.
   Fixing this is just not worth it. */
#define IN_TO_REG(val)  (SENSORS_LIMIT((((val) * 10 + 8)/16),0,255))
#define IN_FROM_REG(val) (((val) *  16 + 5) / 10)

static inline u8 FAN_TO_REG(long rpm, int div)
{
	if (rpm == 0)
		return 255;
	rpm = SENSORS_LIMIT(rpm, 1, 1000000);
	return SENSORS_LIMIT((1350000 + rpm * div / 2) / (rpm * div), 1,
			     254);
}

#define FAN_FROM_REG(val,div) ((val)==0?-1:(val)==255?0:1350000/((val)*(div)))

#define TEMP_TO_REG(val) (SENSORS_LIMIT(((val)<0?(((val)-5)/10):\
                                                 ((val)+5)/10), -128, 127))
#define TEMP_FROM_REG(val) ((val) * 10)

#define VID_FROM_REG(val) ((val)==0x1f?0:(val)>=0x10?510-(val)*10:\
                           205-(val)*5)
#define ALARMS_FROM_REG(val) (val)

extern inline u8 DIV_TO_REG(long val)
{
	u8 i;
	for( i = 0; i <= 7; i++ )
	{
		if( val>>i == 1 )
			return i;
	}
	return 1;
}
#define DIV_FROM_REG(val) (1 << (val))

/* For each registered IT87, we need to keep some data in memory. It is
   dynamically allocated, at the same time when a new it87 client is
   allocated. */
struct it87_data {
	struct i2c_client client;
	struct semaphore lock;
	int sysctl_id;
	enum chips type;

	struct semaphore update_lock;
	char valid;		/* !=0 if following fields are valid */
	unsigned long last_updated;	/* In jiffies */

	u8 in[9];		/* Register value */
	u8 in_max[9];		/* Register value */
	u8 in_min[9];		/* Register value */
	u8 fan[3];		/* Register value */
	u8 fan_min[3];		/* Register value */
	s8 temp[3];		/* Register value */
	s8 temp_high[3];	/* Register value */
	s8 temp_low[3];		/* Register value */
	u8 fan_div[3];		/* Register encoding, shifted right */
	u8 vid;			/* Register encoding, combined */
	u32 alarms;		/* Register encoding, combined */
	u8 pwm[3];		/* Register value */
	u8 fan_ctl[2];		/* Register encoding */
	s8 sg_tl[3][5];		/* Register value */
	u8 sg_pwm[3][3];	/* Register value */
	u8 sens[3];		/* 2 = Thermistor,
				   3 = PII/Celeron diode */
};


static int it87_attach_adapter(struct i2c_adapter *adapter);
static int it87_detect(struct i2c_adapter *adapter, int address,
		       unsigned short flags, int kind);
static int it87_detach_client(struct i2c_client *client);

static int it87_read_value(struct i2c_client *client, u8 reg);
static int it87_write_value(struct i2c_client *client, u8 reg,
			    u8 value);
static void it87_update_client(struct i2c_client *client);
static void it87_init_client(struct i2c_client *client);


static void it87_in(struct i2c_client *client, int operation, int ctl_name,
		    int *nrels_mag, long *results);
static void it87_fan(struct i2c_client *client, int operation,
		     int ctl_name, int *nrels_mag, long *results);
static void it87_temp(struct i2c_client *client, int operation,
		      int ctl_name, int *nrels_mag, long *results);
static void it87_vid(struct i2c_client *client, int operation,
		     int ctl_name, int *nrels_mag, long *results);
static void it87_alarms(struct i2c_client *client, int operation,
			    int ctl_name, int *nrels_mag, long *results);
static void it87_fan_div(struct i2c_client *client, int operation,
			 int ctl_name, int *nrels_mag, long *results);
static void it87_fan_ctl(struct i2c_client *client, int operation,
			 int ctl_name, int *nrels_mag, long *results);
static void it87_pwm(struct i2c_client *client, int operation,
			 int ctl_name, int *nrels_mag, long *results);
static void it87_sgpwm(struct i2c_client *client, int operation,
			 int ctl_name, int *nrels_mag, long *results);
static void it87_sgtl(struct i2c_client *client, int operation,
			 int ctl_name, int *nrels_mag, long *results);
static void it87_sens(struct i2c_client *client, int operation,
			 int ctl_name, int *nrels_mag, long *results);

static struct i2c_driver it87_driver = {
	.name		= "IT87xx sensor driver",
	.id		= I2C_DRIVERID_IT87,
	.flags		= I2C_DF_NOTIFY,
	.attach_adapter	= it87_attach_adapter,
	.detach_client	= it87_detach_client,
};

/* The /proc/sys entries */

/* -- SENSORS SYSCTL START -- */
#define IT87_SYSCTL_IN0 1000    /* Volts * 100 */
#define IT87_SYSCTL_IN1 1001
#define IT87_SYSCTL_IN2 1002
#define IT87_SYSCTL_IN3 1003
#define IT87_SYSCTL_IN4 1004
#define IT87_SYSCTL_IN5 1005
#define IT87_SYSCTL_IN6 1006
#define IT87_SYSCTL_IN7 1007
#define IT87_SYSCTL_IN8 1008
#define IT87_SYSCTL_FAN1 1101   /* Rotations/min */
#define IT87_SYSCTL_FAN2 1102
#define IT87_SYSCTL_FAN3 1103
#define IT87_SYSCTL_TEMP1 1200  /* Degrees Celsius * 10 */
#define IT87_SYSCTL_TEMP2 1201  /* Degrees Celsius * 10 */
#define IT87_SYSCTL_TEMP3 1202  /* Degrees Celsius * 10 */
#define IT87_SYSCTL_VID 1300    /* Volts * 100 */
#define IT87_SYSCTL_FAN_DIV 2000        /* 1, 2, 4 or 8 */
#define IT87_SYSCTL_ALARMS 2004    /* bitvector */

#define IT87_SYSCTL_PWM1 1401
#define IT87_SYSCTL_PWM2 1402
#define IT87_SYSCTL_PWM3 1403
#define IT87_SYSCTL_FAN_CTL  1501
#define IT87_SYSCTL_FAN_ON_OFF  1502
#define IT87_SYSCTL_SENS1 1601	/* 1, 2, or Beta (3000-5000) */
#define IT87_SYSCTL_SENS2 1602
#define IT87_SYSCTL_SENS3 1603

#define IT87_ALARM_IN0 0x000100
#define IT87_ALARM_IN1 0x000200
#define IT87_ALARM_IN2 0x000400
#define IT87_ALARM_IN3 0x000800
#define IT87_ALARM_IN4 0x001000
#define IT87_ALARM_IN5 0x002000
#define IT87_ALARM_IN6 0x004000
#define IT87_ALARM_IN7 0x008000
#define IT87_ALARM_FAN1 0x0001
#define IT87_ALARM_FAN2 0x0002
#define IT87_ALARM_FAN3 0x0004
#define IT87_ALARM_FAN4 0x0008
#define IT87_ALARM_FAN5 0x0040
#define IT87_ALARM_TEMP1 0x00010000
#define IT87_ALARM_TEMP2 0x00020000
#define IT87_ALARM_TEMP3 0x00040000

/* -- SENSORS SYSCTL END -- */

/* These files are created for each detected IT87. This is just a template;
   though at first sight, you might think we could use a statically
   allocated list, we need some way to get back to the parent - which
   is done through one of the 'extra' fields which are initialized 
   when a new copy is allocated. */
static ctl_table it87_dir_table_template[] = {
	{IT87_SYSCTL_IN0, "in0", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &it87_in},
	{IT87_SYSCTL_IN1, "in1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &it87_in},
	{IT87_SYSCTL_IN2, "in2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &it87_in},
	{IT87_SYSCTL_IN3, "in3", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &it87_in},
	{IT87_SYSCTL_IN4, "in4", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &it87_in},
	{IT87_SYSCTL_IN5, "in5", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &it87_in},
	{IT87_SYSCTL_IN6, "in6", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &it87_in},
	{IT87_SYSCTL_IN7, "in7", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &it87_in},
	{IT87_SYSCTL_IN8, "in8", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &it87_in},
	{IT87_SYSCTL_FAN1, "fan1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &it87_fan},
	{IT87_SYSCTL_FAN2, "fan2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &it87_fan},
	{IT87_SYSCTL_FAN3, "fan3", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &it87_fan},
	{IT87_SYSCTL_TEMP1, "temp1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &it87_temp},
	{IT87_SYSCTL_TEMP2, "temp2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &it87_temp},
	{IT87_SYSCTL_TEMP3, "temp3", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &it87_temp},
	{IT87_SYSCTL_VID, "vid", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &it87_vid},
	{IT87_SYSCTL_FAN_DIV, "fan_div", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &it87_fan_div},
	{IT87_SYSCTL_ALARMS, "alarms", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &it87_alarms},
	{IT87_SYSCTL_FAN_CTL, "fan_ctl", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &it87_fan_ctl},
	{IT87_SYSCTL_FAN_ON_OFF, "fan_on_off", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &it87_fan_ctl},
	{IT87_SYSCTL_PWM1, "pwm1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &it87_pwm},
	{IT87_SYSCTL_PWM2, "pwm2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &it87_pwm},
	{IT87_SYSCTL_PWM3, "pwm3", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &it87_pwm},
	{IT87_SYSCTL_PWM1, "sg_pwm1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &it87_sgpwm},
	{IT87_SYSCTL_PWM2, "sg_pwm2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &it87_sgpwm},
	{IT87_SYSCTL_PWM3, "sg_pwm3", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &it87_sgpwm},
	{IT87_SYSCTL_PWM1, "sg_tl1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &it87_sgtl},
	{IT87_SYSCTL_PWM2, "sg_tl2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &it87_sgtl},
	{IT87_SYSCTL_PWM3, "sg_tl3", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &it87_sgtl},
	{IT87_SYSCTL_SENS1, "sensor1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &it87_sens},
	{IT87_SYSCTL_SENS2, "sensor2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &it87_sens},
	{IT87_SYSCTL_SENS3, "sensor3", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &it87_sens},
	{0}
};


/* This function is called when:
     * it87_driver is inserted (when this module is loaded), for each
       available adapter
     * when a new adapter is inserted (and it87_driver is still present) */
static int it87_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_detect(adapter, &addr_data, it87_detect);
}

static int __init it87_find(int *address)
{
	int err = -ENODEV;
	u16 devid;

	superio_enter();
	devid = superio_inw(DEVID);
	if (!IT87_DEVID_MATCH(devid))
		goto exit;

	superio_select();
	if (!(superio_inb(IT87_ACT_REG) & 0x01)) {
		printk(KERN_INFO "it87: Device not activated, skipping\n");
		goto exit;
	}

	*address = superio_inw(IT87_BASE_REG) & ~(IT87_EXTENT - 1);
	if (*address == 0) {
		printk(KERN_INFO "it87: Base address not set, skipping\n");
		goto exit;
	}

	err = 0;
	printk(KERN_INFO "it87: Found IT%04xF chip at 0x%x, revision %d\n",
	       devid, *address, superio_inb(DEVREV) & 0x0f);

exit:
	superio_exit();
	return err;
}

/* This function is called by i2c_detect */
static int it87_detect(struct i2c_adapter *adapter, int address,
		       unsigned short flags, int kind)
{
	int i;
	struct i2c_client *new_client;
	struct it87_data *data;
	int err = 0;
	const char *type_name = "";
	const char *client_name = "";
	int is_isa = i2c_is_isa_adapter(adapter);

	if (!is_isa
	 && !i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
	 	return 0;

	if (is_isa
	 && check_region(address, IT87_EXTENT))
	 	return 0;

	/* Probe whether there is anything available on this address. Already
	   done for SMBus clients */
	if (is_isa && kind < 0) {
#define REALLY_SLOW_IO
		/* We need the timeouts for at least some IT87-like chips.
		   But only if we read 'undefined' registers. */
		i = inb_p(address + 1);
		if (inb_p(address + 2) != i
		 || inb_p(address + 3) != i
		 || inb_p(address + 7) != i)
			return 0;
#undef REALLY_SLOW_IO

		/* Let's just hope nothing breaks here */
		i = inb_p(address + 5) & 0x7f;
		outb_p(~i & 0x7f, address + 5);
		if ((inb_p(address + 5) & 0x7f) != (~i & 0x7f)) {
			outb_p(i, address + 5);
			return 0;
		}
	}

	/* OK. For now, we presume we have a valid client. We now create the
	   client structure, even though we cannot fill it completely yet.
	   But it allows us to access it87_{read,write}_value. */

	if (!(data = kmalloc(sizeof(struct it87_data), GFP_KERNEL))) {
		err = -ENOMEM;
		goto ERROR0;
	}

	new_client = &data->client;
	if (is_isa)
		init_MUTEX(&data->lock);
	new_client->addr = address;
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &it87_driver;
	new_client->flags = 0;

	/* Now, we do the remaining detection. */

	if (kind < 0) {
		if ((it87_read_value(new_client, IT87_REG_CONFIG) & 0x80)
		 || (!is_isa
		  && it87_read_value(new_client, IT87_REG_I2C_ADDR) != address))
		 	goto ERROR1;
	}

	/* Determine the chip type. */
	if (kind <= 0) {
		i = it87_read_value(new_client, IT87_REG_CHIPID);
		if (i == 0x90) {
			kind = it87;
			i = it87_read_value(new_client, IT87_REG_CHIPID2);
			if (i == 0x12)
				kind = it8712;
		}
		else {
			if (kind == 0)
				printk
				    ("it87.o: Ignoring 'force' parameter for unknown chip at "
				     "adapter %d, address 0x%02x\n",
				     i2c_adapter_id(adapter), address);
			goto ERROR1;
		}
	}

	if (kind == it87) {
		type_name = "it87";
		client_name = "IT87 chip";
	} else if (kind == it8712) {
		type_name = "it8712";
		client_name = "IT8712 chip";
	} else {
#ifdef DEBUG
		printk("it87.o: Internal error: unknown kind (%d)\n",
		       kind);
#endif
		goto ERROR1;
	}

	/* Reserve the ISA region */
	if (is_isa)
		request_region(address, IT87_EXTENT, type_name);

	/* Fill in the remaining client fields and put it into the global list */
	strcpy(new_client->name, client_name);
	data->type = kind;
	data->valid = 0;
	init_MUTEX(&data->update_lock);

	/* The IT8705F doesn't have VID capability */
	data->vid = 0x1f;

	/* Tell the I2C layer a new client has arrived */
	if ((err = i2c_attach_client(new_client)))
		goto ERROR3;

	/* Register a new directory entry with module sensors */
	if ((i = i2c_register_entry(new_client,
				    type_name,
				    it87_dir_table_template,
				    THIS_MODULE)) < 0) {
		err = i;
		goto ERROR4;
	}
	data->sysctl_id = i;

	/* Initialize the IT87 chip */
	it87_init_client(new_client);
	return 0;

/* OK, this is not exactly good programming practice, usually. But it is
   very code-efficient in this case. */

      ERROR4:
	i2c_detach_client(new_client);
      ERROR3:
	if (is_isa)
		release_region(address, IT87_EXTENT);
      ERROR1:
	kfree(data);
      ERROR0:
	return err;
}

static int it87_detach_client(struct i2c_client *client)
{
	int err;

	i2c_deregister_entry(((struct it87_data *) (client->data))->
				sysctl_id);

	if ((err = i2c_detach_client(client))) {
		printk
		    ("it87.o: Client deregistration failed, client not detached.\n");
		return err;
	}

	if(i2c_is_isa_client(client))
		release_region(client->addr, IT87_EXTENT);
	kfree(client->data);

	return 0;
}

/* The SMBus locks itself, but ISA access must be locked explicitly! 
   We don't want to lock the whole ISA bus, so we lock each client
   separately.
   We ignore the IT87 BUSY flag at this moment - it could lead to deadlocks,
   would slow down the IT87 access and should not be necessary. 
   There are some ugly typecasts here, but the good new is - they should
   nowhere else be necessary! */
static int it87_read_value(struct i2c_client *client, u8 reg)
{
	int res;
	if (i2c_is_isa_client(client)) {
		down(&(((struct it87_data *) (client->data))->lock));
		outb_p(reg, client->addr + IT87_ADDR_REG_OFFSET);
		res = inb_p(client->addr + IT87_DATA_REG_OFFSET);
		up(&(((struct it87_data *) (client->data))->lock));
		return res;
	} else
		return i2c_smbus_read_byte_data(client, reg);
}

/* The SMBus locks itself, but ISA access muse be locked explicitly! 
   We don't want to lock the whole ISA bus, so we lock each client
   separately.
   We ignore the IT87 BUSY flag at this moment - it could lead to deadlocks,
   would slow down the IT87 access and should not be necessary. 
   There are some ugly typecasts here, but the good new is - they should
   nowhere else be necessary! */
static int it87_write_value(struct i2c_client *client, u8 reg, u8 value)
{
	if (i2c_is_isa_client(client)) {
		down(&(((struct it87_data *) (client->data))->lock));
		outb_p(reg, client->addr + IT87_ADDR_REG_OFFSET);
		outb_p(value, client->addr + IT87_DATA_REG_OFFSET);
		up(&(((struct it87_data *) (client->data))->lock));
		return 0;
	} else
		return i2c_smbus_write_byte_data(client, reg, value);
}

/* Called when we have found a new IT87. */
static void it87_init_client(struct i2c_client *client)
{
	int tmp;

	if (reset) {
		/* Reset all except Watchdog values and last conversion values
		   This sets fan-divs to 2, among others */
		it87_write_value(client, IT87_REG_CONFIG, 0x80);
	}

	/* Check if temperature channnels are reset manually or by some reason */
	tmp = it87_read_value(client, IT87_REG_TEMP_ENABLE);
	if ((tmp & 0x3f) == 0) {
		/* Temp1,Temp3=thermistor; Temp2=thermal diode */
		tmp = (tmp & 0xc0) | 0x2a;
		it87_write_value(client, IT87_REG_TEMP_ENABLE, tmp);
	}

	/* Check if voltage monitors are reset manually or by some reason */
	tmp = it87_read_value(client, IT87_REG_VIN_ENABLE);
	if ((tmp & 0xff) == 0) {
		/* Enable all voltage monitors */
		it87_write_value(client, IT87_REG_VIN_ENABLE, 0xff);
	}

	/* Check if tachometers are reset manually or by some reason */
	tmp = it87_read_value(client, IT87_REG_FAN_CTRL);
	if ((tmp & 0x70) == 0) {
		/* Enable all fan tachometers */
		tmp = (tmp & 0x8f) | 0x70;
		it87_write_value(client, IT87_REG_FAN_CTRL, tmp);
	}

	/* Start monitoring */
	it87_write_value(client, IT87_REG_CONFIG,
			 (it87_read_value(client, IT87_REG_CONFIG) & 0x36)
			 | (update_vbat ? 0x41 : 0x01));
}

static void it87_update_client(struct i2c_client *client)
{
	struct it87_data *data = client->data;
	int i, tmp, tmp2;

	down(&data->update_lock);

	if ((jiffies - data->last_updated > HZ + HZ / 2) ||
	    (jiffies < data->last_updated) || !data->valid) {

		if (update_vbat) {
	/* Cleared after each update, so reenable.  Value
	   returned by this read will be previous value */
			it87_write_value(client, IT87_REG_CONFIG,
			   it87_read_value(client, IT87_REG_CONFIG) | 0x40);
		}
		for (i = 0; i <= 7; i++) {
			data->in[i] =
			    it87_read_value(client, IT87_REG_VIN(i));
			data->in_min[i] =
			    it87_read_value(client, IT87_REG_VIN_MIN(i));
			data->in_max[i] =
			    it87_read_value(client, IT87_REG_VIN_MAX(i));
		}
		data->in[8] =
		    it87_read_value(client, IT87_REG_VIN(8));
		/* VBAT sensor doesn't have limit registers, set
		   to min and max value */
		data->in_min[8] = 0;
		data->in_max[8] = 255;
                
		for (i = 1; i <= 3; i++) {
			data->fan[i - 1] =
			    it87_read_value(client, IT87_REG_FAN(i));
			data->fan_min[i - 1] =
			    it87_read_value(client, IT87_REG_FAN_MIN(i));
		}
		for (i = 1; i <= 3; i++) {
			data->temp[i - 1] =
			    it87_read_value(client, IT87_REG_TEMP(i));
			data->temp_high[i - 1] =
			    it87_read_value(client, IT87_REG_TEMP_HIGH(i));
			data->temp_low[i - 1] =
			    it87_read_value(client, IT87_REG_TEMP_LOW(i));
		}

		if (data->type == it8712) {
			data->vid = it87_read_value(client, IT87_REG_VID);
			data->vid &= 0x1f;
		}

		i = it87_read_value(client, IT87_REG_FAN_DIV);
		data->fan_div[0] = i & 0x07;
		data->fan_div[1] = (i >> 3) & 0x07;
		data->fan_div[2] = ( (i&0x40)==0x40 ? 3 : 1 );

		for( i = 1; i <= 3; i++ ) {
			data->pwm[i-1] = it87_read_value(client, IT87_REG_PWM(i));
			data->sg_tl[i-1][0] = it87_read_value(client, IT87_REG_SG_TL_OFF(i));
			data->sg_tl[i-1][1] = it87_read_value(client, IT87_REG_SG_TL_LOW(i));
			data->sg_tl[i-1][2] = it87_read_value(client, IT87_REG_SG_TL_MED(i));
			data->sg_tl[i-1][3] = it87_read_value(client, IT87_REG_SG_TL_HI(i));
			data->sg_tl[i-1][4] = it87_read_value(client, IT87_REG_SG_TL_OVR(i));
			data->sg_pwm[i-1][0] = it87_read_value(client, IT87_REG_SG_PWM_LOW(i));
			data->sg_pwm[i-1][1] = it87_read_value(client, IT87_REG_SG_PWM_MED(i));
			data->sg_pwm[i-1][2] = it87_read_value(client, IT87_REG_SG_PWM_HI(i));
		}
		data->alarms =
			it87_read_value(client, IT87_REG_ALARM1) |
			(it87_read_value(client, IT87_REG_ALARM2) << 8) |
			(it87_read_value(client, IT87_REG_ALARM3) << 16);
		data->fan_ctl[0] = it87_read_value(client, IT87_REG_FAN_CTRL);
		data->fan_ctl[1] = it87_read_value(client, IT87_REG_FAN_ONOFF);

		tmp = it87_read_value(client, IT87_REG_TEMP_ENABLE);
		for(i = 0; i < 3; i++) {
			tmp2 = (tmp >> i) & 0x09;
			if(tmp2 == 0x01)
				data->sens[i] = PIIDIODE;
			else if(tmp2 == 0x08)
				data->sens[i] = THERMISTOR;
			else
				data->sens[i] = UNUSED;
		}

		data->last_updated = jiffies;
		data->valid = 1;
	}

	up(&data->update_lock);
}


/* The next few functions are the call-back functions of the /proc/sys and
   sysctl files. Which function is used is defined in the ctl_table in
   the extra1 field.
    - Each function must return the magnitude (power of 10 to divide the
      data with) if it is called with operation==SENSORS_PROC_REAL_INFO.
    - It must put a maximum of *nrels elements in results reflecting the
      data of this file, and set *nrels to the number it actually put 
      in it, if operation==SENSORS_PROC_REAL_READ.
    - Finally, it must get upto *nrels elements from results and write them
      to the chip, if operations==SENSORS_PROC_REAL_WRITE.
   Note that on SENSORS_PROC_REAL_READ, I do not check whether results is
   large enough (by checking the incoming value of *nrels). This is not very
   good practice, but as long as you put less than about 5 values in results,
   you can assume it is large enough. */
void it87_in(struct i2c_client *client, int operation, int ctl_name,
	     int *nrels_mag, long *results)
{
	struct it87_data *data = client->data;
	int nr = ctl_name - IT87_SYSCTL_IN0;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 2;
	else if (operation == SENSORS_PROC_REAL_READ) {
		it87_update_client(client);
		results[0] = IN_FROM_REG(data->in_min[nr]);
		results[1] = IN_FROM_REG(data->in_max[nr]);
		results[2] = IN_FROM_REG(data->in[nr]);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->in_min[nr] = IN_TO_REG(results[0]);
			it87_write_value(client, IT87_REG_VIN_MIN(nr),
					 data->in_min[nr]);
		}
		if (*nrels_mag >= 2) {
			data->in_max[nr] = IN_TO_REG(results[1]);
			it87_write_value(client, IT87_REG_VIN_MAX(nr),
					 data->in_max[nr]);
		}
	}
}

void it87_fan(struct i2c_client *client, int operation, int ctl_name,
	      int *nrels_mag, long *results)
{
	struct it87_data *data = client->data;
	int nr = ctl_name - IT87_SYSCTL_FAN1 + 1;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		it87_update_client(client);
		results[0] = FAN_FROM_REG(data->fan_min[nr - 1],
					  DIV_FROM_REG(data->fan_div[nr - 1]));
		results[1] = FAN_FROM_REG(data->fan[nr - 1],
				 DIV_FROM_REG(data->fan_div[nr - 1]));
		*nrels_mag = 2;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->fan_min[nr - 1] = FAN_TO_REG(results[0],
							   DIV_FROM_REG(data->fan_div[nr - 1]));
			it87_write_value(client, IT87_REG_FAN_MIN(nr),
					 data->fan_min[nr - 1]);
		}
	}
}


void it87_temp(struct i2c_client *client, int operation, int ctl_name,
	       int *nrels_mag, long *results)
{
	struct it87_data *data = client->data;
	int nr = ctl_name - IT87_SYSCTL_TEMP1 + 1;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 1;
	else if (operation == SENSORS_PROC_REAL_READ) {
		it87_update_client(client);
		results[0] = TEMP_FROM_REG(data->temp_high[nr - 1]);
		results[1] = TEMP_FROM_REG(data->temp_low[nr - 1]);
		results[2] = TEMP_FROM_REG(data->temp[nr - 1]);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->temp_high[nr - 1] = TEMP_TO_REG(results[0]);
			it87_write_value(client, IT87_REG_TEMP_HIGH(nr),
					 data->temp_high[nr - 1]);
		}
		if (*nrels_mag >= 2) {
			data->temp_low[nr - 1] = TEMP_TO_REG(results[1]);
			it87_write_value(client, IT87_REG_TEMP_LOW(nr),
					 data->temp_low[nr - 1]);
		}
	}
}

void it87_pwm(struct i2c_client *client, int operation, int ctl_name,
	       int *nrels_mag, long *results)
{
	struct it87_data *data = client->data;
	int nr = ctl_name - IT87_SYSCTL_PWM1 + 1;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		it87_update_client(client);
		results[0] = data->pwm[nr - 1];
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->pwm[nr - 1] = results[0];
			it87_write_value(client, IT87_REG_PWM(nr), data->pwm[nr - 1]);
		}
	}
}

void it87_sgpwm(struct i2c_client *client, int operation, int ctl_name,
	       int *nrels_mag, long *results)
{
	struct it87_data *data = client->data;
	int nr = ctl_name - IT87_SYSCTL_PWM1 + 1;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		it87_update_client(client);
		results[0] = data->sg_pwm[nr - 1][0];
		results[1] = data->sg_pwm[nr - 1][1];
		results[2] = data->sg_pwm[nr - 1][2];
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->sg_pwm[nr - 1][0] = results[0];
			it87_write_value(client, IT87_REG_SG_PWM_LOW(nr), data->sg_pwm[nr - 1][0]);
		}
		if (*nrels_mag >= 2) {
			data->sg_pwm[nr - 1][1] = results[1];
			it87_write_value(client, IT87_REG_SG_PWM_MED(nr), data->sg_pwm[nr - 1][1]);
		}
		if (*nrels_mag >= 3) {
			data->sg_pwm[nr - 1][2] = results[2];
			it87_write_value(client, IT87_REG_SG_PWM_HI(nr), data->sg_pwm[nr - 1][2]);
		}
	}
}

void it87_sgtl(struct i2c_client *client, int operation, int ctl_name,
	       int *nrels_mag, long *results)
{
	struct it87_data *data = client->data;
	int nr = ctl_name - IT87_SYSCTL_PWM1 + 1;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 1;
	else if (operation == SENSORS_PROC_REAL_READ) {
		it87_update_client(client);
		results[0] = TEMP_FROM_REG(data->sg_tl[nr - 1][0]);
		results[1] = TEMP_FROM_REG(data->sg_tl[nr - 1][1]);
		results[2] = TEMP_FROM_REG(data->sg_tl[nr - 1][2]);
		results[3] = TEMP_FROM_REG(data->sg_tl[nr - 1][3]);
		results[4] = TEMP_FROM_REG(data->sg_tl[nr - 1][4]);
		*nrels_mag = 5;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->sg_tl[nr - 1][0] = TEMP_TO_REG(results[0]);
			it87_write_value(client, IT87_REG_SG_TL_OFF(nr), data->sg_tl[nr - 1][0]);
		}
		if (*nrels_mag >= 2) {
			data->sg_tl[nr - 1][1] = TEMP_TO_REG(results[1]);
			it87_write_value(client, IT87_REG_SG_TL_LOW(nr), data->sg_tl[nr - 1][1]);
		}
		if (*nrels_mag >= 3) {
			data->sg_tl[nr - 1][2] = TEMP_TO_REG(results[2]);
			it87_write_value(client, IT87_REG_SG_TL_MED(nr), data->sg_tl[nr - 1][2]);
		}
		if (*nrels_mag >= 4) {
			data->sg_tl[nr - 1][3] = TEMP_TO_REG(results[3]);
			it87_write_value(client, IT87_REG_SG_TL_HI(nr), data->sg_tl[nr - 1][3]);
		}
		if (*nrels_mag >= 5) {
			data->sg_tl[nr - 1][4] = TEMP_TO_REG(results[4]);
			it87_write_value(client, IT87_REG_SG_TL_OVR(nr), data->sg_tl[nr - 1][4]);
		}
	}
}

void it87_vid(struct i2c_client *client, int operation, int ctl_name,
	      int *nrels_mag, long *results)
{
	struct it87_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 2;
	else if (operation == SENSORS_PROC_REAL_READ) {
		it87_update_client(client);
		results[0] = VID_FROM_REG(data->vid);
		*nrels_mag = 1;
	}
}

void it87_alarms(struct i2c_client *client, int operation,
                     int ctl_name, int *nrels_mag, long *results)
{
	struct it87_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		it87_update_client(client);
		results[0] = ALARMS_FROM_REG(data->alarms);
		*nrels_mag = 1;
	}
}

void it87_fan_div(struct i2c_client *client, int operation, int ctl_name,
		  int *nrels_mag, long *results)
{
	struct it87_data *data = client->data;
	int old;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		it87_update_client(client);
		results[0] = DIV_FROM_REG(data->fan_div[0]);
		results[1] = DIV_FROM_REG(data->fan_div[1]);
		results[2] = DIV_FROM_REG(data->fan_div[2]);;
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		old = it87_read_value(client, IT87_REG_FAN_DIV);
		if (*nrels_mag >= 3) {
			data->fan_div[2] = DIV_TO_REG(results[2]);
			if (data->fan_div[2] != 3) {
				data->fan_div[2] = 1;
				old = (old & 0xbf);
			} else {
				old = (old | 0x40);
			}
		}
		if (*nrels_mag >= 2) {
			data->fan_div[1] = DIV_TO_REG(results[1]);
			old = (old & 0xc3) | (data->fan_div[1] << 3);
		}
		if (*nrels_mag >= 1) {
			data->fan_div[0] = DIV_TO_REG(results[0]);
			old = (old & 0xf8) | data->fan_div[0];
			it87_write_value(client, IT87_REG_FAN_DIV, old);
		}
	}
}

void it87_fan_ctl(struct i2c_client *client, int operation, int ctl_name,
	       int *nrels_mag, long *results)
{
	struct it87_data *data = client->data;
	int index = ctl_name - IT87_SYSCTL_FAN_CTL;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		it87_update_client(client);
		results[0] = data->fan_ctl[index];
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->fan_ctl[index] = results[0];
			if( index == 0 )
				it87_write_value(client, IT87_REG_FAN_CTRL, data->fan_ctl[index] );
			else
				it87_write_value(client, IT87_REG_FAN_ONOFF, data->fan_ctl[index] );
		}
	}
}

void it87_sens(struct i2c_client *client, int operation, int ctl_name,
		  int *nrels_mag, long *results)
{
	struct it87_data *data = client->data;
	int nr = 1 + ctl_name - IT87_SYSCTL_SENS1;
	u8 tmp, val1, val2;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		results[0] = data->sens[nr - 1];
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			val1 = 0x01 << (nr - 1);
			val2 = 0x08 << (nr - 1);
			tmp = it87_read_value(client, IT87_REG_TEMP_ENABLE);
			switch (results[0]) {
			case PIIDIODE:
				tmp &= ~ val2;
				tmp |= val1;
				break;
			case THERMISTOR:
				tmp &= ~ val1;
				tmp |= val2;
				break;
			case UNUSED:
				tmp &= ~ val1;
				tmp &= ~ val2;
				break;
			default:
				printk(KERN_ERR "it87.o: Invalid sensor type %ld; "
				       "must be 0 (unused), 2 (thermistor) "
				       "or 3 (diode)\n", results[0]);
				return;
			}
			it87_write_value(client,
					 IT87_REG_TEMP_ENABLE, tmp);
			data->sens[nr - 1] = results[0];
		}
	}
}

static int __init sm_it87_init(void)
{
	int addr;

	printk("it87.o version %s (%s)\n", LM_VERSION, LM_DATE);
	if (!it87_find(&addr)) {
		normal_isa[0] = addr;
	}
	return i2c_add_driver(&it87_driver);
}

static void __exit sm_it87_exit(void)
{
	i2c_del_driver(&it87_driver);
}


MODULE_AUTHOR("Chris Gauthron");
MODULE_DESCRIPTION("IT8705F, IT8712F, Sis950 driver");
MODULE_PARM(update_vbat, "i");
MODULE_PARM_DESC(update_vbat, "Update vbat if set else return powerup value");
MODULE_PARM(reset, "i");
MODULE_PARM_DESC(reset, "Reset the chip's registers, default no");

module_init(sm_it87_init);
module_exit(sm_it87_exit);
