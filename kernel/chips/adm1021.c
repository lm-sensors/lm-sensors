/*
    adm1021.c - Part of lm_sensors, Linux kernel modules for hardware
             monitoring
    Copyright (c) 1998, 1999  Frodo Looijaard <frodol@dds.nl> and
    Philip Edelbrock <phil@netroedge.com>

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
#include "version.h"

/* Addresses to scan */
static unsigned short normal_i2c[] = { SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { 0x18, 0x1a, 0x29, 0x2b,
	0x4c, 0x4e, SENSORS_I2C_END
};
static unsigned int normal_isa[] = { SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/* Insmod parameters */
SENSORS_INSMOD_8(adm1021, adm1023, max1617, max1617a, thmc10, lm84, gl523sm, mc1066);

/* adm1021 constants specified below */

/* The adm1021 registers */
/* Read-only */
#define ADM1021_REG_TEMP 0x00
#define ADM1021_REG_REMOTE_TEMP 0x01
#define ADM1021_REG_STATUS 0x02
#define ADM1021_REG_MAN_ID 0x0FE	/* 0x41 = Analog Devices, 0x49 = TI,
                                       0x4D = Maxim, 0x23 = Genesys , 0x54 = Onsemi*/
#define ADM1021_REG_DEV_ID 0x0FF	/* ADM1021 = 0x0X, ADM1021A/ADM1023 = 0x3X */
#define ADM1021_REG_DIE_CODE 0x0FF	/* MAX1617A */
/* These use different addresses for reading/writing */
#define ADM1021_REG_CONFIG_R 0x03
#define ADM1021_REG_CONFIG_W 0x09
#define ADM1021_REG_CONV_RATE_R 0x04
#define ADM1021_REG_CONV_RATE_W 0x0A
/* These are for the ADM1023's additional precision on the remote temp sensor */
#define ADM1021_REG_REM_TEMP_PREC 0x010
#define ADM1021_REG_REM_OFFSET 0x011
#define ADM1021_REG_REM_OFFSET_PREC 0x012
#define ADM1021_REG_REM_TOS_PREC 0x013
#define ADM1021_REG_REM_THYST_PREC 0x014
/* limits */
#define ADM1021_REG_TOS_R 0x05
#define ADM1021_REG_TOS_W 0x0B
#define ADM1021_REG_REMOTE_TOS_R 0x07
#define ADM1021_REG_REMOTE_TOS_W 0x0D
#define ADM1021_REG_THYST_R 0x06
#define ADM1021_REG_THYST_W 0x0C
#define ADM1021_REG_REMOTE_THYST_R 0x08
#define ADM1021_REG_REMOTE_THYST_W 0x0E
/* write-only */
#define ADM1021_REG_ONESHOT 0x0F

#define ADM1021_ALARM_TEMP (ADM1021_ALARM_TEMP_HIGH | ADM1021_ALARM_TEMP_LOW)
#define ADM1021_ALARM_RTEMP (ADM1021_ALARM_RTEMP_HIGH | ADM1021_ALARM_RTEMP_LOW\
                             | ADM1021_ALARM_RTEMP_NA)
#define ADM1021_ALARM_ALL  (ADM1021_ALARM_TEMP | ADM1021_ALARM_RTEMP)

/* Conversions. Rounding and limit checking is only done on the TO_REG
   variants. Note that you should be a bit careful with which arguments
   these macros are called: arguments may be evaluated more than once.
   Fixing this is just not worth it. */
/* Conversions  note: 1021 uses normal integer signed-byte format*/
#define TEMP_FROM_REG(val) (val > 127 ? val-256 : val)
#define TEMP_TO_REG(val)   (SENSORS_LIMIT((val < 0 ? val+256 : val),0,255))

/* Each client has this additional data */
struct adm1021_data {
	struct i2c_client client;
	int sysctl_id;
	enum chips type;

	struct semaphore update_lock;
	char valid;		/* !=0 if following fields are valid */
	unsigned long last_updated;	/* In jiffies */

	u8 temp, temp_os, temp_hyst;	/* Register values */
	u8 remote_temp, remote_temp_os, remote_temp_hyst, alarms, die_code;
	u8 fail;
        /* Special values for ADM1023 only */
	u8 remote_temp_prec, remote_temp_os_prec, remote_temp_hyst_prec, 
	   remote_temp_offset, remote_temp_offset_prec;
};

static int adm1021_attach_adapter(struct i2c_adapter *adapter);
static int adm1021_detect(struct i2c_adapter *adapter, int address,
			  unsigned short flags, int kind);
static void adm1021_init_client(struct i2c_client *client);
static int adm1021_detach_client(struct i2c_client *client);
static int adm1021_read_value(struct i2c_client *client, u8 reg);
static int adm1021_rd_good(u8 *val, struct i2c_client *client, u8 reg, u8 mask);
static int adm1021_write_value(struct i2c_client *client, u8 reg,
			       u16 value);
static void adm1021_temp(struct i2c_client *client, int operation,
			 int ctl_name, int *nrels_mag, long *results);
static void adm1021_remote_temp(struct i2c_client *client, int operation,
				int ctl_name, int *nrels_mag,
				long *results);
static void adm1021_alarms(struct i2c_client *client, int operation,
			   int ctl_name, int *nrels_mag, long *results);
static void adm1021_die_code(struct i2c_client *client, int operation,
			     int ctl_name, int *nrels_mag, long *results);
static void adm1021_update_client(struct i2c_client *client);

/* (amalysh) read only mode, otherwise any limit's writing confuse BIOS */
static int read_only = 0;


/* This is the driver that will be inserted */
static struct i2c_driver adm1021_driver = {
	.name		= "ADM1021, MAX1617 sensor driver",
	.id		= I2C_DRIVERID_ADM1021,
	.flags		= I2C_DF_NOTIFY,
	.attach_adapter	= adm1021_attach_adapter,
	.detach_client	= adm1021_detach_client,
};

/* -- SENSORS SYSCTL START -- */

#define ADM1021_SYSCTL_TEMP 1200
#define ADM1021_SYSCTL_REMOTE_TEMP 1201
#define ADM1021_SYSCTL_DIE_CODE 1202
#define ADM1021_SYSCTL_ALARMS 1203

#define ADM1021_ALARM_TEMP_HIGH 0x40
#define ADM1021_ALARM_TEMP_LOW 0x20
#define ADM1021_ALARM_RTEMP_HIGH 0x10
#define ADM1021_ALARM_RTEMP_LOW 0x08
#define ADM1021_ALARM_RTEMP_NA 0x04

/* -- SENSORS SYSCTL END -- */

/* These files are created for each detected adm1021. This is just a template;
   though at first sight, you might think we could use a statically
   allocated list, we need some way to get back to the parent - which
   is done through one of the 'extra' fields which are initialized
   when a new copy is allocated. */
static ctl_table adm1021_dir_table_template[] = {
	{ADM1021_SYSCTL_TEMP, "temp1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm1021_temp},
	{ADM1021_SYSCTL_REMOTE_TEMP, "temp2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm1021_remote_temp},
	{ADM1021_SYSCTL_DIE_CODE, "die_code", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm1021_die_code},
	{ADM1021_SYSCTL_ALARMS, "alarms", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm1021_alarms},
	{0}
};

static ctl_table adm1021_max_dir_table_template[] = {
	{ADM1021_SYSCTL_TEMP, "temp1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm1021_temp},
	{ADM1021_SYSCTL_REMOTE_TEMP, "temp2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm1021_remote_temp},
	{ADM1021_SYSCTL_ALARMS, "alarms", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &adm1021_alarms},
	{0}
};

static int adm1021_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_detect(adapter, &addr_data, adm1021_detect);
}

static int adm1021_detect(struct i2c_adapter *adapter, int address,
			  unsigned short flags, int kind)
{
	int i;
	struct i2c_client *new_client;
	struct adm1021_data *data;
	int err = 0;
	const char *type_name = "";
	const char *client_name = "";

	/* Make sure we aren't probing the ISA bus!! This is just a safety check
	   at this moment; i2c_detect really won't call us. */
#ifdef DEBUG
	if (i2c_is_isa_adapter(adapter)) {
		printk
		    ("adm1021.o: adm1021_detect called for an ISA bus adapter?!?\n");
		return 0;
	}
#endif

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		goto error0;

	/* OK. For now, we presume we have a valid client. We now create the
	   client structure, even though we cannot fill it completely yet.
	   But it allows us to access adm1021_{read,write}_value. */

	if (!(data = kmalloc(sizeof(struct adm1021_data), GFP_KERNEL))) {
		err = -ENOMEM;
		goto error0;
	}

	new_client = &data->client;
	new_client->addr = address;
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &adm1021_driver;
	new_client->flags = 0;

	/* Now, we do the remaining detection. */

	if (kind < 0) {
		if ((adm1021_read_value(new_client, ADM1021_REG_STATUS) & 0x03) != 0x00
		 || (adm1021_read_value(new_client, ADM1021_REG_CONFIG_R) & 0x3F) != 0x00
		 || (adm1021_read_value(new_client, ADM1021_REG_CONV_RATE_R) & 0xF8) != 0x00)
 			goto error1;
	}

	/* Determine the chip type. */

	if (kind <= 0) {
		i = adm1021_read_value(new_client, ADM1021_REG_MAN_ID);
		if (i == 0x41)
		  if ((adm1021_read_value (new_client, ADM1021_REG_DEV_ID) & 0xF0) == 0x30)
			kind = adm1023;
		  else
			kind = adm1021;
		else if (i == 0x49)
			kind = thmc10;
		else if (i == 0x23)
			kind = gl523sm;
		else if ((i == 0x4d) &&
			 (adm1021_read_value
			  (new_client, ADM1021_REG_DEV_ID) == 0x01))
			kind = max1617a;
		else if (i == 0x54)
			kind = mc1066;
		/* LM84 Mfr ID in a different place, and it has more unused bits */
		else if (adm1021_read_value(new_client, ADM1021_REG_CONV_RATE_R) == 0x00
		      && (kind == 0 /* skip extra detection */
		       || ((adm1021_read_value(new_client, ADM1021_REG_CONFIG_R) & 0x7F) == 0x00
			&& (adm1021_read_value(new_client, ADM1021_REG_STATUS) & 0xAB) == 0x00)))
			kind = lm84;
		else
			kind = max1617;
	}

	if (kind == max1617) {
		type_name = "max1617";
		client_name = "MAX1617 chip";
	} else if (kind == max1617a) {
		type_name = "max1617a";
		client_name = "MAX1617A chip";
	} else if (kind == adm1021) {
		type_name = "adm1021";
		client_name = "ADM1021 chip";
	} else if (kind == adm1023) {
		type_name = "adm1023";
		client_name = "ADM1023 chip";
	} else if (kind == thmc10) {
		type_name = "thmc10";
		client_name = "THMC10 chip";
	} else if (kind == lm84) {
		type_name = "lm84";
		client_name = "LM84 chip";
	} else if (kind == gl523sm) {
		type_name = "gl523sm";
		client_name = "GL523SM chip";
	} else if (kind == mc1066) {
		type_name = "mc1066";
		client_name = "MC1066 chip";
	}

	/* Fill in the remaining client fields and put it into the global list */
	strcpy(new_client->name, client_name);
	data->type = kind;
	data->valid = 0;
	init_MUTEX(&data->update_lock);

	/* Tell the I2C layer a new client has arrived */
	if ((err = i2c_attach_client(new_client)))
		goto error3;

	/* Register a new directory entry with module sensors */
	if ((i = i2c_register_entry(new_client,	type_name,
					data->type == adm1021 ? adm1021_dir_table_template :
					adm1021_max_dir_table_template,
					THIS_MODULE)) < 0) {
		err = i;
		goto error4;
	}
	data->sysctl_id = i;

	/* Initialize the ADM1021 chip */
	if (kind != lm84)
		adm1021_init_client(new_client);
	return 0;

      error4:
	i2c_detach_client(new_client);
      error3:
      error1:
	kfree(data);
      error0:
	return err;
}

static void adm1021_init_client(struct i2c_client *client)
{
	/* Enable ADC and disable suspend mode */
	adm1021_write_value(client, ADM1021_REG_CONFIG_W,
		adm1021_read_value(client, ADM1021_REG_CONFIG_R) & 0xBF);
	/* Set Conversion rate to 1/sec (this can be tinkered with) */
	adm1021_write_value(client, ADM1021_REG_CONV_RATE_W, 0x04);
}

static int adm1021_detach_client(struct i2c_client *client)
{
	int err;

	i2c_deregister_entry(((struct adm1021_data *) (client->data))->
				 sysctl_id);

	if ((err = i2c_detach_client(client))) {
		printk
		    ("adm1021.o: Client deregistration failed, client not detached.\n");
		return err;
	}

	kfree(client->data);

	return 0;
}


/* All registers are byte-sized */
static int adm1021_read_value(struct i2c_client *client, u8 reg)
{
	return i2c_smbus_read_byte_data(client, reg);
}

/* only update value if read succeeded; set fail bit if failed */
static int adm1021_rd_good(u8 *val, struct i2c_client *client, u8 reg, u8 mask)
{
	int i;
	struct adm1021_data *data = client->data;

	i = i2c_smbus_read_byte_data(client, reg);
	if (i < 0) {
		data->fail |= mask;
		return i;
	}
	*val = i;
	return 0;
}

static int adm1021_write_value(struct i2c_client *client, u8 reg, u16 value)
{
	if (read_only > 0)
		return 0;

	return i2c_smbus_write_byte_data(client, reg, value);
}

static void adm1021_update_client(struct i2c_client *client)
{
	struct adm1021_data *data = client->data;

	down(&data->update_lock);

	if ((jiffies - data->last_updated > HZ + HZ / 2) ||
	    (jiffies < data->last_updated) || !data->valid) {

#ifdef DEBUG
		printk("Starting adm1021 update\n");
#endif

		data->fail = 0;
		adm1021_rd_good(&(data->temp), client, ADM1021_REG_TEMP,
		                ADM1021_ALARM_TEMP);
		adm1021_rd_good(&(data->temp_os), client, ADM1021_REG_TOS_R,
		                ADM1021_ALARM_TEMP);
		adm1021_rd_good(&(data->temp_hyst), client,
		                ADM1021_REG_THYST_R, ADM1021_ALARM_TEMP);
		adm1021_rd_good(&(data->remote_temp), client,
		                ADM1021_REG_REMOTE_TEMP, ADM1021_ALARM_RTEMP);
		adm1021_rd_good(&(data->remote_temp_os), client,
		                ADM1021_REG_REMOTE_TOS_R, ADM1021_ALARM_RTEMP);
		adm1021_rd_good(&(data->remote_temp_hyst), client,
		                ADM1021_REG_REMOTE_THYST_R,
		                ADM1021_ALARM_RTEMP);
		data->alarms = ADM1021_ALARM_ALL;
		if (!adm1021_rd_good(&(data->alarms), client,
		                     ADM1021_REG_STATUS, 0))
			data->alarms &= ADM1021_ALARM_ALL;
		if (data->type == adm1021)
			adm1021_rd_good(&(data->die_code), client,
			                ADM1021_REG_DIE_CODE, 0);
		if (data->type == adm1023) {
			adm1021_rd_good(&(data->remote_temp_prec), client,
			                ADM1021_REG_REM_TEMP_PREC,
			                ADM1021_ALARM_TEMP);
			adm1021_rd_good(&(data->remote_temp_os_prec), client,
			                ADM1021_REG_REM_TOS_PREC,
			                ADM1021_ALARM_RTEMP);
			adm1021_rd_good(&(data->remote_temp_hyst_prec), client,
			                ADM1021_REG_REM_THYST_PREC,
			                ADM1021_ALARM_RTEMP);
			adm1021_rd_good(&(data->remote_temp_offset), client,
			                ADM1021_REG_REM_OFFSET,
			                ADM1021_ALARM_RTEMP);
			adm1021_rd_good(&(data->remote_temp_offset_prec),
			                client, ADM1021_REG_REM_OFFSET_PREC,
			                ADM1021_ALARM_RTEMP);
		}
		data->last_updated = jiffies;
		data->valid = 1;
	}

	up(&data->update_lock);
}


void adm1021_temp(struct i2c_client *client, int operation, int ctl_name,
		  int *nrels_mag, long *results)
{
	struct adm1021_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		adm1021_update_client(client);
		results[0] = TEMP_FROM_REG(data->temp_os);
		results[1] = TEMP_FROM_REG(data->temp_hyst);
		results[2] = TEMP_FROM_REG(data->temp);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->temp_os = TEMP_TO_REG(results[0]);
			adm1021_write_value(client, ADM1021_REG_TOS_W,
					    data->temp_os);
		}
		if (*nrels_mag >= 2) {
			data->temp_hyst = TEMP_TO_REG(results[1]);
			adm1021_write_value(client, ADM1021_REG_THYST_W,
					    data->temp_hyst);
		}
	}
}

void adm1021_remote_temp(struct i2c_client *client, int operation,
			 int ctl_name, int *nrels_mag, long *results)
{
	struct adm1021_data *data = client->data;
	int prec = 0;

	if (operation == SENSORS_PROC_REAL_INFO)
		if (data->type == adm1023) { *nrels_mag = 3; }
                 else { *nrels_mag = 0; }
	else if (operation == SENSORS_PROC_REAL_READ) {
		adm1021_update_client(client);
		results[0] = TEMP_FROM_REG(data->remote_temp_os);
		results[1] = TEMP_FROM_REG(data->remote_temp_hyst);
		results[2] = TEMP_FROM_REG(data->remote_temp);
		if (data->type == adm1023) {
		  results[0]=results[0]*1000 + 
		   ((data->remote_temp_os_prec >> 5) * 125);
		  results[1]=results[1]*1000 + 
		   ((data->remote_temp_hyst_prec >> 5) * 125);
		  results[2]=(TEMP_FROM_REG(data->remote_temp_offset)*1000) + 
                   ((data->remote_temp_offset_prec >> 5) * 125);
		  results[3]=TEMP_FROM_REG(data->remote_temp)*1000 + 
		   ((data->remote_temp_prec >> 5) * 125);
 		  *nrels_mag = 4;
		} else {
 		  *nrels_mag = 3;
		}
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			if (data->type == adm1023) {
			  prec=((results[0]-((results[0]/1000)*1000))/125)<<5;
			  adm1021_write_value(client,
                                            ADM1021_REG_REM_TOS_PREC,
                                            prec);
			  results[0]=results[0]/1000;
			  data->remote_temp_os_prec=prec;
			}
			data->remote_temp_os = TEMP_TO_REG(results[0]);
			adm1021_write_value(client,
					    ADM1021_REG_REMOTE_TOS_W,
					    data->remote_temp_os);
		}
		if (*nrels_mag >= 2) {
			if (data->type == adm1023) {
			  prec=((results[1]-((results[1]/1000)*1000))/125)<<5;
			  adm1021_write_value(client,
                                            ADM1021_REG_REM_THYST_PREC,
                                            prec);
			  results[1]=results[1]/1000;
			  data->remote_temp_hyst_prec=prec;
			}
			data->remote_temp_hyst = TEMP_TO_REG(results[1]);
			adm1021_write_value(client,
					    ADM1021_REG_REMOTE_THYST_W,
					    data->remote_temp_hyst);
		}
		if (*nrels_mag >= 3) {
			if (data->type == adm1023) {
			  prec=((results[2]-((results[2]/1000)*1000))/125)<<5;
			  adm1021_write_value(client,
                                            ADM1021_REG_REM_OFFSET_PREC,
                                            prec);
			  results[2]=results[2]/1000;
			  data->remote_temp_offset_prec=prec;
			  data->remote_temp_offset=results[2];
			  adm1021_write_value(client,
                                            ADM1021_REG_REM_OFFSET,
                                            data->remote_temp_offset);
			}
		}
	}
}

void adm1021_die_code(struct i2c_client *client, int operation,
		      int ctl_name, int *nrels_mag, long *results)
{
	struct adm1021_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		adm1021_update_client(client);
		results[0] = data->die_code;
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		/* Can't write to it */
	}
}

void adm1021_alarms(struct i2c_client *client, int operation, int ctl_name,
		    int *nrels_mag, long *results)
{
	struct adm1021_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		adm1021_update_client(client);
		results[0] = data->alarms | data->fail;
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		/* Can't write to it */
	}
}

static int __init sm_adm1021_init(void)
{
	printk(KERN_INFO "adm1021.o version %s (%s)\n", LM_VERSION, LM_DATE);
	return i2c_add_driver(&adm1021_driver);
}

static void __exit sm_adm1021_exit(void)
{
	i2c_del_driver(&adm1021_driver);
}

MODULE_AUTHOR
    ("Frodo Looijaard <frodol@dds.nl> and Philip Edelbrock <phil@netroedge.com>");
MODULE_DESCRIPTION("adm1021 driver");
MODULE_LICENSE("GPL");

MODULE_PARM(read_only, "i");
MODULE_PARM_DESC(read_only, "Don't set any values, read only mode");

module_init(sm_adm1021_init)
module_exit(sm_adm1021_exit)
