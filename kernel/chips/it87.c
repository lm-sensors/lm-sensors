/*
    it87.c - Part of lm_sensors, Linux kernel modules for hardware
             monitoring.

    Supports: IT8705F  Super I/O chip w/LPC interface
              IT8712F  Super I/O chup w/LPC interface & SMbus
              Sis950   A clone of the IT8705F

    Copyright (c) 2001 Chris Gauthron <chrisg@0-in.com> 
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
static unsigned short normal_i2c[] = { SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { 0x20, 0x2f, SENSORS_I2C_END };
static unsigned int normal_isa[] = { 0x0290, SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/* Insmod parameters */
SENSORS_INSMOD_4(it87, it8705, it8712, sis950);


/* Update battery voltage after every reading if true */
static int update_vbat = 0;


/* Enable Temp1 as thermal resistor */
/* Enable Temp2 as thermal diode */
/* Enable Temp3 as thermal resistor */
static int temp_type = 0x2a;

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

/* Monitors: 9 voltage (0 to 7, battery), 3 temp (1 to 3), 3 fan (1 to 3) */

#define IT87_REG_FAN(nr)       (0x0c + (nr))
#define IT87_REG_FAN_MIN(nr)   (0x0f + (nr))
#define IT87_REG_FAN_CTRL      0x13

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


/* Conversions. Rounding and limit checking is only done on the TO_REG 
   variants. Note that you should be a bit careful with which arguments
   these macros are called: arguments may be evaluated more than once.
   Fixing this is just not worth it. */
#define IN_TO_REG(val)  (SENSORS_LIMIT((((val) * 10 + 8)/16),0,255))
#define IN_FROM_REG(val) (((val) *  16) / 10)

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

#define VID_FROM_REG(val) ((val)==0x1f?0:(val)>=0x10?510-(val)*10:\
                           205-(val)*5)
#define ALARMS_FROM_REG(val) (val)

#define DIV_TO_REG(val) ((val)==8?3:(val)==4?2:(val)==1?0:1)
#define DIV_FROM_REG(val) (1 << (val))

/* Initial limits. Use the config file to set better limits. */
#define IT87_INIT_IN_0 170
#define IT87_INIT_IN_1 250
#define IT87_INIT_IN_2 (330 / 2)
#define IT87_INIT_IN_3 (((500)   * 100)/168)
#define IT87_INIT_IN_4 (((1200)  * 10)/38)
#define IT87_INIT_IN_5 (((1200)  * 10)/72)
#define IT87_INIT_IN_6 (((500)   * 10)/56)
#define IT87_INIT_IN_7 (((500)   * 100)/168)

#define IT87_INIT_IN_PERCENTAGE 10

#define IT87_INIT_IN_MIN_0 \
        (IT87_INIT_IN_0 - IT87_INIT_IN_0 * IT87_INIT_IN_PERCENTAGE / 100)
#define IT87_INIT_IN_MAX_0 \
        (IT87_INIT_IN_0 + IT87_INIT_IN_0 * IT87_INIT_IN_PERCENTAGE / 100)
#define IT87_INIT_IN_MIN_1 \
        (IT87_INIT_IN_1 - IT87_INIT_IN_1 * IT87_INIT_IN_PERCENTAGE / 100)
#define IT87_INIT_IN_MAX_1 \
        (IT87_INIT_IN_1 + IT87_INIT_IN_1 * IT87_INIT_IN_PERCENTAGE / 100)
#define IT87_INIT_IN_MIN_2 \
        (IT87_INIT_IN_2 - IT87_INIT_IN_2 * IT87_INIT_IN_PERCENTAGE / 100)
#define IT87_INIT_IN_MAX_2 \
        (IT87_INIT_IN_2 + IT87_INIT_IN_2 * IT87_INIT_IN_PERCENTAGE / 100)
#define IT87_INIT_IN_MIN_3 \
        (IT87_INIT_IN_3 - IT87_INIT_IN_3 * IT87_INIT_IN_PERCENTAGE / 100)
