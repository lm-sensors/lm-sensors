/*
    pcf8591.c - Part of lm_sensors, Linux kernel modules for hardware
             monitoring
    Copyright (c) 2001  Aurelien Jarno <aurelien@aurel32.net>

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
static unsigned short normal_i2c_range[] = { 0x48, 0x4f, SENSORS_I2C_END };
static unsigned int normal_isa[] = { SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/* Insmod parameters */
SENSORS_INSMOD_1(pcf8591);

/* The PCF8591 control byte */
/*    7    6    5    4    3    2    1    0   */
/* |  0 |AOEF|   AIP   |  0 |AINC|  AICH   | */

#define PCF8591_CONTROL_BYTE_AOEF 0x40  /* Analog Output Enable Flag */
                                        /* (analog output active if 1) */

#define PCF8591_CONTROL_BYTE_AIP 0x30   /* Analog Input Programming */
                                        /* 0x00 = four single ended inputs */
                                        /* 0x10 = three differential inputs */
                                        /* 0x20 = single ended and differential mixed */
                                        /* 0x30 = two differential inputs */

#define PCF8591_CONTROL_BYTE_AINC 0x04  /* Autoincrement Flag */
                                        /* (switch on if 1) */

#define PCF8591_CONTROL_BYTE_AICH 0x03  /* Analog Output Enable Flag */
                                        /* 0x00 = channel 0 */
                                        /* 0x01 = channel 1 */
                                        /* 0x02 = channel 2 */
                                        /* 0x03 = channel 3 */


/* Initial values */
#define PCF8591_INIT_CONTROL_BYTE (PCF8591_CONTROL_BYTE_AOEF | PCF8591_CONTROL_BYTE_AINC)
                /* DAC out enabled, four single ended inputs, autoincrement */

#define PCF8591_INIT_AOUT 0             /* DAC out = 0 */


/* Conversions. */
#define REG_TO_SIGNED(reg) (reg & 0x80)?(reg - 256):(reg)
                          /* Convert signed 8 bit value to signed value */


struct pcf8591_data {
	struct i2c_client client;
        int sysctl_id;

        struct semaphore update_lock;
        char valid;             /* !=0 if following fields are valid */
        unsigned long last_updated;     /* In jiffies */

        u8 control_byte;
        u8 ch[4];
        u8 aout;
};

static int pcf8591_attach_adapter(struct i2c_adapter *adapter);
static int pcf8591_detect(struct i2c_adapter *adapter, int address,
                          unsigned short flags, int kind);
static int pcf8591_detach_client(struct i2c_client *client);

static void pcf8591_update_client(struct i2c_client *client);
static void pcf8591_init_client(struct i2c_client *client);

static void pcf8591_ain_conf(struct i2c_client *client, int operation,
                             int ctl_name, int *nrels_mag, long *results);
static void pcf8591_ain(struct i2c_client *client, int operation,
                        int ctl_name, int *nrels_mag, long *results);
static void pcf8591_aout_enable(struct i2c_client *client, int operation,
                                int ctl_name, int *nrels_mag, long *results);
static void pcf8591_aout(struct i2c_client *client, int operation,
                         int ctl_name, int *nrels_mag, long *results);


/* This is the driver that will be inserted */
static struct i2c_driver pcf8591_driver = {
	.name		= "PCF8591 sensor chip driver",
	.id		= I2C_DRIVERID_PCF8591,
	.flags		= I2C_DF_NOTIFY,
	.attach_adapter	= pcf8591_attach_adapter,
	.detach_client	= pcf8591_detach_client,
};

/* The /proc/sys entries */

/* -- SENSORS SYSCTL START -- */
#define PCF8591_SYSCTL_AIN_CONF 1000      /* Analog input configuration */
#define PCF8591_SYSCTL_CH0 1001           /* Input channel 1 */
#define PCF8591_SYSCTL_CH1 1002           /* Input channel 2 */
#define PCF8591_SYSCTL_CH2 1003           /* Input channel 3 */
#define PCF8591_SYSCTL_CH3 1004           /* Input channel 4 */
#define PCF8591_SYSCTL_AOUT_ENABLE 1005   /* Analog output enable flag */
#define PCF8591_SYSCTL_AOUT 1006          /* Analog output */
/* -- SENSORS SYSCTL END -- */

