/*
    adm1025.c - Part of lm_sensors, Linux kernel modules for hardware
               monitoring
    Add by Gordon Wu <gwu@esoft.com> according to the adm9240.c written by
    Frodo Looijaard <frodol@dds.nl>
    and Philip Edelbrock <phil@netroedge.com>

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

/* Supports the Analog Devices ADM1025. See doc/chips/adm1025 for details */

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
static unsigned short normal_i2c_range[] = { 0x2c, 0x2e, SENSORS_I2C_END };
static unsigned int normal_isa[] = { SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/* Insmod parameters */
SENSORS_INSMOD_1(adm1025);

/* Many ADM1025 constants specified below */

#define ADM1025_REG_IN_MAX(nr) (0x2b + (nr) * 2)
#define ADM1025_REG_IN_MIN(nr) (0x2c + (nr) * 2)
#define ADM1025_REG_IN(nr) (0x20 + (nr))

/* The ADM1025 registers */
#define ADM1025_REG_TEST 0x15
/* These are all read-only */
#define ADM1025_REG_2_5V 0x20
#define ADM1025_REG_VCCP1 0x21
#define ADM1025_REG_3_3V 0x22
#define ADM1025_REG_5V 0x23
#define ADM1025_REG_12V 0x24
#define ADM1025_REG_VCC 0x25
#define ADM1025_REG_RTEMP 0x26
#define ADM1025_REG_TEMP 0x27
#define ADM1025_REG_COMPANY_ID 0x3E	/* 0x41 for ADM1025 */
#define ADM1025_REG_DIE_REV 0x3F
/* These are read/write */
#define ADM1025_REG_2_5V_HIGH 0x2B
#define ADM1025_REG_2_5V_LOW 0x2C
#define ADM1025_REG_VCCP1_HIGH 0x2D
#define ADM1025_REG_VCCP1_LOW 0x2E
#define ADM1025_REG_3_3V_HIGH 0x2F
#define ADM1025_REG_3_3V_LOW 0x30
#define ADM1025_REG_5V_HIGH 0x31
#define ADM1025_REG_5V_LOW 0x32
#define ADM1025_REG_12V_HIGH 0x33
#define ADM1025_REG_12V_LOW 0x34
#define ADM1025_REG_VCC_HIGH 0x35
#define ADM1025_REG_VCC_LOW 0x36
#define ADM1025_REG_RTEMP_HIGH 0x37	
#define ADM1025_REG_RTEMP_LOW 0x38	
#define ADM1025_REG_TEMP_HIGH 0x39
#define ADM1025_REG_TEMP_LOW 0x3A

#define ADM1025_REG_CONFIG 0x40
#define ADM1025_REG_INT1_STAT 0x41
#define ADM1025_REG_INT2_STAT 0x42

#define ADM1025_REG_VID 0x47
#define ADM1025_REG_VID4 0x49

/* Conversions. Rounding and limit checking is only done on the TO_REG
   variants. Note that you should be a bit careful with which arguments
   these macros are called: arguments may be evaluated more than once.
   Fixing this is just not worth it. */
#define IN_TO_REG(val,nr) (SENSORS_LIMIT(((val) & 0xff),0,255))
#define IN_FROM_REG(val,nr) (val)

#define TEMP_FROM_REG(val) (((val)>0x80?(val)-0x100:(val))*10)
#define TEMP_LIMIT_FROM_REG(val) TEMP_FROM_REG(val)
#define TEMP_LIMIT_TO_REG(val) SENSORS_LIMIT(((val)<0?(((val)-5)/10):\
                                                      ((val)+5)/10), 0, 255)

#define ALARMS_FROM_REG(val) (val)

#define VID_FROM_REG(val) ((val)==0x1f?0:(val)>=0x10?510-(val)*10:\
                           205-(val)*5)

/* Initial limits */
#define ADM1025_INIT_IN_0 190
#define ADM1025_INIT_IN_1 190
#define ADM1025_INIT_IN_2 190
#define ADM1025_INIT_IN_3 190
#define ADM1025_INIT_IN_4 190
#define ADM1025_INIT_IN_5 190

#define ADM1025_INIT_IN_PERCENTAGE 10

#define ADM1025_INIT_IN_MIN_0 \
        (ADM1025_INIT_IN_0 - ADM1025_INIT_IN_0 * ADM1025_INIT_IN_PERCENTAGE / 100)