#define IT87_INIT_IN_MAX_3 \
        (IT87_INIT_IN_3 + IT87_INIT_IN_3 * IT87_INIT_IN_PERCENTAGE / 100)
#define IT87_INIT_IN_MIN_4 \
        (IT87_INIT_IN_4 - IT87_INIT_IN_4 * IT87_INIT_IN_PERCENTAGE / 100)
#define IT87_INIT_IN_MAX_4 \
        (IT87_INIT_IN_4 + IT87_INIT_IN_4 * IT87_INIT_IN_PERCENTAGE / 100)
#define IT87_INIT_IN_MIN_5 \
        (IT87_INIT_IN_5 - IT87_INIT_IN_5 * IT87_INIT_IN_PERCENTAGE / 100)
#define IT87_INIT_IN_MAX_5 \
        (IT87_INIT_IN_5 + IT87_INIT_IN_5 * IT87_INIT_IN_PERCENTAGE / 100)
#define IT87_INIT_IN_MIN_6 \
        (IT87_INIT_IN_6 - IT87_INIT_IN_6 * IT87_INIT_IN_PERCENTAGE / 100)
#define IT87_INIT_IN_MAX_6 \
        (IT87_INIT_IN_6 + IT87_INIT_IN_6 * IT87_INIT_IN_PERCENTAGE / 100)
#define IT87_INIT_IN_MIN_7 \
        (IT87_INIT_IN_7 - IT87_INIT_IN_7 * IT87_INIT_IN_PERCENTAGE / 100)
#define IT87_INIT_IN_MAX_7 \
        (IT87_INIT_IN_7 + IT87_INIT_IN_7 * IT87_INIT_IN_PERCENTAGE / 100)

#define IT87_INIT_FAN_MIN_1 3000
#define IT87_INIT_FAN_MIN_2 3000
#define IT87_INIT_FAN_MIN_3 3000

#define IT87_INIT_TEMP_HIGH_1 600
#define IT87_INIT_TEMP_LOW_1  200
#define IT87_INIT_TEMP_HIGH_2 600
#define IT87_INIT_TEMP_LOW_2  200
#define IT87_INIT_TEMP_HIGH_3 600 
#define IT87_INIT_TEMP_LOW_3  200

#ifdef MODULE
extern int init_module(void);
extern int cleanup_module(void);
#endif				/* MODULE */

/* For each registered IT87, we need to keep some data in memory. That
   data is pointed to by it87_list[NR]->data. The structure itself is
   dynamically allocated, at the same time when a new it87 client is
   allocated. */
struct it87_data {
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
	u8 temp[3];		/* Register value */
	u8 temp_high[3];	/* Register value */
	u8 temp_low[3];		/* Register value */
	u8 fan_div[3];		/* Register encoding, shifted right */
	u8 vid;			/* Register encoding, combined */
	u32 alarms;		/* Register encoding, combined */
};


#ifdef MODULE
static
#else
extern
#endif
int __init sensors_it87_init(void);
static int __init it87_cleanup(void);

static int it87_attach_adapter(struct i2c_adapter *adapter);
static int it87_detect(struct i2c_adapter *adapter, int address,
		       unsigned short flags, int kind);
static int it87_detach_client(struct i2c_client *client);
static int it87_command(struct i2c_client *client, unsigned int cmd,
			void *arg);
static void it87_inc_use(struct i2c_client *client);
static void it87_dec_use(struct i2c_client *client);

static int it87_read_value(struct i2c_client *client, u8 register);
static int it87_write_value(struct i2c_client *client, u8 register,
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

static struct i2c_driver it87_driver = {
	/* name */ "IT87xx sensor driver",
	/* id */ I2C_DRIVERID_IT87,
	/* flags */ I2C_DF_NOTIFY,
	/* attach_adapter */ &it87_attach_adapter,
	/* detach_client */ &it87_detach_client,
	/* command */ &it87_command,
	/* inc_use */ &it87_inc_use,
	/* dec_use */ &it87_dec_use
};

/* Used by it87_init/cleanup */
static int __initdata it87_initialized = 0;

static int it87_id = 0;

/* The /proc/sys entries */
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
	{0}
};


