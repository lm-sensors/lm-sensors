/*
    adm1025.c - Part of lm_sensors, Linux kernel modules for hardware
               monitoring
    Copyright (c) 2000 Chen-Yuan Wu <gwu@esoft.com>
    Copyright (c) 2003-2004 Jean Delvare <khali@linux-fr.org>

    Based on the adm9240 driver.

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

/* Supports the Analog Devices ADM1025 and the Philips NE1619.
   See doc/chips/adm1025 for details */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/i2c-proc.h>
#include <linux/init.h>
#include "version.h"
#include "sensors_vid.h"

MODULE_LICENSE("GPL");

/* Addresses to scan */
static unsigned short normal_i2c[] = { SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { 0x2c, 0x2e, SENSORS_I2C_END };
static unsigned int normal_isa[] = { SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/* Insmod parameters */
SENSORS_INSMOD_2(adm1025, ne1619);

/* Many ADM1025 constants specified below */


/* The ADM1025 registers */

/* These are all read-only */
#define ADM1025_REG_2_5V        0x20 /* not used directly, see   */
#define ADM1025_REG_VCCP1       0x21 /* ADM1025_REG_IN(nr) below */
#define ADM1025_REG_3_3V        0x22
#define ADM1025_REG_5V          0x23
#define ADM1025_REG_12V         0x24
#define ADM1025_REG_VCC         0x25

#define ADM1025_REG_RTEMP       0x26 /* not used directly, see     */
#define ADM1025_REG_LTEMP       0x27 /* ADM1025_REG_TEMP(nr) below */

#define ADM1025_REG_COMPANY_ID  0x3E /* 0x41 for Analog Devices,
                                        0xA1 for Philips */
#define ADM1025_REG_DIE_REV     0x3F /* 0x20-0x2F for ADM1025 and compatible */

#define ADM1025_REG_STATUS1     0x41
#define ADM1025_REG_STATUS2     0x42

#define ADM1025_REG_VID         0x47
#define ADM1025_REG_VID4        0x49 /* actually R/W
                                        but we don't write to it */

/* These are read/write */
#define ADM1025_REG_2_5V_HIGH   0x2B /* not used directly, see       */
#define ADM1025_REG_2_5V_LOW    0x2C /* ADM1025_REG_IN_MAX(nr) and   */
#define ADM1025_REG_VCCP1_HIGH  0x2D /* ADM1025_REG_IN_MIN(nr) below */
#define ADM1025_REG_VCCP1_LOW   0x2E
#define ADM1025_REG_3_3V_HIGH   0x2F
#define ADM1025_REG_3_3V_LOW    0x30
#define ADM1025_REG_5V_HIGH     0x31
#define ADM1025_REG_5V_LOW      0x32
#define ADM1025_REG_12V_HIGH    0x33
#define ADM1025_REG_12V_LOW     0x34
#define ADM1025_REG_VCC_HIGH    0x35
#define ADM1025_REG_VCC_LOW     0x36

#define ADM1025_REG_RTEMP_HIGH  0x37 /* not used directly, see         */
#define ADM1025_REG_RTEMP_LOW   0x38 /* ADM1025_REG_TEMP_MAX(nr) and   */
#define ADM1025_REG_LTEMP_HIGH  0x39 /* ADM1025_REG_TEMP_MIN(nr) below */
#define ADM1025_REG_LTEMP_LOW   0x3A

#define ADM1025_REG_CONFIG      0x40

/* Useful macros */
#define ADM1025_REG_IN(nr)        (ADM1025_REG_2_5V + (nr))
#define ADM1025_REG_IN_MAX(nr)    (ADM1025_REG_2_5V_HIGH + (nr) * 2)
#define ADM1025_REG_IN_MIN(nr)    (ADM1025_REG_2_5V_LOW + (nr) * 2)
#define ADM1025_REG_TEMP(nr)      (ADM1025_REG_RTEMP + (nr))
#define ADM1025_REG_TEMP_HIGH(nr) (ADM1025_REG_RTEMP_HIGH + (nr) * 2)
#define ADM1025_REG_TEMP_LOW(nr)  (ADM1025_REG_RTEMP_LOW + (nr) * 2)

/* Conversions. Rounding and limit checking is only done on the TO_REG
   variants. Note that you should be a bit careful with which arguments
   these macros are called: arguments may be evaluated more than once.
   Fixing this is just not worth it. */
#define IN_TO_REG(val) SENSORS_LIMIT(val, 0, 255)
#define IN_FROM_REG(val) (val)

#define TEMP_FROM_REG(val) (((val)>=0x80?(val)-0x100:(val))*10)
#define TEMP_TO_REG(val) SENSORS_LIMIT(((val)<0?(((val)-5)/10):\
                                                 ((val)+5)/10),-128,127)

