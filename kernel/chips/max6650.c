/*
 * max6650.c - Part of lm_sensors, Linux kernel modules for hardware
 *             monitoring. 
 * 
 * Author: John Morris <john.morris@spirentcom.com>
 *
 * Copyright (c) 2003 Spirent Communications
 *
 * This module has only been tested with the MAX6651 chip. It should
 * work with the MAX6650 also, though with reduced functionality. It
 * does not yet distinguish max6650 and max6651 chips.
 * 
 * Tha datasheet was last seen at: 
 *
 *        http://pdfserv.maxim-ic.com/en/ds/MAX6650-MAX6651.pdf
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/i2c-proc.h>
#include <linux/i2c-id.h>
#include <linux/init.h>
#include "version.h"

#ifndef I2C_DRIVERID_MAX6650
#define I2C_DRIVERID_MAX6650	1044
#endif

/*
 * Addresses to scan. There are four disjoint possibilities, by pin config.
 */

static unsigned short normal_i2c[] = {0x1b, 0x1f, 0x48, 0x4b, SENSORS_I2C_END};
static unsigned short normal_i2c_range[] = { SENSORS_I2C_END };
static unsigned int normal_isa[] = { SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/*
 * Insmod parameters
 */

SENSORS_INSMOD_1(max6650);

/* 
 * MAX 6650/6651 registers
 */

#define MAX6650_REG_SPEED       0x00
#define MAX6650_REG_CONFIG      0x02
#define MAX6650_REG_GPIO_DEF    0x04
#define MAX6650_REG_DAC         0x06
#define MAX6650_REG_ALARM_EN    0x08
#define MAX6650_REG_ALARM       0x0A
#define MAX6650_REG_TACH0       0x0C
#define MAX6650_REG_TACH1       0x0E
#define MAX6650_REG_TACH2       0x10
#define MAX6650_REG_TACH3       0x12
#define MAX6650_REG_GPIO_STAT   0x14
#define MAX6650_REG_COUNT       0x16

/*
 * Config register bits
 */
 
#define MAX6650_CFG_MODE_MASK           0x30
#define MAX6650_CFG_MODE_ON             0x00
#define MAX6650_CFG_MODE_OFF            0x10
#define MAX6650_CFG_MODE_CLOSED_LOOP    0x20
#define MAX6650_CFG_MODE_OPEN_LOOP      0x30

static const u8 tach_reg[] = 
{
    MAX6650_REG_TACH0, MAX6650_REG_TACH1, 
    MAX6650_REG_TACH2, MAX6650_REG_TACH3 
};

#define MAX6650_INT_CLK 254000  /* Default clock speed - 254 kHz */

/*
 * Functions declaration
 */

static void max6650_fan (struct i2c_client *client, int operation, int
	ctl_name, int *nrels_mag, long *results);
static void max6650_speed (struct i2c_client *client, int operation, int
	ctl_name, int *nrels_mag, long *results);
static void max6650_xdump (struct i2c_client *client, int operation, int
	ctl_name, int *nrels_mag, long *results);
static int max6650_detect(struct i2c_adapter *adapter, int address, unsigned
	short flags, int kind);
static int max6650_attach_adapter(struct i2c_adapter *adapter);
static int max6650_detach_client(struct i2c_client *client);
static void max6650_init_client(struct i2c_client *client);
static int max6650_read(struct i2c_client *client, u8 reg);

/*
 * Driver data (common to all clients)
 */


static struct i2c_driver max6650_driver = {
    .name           = "MAX6650/1 sensor driver",
    .id             = I2C_DRIVERID_MAX6650,
    .flags          = I2C_DF_NOTIFY,
    .attach_adapter = max6650_attach_adapter,
    .detach_client  = max6650_detach_client
};

/*
 * Client data (each client gets its own)
 */

struct max6650_data
{
	struct i2c_client client;
    int sysctl_id;
    struct semaphore update_lock;
    char valid;                 /* zero until following fields are valid */
    unsigned long last_updated; /* in jiffies */

    /* register values */
       
    u8 speed;
    u8 config;
    u8 tach[4];
    u8 count;
};

/*
 * Proc entries
 * These files are created for each detected max6650.
 */

/* -- SENSORS SYSCTL START -- */

#define MAX6650_SYSCTL_FAN1     1101
#define MAX6650_SYSCTL_FAN2     1102
#define MAX6650_SYSCTL_FAN3     1103
#define MAX6650_SYSCTL_FAN4     1104
#define MAX6650_SYSCTL_SPEED    1105
#define MAX6650_SYSCTL_XDUMP    1106


/* -- SENSORS SYSCTL END -- */


static ctl_table max6650_dir_table_template[] =
{
    {MAX6650_SYSCTL_FAN1, "fan1", NULL, 0, 0644, NULL,
        	 &i2c_proc_real, &i2c_sysctl_real, NULL, &max6650_fan},
    {MAX6650_SYSCTL_FAN2, "fan2", NULL, 0, 0644, NULL,
        	 &i2c_proc_real, &i2c_sysctl_real, NULL, &max6650_fan},
    {MAX6650_SYSCTL_FAN3, "fan3", NULL, 0, 0644, NULL,
        	 &i2c_proc_real, &i2c_sysctl_real, NULL, &max6650_fan},
    {MAX6650_SYSCTL_FAN4, "fan4", NULL, 0, 0644, NULL,
        	 &i2c_proc_real, &i2c_sysctl_real, NULL, &max6650_fan},
    {MAX6650_SYSCTL_SPEED, "speed", NULL, 0, 0644, NULL,
        	 &i2c_proc_real, &i2c_sysctl_real, NULL, &max6650_speed},
    {MAX6650_SYSCTL_XDUMP, "xdump", NULL, 0, 0644, NULL,
        	 &i2c_proc_real, &i2c_sysctl_real, NULL, &max6650_xdump},
    {0}
};

/*
 * Real code
 */

static int max6650_attach_adapter(struct i2c_adapter *adapter)
{
    return i2c_detect(adapter, &addr_data, max6650_detect);
}

/*
 * The following function does more than just detection. If detection
 * succeeds, it also registers the new chip.
 */

static int max6650_detect(struct i2c_adapter *adapter, int address, unsigned
	short flags, int kind)
{
    struct i2c_client *new_client;
    struct max6650_data *data;
    int err = 0;
    const char *type_name = "";
    const char *client_name = "";

#ifdef DEBUG
    if (i2c_is_isa_adapter(adapter)) {
        printk("max6650.o: Called for an ISA bus adapter, aborting.\n");
        return 0;
    }
#endif

    if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
#ifdef DEBUG
        printk("max6650.o: I2C bus doesn't support byte read mode, skipping.\n");
#endif
        return 0;
    }

    if (!(data = kmalloc(sizeof(struct max6650_data), GFP_KERNEL))) {
        printk("max6650.o: Out of memory in max6650_detect (new_client).\n");
        return -ENOMEM;
    }

    /*
     * The common I2C client data is placed right before the
     * max6650-specific data. The max6650-specific data is pointed to by the
	 * data field from the I2C client data.
     */

	new_client = &data->client;
    new_client->addr = address;
    new_client->data = data;
    new_client->adapter = adapter;
    new_client->driver = &max6650_driver;
    new_client->flags = 0;

    /*
     * Now we do the remaining detection. A negative kind means that
     * the driver was loaded with no force parameter (default), so we
     * must both detect and identify the chip (actually there is only
     * one possible kind of chip for now, max6650). A zero kind means that
     * the driver was loaded with the force parameter, the detection
     * step shall be skipped. A positive kind means that the driver
     * was loaded with the force parameter and a given kind of chip is
     * requested, so both the detection and the identification steps
     * are skipped.
     *
     * Currently I can find no way to distinguish between a MAX6650 and 
     * a MAX6651. This driver has only been tried on the latter.
     */

    if (kind < 0) {     /* detection */
        if (
            (max6650_read(new_client, MAX6650_REG_CONFIG) & 0xC0) ||
            (max6650_read(new_client, MAX6650_REG_GPIO_STAT) & 0xE0) ||
            (max6650_read(new_client, MAX6650_REG_ALARM_EN) & 0xE0) ||
            (max6650_read(new_client, MAX6650_REG_ALARM) & 0xE0) ||
            (max6650_read(new_client, MAX6650_REG_COUNT) & 0xFC) 
        )
        {
#ifdef DEBUG
            printk("max6650.o: max6650 detection failed at 0x%02x.\n",
                                                                    address);
#endif
            goto ERROR1;
        }
    }

    if (kind <= 0) { /* identification */
        kind = max6650;
    }

    if (kind <= 0) {    /* identification failed */
        printk("max6650.o: Unsupported chip.\n");
        goto ERROR1;
    }

    if (kind == max6650) {
        type_name = "max6650";
        client_name = "max6650 chip";
    } else {
        printk("max6650.o: Unknown kind %d.\n", kind);
        goto ERROR1;
    }
	
    /*
     * OK, we got a valid chip so we can fill in the remaining client
     * fields.
     */

    strcpy(new_client->name, client_name);
    data->valid = 0;
    init_MUTEX(&data->update_lock);

    /*
     * Tell the I2C layer a new client has arrived.
     */

    if ((err = i2c_attach_client(new_client))) {
#ifdef DEBUG
        printk("max6650.o: Failed attaching client.\n");
#endif
        goto ERROR1;
    }

    /*
     * Register a new directory entry.
     */
    if ((err = i2c_register_entry(new_client, type_name,
                                    max6650_dir_table_template,
				    THIS_MODULE)) < 0) {
#ifdef DEBUG
        printk("max6650.o: Failed registering directory entry.\n");
#endif
        goto ERROR2;
    }
    data->sysctl_id = err;

    /*
     * Initialize the max6650 chip
     */
    max6650_init_client(new_client);
    return 0;

ERROR2:
    i2c_detach_client(new_client);
ERROR1:
    kfree(data);
    return err;
}