/* These files are created for each detected PCF8591. This is just a template;
   though at first sight, you might think we could use a statically
   allocated list, we need some way to get back to the parent - which
   is done through one of the 'extra' fields which are initialized
   when a new copy is allocated. */
static ctl_table pcf8591_dir_table_template[] = {
        {PCF8591_SYSCTL_AIN_CONF, "ain_conf", NULL, 0, 0644, NULL, &i2c_proc_real,
         &i2c_sysctl_real, NULL, &pcf8591_ain_conf},
        {PCF8591_SYSCTL_CH0, "ch0", NULL, 0, 0444, NULL, &i2c_proc_real,
         &i2c_sysctl_real, NULL, &pcf8591_ain},
        {PCF8591_SYSCTL_CH1, "ch1", NULL, 0, 0444, NULL, &i2c_proc_real,
         &i2c_sysctl_real, NULL, &pcf8591_ain},
        {PCF8591_SYSCTL_CH2, "ch2", NULL, 0, 0444, NULL, &i2c_proc_real,
         &i2c_sysctl_real, NULL, &pcf8591_ain},
        {PCF8591_SYSCTL_CH3, "ch3", NULL, 0, 0444, NULL, &i2c_proc_real,
         &i2c_sysctl_real, NULL, &pcf8591_ain},
        {PCF8591_SYSCTL_AOUT_ENABLE, "aout_enable", NULL, 0, 0644, NULL, &i2c_proc_real,
         &i2c_sysctl_real, NULL, &pcf8591_aout_enable},
        {PCF8591_SYSCTL_AOUT, "aout", NULL, 0, 0644, NULL, &i2c_proc_real,
         &i2c_sysctl_real, NULL, &pcf8591_aout},
        {0}
};


/* This function is called when:
     * pcf8591_driver is inserted (when this module is loaded), for each
       available adapter
     * when a new adapter is inserted (and pcf8591_driver is still present) */
static int pcf8591_attach_adapter(struct i2c_adapter *adapter)
{
        return i2c_detect(adapter, &addr_data, pcf8591_detect);
}

/* This function is called by i2c_detect */
int pcf8591_detect(struct i2c_adapter *adapter, int address,
                unsigned short flags, int kind)
{
        int i;
        struct i2c_client *new_client;
        struct pcf8591_data *data;
        int err = 0;

        const char *type_name = "";
        const char *client_name = "";

        /* Make sure we aren't probing the ISA bus!! This is just a safety check at this moment; i2c_detect really won't call us. */
#ifdef DEBUG
        if (i2c_is_isa_adapter(adapter)) {
                printk
                    (KERN_ERR "pcf8591.o: pcf8591_detect called for an ISA bus adapter?!?\n");
                return 0;
        }
#endif

        if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE
                                     | I2C_FUNC_SMBUS_WRITE_BYTE_DATA))
                goto ERROR0;

        /* OK. For now, we presume we have a valid client. We now create the
           client structure, even though we cannot fill it completely yet. */
	if (!(data = kmalloc(sizeof(struct pcf8591_data), GFP_KERNEL))) {
                err = -ENOMEM;
                goto ERROR0;
        }

	new_client = &data->client;
        new_client->addr = address;
        new_client->data = data;
        new_client->adapter = adapter;
        new_client->driver = &pcf8591_driver;
        new_client->flags = 0;

        /* Now, we would do the remaining detection. But the PCF8591 is plainly
           impossible to detect! Stupid chip. */

        /* Determine the chip type - only one kind supported! */
        if (kind <= 0)
                kind = pcf8591;

	type_name = "pcf8591";
	client_name = "PCF8591 chip";

        /* Fill in the remaining client fields and put it into the global list */
        strcpy(new_client->name, client_name);
        data->valid = 0;
        init_MUTEX(&data->update_lock);

        /* Tell the I2C layer a new client has arrived */
        if ((err = i2c_attach_client(new_client)))
                goto ERROR3;

        /* Register a new directory entry with module sensors */
        if ((i = i2c_register_entry(new_client,
                                        type_name,
                                        pcf8591_dir_table_template,
					THIS_MODULE)) < 0) {
                err = i;
                goto ERROR4;
        }
        data->sysctl_id = i;

        /* Initialize the PCF8591 chip */
        pcf8591_init_client(new_client);
        return 0;