#define ALARMS_FROM_REG(val) (val)

/* For each registered ADM1025, we need to keep some data in memory. That
   data is pointed to by adm1025_list[NR]->data. The structure itself is
   dynamically allocated, at the same time when a new adm1025 client is
   allocated. */
struct adm1025_data {
	struct i2c_client client;
	int sysctl_id;
	enum chips type;

	struct semaphore update_lock;
	char valid;	              /* !=0 if following fields are valid */
	unsigned long last_updated;   /* In jiffies */

	u8 in[6];               /* Register value */
	u8 in_max[6];           /* Register value */
	u8 in_min[6];           /* Register value */
	u8 temp[2];             /* Register value */
	u8 temp_high[2];        /* Register value */
	u8 temp_low[2];         /* Register value */
	u16 alarms;             /* Register encoding, combined */
	u8 vid;                 /* Register value combined */
	u8 vrm;
};


static int adm1025_attach_adapter(struct i2c_adapter *adapter);
static int adm1025_detect(struct i2c_adapter *adapter, int address,
			  unsigned short flags, int kind);
static int adm1025_detach_client(struct i2c_client *client);
static void adm1025_update_client(struct i2c_client *client);
static void adm1025_init_client(struct i2c_client *client);


static void adm1025_in(struct i2c_client *client, int operation,
		       int ctl_name, int *nrels_mag, long *results);
static void adm1025_temp(struct i2c_client *client, int operation,
			 int ctl_name, int *nrels_mag, long *results);
static void adm1025_alarms(struct i2c_client *client, int operation,
			   int ctl_name, int *nrels_mag, long *results);
