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

#include <linux/version.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include "sensors.h"
#include "version.h"
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
static unsigned short normal_i2c_range[] = { 0x18, 0x1a, 0x29, 0x2b,
	0x4c, 0x4e, SENSORS_I2C_END
};
static unsigned int normal_isa[] = { SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/* Insmod parameters */
SENSORS_INSMOD_7(adm1021, adm1023, max1617, max1617a, thmc10, lm84, gl523sm);

/* adm1021 constants specified below */

/* The adm1021 registers */
/* Read-only */
#define ADM1021_REG_TEMP 0x00
#define ADM1021_REG_REMOTE_TEMP 0x01
#define ADM1021_REG_STATUS 0x02
#define ADM1021_REG_MAN_ID 0x0FE	/* 0x41 = AMD, 0x49 = TI, 0x4D = Maxim, 0x23 = Genesys */
#define ADM1021_REG_DEV_ID 0x0FF	/* ADM1021 = 0x0X, ADM1023 = 0x3X */
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


/* Conversions. Rounding and limit checking is only done on the TO_REG
   variants. Note that you should be a bit careful with which arguments
   these macros are called: arguments may be evaluated more than once.
   Fixing this is just not worth it. */
/* Conversions  note: 1021 uses normal integer signed-byte format*/
#define TEMP_FROM_REG(val) (val > 127 ? val-256 : val)
#define TEMP_TO_REG(val)   (SENSORS_LIMIT((val < 0 ? val+256 : val),0,255))

/* Initial values */

/* Note: Eventhough I left the low and high limits named os and hyst, 
they don't quite work like a thermostat the way the LM75 does.  I.e., 
a lower temp than THYST actuall triggers an alarm instead of 
clearing it.  Weird, ey?   --Phil  */
#define adm1021_INIT_TOS 60
#define adm1021_INIT_THYST 20
#define adm1021_INIT_REMOTE_TOS 60
#define adm1021_INIT_REMOTE_THYST 20

/* Each client has this additional data */
struct adm1021_data {
	int sysctl_id;
	enum chips type;

	struct semaphore update_lock;
	char valid;		/* !=0 if following fields are valid */
	unsigned long last_updated;	/* In jiffies */

	u8 temp, temp_os, temp_hyst;	/* Register values */
	u8 remote_temp, remote_temp_os, remote_temp_hyst, alarms, die_code;
        /* Special values for ADM1023 only */
	u8 remote_temp_prec, remote_temp_os_prec, remote_temp_hyst_prec, 
	   remote_temp_offset, remote_temp_offset_prec;
};

#ifdef MODULE
extern int init_module(void);
extern int cleanup_module(void);
#endif				/* MODULE */

#ifdef MODULE
static
#else
extern
#endif
int __init sensors_adm1021_init(void);
static int __init adm1021_cleanup(void);
static int adm1021_attach_adapter(struct i2c_adapter *adapter);
static int adm1021_detect(struct i2c_adapter *adapter, int address,
			  unsigned short flags, int kind);
static void adm1021_init_client(struct i2c_client *client);
static int adm1021_detach_client(struct i2c_client *client);
static int adm1021_command(struct i2c_client *client, unsigned int cmd,
			   void *arg);
static void adm1021_inc_use(struct i2c_client *client);
static void adm1021_dec_use(struct i2c_client *client);
static int adm1021_read_value(struct i2c_client *client, u8 reg);
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


/* This is the driver that will be inserted */
static struct i2c_driver adm1021_driver = {
	/* name */ "ADM1021, MAX1617 sensor driver",
	/* id */ I2C_DRIVERID_ADM1021,
	/* flags */ I2C_DF_NOTIFY,
	/* attach_adapter */ &adm1021_attach_adapter,
	/* detach_client */ &adm1021_detach_client,
	/* command */ &adm1021_command,
	/* inc_use */ &adm1021_inc_use,
	/* dec_use */ &adm1021_dec_use
};

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

/* Used by init/cleanup */
static int __initdata adm1021_initialized = 0;

/* I choose here for semi-static allocation. Complete dynamic
   allocation could also be used; the code needed for this would probably
   take more memory than the datastructure takes now. */
static int adm1021_id = 0;

int adm1021_attach_adapter(struct i2c_adapter *adapter)
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
		goto ERROR0;

	/* OK. For now, we presume we have a valid client. We now create the
	   client structure, even though we cannot fill it completely yet.
	   But it allows us to access adm1021_{read,write}_value. */

	if (!(new_client = kmalloc(sizeof(struct i2c_client) +
				   sizeof(struct adm1021_data),
				   GFP_KERNEL))) {
		err = -ENOMEM;
		goto ERROR0;
	}

	data = (struct adm1021_data *) (new_client + 1);
	new_client->addr = address;
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &adm1021_driver;
	new_client->flags = 0;

	/* Now, we do the remaining detection. */

	if (kind < 0) {
		if (
		    (adm1021_read_value(new_client, ADM1021_REG_STATUS) &
		     0x03) != 0x00)
			goto ERROR1;
	}

	/* Determine the chip type. */

	if (kind <= 0) {
		i = adm1021_read_value(new_client, ADM1021_REG_MAN_ID);
		if (i == 0x41)
		  if ((adm1021_read_value (new_client, ADM1021_REG_DEV_ID) & 0x0F0) == 0x030)
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
		/* LM84 Mfr ID in a different place */
		else
		    if (adm1021_read_value
			(new_client, ADM1021_REG_CONV_RATE_R) == 0x00)
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
	} else {
#ifdef DEBUG
		printk("adm1021.o: Internal error: unknown kind (%d)?!?",
		       kind);
#endif
		goto ERROR1;
	}

	/* Fill in the remaining client fields and put it into the global list */
	strcpy(new_client->name, client_name);
	data->type = kind;

	new_client->id = adm1021_id++;
	data->valid = 0;
	init_MUTEX(&data->update_lock);

	/* Tell the I2C layer a new client has arrived */
	if ((err = i2c_attach_client(new_client)))
		goto ERROR3;

	/* Register a new directory entry with module sensors */
	if ((i = i2c_register_entry(new_client,
					type_name,
					data->type ==
					adm1021 ?
					adm1021_dir_table_template :
					adm1021_max_dir_table_template,
					THIS_MODULE)) < 0) {
		err = i;
		goto ERROR4;
	}
	data->sysctl_id = i;

	/* Initialize the ADM1021 chip */
	adm1021_init_client(new_client);
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

void adm1021_init_client(struct i2c_client *client)
{
	/* Initialize the adm1021 chip */
	adm1021_write_value(client, ADM1021_REG_TOS_W,
			    TEMP_TO_REG(adm1021_INIT_TOS));
	adm1021_write_value(client, ADM1021_REG_THYST_W,
			    TEMP_TO_REG(adm1021_INIT_THYST));
	adm1021_write_value(client, ADM1021_REG_REMOTE_TOS_W,
			    TEMP_TO_REG(adm1021_INIT_REMOTE_TOS));
	adm1021_write_value(client, ADM1021_REG_REMOTE_THYST_W,
			    TEMP_TO_REG(adm1021_INIT_REMOTE_THYST));
	/* Enable ADC and disable suspend mode */
	adm1021_write_value(client, ADM1021_REG_CONFIG_W, 0);
	/* Set Conversion rate to 1/sec (this can be tinkered with) */
	adm1021_write_value(client, ADM1021_REG_CONV_RATE_W, 0x04);
}

int adm1021_detach_client(struct i2c_client *client)
{

	int err;

	i2c_deregister_entry(((struct adm1021_data *) (client->data))->
				 sysctl_id);

	if ((err = i2c_detach_client(client))) {
		printk
		    ("adm1021.o: Client deregistration failed, client not detached.\n");
		return err;
	}

	kfree(client);

	return 0;

}


/* No commands defined yet */
int adm1021_command(struct i2c_client *client, unsigned int cmd, void *arg)
{
	return 0;
}

void adm1021_inc_use(struct i2c_client *client)
{
#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif
}

void adm1021_dec_use(struct i2c_client *client)
{
#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
}

/* All registers are byte-sized */
int adm1021_read_value(struct i2c_client *client, u8 reg)
{
	return i2c_smbus_read_byte_data(client, reg);
}

int adm1021_write_value(struct i2c_client *client, u8 reg, u16 value)
{
	return i2c_smbus_write_byte_data(client, reg, value);
}

void adm1021_update_client(struct i2c_client *client)
{
	struct adm1021_data *data = client->data;

	down(&data->update_lock);

	if ((jiffies - data->last_updated > HZ + HZ / 2) ||
	    (jiffies < data->last_updated) || !data->valid) {

#ifdef DEBUG
		printk("Starting adm1021 update\n");
#endif

		data->temp = adm1021_read_value(client, ADM1021_REG_TEMP);
		data->temp_os =
		    adm1021_read_value(client, ADM1021_REG_TOS_R);
		data->temp_hyst =
		    adm1021_read_value(client, ADM1021_REG_THYST_R);
		data->remote_temp =
		    adm1021_read_value(client, ADM1021_REG_REMOTE_TEMP);
		data->remote_temp_os =
		    adm1021_read_value(client, ADM1021_REG_REMOTE_TOS_R);
		data->remote_temp_hyst =
		    adm1021_read_value(client, ADM1021_REG_REMOTE_THYST_R);
		data->alarms =
		    adm1021_read_value(client, ADM1021_REG_STATUS) & 0xec;
		if (data->type == adm1021)
			data->die_code =
			    adm1021_read_value(client,
					       ADM1021_REG_DIE_CODE);
		if (data->type == adm1023) {
		  data->remote_temp_prec =
		    adm1021_read_value(client, ADM1021_REG_REM_TEMP_PREC);
		  data->remote_temp_os_prec =
		    adm1021_read_value(client, ADM1021_REG_REM_TOS_PREC);
		  data->remote_temp_hyst_prec =
		    adm1021_read_value(client, ADM1021_REG_REM_THYST_PREC);
		  data->remote_temp_offset =
		    adm1021_read_value(client, ADM1021_REG_REM_OFFSET);
		  data->remote_temp_offset_prec =
		    adm1021_read_value(client, ADM1021_REG_REM_OFFSET_PREC);
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
int prec=0;
	struct adm1021_data *data = client->data;
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
		results[0] = data->alarms;
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		/* Can't write to it */
	}
}

int __init sensors_adm1021_init(void)
{
	int res;

	printk("adm1021.o version %s (%s)\n", LM_VERSION, LM_DATE);
	adm1021_initialized = 0;
	if ((res = i2c_add_driver(&adm1021_driver))) {
		printk
		    ("adm1021.o: Driver registration failed, module not inserted.\n");
		adm1021_cleanup();
		return res;
	}
	adm1021_initialized++;
	return 0;
}

int __init adm1021_cleanup(void)
{
	int res;

	if (adm1021_initialized >= 1) {
		if ((res = i2c_del_driver(&adm1021_driver))) {
			printk
			    ("adm1021.o: Driver deregistration failed, module not removed.\n");
			return res;
		}
		adm1021_initialized--;
	}

	return 0;
}

EXPORT_NO_SYMBOLS;

#ifdef MODULE

MODULE_AUTHOR
    ("Frodo Looijaard <frodol@dds.nl> and Philip Edelbrock <phil@netroedge.com>");
MODULE_DESCRIPTION("adm1021 driver");

int init_module(void)
{
	return sensors_adm1021_init();
}

int cleanup_module(void)
{
	return adm1021_cleanup();
}

#endif				/* MODULE */
