
/*
 * linux/drivers/i2c/ds1307.c
 *
 * Author: Abraham van der Merwe <abraham@2d3d.co.za>
 *
 * Linux support for the Dallas Semiconductor DS1307 Serial Real-Time
 * Clock.
 *
 * Based on code from the lm-sensors project which is available
 * at http://www.lm-sensors.nu/ and Russell King's PCF8583 Real-Time
 * Clock driver (linux/drivers/acorn/char/pcf8583.c).
 *
 * This source code is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/i2c-proc.h>
#include <asm/semaphore.h>
#include "version.h"
#include "ds1307.h"

#define BCD_TO_BIN(x) (((x) & 15) + ((x) >> 4) * 10)
#define BIN_TO_BCD(x) ((((x) / 10) << 4) + (x) % 10)

static struct i2c_driver ds1307;
static DECLARE_MUTEX (mutex);

/*
 * The DS1307 Real-Time Clock wants the address in a different
 * message, so we can't use the normal i2c_master_recv() routine
 * for receiving data.
 */
static int ds1307_i2c_recv (struct i2c_client *client,char *buf,char addr,int count)
{
	struct i2c_msg msg[] = {
		{ addr: client->addr, flags: 0,        len: 1,     buf: &addr },
		{ addr: client->addr, flags: I2C_M_RD, len: count, buf: buf }
	};
	int result = 0;

	if (down_interruptible (&mutex))
		return (-ERESTARTSYS);

	if (i2c_transfer (client->adapter,msg,2) != 2)
		result = -EIO;

	up (&mutex);

	return (result);
}

/*
 * Would've been nice to specify the address to this as well, but then we
 * would need to copy the buffer twice - not worth it...
 */
static int ds1307_i2c_send (struct i2c_client *client,const char *buf,int count)
{
	int result = 0;

	if (down_interruptible (&mutex))
		return (-ERESTARTSYS);

	if (i2c_master_send (client,(const char *) buf,count) != count)
		result = -EIO;

	up (&mutex);

	return (result);
}

static int ds1307_attach (struct i2c_adapter *adapter,int addr,unsigned short flags,int kind)
{
	struct i2c_client *client;
	int result;

	if ((client = (struct i2c_client *) kmalloc (sizeof (struct i2c_client),GFP_KERNEL)) == NULL)
		return (-ENOMEM);

	strcpy (client->name,ds1307.name);
	client->flags = I2C_CLIENT_ALLOW_USE | I2C_CLIENT_ALLOW_MULTIPLE_USE;
	client->addr = addr;
	client->adapter = adapter;
	client->driver = &ds1307;
	client->data = NULL;

	if ((result = i2c_attach_client (client))) {
		kfree (client);
		return (result);
	}

	return (0);
}

static int ds1307_attach_adapter (struct i2c_adapter *adapter)
{
	static unsigned short ignore[] = { I2C_CLIENT_END };
	static unsigned short addr[] = { 0x68, I2C_CLIENT_END };
	static struct i2c_client_address_data ds1307_addr_data = {
		normal_i2c:			addr,
		normal_i2c_range:	ignore,
		probe:				ignore,
		probe_range:		ignore,
		ignore:				ignore,
		ignore_range:		ignore,
		force:				ignore
	};

	return (i2c_probe (adapter,&ds1307_addr_data,ds1307_attach));
}

static int ds1307_detach_client (struct i2c_client *client)
{
	int result;

	if ((result = i2c_detach_client (client)))
		return (result);

	kfree (client);

	return (0);
}

static int ds1307_getdate (struct i2c_client *client,void *arg)
{
	struct ds1307_date *date = (struct ds1307_date *) arg;
	u8 buf[7];

	/* this also enables the oscillator */
	memset (buf,0,7);

	/* enable 24-hour mode */
	buf[2] = 0x40;

	if (ds1307_i2c_recv (client,(char *) buf,0,7) < 0)
		return (-EIO);

	date->tm_sec = BCD_TO_BIN (buf[0] & ~0x80);
	date->tm_min = BCD_TO_BIN (buf[1]);
	date->tm_hour = BCD_TO_BIN (buf[2] & 0x3f);
	date->tm_wday = BCD_TO_BIN (buf[3]) - 1;
	date->tm_mday = BCD_TO_BIN (buf[4]);
	date->tm_mon = BCD_TO_BIN (buf[5]) - 1;
	date->tm_year = BCD_TO_BIN (buf[6]) + 100;

	return (0);
}

static int ds1307_setdate (struct i2c_client *client,void *arg)
{
	struct ds1307_date *date = (struct ds1307_date *) arg;
	u8 buf[8];

	/* select address 0 */
	buf[0] = 0;

	buf[1] = BIN_TO_BCD (date->tm_sec);
	buf[2] = BIN_TO_BCD (date->tm_min);
	buf[3] = BIN_TO_BCD (date->tm_hour) | 0x40;
	buf[4] = BIN_TO_BCD (date->tm_wday + 1);
	buf[5] = BIN_TO_BCD (date->tm_mday);
	buf[6] = BIN_TO_BCD (date->tm_mon + 1);
	buf[7] = BIN_TO_BCD (date->tm_year - 100);

	return (ds1307_i2c_send (client,(const char *) buf,8));
}