static void adm1025_vid(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void adm1025_vrm(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);

static struct i2c_driver adm1025_driver = {
	.name		= "ADM1025 sensor driver",
	.id		= I2C_DRIVERID_ADM1025,
	.flags		= I2C_DF_NOTIFY,
	.attach_adapter	= adm1025_attach_adapter,
	.detach_client	= adm1025_detach_client,
};

/* The /proc/sys entries */
/* -- SENSORS SYSCTL START -- */

#define ADM1025_SYSCTL_IN0     1000 /* Volts * 100 */
#define ADM1025_SYSCTL_IN1     1001
#define ADM1025_SYSCTL_IN2     1002
#define ADM1025_SYSCTL_IN3     1003
#define ADM1025_SYSCTL_IN4     1004
#define ADM1025_SYSCTL_IN5     1005

#define ADM1025_SYSCTL_RTEMP   1250 /* Degrees Celcius * 10 */
#define ADM1025_SYSCTL_TEMP    1251

#define ADM1025_SYSCTL_ALARMS  2001 /* bitvector */
#define ADM1025_SYSCTL_VID     2003 /* Volts * 1000 */
#define ADM1025_SYSCTL_VRM     2004

#define ADM1025_ALARM_IN0     0x0001
#define ADM1025_ALARM_IN1     0x0002
#define ADM1025_ALARM_IN2     0x0004
#define ADM1025_ALARM_IN3     0x0008
#define ADM1025_ALARM_IN4     0x0100
#define ADM1025_ALARM_IN5     0x0200
#define ADM1025_ALARM_RTEMP   0x0020
#define ADM1025_ALARM_TEMP    0x0010
#define ADM1025_ALARM_RFAULT  0x4000

/* -- SENSORS SYSCTL END -- */

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
	{ADM1025_SYSCTL_RTEMP, "temp1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm1025_temp},
	{ADM1025_SYSCTL_TEMP, "temp2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm1025_temp},
	{ADM1025_SYSCTL_ALARMS, "alarms", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm1025_alarms},
	{ADM1025_SYSCTL_VID, "vid", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm1025_vid},
	{ADM1025_SYSCTL_VRM, "vrm", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm1025_vrm},
	{0}
};

static int adm1025_attach_adapter(struct i2c_adapter *adapter)
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

	if (!(data = kmalloc(sizeof(struct adm1025_data), GFP_KERNEL))) {
		err = -ENOMEM;
		goto ERROR0;
	}

	new_client = &data->client;
	new_client->addr = address;
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &adm1025_driver;
	new_client->flags = 0;

	/* Now, we do the remaining detection. */

	if (kind < 0) {
		if ((i2c_smbus_read_byte_data(new_client,
		     ADM1025_REG_CONFIG) & 0x80) != 0x00
		 || (i2c_smbus_read_byte_data(new_client,
		     ADM1025_REG_STATUS1) & 0xC0) != 0x00
		 || (i2c_smbus_read_byte_data(new_client,
		     ADM1025_REG_STATUS2) & 0xBC) != 0x00)
			goto ERROR1;
	}

	/* Determine the chip type. */
	if (kind <= 0) {
		u8 man_id, chip_id;
		
		man_id = i2c_smbus_read_byte_data(new_client,
			 ADM1025_REG_COMPANY_ID);
		chip_id = i2c_smbus_read_byte_data(new_client,
			  ADM1025_REG_DIE_REV);
		
		if (man_id == 0x41) { /* Analog Devices */
			if ((chip_id & 0xF0) == 0x20) /* ADM1025 */
				kind = adm1025;
		} else if (man_id == 0xA1) { /* Philips */
			if (address != 0x2E
			 && (chip_id & 0xF0) == 0x20) /* NE1619 */
				kind = ne1619;
		}
	}

	if (kind <= 0) { /* Identification failed */
		printk("adm1025.o: Unsupported chip.\n");
		goto ERROR1;
	}

	if (kind == adm1025) {
		type_name = "adm1025";
		client_name = "ADM1025 chip";
	} else if (kind == ne1619) {
		type_name = "ne1619";
		client_name = "NE1619 chip";		
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
	kfree(data);
      ERROR0:
	return err;
}

static int adm1025_detach_client(struct i2c_client *client)
{
	int err;

	i2c_deregister_entry(((struct adm1025_data *) (client->data))->
				 sysctl_id);

	if ((err = i2c_detach_client(client))) {
		printk
		    ("adm1025.o: Client deregistration failed, client not detached.\n");
		return err;
	}

	kfree(client->data);

	return 0;
}

/* Called when we have found a new ADM1025. */
static void adm1025_init_client(struct i2c_client *client)
{
	struct adm1025_data *data = client->data;
	u8 reg;
	int i;

	data->vrm = 82;

	/* Set high limits
	   Usually we avoid setting limits on driver init, but it happens
	   that the ADM1025 comes with stupid default limits (all registers
	   set to 0). In case the chip has not gone through any limit
	   setting yet, we better set the high limits to the max so that
	   no alarm triggers. */
	for (i=0; i<6; i++) {
		reg = i2c_smbus_read_byte_data(client,
					       ADM1025_REG_IN_MAX(i));
		if (reg == 0)
			i2c_smbus_write_byte_data(client,
						  ADM1025_REG_IN_MAX(i),
						  0xFF);
	}
	for (i=0; i<2; i++) {
		reg = i2c_smbus_read_byte_data(client,
					       ADM1025_REG_TEMP_HIGH(i));
		if (reg == 0)
			i2c_smbus_write_byte_data(client,
						  ADM1025_REG_TEMP_HIGH(i),
						  0x7F);
	}

	/* Start monitoring */
	reg = i2c_smbus_read_byte_data(client, ADM1025_REG_CONFIG);
	i2c_smbus_write_byte_data(client, ADM1025_REG_CONFIG, (reg|0x01)&0x7F);
}

static void adm1025_update_client(struct i2c_client *client)
{
	struct adm1025_data *data = client->data;
	u8 nr;

	down(&data->update_lock);

	if ((jiffies - data->last_updated > 2 * HZ)
	 || (jiffies < data->last_updated) || !data->valid) {
#ifdef DEBUG
		printk("Starting adm1025 update\n");
#endif

		/* Voltages */
		for (nr = 0; nr < 6; nr++) {
			data->in[nr] = i2c_smbus_read_byte_data(client, ADM1025_REG_IN(nr));
			data->in_min[nr] = i2c_smbus_read_byte_data(client, ADM1025_REG_IN_MIN(nr));
			data->in_max[nr] = i2c_smbus_read_byte_data(client, ADM1025_REG_IN_MAX(nr));
		}

		/* Temperatures */
		for (nr = 0; nr < 2; nr++) {
			data->temp[nr] = i2c_smbus_read_byte_data(client, ADM1025_REG_TEMP(nr));
			data->temp_high[nr] = i2c_smbus_read_byte_data(client, ADM1025_REG_TEMP_HIGH(nr));
			data->temp_low[nr] = i2c_smbus_read_byte_data(client, ADM1025_REG_TEMP_LOW(nr));
		}

		/* VID */
		data->vid = (i2c_smbus_read_byte_data(client, ADM1025_REG_VID) & 0x0f)
		         + ((i2c_smbus_read_byte_data(client, ADM1025_REG_VID4) & 0x01) << 4);

		/* Alarms */
		data->alarms = (i2c_smbus_read_byte_data(client, ADM1025_REG_STATUS1) & 0x3f)
		            + ((i2c_smbus_read_byte_data(client, ADM1025_REG_STATUS2) & 0x43) << 8);

		data->last_updated = jiffies;
		data->valid = 1;
	}

	up(&data->update_lock);
}


/* The next few functions are the call-back functions of the /proc/sys and
   sysctl files. Which function is used is defined in the ctl_table in
   the extra1 field.
   Each function must return the magnitude (power of 10 to divide the data
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
		results[0] = (IN_FROM_REG(data->in_min[nr]) * scales[nr] + 96) / 192;
		results[1] = (IN_FROM_REG(data->in_max[nr]) * scales[nr] + 96) / 192;
		results[2] = (IN_FROM_REG(data->in[nr]) * scales[nr] + 96) / 192;
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->in_min[nr] = IN_TO_REG((results[0] * 192 + scales[nr] / 2)
					   / scales[nr]);
			i2c_smbus_write_byte_data(client, ADM1025_REG_IN_MIN(nr),
					    data->in_min[nr]);
		}
		if (*nrels_mag >= 2) {
			data->in_max[nr] = IN_TO_REG((results[1] * 192 + scales[nr] / 2)
					   / scales[nr]);
			i2c_smbus_write_byte_data(client, ADM1025_REG_IN_MAX(nr),
					    data->in_max[nr]);
		}
	}
}

void adm1025_temp(struct i2c_client *client, int operation, int ctl_name,
		  int *nrels_mag, long *results)
{
	struct adm1025_data *data = client->data;
	int nr = ctl_name - ADM1025_SYSCTL_RTEMP;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 1;
	else if (operation == SENSORS_PROC_REAL_READ) {
		adm1025_update_client(client);
		results[0] = TEMP_FROM_REG(data->temp_high[nr]);
		results[1] = TEMP_FROM_REG(data->temp_low[nr]);
		results[2] = TEMP_FROM_REG(data->temp[nr]);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->temp_high[nr] = TEMP_TO_REG(results[0]);
			i2c_smbus_write_byte_data(client, ADM1025_REG_TEMP_HIGH(nr),
					    data->temp_high[nr]);
		}
		if (*nrels_mag >= 2) {
			data->temp_low[nr] = TEMP_TO_REG(results[1]);
			i2c_smbus_write_byte_data(client, ADM1025_REG_TEMP_LOW(nr),
					    data->temp_low[nr]);
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

void adm1025_vid(struct i2c_client *client, int operation, int ctl_name,
		 int *nrels_mag, long *results)
{
	struct adm1025_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 3;
	else if (operation == SENSORS_PROC_REAL_READ) {
		adm1025_update_client(client);
		results[0] = vid_from_reg(data->vid, data->vrm);
		*nrels_mag = 1;
	}
}

void adm1025_vrm(struct i2c_client *client, int operation, int ctl_name,
		 int *nrels_mag, long *results)
{
	struct adm1025_data *data = client->data;
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

static int __init sm_adm1025_init(void)
{
	printk("adm1025.o version %s (%s)\n", LM_VERSION, LM_DATE);
	return i2c_add_driver(&adm1025_driver);
}

static void __exit sm_adm1025_exit(void)
{
	i2c_del_driver(&adm1025_driver);
}



MODULE_AUTHOR("Chen-Yuan Wu <gwu@esoft.com>"
	" and Jean Delvare <khali@linux-fr.org>");
MODULE_DESCRIPTION("ADM1025 driver");

module_init(sm_adm1025_init);
module_exit(sm_adm1025_exit);