#define ADM1025_INIT_IN_MAX_0 \
        (ADM1025_INIT_IN_0 + ADM1025_INIT_IN_0 * ADM1025_INIT_IN_PERCENTAGE / 100)
#define ADM1025_INIT_IN_MIN_1 \
        (ADM1025_INIT_IN_1 - ADM1025_INIT_IN_1 * ADM1025_INIT_IN_PERCENTAGE / 100)
#define ADM1025_INIT_IN_MAX_1 \
        (ADM1025_INIT_IN_1 + ADM1025_INIT_IN_1 * ADM1025_INIT_IN_PERCENTAGE / 100)
#define ADM1025_INIT_IN_MIN_2 \
        (ADM1025_INIT_IN_2 - ADM1025_INIT_IN_2 * ADM1025_INIT_IN_PERCENTAGE / 100)
#define ADM1025_INIT_IN_MAX_2 \
        (ADM1025_INIT_IN_2 + ADM1025_INIT_IN_2 * ADM1025_INIT_IN_PERCENTAGE / 100)
#define ADM1025_INIT_IN_MIN_3 \
        (ADM1025_INIT_IN_3 - ADM1025_INIT_IN_3 * ADM1025_INIT_IN_PERCENTAGE / 100)
#define ADM1025_INIT_IN_MAX_3 \
        (ADM1025_INIT_IN_3 + ADM1025_INIT_IN_3 * ADM1025_INIT_IN_PERCENTAGE / 100)
#define ADM1025_INIT_IN_MIN_4 \
        (ADM1025_INIT_IN_4 - ADM1025_INIT_IN_4 * ADM1025_INIT_IN_PERCENTAGE / 100)
#define ADM1025_INIT_IN_MAX_4 \
        (ADM1025_INIT_IN_4 + ADM1025_INIT_IN_4 * ADM1025_INIT_IN_PERCENTAGE / 100)
#define ADM1025_INIT_IN_MIN_5 \
        (ADM1025_INIT_IN_5 - ADM1025_INIT_IN_5 * ADM1025_INIT_IN_PERCENTAGE / 100)
#define ADM1025_INIT_IN_MAX_5 \
        (ADM1025_INIT_IN_5 + ADM1025_INIT_IN_5 * ADM1025_INIT_IN_PERCENTAGE / 100)

#define ADM1025_INIT_RTEMP_MAX 600
#define ADM1025_INIT_RTEMP_MIN 0
#define ADM1025_INIT_TEMP_MAX 600
#define ADM1025_INIT_TEMP_MIN 0

#ifdef MODULE
extern int init_module(void);
extern int cleanup_module(void);
#endif				/* MODULE */

/* For each registered ADM1025, we need to keep some data in memory. That
   data is pointed to by adm1025_list[NR]->data. The structure itself is
   dynamically allocated, at the same time when a new adm1025 client is
   allocated. */
struct adm1025_data {
	int sysctl_id;
	enum chips type;

	struct semaphore update_lock;
	char valid;		/* !=0 if following fields are valid */
	unsigned long last_updated;	/* In jiffies */

	u8 in[6];		/* Register value */
	u8 in_max[6];		/* Register value */
	u8 in_min[6];		/* Register value */
	u8 rtemp;		/* Register value */
	u8 rtemp_max;		/* Register value */
	u8 rtemp_min;		/* Register value */
	u8 temp;		/* Register value */
	u8 temp_max;		/* Register value */
	u8 temp_min;		/* Register value */
	u16 alarms;		/* Register encoding, combined */
	u8 analog_out;		/* Register value */
	u8 vid;			/* Register value combined */
};


#ifdef MODULE
static
#else
extern
#endif
int __init sensors_adm1025_init(void);
static int __init adm1025_cleanup(void);

static int adm1025_attach_adapter(struct i2c_adapter *adapter);
static int adm1025_detect(struct i2c_adapter *adapter, int address,
			  unsigned short flags, int kind);
static int adm1025_detach_client(struct i2c_client *client);
static int adm1025_command(struct i2c_client *client, unsigned int cmd,
			   void *arg);
static void adm1025_inc_use(struct i2c_client *client);
static void adm1025_dec_use(struct i2c_client *client);

static int adm1025_read_value(struct i2c_client *client, u8 register);
static int adm1025_write_value(struct i2c_client *client, u8 register,
			       u8 value);