static void max6650_init_client(struct i2c_client *client)
{
    /* Nothing to do here - assume the BIOS has initialized the chip */
}

static int max6650_detach_client(struct i2c_client *client)
{
    int err;

    i2c_deregister_entry(((struct max6650_data *) (client->data))->sysctl_id);
    if ((err = i2c_detach_client(client))) {
        printk("max6650.o: Client deregistration failed, "
                                        "client not detached.\n");
        return err;
    }

    kfree(client->data);
    return 0;
}

static int max6650_read(struct i2c_client *client, u8 reg)
{
    return i2c_smbus_read_byte_data(client, reg);
}

static int max6650_write(struct i2c_client *client, u8 reg, u8 value)
{
    return i2c_smbus_write_byte_data(client, reg, value);
}

static void max6650_update_client(struct i2c_client *client)
{
    int i;
    struct max6650_data *data = client->data;

    down(&data->update_lock);
    
    if ((jiffies - data->last_updated > HZ) ||
        (jiffies < data->last_updated) || !data->valid) {
#ifdef DEBUG
        printk("max6650.o: Updating max6650 data.\n");
#endif
        data->speed  = max6650_read (client, MAX6650_REG_SPEED);
        data->config = max6650_read (client, MAX6650_REG_CONFIG);
        for (i = 0; i < 4; i++) {
            data->tach[i] = max6650_read(client, tach_reg[i]);
        }
        data->count = max6650_read (client, MAX6650_REG_COUNT);
        data->last_updated = jiffies;
        data->valid = 1;
    }
    up(&data->update_lock);
}