/* OK, this is not exactly good programming practice, usually. But it is
   very code-efficient in this case. */

      ERROR4:
        i2c_detach_client(new_client);
      ERROR3:
	kfree(data);
      ERROR0:
        return err;
}

static int pcf8591_detach_client(struct i2c_client *client)
{
        int err;

        i2c_deregister_entry(((struct pcf8591_data *) (client->data))->
                                 sysctl_id);

        if ((err = i2c_detach_client(client))) {
                printk
                    (KERN_ERR "pcf8591.o: Client deregistration failed, client not detached.\n");
                return err;
        }

	kfree(client->data);

        return 0;
}

/* Called when we have found a new PCF8591. */
static void pcf8591_init_client(struct i2c_client *client)
{
        struct pcf8591_data *data = client->data;
        data->control_byte = PCF8591_INIT_CONTROL_BYTE;
        data->aout = PCF8591_INIT_AOUT;

        i2c_smbus_write_byte_data(client, data->control_byte, data->aout);
}

static void pcf8591_update_client(struct i2c_client *client)
{
        struct pcf8591_data *data = client->data;

        down(&data->update_lock);

        if ((jiffies - data->last_updated > HZ + HZ / 2) ||
            (jiffies < data->last_updated) || !data->valid) {

#ifdef DEBUG
                printk(KERN_DEBUG "Starting pcf8591 update\n");
#endif

                i2c_smbus_write_byte(client, data->control_byte);
                i2c_smbus_read_byte(client);    /* The first byte transmitted contains the */
                                                /* conversion code of the previous read cycled */
                                                /* FLUSH IT ! */


                /* Number of byte to read to signed depend on the analog input mode */
                data->ch[0] = i2c_smbus_read_byte(client);
                data->ch[1] = i2c_smbus_read_byte(client);
                        /* In all case, read at least two values */

                if ((data->control_byte & PCF8591_CONTROL_BYTE_AIP) != 0x30)
                        data->ch[2] = i2c_smbus_read_byte(client);
                        /* Read the third value if not in "two differential inputs" mode */

                if ((data->control_byte & PCF8591_CONTROL_BYTE_AIP) == 0x00)
                        data->ch[3] = i2c_smbus_read_byte(client);
                        /* Read the fourth value only in "four single ended inputs" mode */

                data->last_updated = jiffies;
                data->valid = 1;
        }

        up(&data->update_lock);
}

/* The next few functions are the call-back functions of the /proc/sys and
   sysctl files. Which function is used is defined in the ctl_table in
   the extra1 field. */
void pcf8591_ain_conf(struct i2c_client *client, int operation, int ctl_name,
             int *nrels_mag, long *results)
{
        struct pcf8591_data *data = client->data;

        if (operation == SENSORS_PROC_REAL_INFO)
                *nrels_mag = 0;
        else if (operation == SENSORS_PROC_REAL_READ) {
                results[0] = (data->control_byte & PCF8591_CONTROL_BYTE_AIP) >> 4;
                *nrels_mag = 1;
        } else if (operation == SENSORS_PROC_REAL_WRITE) {
                if (*nrels_mag >= 1) {
                        if (results[0] >= 0 && results[0] <= 3)
                        {
                                data->control_byte &= ~PCF8591_CONTROL_BYTE_AIP;
                                data->control_byte |= (results[0] << 4);
                                i2c_smbus_write_byte(client, data->control_byte);
                                data->valid = 0;
                        }
                }
        }
}