static void adm1025_update_client(struct i2c_client *client);
static void adm1025_init_client(struct i2c_client *client);


static void adm1025_in(struct i2c_client *client, int operation,
		       int ctl_name, int *nrels_mag, long *results);
static void adm1025_temp(struct i2c_client *client, int operation,
			 int ctl_name, int *nrels_mag, long *results);
static void adm1025_rm_temp(struct i2c_client *client, int operation,
			 int ctl_name, int *nrels_mag, long *results);
static void adm1025_alarms(struct i2c_client *client, int operation,
			   int ctl_name, int *nrels_mag, long *results);
/*static void adm1025_analog_out(struct i2c_client *client, int operation,
			       int ctl_name, int *nrels_mag,
			       long *results);*/
static void adm1025_vid(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);

/* I choose here for semi-static ADM1025 allocation. Complete dynamic
   allocation could also be used; the code needed for this would probably
   take more memory than the datastructure takes now. */
static int adm1025_id = 0;

static struct i2c_driver adm1025_driver = {
	/* name */ "ADM1025 sensor driver",
	/* id */ I2C_DRIVERID_ADM1025,
	/* flags */ I2C_DF_NOTIFY,
	/* attach_adapter */ &adm1025_attach_adapter,
	/* detach_client */ &adm1025_detach_client,
	/* command */ &adm1025_command,
	/* inc_use */ &adm1025_inc_use,
	/* dec_use */ &adm1025_dec_use
};

/* Used by adm1025_init/cleanup */
static int __initdata adm1025_initialized = 0;

/* The /proc/sys entries */
/* These files are created for each detected ADM1025. This is just a template;
   though at first sight, you might think we could use a statically
   allocated list, we need some way to get back to the parent - which
   is done through one of the 'extra' fields which are initialized 
   when a new copy is allocated. */