static int ds1307_enable (struct i2c_client *client,void *arg)
{
	u8 buf[2];

	if (ds1307_i2c_recv (client,(char *) buf + 1,0,1) < 0)
		return (-EIO);

	if ((buf[1] & 0x80)) {
		buf[0] = 0, buf[1] &= ~0x80;
		return (ds1307_i2c_send (client,(const char *) buf,2));
	}

	return (0);
}

static int ds1307_irqon (struct i2c_client *client,void *arg)
{
	u8 buf[2];

	if (ds1307_i2c_recv (client,(char *) buf + 1,7,1) < 0)
		return (-EIO);

	buf[0] = 7;
	buf[1] |= 0x10;

	return (ds1307_i2c_send (client,(const char *) buf,2));
}

static int ds1307_irqoff (struct i2c_client *client,void *arg)
{
	u8 buf[2];

	if (ds1307_i2c_recv (client,(char *) buf + 1,7,1) < 0)
		return (-EIO);

	buf[0] = 7;
	buf[1] &= ~0x10;

	return (ds1307_i2c_send (client,(const char *) buf,2));
}

static int ds1307_getfreq (struct i2c_client *client,void *arg)
{
	u16 *freq = (u16 *) arg;
	u8 buf;
	static const u16 table[] = {
		DS1307_FREQ_1HZ,
		DS1307_FREQ_4KHZ,
		DS1307_FREQ_8KHZ,
		DS1307_FREQ_32KHZ
	};

	if (ds1307_i2c_recv (client,(char *) &buf,7,1) < 0)
		return (-EIO);

	*freq = table[buf & 3];

	return (0);
}

static int ds1307_setfreq (struct i2c_client *client,void *arg)
{
	u16 *freq = (u16 *) arg;
	u8 buf[2];

	/* select address 7 */
	buf[0] = 7;

	/* default to 1HZ */
	buf[1] = 0;

	switch (*freq) {
	case DS1307_FREQ_32KHZ:		buf[1]++;
	case DS1307_FREQ_8KHZ:		buf[1]++;
	case DS1307_FREQ_4KHZ:		buf[1]++;
	case DS1307_FREQ_1HZ:		break;
	default:
		return (-EINVAL);
	}

	return (ds1307_i2c_send (client,(const char *) buf,2));
}

static int ds1307_read (struct i2c_client *client,void *arg)
{
	struct ds1307_memory *mem = (struct ds1307_memory *) arg;
	u8 buf[DS1307_SIZE];

	if (mem->offset >= DS1307_SIZE || mem->offset + mem->length > DS1307_SIZE)
		return (-EINVAL);

	if (ds1307_i2c_recv (client,(char *) buf,mem->offset + 8,mem->length) < 0)
		return (-EIO);

	memcpy (mem->buf,buf,mem->length);

	return (0);
}

static int ds1307_write (struct i2c_client *client,void *arg)
{
	struct ds1307_memory *mem = (struct ds1307_memory *) arg;
	u8 buf[DS1307_SIZE + 1];

	if (mem->offset >= DS1307_SIZE || mem->offset + mem->length > DS1307_SIZE)
		return (-EINVAL);

	buf[0] = mem->offset + 8;

	memcpy (buf + 1,mem->buf,mem->length);

	return (ds1307_i2c_send (client,(const char *) buf,mem->length + 1));
}

static int ds1307_command (struct i2c_client *client,unsigned int cmd,void *arg)
{
	static const struct {
		int cmd;
		int (*function) (struct i2c_client *,void *arg);
	} ioctl[] = {
		{ DS1307_ENABLE, ds1307_enable },
		{ DS1307_GET_DATE, ds1307_getdate },
		{ DS1307_SET_DATE, ds1307_setdate },
		{ DS1307_IRQ_ON, ds1307_irqon },
		{ DS1307_IRQ_OFF, ds1307_irqoff },
		{ DS1307_GET_FREQ, ds1307_getfreq },
		{ DS1307_SET_FREQ, ds1307_setfreq },
		{ DS1307_READ, ds1307_read },
		{ DS1307_WRITE, ds1307_write }
	};
	int i;

	for (i = 0; i < sizeof (ioctl) / sizeof (ioctl[0]); i++)
		if (ioctl[i].cmd == cmd)
			return (ioctl[i].function (client,arg));

	return (-EINVAL);
}


static struct i2c_driver ds1307 = {
	.name		= "ds1307",
	.id		= I2C_DRIVERID_DS1307,
	.flags		= I2C_DF_NOTIFY,
	.attach_adapter	= ds1307_attach_adapter,
	.detach_client	= ds1307_detach_client,
	.command	= ds1307_command,
};

static int __init sm_ds1307_init(void)
{
	printk("ds1307.o version %s (%s)\n", LM_VERSION, LM_DATE);
	return i2c_add_driver(&ds1307);
}

static void __exit sm_ds1307_exit(void)
{
	i2c_del_driver(&ds1307);
}



MODULE_AUTHOR ("Abraham van der Merwe <abraham@2d3d.co.za>");
MODULE_DESCRIPTION ("Linux support for DS1307 Real-Time Clock");

MODULE_LICENSE ("GPL");

module_init(sm_ds1307_init);
module_exit(sm_ds1307_exit);