void pcf8591_ain(struct i2c_client *client, int operation, int ctl_name,
             int *nrels_mag, long *results)
{
        struct pcf8591_data *data = client->data;
        int nr = ctl_name - PCF8591_SYSCTL_CH0;

        if (operation == SENSORS_PROC_REAL_INFO)
                *nrels_mag = 0;
        else if (operation == SENSORS_PROC_REAL_READ) {
                pcf8591_update_client(client);

                /* Number of data to show and conversion to signed depend on */
                /* the analog input mode */

                switch(nr) {
                        case 0:
                                if (((data->control_byte & PCF8591_CONTROL_BYTE_AIP) == 0)
                                   | ((data->control_byte & PCF8591_CONTROL_BYTE_AIP) == 2))
                                        results[0] = data->ch[0];               /* single ended */
                                else
                                        results[0] = REG_TO_SIGNED(data->ch[0]);/* differential */
                                break;
                        case 1:
                                if (((data->control_byte & PCF8591_CONTROL_BYTE_AIP) == 0)
                                   | ((data->control_byte & PCF8591_CONTROL_BYTE_AIP) == 2))
                                        results[0] = data->ch[1]; /* single ended */
                                else
                                        results[0] = REG_TO_SIGNED(data->ch[1]);/* differential */
                                break;
                        case 2:
                                if ((data->control_byte & PCF8591_CONTROL_BYTE_AIP) == 3)
                                        results[0] = 0;  /* channel not used */
                                else if ((data->control_byte & PCF8591_CONTROL_BYTE_AIP) == 0)
                                        results[0] = data->ch[2]; /* single ended */
                                else
                                        results[0] = REG_TO_SIGNED(data->ch[2]);/* differential */
                                break;
                        case 3:
                                if (((data->control_byte & PCF8591_CONTROL_BYTE_AIP) == 0)
                                   | ((data->control_byte & PCF8591_CONTROL_BYTE_AIP) == 2))
                                        results[0] = data->ch[3]; /* single ended */
                                else
                                        results[0] = 0;  /* channel not used */
                                break;
                }
        *nrels_mag = 1;
        }
}

void pcf8591_aout_enable(struct i2c_client *client, int operation, int ctl_name,
             int *nrels_mag, long *results)
{
        struct pcf8591_data *data = client->data;

        if (operation == SENSORS_PROC_REAL_INFO)
                *nrels_mag = 0;
        else if (operation == SENSORS_PROC_REAL_READ) {
                results[0] = !(!(data->control_byte & PCF8591_CONTROL_BYTE_AOEF));
                *nrels_mag = 1;
        } else if (operation == SENSORS_PROC_REAL_WRITE) {
                if (*nrels_mag >= 1) {
                        if (results[0])
                                data->control_byte |= PCF8591_CONTROL_BYTE_AOEF;
                        else
                                data->control_byte &= ~PCF8591_CONTROL_BYTE_AOEF;

                        i2c_smbus_write_byte(client, data->control_byte);
                }
        }
}

void pcf8591_aout(struct i2c_client *client, int operation, int ctl_name,
             int *nrels_mag, long *results)
{
        struct pcf8591_data *data = client->data;

        if (operation == SENSORS_PROC_REAL_INFO)
                *nrels_mag = 0;
        else if (operation == SENSORS_PROC_REAL_READ) {
                results[0] = data->aout;
                *nrels_mag = 1;
        } else if (operation == SENSORS_PROC_REAL_WRITE) {
                if (*nrels_mag >= 1) {
                        if (results[0] >= 0 && results[0] <= 255) /* ignore values outside DAC range */
                        {
                                data->aout = results[0];
                                i2c_smbus_write_byte_data(client, data->control_byte, data->aout);
                        }
                }
        }
}

static int __init sm_pcf8591_init(void)
{
        printk(KERN_INFO "pcf8591.o version %s (%s)\n", LM_VERSION, LM_DATE);
	return i2c_add_driver(&pcf8591_driver);
}

static void __exit sm_pcf8591_exit(void)
{
        i2c_del_driver(&pcf8591_driver);
}



MODULE_AUTHOR("Aurelien Jarno <aurelien@aurel32.net>");
MODULE_DESCRIPTION("PCF8591 driver");
MODULE_LICENSE("GPL");

module_init(sm_pcf8591_init);
module_exit(sm_pcf8591_exit);