static void max6650_fan (struct i2c_client *client, int operation, int
	ctl_name, int *nrels_mag, long *results)
{
    int index = ctl_name - MAX6650_SYSCTL_FAN1;
    struct max6650_data *data = client->data;
    int tcount;         /* Tachometer count time, 0.25 second units */

    if (operation == SENSORS_PROC_REAL_INFO) {
	*nrels_mag = 0;
    } else if (operation == SENSORS_PROC_REAL_READ) {
        max6650_update_client(client);

        /*
         * Calculation details:
         *
         * Each tachometer counts over an interval given by the "count"
         * register (0.25, 0.5, 1 or 2 seconds). This module assumes
         * that the fans produce two pulses per revolution (this seems
         * to be the most common).
         */
         
        tcount = 1 << data->count;         /* 0.25 second units */
        results[0] = (data->tach[index] * 240) / tcount;    /* counts per min */
        results[0] /= 2;                   /* Assume two counts per rev */
        *nrels_mag = 1;
    }
}

/*
 * Set the fan speed to the specified RPM (or read back the RPM setting).
 *
 * The MAX6650/1 will automatically control fan speed when in closed loop
 * mode.
 *
 * Assumptions:
 *
 * 1) The MAX6650/1 is running from its internal 254kHz clock (perhaps
 *    this should be made a module parameter).
 *
 * 2) The prescaler (low three bits of the config register) has already
 *    been set to an appropriate value.
 *
 * The relevant equations are given on pages 21 and 22 of the datasheet.
 *
 * From the datasheet, the relevant equation when in regulation is:
 *
 *    [fCLK / (128 x (KTACH + 1))] = 2 x FanSpeed / KSCALE
 *
 * where:
 *
 *    fCLK is the oscillator frequency (either the 254kHz internal 
 *         oscillator or the externally applied clock)
 *
 *    KTACH is the value in the speed register
 *
 *    FanSpeed is the speed of the fan in rps
 *
 *    KSCALE is the prescaler value (1, 2, 4, 8, or 16)
 *
 * When reading, we need to solve for FanSpeed. When writing, we need to
 * solve for KTACH.
 *
 * Note: this tachometer is completely separate from the tachometers
 * used to measure the fan speeds. Only one fan's speed (fan1) is
 * controlled.
 */