static ctl_table adm1025_dir_table_template[] = {
	{ADM1025_SYSCTL_IN0, "in0", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm1025_in},
	{ADM1025_SYSCTL_IN1, "in1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm1025_in},
	{ADM1025_SYSCTL_IN2, "in2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm1025_in},
	{ADM1025_SYSCTL_IN3, "in3", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm1025_in},
	{ADM1025_SYSCTL_IN4, "in4", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm1025_in},
	{ADM1025_SYSCTL_IN5, "in5", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm1025_in},
	{ADM1025_SYSCTL_RTEMP, "temp2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm1025_rm_temp},
	{ADM1025_SYSCTL_TEMP, "temp1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm1025_temp},
	{ADM1025_SYSCTL_ALARMS, "alarms", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm1025_alarms},
/*	{ADM1025_SYSCTL_ANALOG_OUT, "analog_out", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm1025_analog_out},*/
	{ADM1025_SYSCTL_VID, "vid", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm1025_vid},
	{0}
};

int adm1025_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_detect(adapter, &addr_data, adm1025_detect);
}

static int adm1025_detect(struct i2c_adapter *adapter, int address,
			  unsigned short flags, int kind)
{
	int i;
	struct i2c_client *new_client;
	struct adm1025_data *data;
	int err = 0;
	const char *type_name = "";
	const char *client_name = "";

	/* Make sure we aren't probing the ISA bus!! This is just a safety check
	   at this moment; i2c_detect really won't call us. */
#ifdef DEBUG
	if (i2c_is_isa_adapter(adapter)) {
		printk
		    ("adm1025.o: adm1025_detect called for an ISA bus adapter?!?\n");
		return 0;
	}
#endif

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		goto ERROR0;

	/* OK. For now, we presume we have a valid client. We now create the
	   client structure, even though we cannot fill it completely yet.
	   But it allows us to access adm1025_{read,write}_value. */

	if (!(new_client = kmalloc(sizeof(struct i2c_client) +
				   sizeof(struct adm1025_data),
				   GFP_KERNEL))) {
		err = -ENOMEM;
		goto ERROR0;
	}

	data = (struct adm1025_data *) (new_client + 1);
	new_client->addr = address;
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &adm1025_driver;
	new_client->flags = 0;

	/* Now, we do the remaining detection. */

	if (kind < 0) {
		if((adm1025_read_value(new_client,ADM1025_REG_CONFIG) & 0x80) != 0x00)
			goto ERROR1;
	}

	/* Determine the chip type. */
	if (kind <= 0) {
		i = adm1025_read_value(new_client, ADM1025_REG_COMPANY_ID);
		if (i == 0x41)
			kind = adm1025;
		else {
			if (kind == 0)
				printk
				    ("adm1025.o: Ignoring 'force' parameter for unknown chip at "
				     "adapter %d, address 0x%02x\n",
				     i2c_adapter_id(adapter), address);
			goto ERROR1;
		}
	}

	if (kind == adm1025) {
		type_name = "adm1025";
		client_name = "ADM1025 chip";
	} else {
#ifdef DEBUG
		printk("adm1025.o: Internal error: unknown kind (%d)?!?",
		       kind);
#endif
		goto ERROR1;
	}

	/* Fill in the remaining client fields and put it into the global list */
	strcpy(new_client->name, client_name);
	data->type = kind;

	new_client->id = adm1025_id++;
	data->valid = 0;
	init_MUTEX(&data->update_lock);

	/* Tell the I2C layer a new client has arrived */
	if ((err = i2c_attach_client(new_client)))
		goto ERROR3;

	/* Register a new directory entry with module sensors */
	if ((i = i2c_register_entry(new_client,
					type_name,
					adm1025_dir_table_template,
					THIS_MODULE)) < 0) {
		err = i;
		goto ERROR4;
	}
	data->sysctl_id = i;

	/* Initialize the ADM1025 chip */
	adm1025_init_client(new_client);
	return 0;

/* OK, this is not exactly good programming practice, usually. But it is
   very code-efficient in this case. */

      ERROR4:
	i2c_detach_client(new_client);
      ERROR3:
      ERROR1:
	kfree(new_client);
      ERROR0:
	return err;
}

int adm1025_detach_client(struct i2c_client *client)
{
	int err;

	i2c_deregister_entry(((struct adm1025_data *) (client->data))->
				 sysctl_id);

	if ((err = i2c_detach_client(client))) {
		printk
		    ("adm1025.o: Client deregistration failed, client not detached.\n");
		return err;
	}

	kfree(client);

	return 0;

}

/* No commands defined yet */
int adm1025_command(struct i2c_client *client, unsigned int cmd, void *arg)
{
	return 0;
}

void adm1025_inc_use(struct i2c_client *client)
{
#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif
}

void adm1025_dec_use(struct i2c_client *client)
{
#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
}

int adm1025_read_value(struct i2c_client *client, u8 reg)
{
	return 0xFF & i2c_smbus_read_byte_data(client, reg);
}

int adm1025_write_value(struct i2c_client *client, u8 reg, u8 value)
{
	return i2c_smbus_write_byte_data(client, reg, value);
}

/* Called when we have found a new ADM1025. It should set limits, etc. */
void adm1025_init_client(struct i2c_client *client)
{
	/* Reset all except Watchdog values and last conversion values
	   This sets fan-divs to 2, among others. This makes most other
	   initializations unnecessary */
	adm1025_write_value(client, ADM1025_REG_CONFIG, 0x80);

	adm1025_write_value(client, ADM1025_REG_IN_MIN(0),
			    IN_TO_REG(ADM1025_INIT_IN_MIN_0, 0));
	adm1025_write_value(client, ADM1025_REG_IN_MAX(0),
			    IN_TO_REG(ADM1025_INIT_IN_MAX_0, 0));
	adm1025_write_value(client, ADM1025_REG_IN_MIN(1),
			    IN_TO_REG(ADM1025_INIT_IN_MIN_1, 1));
	adm1025_write_value(client, ADM1025_REG_IN_MAX(1),
			    IN_TO_REG(ADM1025_INIT_IN_MAX_1, 1));
	adm1025_write_value(client, ADM1025_REG_IN_MIN(2),
			    IN_TO_REG(ADM1025_INIT_IN_MIN_2, 2));
	adm1025_write_value(client, ADM1025_REG_IN_MAX(2),
			    IN_TO_REG(ADM1025_INIT_IN_MAX_2, 2));
	adm1025_write_value(client, ADM1025_REG_IN_MIN(3),
			    IN_TO_REG(ADM1025_INIT_IN_MIN_3, 3));
	adm1025_write_value(client, ADM1025_REG_IN_MAX(3),
			    IN_TO_REG(ADM1025_INIT_IN_MAX_3, 3));
	adm1025_write_value(client, ADM1025_REG_IN_MIN(4),
			    IN_TO_REG(ADM1025_INIT_IN_MIN_4, 4));
	adm1025_write_value(client, ADM1025_REG_IN_MAX(4),
			    IN_TO_REG(ADM1025_INIT_IN_MAX_4, 4));
	adm1025_write_value(client, ADM1025_REG_IN_MIN(5),
			    IN_TO_REG(ADM1025_INIT_IN_MIN_5, 5));
	adm1025_write_value(client, ADM1025_REG_IN_MAX(5),
			    IN_TO_REG(ADM1025_INIT_IN_MAX_5, 5));

	adm1025_write_value(client, ADM1025_REG_RTEMP_HIGH,
			    TEMP_LIMIT_TO_REG(ADM1025_INIT_RTEMP_MAX));
	adm1025_write_value(client, ADM1025_REG_RTEMP_LOW,
			    TEMP_LIMIT_TO_REG(ADM1025_INIT_RTEMP_MIN));
	adm1025_write_value(client, ADM1025_REG_TEMP_HIGH,
			    TEMP_LIMIT_TO_REG(ADM1025_INIT_TEMP_MAX));
	adm1025_write_value(client, ADM1025_REG_TEMP_LOW,
			    TEMP_LIMIT_TO_REG(ADM1025_INIT_TEMP_MIN));

	/* Start monitoring */
	adm1025_write_value(client, ADM1025_REG_CONFIG, 0x01);
}

void adm1025_update_client(struct i2c_client *client)
{
	struct adm1025_data *data = client->data;
	u8 i;

	down(&data->update_lock);

	if (
	    (jiffies - data->last_updated >
	     (data->type == adm1025 ? HZ / 2 : HZ * 2))
	    || (jiffies < data->last_updated) || !data->valid) {

#ifdef DEBUG
		printk("Starting adm1025 update\n");
#endif
		for (i = 0; i <= 5; i++) {
			data->in[i] =
			    adm1025_read_value(client, ADM1025_REG_IN(i));
			data->in_min[i] =
			    adm1025_read_value(client,
					       ADM1025_REG_IN_MIN(i));
			data->in_max[i] =
			    adm1025_read_value(client,
					       ADM1025_REG_IN_MAX(i));
		}
		data->temp =
		    adm1025_read_value(client, ADM1025_REG_TEMP);
		data->rtemp =
		    adm1025_read_value(client, ADM1025_REG_RTEMP);
#ifdef DEBUG
		printk("The temp is %2x\n",data->temp);
#endif
		data->temp_max =
		    adm1025_read_value(client, ADM1025_REG_TEMP_HIGH);
		data->temp_min =
		    adm1025_read_value(client, ADM1025_REG_TEMP_LOW);
		data->rtemp_max =
		    adm1025_read_value(client, ADM1025_REG_RTEMP_HIGH);
		data->rtemp_min =
		    adm1025_read_value(client, ADM1025_REG_RTEMP_LOW);

		i = adm1025_read_value(client, ADM1025_REG_VID);
		data->vid = i & 0x0f;
		data->vid |=
		    (adm1025_read_value(client, ADM1025_REG_VID4) & 0x01)
		    << 4;

		data->alarms =
		    adm1025_read_value(client,
				       ADM1025_REG_INT1_STAT) +
		    (adm1025_read_value(client, ADM1025_REG_INT2_STAT) <<
		     8);
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
void adm1025_in(struct i2c_client *client, int operation, int ctl_name,
		int *nrels_mag, long *results)
{

	int scales[6] = { 250, 225, 330, 500, 1200, 330 };

	struct adm1025_data *data = client->data;
	int nr = ctl_name - ADM1025_SYSCTL_IN0;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 2;
	else if (operation == SENSORS_PROC_REAL_READ) {
		adm1025_update_client(client);
		results[0] =
		    IN_FROM_REG(data->in_min[nr], nr) * scales[nr] / 192;
		results[1] =
		    IN_FROM_REG(data->in_max[nr], nr) * scales[nr] / 192;
		results[2] =
		    IN_FROM_REG(data->in[nr], nr) * scales[nr] / 192;
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->in_min[nr] =
			    IN_TO_REG((results[0] * 192) / scales[nr], nr);
			adm1025_write_value(client, ADM1025_REG_IN_MIN(nr),
					    data->in_min[nr]);
		}
		if (*nrels_mag >= 2) {
			data->in_max[nr] =
			    IN_TO_REG((results[1] * 192) / scales[nr], nr);
			adm1025_write_value(client, ADM1025_REG_IN_MAX(nr),
					    data->in_max[nr]);
		}
	}
}


void adm1025_temp(struct i2c_client *client, int operation, int ctl_name,
		  int *nrels_mag, long *results)
{
	struct adm1025_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 1;
	else if (operation == SENSORS_PROC_REAL_READ) {
		adm1025_update_client(client);
		results[0] = TEMP_LIMIT_FROM_REG(data->temp_max);
		results[1] = TEMP_LIMIT_FROM_REG(data->temp_min);
		results[2] = TEMP_FROM_REG(data->temp);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->temp_max = TEMP_LIMIT_TO_REG(results[0]);
			adm1025_write_value(client, ADM1025_REG_TEMP_HIGH,
					    data->temp_max);
		}
		if (*nrels_mag >= 2) {
			data->temp_min = TEMP_LIMIT_TO_REG(results[1]);
			adm1025_write_value(client, ADM1025_REG_TEMP_LOW,
					    data->temp_min);
		}
	}
}

void adm1025_rm_temp(struct i2c_client *client, int operation, int ctl_name,
		  int *nrels_mag, long *results)
{
	struct adm1025_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 1;
	else if (operation == SENSORS_PROC_REAL_READ) {
		adm1025_update_client(client);
		results[0] = TEMP_LIMIT_FROM_REG(data->rtemp_max);
		results[1] = TEMP_LIMIT_FROM_REG(data->rtemp_min);
		results[2] = TEMP_FROM_REG(data->rtemp);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->rtemp_max = TEMP_LIMIT_TO_REG(results[0]);
			adm1025_write_value(client, ADM1025_REG_RTEMP_HIGH,
					    data->rtemp_max);
		}
		if (*nrels_mag >= 2) {
			data->rtemp_min = TEMP_LIMIT_TO_REG(results[1]);
			adm1025_write_value(client, ADM1025_REG_RTEMP_LOW,
					    data->rtemp_min);
		}
	}
}

void adm1025_alarms(struct i2c_client *client, int operation, int ctl_name,
		    int *nrels_mag, long *results)
{
	struct adm1025_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		adm1025_update_client(client);
		results[0] = ALARMS_FROM_REG(data->alarms);
		*nrels_mag = 1;
	}
}
/*
void adm1025_analog_out(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results)
{
	struct adm1025_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		adm1025_update_client(client);
		results[0] = data->analog_out;
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->analog_out = results[0];
			adm1025_write_value(client, ADM1025_REG_ANALOG_OUT,
					    data->analog_out);
		}
	}
}
*/

void adm1025_vid(struct i2c_client *client, int operation, int ctl_name,
		 int *nrels_mag, long *results)
{
	struct adm1025_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 2;
	else if (operation == SENSORS_PROC_REAL_READ) {
		adm1025_update_client(client);
		results[0] = VID_FROM_REG(data->vid);
		*nrels_mag = 1;
	}
}

int __init sensors_adm1025_init(void)
{
	int res;

	printk("adm1025.o version %s (%s)\n", LM_VERSION, LM_DATE);
	adm1025_initialized = 0;

	if ((res = i2c_add_driver(&adm1025_driver))) {
		printk
		    ("adm1025.o: Driver registration failed, module not inserted.\n");
		adm1025_cleanup();
		return res;
	}
	adm1025_initialized++;
	return 0;
}

int __init adm1025_cleanup(void)
{
	int res;

	if (adm1025_initialized >= 1) {
		if ((res = i2c_del_driver(&adm1025_driver))) {
			printk
			    ("adm1025.o: Driver deregistration failed, module not removed.\n");
			return res;
		}
		adm1025_initialized--;
	}
	return 0;
}

EXPORT_NO_SYMBOLS;

#ifdef MODULE

MODULE_AUTHOR
    ("Frodo Looijaard <frodol@dds.nl> and Philip Edelbrock <phil@netroedge.com>");
MODULE_DESCRIPTION("ADM1025 driver");

int init_module(void)
{
	return sensors_adm1025_init();
}

int cleanup_module(void)
{
	return adm1025_cleanup();
}

#endif				/* MODULE */
