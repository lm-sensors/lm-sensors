/*
    lm78.c - Part of lm_sensors, Linux kernel modules for hardware
             monitoring
    Copyright (c) 1998, 1999  Frodo Looijaard <frodol@dds.nl> 

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

MODULE_LICENSE("GPL");

/* Addresses to scan */
static unsigned short normal_i2c[] = { SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { 0x20, 0x2f, SENSORS_I2C_END };
static unsigned int normal_isa[] = { 0x0290, SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/* Insmod parameters */
SENSORS_INSMOD_3(lm78, lm78j, lm79);

/* Many LM78 constants specified below */

/* Length of ISA address segment */
#define LM78_EXTENT 8

/* Where are the ISA address/data registers relative to the base address */
#define LM78_ADDR_REG_OFFSET 5
#define LM78_DATA_REG_OFFSET 6

/* The LM78 registers */
#define LM78_REG_IN_MAX(nr) (0x2b + (nr) * 2)
#define LM78_REG_IN_MIN(nr) (0x2c + (nr) * 2)
#define LM78_REG_IN(nr) (0x20 + (nr))

#define LM78_REG_FAN_MIN(nr) (0x3a + (nr))
#define LM78_REG_FAN(nr) (0x27 + (nr))

#define LM78_REG_TEMP 0x27
#define LM78_REG_TEMP_OVER 0x39
#define LM78_REG_TEMP_HYST 0x3a

#define LM78_REG_ALARM1 0x41
#define LM78_REG_ALARM2 0x42

#define LM78_REG_VID_FANDIV 0x47

#define LM78_REG_CONFIG 0x40
#define LM78_REG_CHIPID 0x49
#define LM78_REG_I2C_ADDR 0x48


/* Conversions. Limit checking is only done on the TO_REG 
   variants. Note that you should be a bit careful with which arguments
   these macros are called: arguments may be evaluated more than once.
   Fixing this is just not worth it. */
#define IN_TO_REG(val)  (SENSORS_LIMIT((((val) * 10 + 8)/16),0,255))
#define IN_FROM_REG(val) (((val) *  16 + 5) / 10)

static inline u8 FAN_TO_REG(long rpm, int div)
{
	if (rpm <= 0)
		return 255;
	return SENSORS_LIMIT((1350000 + rpm * div / 2) / (rpm * div), 1,
			     254);
}

#define FAN_FROM_REG(val,div) ((val)==0?-1:(val)==255?0:1350000/((val)*(div)))

#define TEMP_TO_REG(val) (SENSORS_LIMIT(((val)<0?(((val)-5)/10):\
                                                 ((val)+5)/10), -128, 127))
#define TEMP_FROM_REG(val) ((val)*10)

#define VID_FROM_REG(val) ((val)==0x1f?0:(val)>=0x10?510-(val)*10:\
                           205-(val)*5)

#define DIV_TO_REG(val) ((val)==8?3:(val)==4?2:(val)==1?0:1)
#define DIV_FROM_REG(val) (1 << (val))

/* There are some complications in a module like this. First off, LM78 chips
   may be both present on the SMBus and the ISA bus, and we have to handle
   those cases separately at some places. Second, there might be several
   LM78 chips available (well, actually, that is probably never done; but
   it is a clean illustration of how to handle a case like that). Finally,
   a specific chip may be attached to *both* ISA and SMBus, and we would
   not like to detect it double. Fortunately, in the case of the LM78 at
   least, a register tells us what SMBus address we are on, so that helps
   a bit - except if there could be more than one SMBus. Groan. No solution
   for this yet. */

/* This module may seem overly long and complicated. In fact, it is not so
   bad. Quite a lot of bookkeeping is done. A real driver can often cut
   some corners. */

/* For each registered LM78, we need to keep some data in memory. It is
   dynamically allocated, at the same time when a new lm78 client is
   allocated. */
struct lm78_data {
	struct i2c_client client;
	struct semaphore lock;
	int sysctl_id;
	enum chips type;

	struct semaphore update_lock;
	char valid;		/* !=0 if following fields are valid */
	unsigned long last_updated;	/* In jiffies */

	u8 in[7];		/* Register value */
	u8 in_max[7];		/* Register value */
	u8 in_min[7];		/* Register value */
	u8 fan[3];		/* Register value */
	u8 fan_min[3];		/* Register value */
	s8 temp;		/* Register value */
	s8 temp_over;		/* Register value */
	s8 temp_hyst;		/* Register value */
	u8 fan_div[3];		/* Register encoding, shifted right */
	u8 vid;			/* Register encoding, combined */
	u16 alarms;		/* Register encoding, combined */
};