static void max6650_speed (struct i2c_client *client, int operation, int
                        	ctl_name, int *nrels_mag, long *results)
{
    struct max6650_data *data = client->data;
    int kscale, ktach, fclk, rpm;
    
    if (operation == SENSORS_PROC_REAL_INFO) {
        *nrels_mag = 0;
    } else if (operation == SENSORS_PROC_REAL_READ) {
        /*
         * Use the datasheet equation:
         *
         *    FanSpeed = KSCALE x fCLK / [256 x (KTACH + 1)]
         *
         * then multiply by 60 to give rpm.
         */

        max6650_update_client(client);

        kscale = 1 << (data->config & 7);
        ktach  = data->speed;
        fclk   = MAX6650_INT_CLK;
        rpm    = 60 * kscale * fclk / (256 * (ktach + 1));

        results[0] = rpm;
        *nrels_mag = 1;
    } else if (operation == SENSORS_PROC_REAL_WRITE && *nrels_mag >= 1) {
        /*
         * Divide the required speed by 60 to get from rpm to rps, then
         * use the datasheet equation:
         *
         *     KTACH = [(fCLK x KSCALE) / (256 x FanSpeed)] - 1
         */

        max6650_update_client(client);

        rpm    = results[0];
        kscale = 1 << (data->config & 7);
        fclk   = MAX6650_INT_CLK;
        ktach  = ((fclk * kscale) / (256 * rpm / 60)) - 1;

        data->speed  = ktach;
        data->config = (data->config & ~MAX6650_CFG_MODE_MASK) | 
                                            MAX6650_CFG_MODE_CLOSED_LOOP;
        max6650_write (client, MAX6650_REG_CONFIG, data->config);
        max6650_write (client, MAX6650_REG_SPEED,  data->speed);
    }
}

/*
 * Debug - dump all registers except the tach counts.
 */
                                 
static void max6650_xdump (struct i2c_client *client, int operation, int
	ctl_name, int *nrels_mag, long *results)
{
    if (operation == SENSORS_PROC_REAL_INFO) {
        *nrels_mag = 0;
    } else if (operation == SENSORS_PROC_REAL_READ) {
        results[0] = max6650_read (client, MAX6650_REG_SPEED);
        results[1] = max6650_read (client, MAX6650_REG_CONFIG);
        results[2] = max6650_read (client, MAX6650_REG_GPIO_DEF);
        results[3] = max6650_read (client, MAX6650_REG_DAC);
        results[4] = max6650_read (client, MAX6650_REG_ALARM_EN);
        results[5] = max6650_read (client, MAX6650_REG_ALARM);
        results[6] = max6650_read (client, MAX6650_REG_GPIO_STAT);
        results[7] = max6650_read (client, MAX6650_REG_COUNT);
        *nrels_mag = 8;
    }
}

static int __init sm_max6650_init(void)
{
    printk(KERN_INFO "max6650.o version %s (%s)\n", LM_VERSION, LM_DATE);
    return i2c_add_driver(&max6650_driver);
}

static void __exit sm_max6650_exit(void)
{
    i2c_del_driver(&max6650_driver);
}

MODULE_AUTHOR("john.morris@spirentcom.com");
MODULE_DESCRIPTION("max6650 sensor driver");
MODULE_LICENSE("GPL");

module_init(sm_max6650_init);
module_exit(sm_max6650_exit);