/* This function is called when:
     * it87_driver is inserted (when this module is loaded), for each
       available adapter
     * when a new adapter is inserted (and it87_driver is still present) */
int it87_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_detect(adapter, &addr_data, it87_detect);
}

/* This function is called by i2c_detect */
int it87_detect(struct i2c_adapter *adapter, int address,
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
	    && !i2c_check_functionality(adapter,
					I2C_FUNC_SMBUS_BYTE_DATA)) goto
		    ERROR0;

	if (is_isa) {
		if (check_region(address, IT87_EXTENT))
			goto ERROR0;
	}

	/* Probe whether there is anything available on this address. Already
	   done for SMBus clients */
	if (kind < 0) {
		if (is_isa) {

#define REALLY_SLOW_IO
			/* We need the timeouts for at least some IT87-like chips. But only
			   if we read 'undefined' registers. */
			i = inb_p(address + 1);
			if (inb_p(address + 2) != i)
				goto ERROR0;
			if (inb_p(address + 3) != i)
				goto ERROR0;
			if (inb_p(address + 7) != i)
				goto ERROR0;
#undef REALLY_SLOW_IO

			/* Let's just hope nothing breaks here */
			i = inb_p(address + 5) & 0x7f;
			outb_p(~i & 0x7f, address + 5);
			if ((inb_p(address + 5) & 0x7f) != (~i & 0x7f)) {
				outb_p(i, address + 5);
				return 0;
			}
		}
	}

	/* OK. For now, we presume we have a valid client. We now create the
	   client structure, even though we cannot fill it completely yet.
	   But it allows us to access it87_{read,write}_value. */

	if (!(new_client = kmalloc((sizeof(struct i2c_client)) +
				   sizeof(struct it87_data),
				   GFP_KERNEL))) {
		err = -ENOMEM;
		goto ERROR0;
	}

	data = (struct it87_data *) (new_client + 1);
	if (is_isa)
		init_MUTEX(&data->lock);
	new_client->addr = address;
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &it87_driver;
	new_client->flags = 0;

	/* Now, we do the remaining detection. */

	if (kind < 0) {
		if (it87_read_value(new_client, IT87_REG_CONFIG) & 0x80)
			goto ERROR1;
		if (!is_isa
		    && (it87_read_value(new_client, IT87_REG_I2C_ADDR) !=
			address)) goto ERROR1;
	}

	/* Determine the chip type. */
	if (kind <= 0) {
		i = it87_read_value(new_client, IT87_REG_CHIPID);
		if (i == 0x90) {
			kind = it87;
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
	} /* else if (kind == it8712) {
		type_name = "it8712";
		client_name = "IT87-J chip";
	} */ else {
#ifdef DEBUG
		printk("it87.o: Internal error: unknown kind (%d)?!?",
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

	new_client->id = it87_id++;
	data->valid = 0;
	init_MUTEX(&data->update_lock);

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
	kfree(new_client);
      ERROR0:
	return err;
}

int it87_detach_client(struct i2c_client *client)
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
	kfree(client);

	return 0;
}

/* No commands defined yet */
int it87_command(struct i2c_client *client, unsigned int cmd, void *arg)
{
	return 0;
}

/* Nothing here yet */
void it87_inc_use(struct i2c_client *client)
{
#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif
}

/* Nothing here yet */
void it87_dec_use(struct i2c_client *client)
{
#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
}


/* The SMBus locks itself, but ISA access must be locked explicitely! 
   We don't want to lock the whole ISA bus, so we lock each client
   separately.
   We ignore the IT87 BUSY flag at this moment - it could lead to deadlocks,
   would slow down the IT87 access and should not be necessary. 
   There are some ugly typecasts here, but the good new is - they should
   nowhere else be necessary! */
int it87_read_value(struct i2c_client *client, u8 reg)
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

/* The SMBus locks itself, but ISA access muse be locked explicitely! 
   We don't want to lock the whole ISA bus, so we lock each client
   separately.
   We ignore the IT87 BUSY flag at this moment - it could lead to deadlocks,
   would slow down the IT87 access and should not be necessary. 
   There are some ugly typecasts here, but the good new is - they should
   nowhere else be necessary! */
int it87_write_value(struct i2c_client *client, u8 reg, u8 value)
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

/* Called when we have found a new IT87. It should set limits, etc. */
void it87_init_client(struct i2c_client *client)
{
	/* Reset all except Watchdog values and last conversion values
	   This sets fan-divs to 2, among others */
	it87_write_value(client, IT87_REG_CONFIG, 0x80);
	it87_write_value(client, IT87_REG_VIN_MIN(0),
			 IN_TO_REG(IT87_INIT_IN_MIN_0));
	it87_write_value(client, IT87_REG_VIN_MAX(0),
			 IN_TO_REG(IT87_INIT_IN_MAX_0));
	it87_write_value(client, IT87_REG_VIN_MIN(1),
			 IN_TO_REG(IT87_INIT_IN_MIN_1));
	it87_write_value(client, IT87_REG_VIN_MAX(1),
			 IN_TO_REG(IT87_INIT_IN_MAX_1));
	it87_write_value(client, IT87_REG_VIN_MIN(2),
			 IN_TO_REG(IT87_INIT_IN_MIN_2));
	it87_write_value(client, IT87_REG_VIN_MAX(2),
			 IN_TO_REG(IT87_INIT_IN_MAX_2));
	it87_write_value(client, IT87_REG_VIN_MIN(3),
			 IN_TO_REG(IT87_INIT_IN_MIN_3));
	it87_write_value(client, IT87_REG_VIN_MAX(3),
			 IN_TO_REG(IT87_INIT_IN_MAX_3));
	it87_write_value(client, IT87_REG_VIN_MIN(4),
			 IN_TO_REG(IT87_INIT_IN_MIN_4));
	it87_write_value(client, IT87_REG_VIN_MAX(4),
			 IN_TO_REG(IT87_INIT_IN_MAX_4));
	it87_write_value(client, IT87_REG_VIN_MIN(5),
			 IN_TO_REG(IT87_INIT_IN_MIN_5));
	it87_write_value(client, IT87_REG_VIN_MAX(5),
			 IN_TO_REG(IT87_INIT_IN_MAX_5));
	it87_write_value(client, IT87_REG_VIN_MIN(6),
			 IN_TO_REG(IT87_INIT_IN_MIN_6));
	it87_write_value(client, IT87_REG_VIN_MAX(6),
			 IN_TO_REG(IT87_INIT_IN_MAX_6));
	it87_write_value(client, IT87_REG_VIN_MIN(7),
			 IN_TO_REG(IT87_INIT_IN_MIN_7));
	it87_write_value(client, IT87_REG_VIN_MAX(7),
			 IN_TO_REG(IT87_INIT_IN_MAX_7));
        /* Note: Battery voltage does not have limit registers */
	it87_write_value(client, IT87_REG_FAN_MIN(1),
			 FAN_TO_REG(IT87_INIT_FAN_MIN_1, 2));
	it87_write_value(client, IT87_REG_FAN_MIN(2),
			 FAN_TO_REG(IT87_INIT_FAN_MIN_2, 2));
	it87_write_value(client, IT87_REG_FAN_MIN(3),
			 FAN_TO_REG(IT87_INIT_FAN_MIN_3, 2));
	it87_write_value(client, IT87_REG_TEMP_HIGH(1),
			 TEMP_TO_REG(IT87_INIT_TEMP_HIGH_1));
	it87_write_value(client, IT87_REG_TEMP_LOW(1),
			 TEMP_TO_REG(IT87_INIT_TEMP_LOW_1));
	it87_write_value(client, IT87_REG_TEMP_HIGH(2),
			 TEMP_TO_REG(IT87_INIT_TEMP_HIGH_2));
	it87_write_value(client, IT87_REG_TEMP_LOW(2),
			 TEMP_TO_REG(IT87_INIT_TEMP_LOW_2));
	it87_write_value(client, IT87_REG_TEMP_HIGH(3),
			 TEMP_TO_REG(IT87_INIT_TEMP_HIGH_3));
	it87_write_value(client, IT87_REG_TEMP_LOW(3),
			 TEMP_TO_REG(IT87_INIT_TEMP_LOW_3));

	/* Enable voltage monitors */
	it87_write_value(client, IT87_REG_VIN_ENABLE, 0xff);

	/* Enable Temp1-Temp3 */
	it87_write_value(client, IT87_REG_TEMP_ENABLE,
			(it87_read_value(client, IT87_REG_TEMP_ENABLE) & 0xc0)
			| (temp_type & 0x3f));

	/* Enable fans */
	it87_write_value(client, IT87_REG_FAN_CTRL,
			(it87_read_value(client, IT87_REG_FAN_CTRL) & 0x8f)
			| 0x70);

	/* Start monitoring */
	it87_write_value(client, IT87_REG_CONFIG,
			 (it87_read_value(client, IT87_REG_CONFIG) & 0xb7)
			 | (update_vbat ? 0x41 : 0x01));
}

void it87_update_client(struct i2c_client *client)
{
	struct it87_data *data = client->data;
	int i;

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
		/* Temperature sensor doesn't have limit registers, set
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

		/* The 8705 does not have VID capability */
		/*if (data->type == it8712) {
			data->vid = it87_read_value(client, IT87_REG_VID);
			data->vid &= 0x1f;
		}
		else */ {
			data->vid = 0x1f;
		}

		i = it87_read_value(client, IT87_REG_FAN_DIV);
		data->fan_div[0] = i & 0x07;
		data->fan_div[1] = (i >> 3) & 0x07;
		data->fan_div[2] = 1;

		data->alarms =
			it87_read_value(client, IT87_REG_ALARM1) |
			(it87_read_value(client, IT87_REG_ALARM2) << 8) |
			(it87_read_value(client, IT87_REG_ALARM3) << 16);

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
					  DIV_FROM_REG(data->
						       fan_div[nr - 1]));
		results[1] =
		    FAN_FROM_REG(data->fan[nr - 1],
				 DIV_FROM_REG(data->fan_div[nr - 1]));
		*nrels_mag = 2;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->fan_min[nr - 1] = FAN_TO_REG(results[0],
							   DIV_FROM_REG
							   (data->
							    fan_div[nr -
								    1]));
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
		results[2] = 2;
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		old = it87_read_value(client, IT87_REG_FAN_DIV);
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

int __init sensors_it87_init(void)
{
	int res;

	printk("it87.o version %s (%s)\n", LM_VERSION, LM_DATE);
	it87_initialized = 0;

	if ((res = i2c_add_driver(&it87_driver))) {
		printk
		    ("it87.o: Driver registration failed, module not inserted.\n");
		it87_cleanup();
		return res;
	}
	it87_initialized++;
	return 0;
}

int __init it87_cleanup(void)
{
	int res;

	if (it87_initialized >= 1) {
		if ((res = i2c_del_driver(&it87_driver))) {
			printk
			    ("it87.o: Driver deregistration failed, module not removed.\n");
			return res;
		}
		it87_initialized--;
	}
	return 0;
}

EXPORT_NO_SYMBOLS;

#ifdef MODULE

MODULE_AUTHOR("Chris Gauthron <chrisg@0-in.com>");
MODULE_DESCRIPTION("IT8705F, IT8712F, Sis950 driver");
MODULE_PARM(update_vbat, "i");
MODULE_PARM_DESC(update_vbat, "Update vbat if set else return powerup value");
MODULE_PARM(temp_type, "i");
MODULE_PARM_DESC(temp_type, "Temperature sensor type, normally leave unset");

int init_module(void)
{
	return sensors_it87_init();
}

int cleanup_module(void)
{
	return it87_cleanup();
}

#endif				/* MODULE */