static int lm78_attach_adapter(struct i2c_adapter *adapter);
static int lm78_detect(struct i2c_adapter *adapter, int address,
		       unsigned short flags, int kind);
static int lm78_detach_client(struct i2c_client *client);

static int lm78_read_value(struct i2c_client *client, u8 reg);
static int lm78_write_value(struct i2c_client *client, u8 reg,
			    u8 value);
static void lm78_update_client(struct i2c_client *client);
static void lm78_init_client(struct i2c_client *client);


static void lm78_in(struct i2c_client *client, int operation, int ctl_name,
		    int *nrels_mag, long *results);
static void lm78_fan(struct i2c_client *client, int operation,
		     int ctl_name, int *nrels_mag, long *results);
static void lm78_temp(struct i2c_client *client, int operation,
		      int ctl_name, int *nrels_mag, long *results);
static void lm78_vid(struct i2c_client *client, int operation,
		     int ctl_name, int *nrels_mag, long *results);
static void lm78_alarms(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void lm78_fan_div(struct i2c_client *client, int operation,
			 int ctl_name, int *nrels_mag, long *results);

static struct i2c_driver lm78_driver = {
	.name		= "LM78(-J) and LM79 sensor driver",
	.id		= I2C_DRIVERID_LM78,
	.flags		= I2C_DF_NOTIFY,
	.attach_adapter	= lm78_attach_adapter,
	.detach_client	= lm78_detach_client,
};

/* The /proc/sys entries */

/* -- SENSORS SYSCTL START -- */
#define LM78_SYSCTL_IN0 1000	/* Volts * 100 */
#define LM78_SYSCTL_IN1 1001
#define LM78_SYSCTL_IN2 1002
#define LM78_SYSCTL_IN3 1003
#define LM78_SYSCTL_IN4 1004
#define LM78_SYSCTL_IN5 1005
#define LM78_SYSCTL_IN6 1006
#define LM78_SYSCTL_FAN1 1101	/* Rotations/min */
#define LM78_SYSCTL_FAN2 1102
#define LM78_SYSCTL_FAN3 1103
#define LM78_SYSCTL_TEMP 1200	/* Degrees Celsius * 10 */
#define LM78_SYSCTL_VID 1300	/* Volts * 100 */
#define LM78_SYSCTL_FAN_DIV 2000	/* 1, 2, 4 or 8 */
#define LM78_SYSCTL_ALARMS 2001	/* bitvector */

#define LM78_ALARM_IN0 0x0001
#define LM78_ALARM_IN1 0x0002
#define LM78_ALARM_IN2 0x0004
#define LM78_ALARM_IN3 0x0008
#define LM78_ALARM_IN4 0x0100
#define LM78_ALARM_IN5 0x0200
#define LM78_ALARM_IN6 0x0400
#define LM78_ALARM_FAN1 0x0040
#define LM78_ALARM_FAN2 0x0080
#define LM78_ALARM_FAN3 0x0800
#define LM78_ALARM_TEMP 0x0010
#define LM78_ALARM_BTI 0x0020
#define LM78_ALARM_CHAS 0x1000
#define LM78_ALARM_FIFO 0x2000
#define LM78_ALARM_SMI_IN 0x4000

/* -- SENSORS SYSCTL END -- */

/* These files are created for each detected LM78. This is just a template;
   though at first sight, you might think we could use a statically
   allocated list, we need some way to get back to the parent - which
   is done through one of the 'extra' fields which are initialized 
   when a new copy is allocated. */
static ctl_table lm78_dir_table_template[] = {
	{LM78_SYSCTL_IN0, "in0", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &lm78_in},
	{LM78_SYSCTL_IN1, "in1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &lm78_in},
	{LM78_SYSCTL_IN2, "in2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &lm78_in},
	{LM78_SYSCTL_IN3, "in3", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &lm78_in},
	{LM78_SYSCTL_IN4, "in4", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &lm78_in},
	{LM78_SYSCTL_IN5, "in5", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &lm78_in},
	{LM78_SYSCTL_IN6, "in6", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &lm78_in},
	{LM78_SYSCTL_FAN1, "fan1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &lm78_fan},
	{LM78_SYSCTL_FAN2, "fan2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &lm78_fan},
	{LM78_SYSCTL_FAN3, "fan3", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &lm78_fan},
	{LM78_SYSCTL_TEMP, "temp", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &lm78_temp},
	{LM78_SYSCTL_VID, "vid", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &lm78_vid},
	{LM78_SYSCTL_FAN_DIV, "fan_div", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &lm78_fan_div},
	{LM78_SYSCTL_ALARMS, "alarms", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &lm78_alarms},
	{0}
};


/* This function is called when:
     * lm78_driver is inserted (when this module is loaded), for each
       available adapter
     * when a new adapter is inserted (and lm78_driver is still present) */
static int lm78_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_detect(adapter, &addr_data, lm78_detect);
}

/* This function is called by i2c_detect */
static int lm78_detect(struct i2c_adapter *adapter, int address,
		       unsigned short flags, int kind)
{
	int i;
	struct i2c_client *new_client;
	struct lm78_data *data;
	int err = 0;
	const char *type_name = "";
	const char *client_name = "";
	int is_isa = i2c_is_isa_adapter(adapter);

	if (!is_isa
	    && !i2c_check_functionality(adapter,
					I2C_FUNC_SMBUS_BYTE_DATA)) goto
		    ERROR0;

	if (is_isa) {
		if (check_region(address, LM78_EXTENT))
			goto ERROR0;
	}

	/* Probe whether there is anything available on this address. Already
	   done for SMBus clients */
	if (kind < 0) {
		if (is_isa) {

#define REALLY_SLOW_IO
			/* We need the timeouts for at least some LM78-like chips. But only
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
	   But it allows us to access lm78_{read,write}_value. */

	if (!(data = kmalloc(sizeof(struct lm78_data), GFP_KERNEL))) {
		err = -ENOMEM;
		goto ERROR0;
	}

	new_client = &data->client;
	if (is_isa)
		init_MUTEX(&data->lock);
	new_client->addr = address;
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &lm78_driver;
	new_client->flags = 0;

	/* Now, we do the remaining detection. */

	if (kind < 0) {
		if (lm78_read_value(new_client, LM78_REG_CONFIG) & 0x80)
			goto ERROR1;
		if (!is_isa
		    && (lm78_read_value(new_client, LM78_REG_I2C_ADDR) !=
			address)) goto ERROR1;
		/* Explicitly prevent the misdetection of Winbond chips */
		i = lm78_read_value(new_client, 0x4f);
		if (i == 0xa3 || i == 0x5c)
			goto ERROR1;
	}

	/* Determine the chip type. */
	if (kind <= 0) {
		i = lm78_read_value(new_client, LM78_REG_CHIPID);
		if (i == 0x00 || i == 0x20)
			kind = lm78;
		else if (i == 0x40)
			kind = lm78j;
		else if ((i & 0xfe) == 0xc0)
			kind = lm79;
		else {
			if (kind == 0)
				printk
				    ("lm78.o: Ignoring 'force' parameter for unknown chip at "
				     "adapter %d, address 0x%02x\n",
				     i2c_adapter_id(adapter), address);
			goto ERROR1;
		}
	}

	if (kind == lm78) {
		type_name = "lm78";
		client_name = "LM78 chip";
	} else if (kind == lm78j) {
		type_name = "lm78-j";
		client_name = "LM78-J chip";
	} else if (kind == lm79) {
		type_name = "lm79";
		client_name = "LM79 chip";
	} else {
#ifdef DEBUG
		printk("lm78.o: Internal error: unknown kind (%d)\n",
		       kind);
#endif
		goto ERROR1;
	}

	/* Reserve the ISA region */
	if (is_isa)
		request_region(address, LM78_EXTENT, type_name);

	/* Fill in the remaining client fields and put it into the global list */
	strcpy(new_client->name, client_name);
	data->type = kind;
	data->valid = 0;
	init_MUTEX(&data->update_lock);

	/* Tell the I2C layer a new client has arrived */
	if ((err = i2c_attach_client(new_client)))
		goto ERROR3;

	/* Register a new directory entry with module sensors */
	if ((i = i2c_register_entry(new_client,
					type_name,
					lm78_dir_table_template,
					THIS_MODULE)) < 0) {
		err = i;
		goto ERROR4;
	}
	data->sysctl_id = i;

	/* Initialize the LM78 chip */
	lm78_init_client(new_client);
	return 0;

/* OK, this is not exactly good programming practice, usually. But it is
   very code-efficient in this case. */

      ERROR4:
	i2c_detach_client(new_client);
      ERROR3:
	if (is_isa)
		release_region(address, LM78_EXTENT);
      ERROR1:
	kfree(data);
      ERROR0:
	return err;
}

static int lm78_detach_client(struct i2c_client *client)
{
	int err;

	i2c_deregister_entry(((struct lm78_data *) (client->data))->
				 sysctl_id);

	if ((err = i2c_detach_client(client))) {
		printk
		    ("lm78.o: Client deregistration failed, client not detached.\n");
		return err;
	}

	if(i2c_is_isa_client(client))
		release_region(client->addr, LM78_EXTENT);
	kfree(client->data);

	return 0;
}

/* The SMBus locks itself, but ISA access must be locked explicitly! 
   We don't want to lock the whole ISA bus, so we lock each client
   separately.
   We ignore the LM78 BUSY flag at this moment - it could lead to deadlocks,
   would slow down the LM78 access and should not be necessary. 
   There are some ugly typecasts here, but the good new is - they should
   nowhere else be necessary! */
static int lm78_read_value(struct i2c_client *client, u8 reg)
{
	int res;
	if (i2c_is_isa_client(client)) {
		down(&(((struct lm78_data *) (client->data))->lock));
		outb_p(reg, client->addr + LM78_ADDR_REG_OFFSET);
		res = inb_p(client->addr + LM78_DATA_REG_OFFSET);
		up(&(((struct lm78_data *) (client->data))->lock));
		return res;
	} else
		return i2c_smbus_read_byte_data(client, reg);
}

/* The SMBus locks itself, but ISA access muse be locked explicitly! 
   We don't want to lock the whole ISA bus, so we lock each client
   separately.
   We ignore the LM78 BUSY flag at this moment - it could lead to deadlocks,
   would slow down the LM78 access and should not be necessary. 
   There are some ugly typecasts here, but the good new is - they should
   nowhere else be necessary! */
static int lm78_write_value(struct i2c_client *client, u8 reg, u8 value)
{
	if (i2c_is_isa_client(client)) {
		down(&(((struct lm78_data *) (client->data))->lock));
		outb_p(reg, client->addr + LM78_ADDR_REG_OFFSET);
		outb_p(value, client->addr + LM78_DATA_REG_OFFSET);
		up(&(((struct lm78_data *) (client->data))->lock));
		return 0;
	} else
		return i2c_smbus_write_byte_data(client, reg, value);
}

/* Called when we have found a new LM78. */
static void lm78_init_client(struct i2c_client *client)
{
	u8 config = lm78_read_value(client, LM78_REG_CONFIG);

	/* Start monitoring */
	if (!(config & 0x01))
		lm78_write_value(client, LM78_REG_CONFIG,
				 (config & 0xf7) | 0x01);
}

static void lm78_update_client(struct i2c_client *client)
{
	struct lm78_data *data = client->data;
	int i;

	down(&data->update_lock);

	if ((jiffies - data->last_updated > HZ + HZ / 2) ||
	    (jiffies < data->last_updated) || !data->valid) {

#ifdef DEBUG
		printk("Starting lm78 update\n");
#endif
		for (i = 0; i <= 6; i++) {
			data->in[i] =
			    lm78_read_value(client, LM78_REG_IN(i));
			data->in_min[i] =
			    lm78_read_value(client, LM78_REG_IN_MIN(i));
			data->in_max[i] =
			    lm78_read_value(client, LM78_REG_IN_MAX(i));
		}
		for (i = 1; i <= 3; i++) {
			data->fan[i - 1] =
			    lm78_read_value(client, LM78_REG_FAN(i));
			data->fan_min[i - 1] =
			    lm78_read_value(client, LM78_REG_FAN_MIN(i));
		}
		data->temp = lm78_read_value(client, LM78_REG_TEMP);
		data->temp_over =
		    lm78_read_value(client, LM78_REG_TEMP_OVER);
		data->temp_hyst =
		    lm78_read_value(client, LM78_REG_TEMP_HYST);
		i = lm78_read_value(client, LM78_REG_VID_FANDIV);
		data->vid = i & 0x0f;
		if (data->type == lm79)
			data->vid |=
			    (lm78_read_value(client, LM78_REG_CHIPID) &
			     0x01) << 4;
		else
			data->vid |= 0x10;
		data->fan_div[0] = (i >> 4) & 0x03;
		data->fan_div[1] = i >> 6;
		data->alarms = lm78_read_value(client, LM78_REG_ALARM1) +
		    (lm78_read_value(client, LM78_REG_ALARM2) << 8);
		data->last_updated = jiffies;
		data->valid = 1;

		data->fan_div[2] = 1;
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
void lm78_in(struct i2c_client *client, int operation, int ctl_name,
	     int *nrels_mag, long *results)
{
	struct lm78_data *data = client->data;
	int nr = ctl_name - LM78_SYSCTL_IN0;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 2;
	else if (operation == SENSORS_PROC_REAL_READ) {
		lm78_update_client(client);
		results[0] = IN_FROM_REG(data->in_min[nr]);
		results[1] = IN_FROM_REG(data->in_max[nr]);
		results[2] = IN_FROM_REG(data->in[nr]);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->in_min[nr] = IN_TO_REG(results[0]);
			lm78_write_value(client, LM78_REG_IN_MIN(nr),
					 data->in_min[nr]);
		}
		if (*nrels_mag >= 2) {
			data->in_max[nr] = IN_TO_REG(results[1]);
			lm78_write_value(client, LM78_REG_IN_MAX(nr),
					 data->in_max[nr]);
		}
	}
}

void lm78_fan(struct i2c_client *client, int operation, int ctl_name,
	      int *nrels_mag, long *results)
{
	struct lm78_data *data = client->data;
	int nr = ctl_name - LM78_SYSCTL_FAN1 + 1;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		lm78_update_client(client);
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
			lm78_write_value(client, LM78_REG_FAN_MIN(nr),
					 data->fan_min[nr - 1]);
		}
	}
}


void lm78_temp(struct i2c_client *client, int operation, int ctl_name,
	       int *nrels_mag, long *results)
{
	struct lm78_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 1;
	else if (operation == SENSORS_PROC_REAL_READ) {
		lm78_update_client(client);
		results[0] = TEMP_FROM_REG(data->temp_over);
		results[1] = TEMP_FROM_REG(data->temp_hyst);
		results[2] = TEMP_FROM_REG(data->temp);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->temp_over = TEMP_TO_REG(results[0]);
			lm78_write_value(client, LM78_REG_TEMP_OVER,
					 data->temp_over);
		}
		if (*nrels_mag >= 2) {
			data->temp_hyst = TEMP_TO_REG(results[1]);
			lm78_write_value(client, LM78_REG_TEMP_HYST,
					 data->temp_hyst);
		}
	}
}

void lm78_vid(struct i2c_client *client, int operation, int ctl_name,
	      int *nrels_mag, long *results)
{
	struct lm78_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 2;
	else if (operation == SENSORS_PROC_REAL_READ) {
		lm78_update_client(client);
		results[0] = VID_FROM_REG(data->vid);
		*nrels_mag = 1;
	}
}

void lm78_alarms(struct i2c_client *client, int operation, int ctl_name,
		 int *nrels_mag, long *results)
{
	struct lm78_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		lm78_update_client(client);
		results[0] = data->alarms;
		*nrels_mag = 1;
	}
}

/* Note: we save and restore the fan minimum here, because its value is
   determined in part by the fan divisor.  This follows the principle of
   least surprise: the user doesn't expect the fan minimum to change just
   because the divisor changed. */
void lm78_fan_div(struct i2c_client *client, int operation, int ctl_name,
		  int *nrels_mag, long *results)
{
	struct lm78_data *data = client->data;
	int old, min;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		lm78_update_client(client);
		results[0] = DIV_FROM_REG(data->fan_div[0]);
		results[1] = DIV_FROM_REG(data->fan_div[1]);
		results[2] = 2;
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		old = lm78_read_value(client, LM78_REG_VID_FANDIV);
		if (*nrels_mag >= 2) {
			min = FAN_FROM_REG(data->fan_min[1], 
					DIV_FROM_REG(data->fan_div[1]));
			data->fan_div[1] = DIV_TO_REG(results[1]);
			old = (old & 0x3f) | (data->fan_div[1] << 6);
			data->fan_min[1] = FAN_TO_REG(min,
					DIV_FROM_REG(data->fan_div[1]));
			lm78_write_value(client, LM78_REG_FAN_MIN(2),
					data->fan_min[1]);
		}
		if (*nrels_mag >= 1) {
			min = FAN_FROM_REG(data->fan_min[0],
					DIV_FROM_REG(data->fan_div[0]));
			data->fan_div[0] = DIV_TO_REG(results[0]);
			old = (old & 0xcf) | (data->fan_div[0] << 4);
			data->fan_min[0] = FAN_TO_REG(min,
					DIV_FROM_REG(data->fan_div[0]));
			lm78_write_value(client, LM78_REG_FAN_MIN(1),
					data->fan_min[0]);
			lm78_write_value(client, LM78_REG_VID_FANDIV, old);
		}
	}
}

static int __init sm_lm78_init(void)
{
	printk("lm78.o version %s (%s)\n", LM_VERSION, LM_DATE);
	return i2c_add_driver(&lm78_driver);
}

static void __exit sm_lm78_exit(void)
{
	i2c_del_driver(&lm78_driver);
}



MODULE_AUTHOR("Frodo Looijaard <frodol@dds.nl>");
MODULE_DESCRIPTION("LM78, LM78-J and LM79 driver");

module_init(sm_lm78_init);
module_exit(sm_lm78_exit);
