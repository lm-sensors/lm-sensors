/*
    smbus-arp.c - Part of lm_sensors, Linux kernel modules for hardware
               monitoring
    Copyright (c) 2002  Mark D. Studebaker <mdsxyz123@yahoo.com>

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
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include "sensors.h"
#include "version.h"
#include <linux/init.h>

#define DEBUG 1

/* So that the module will still compile without i2c-2.6.4;
   if inserted, it will just put out a message and exit in that case */
#ifdef I2C_CLIENT_PEC
#define I2C_PEC_SUPPORTED
#else
#define I2C_DRIVERID_ARP        902    /* SMBus ARP Client              */
#endif

#ifndef THIS_MODULE
#define THIS_MODULE NULL
#endif

/* Addresses to scan */
#define	ARP_ADDRESS	0x61
static unsigned short normal_i2c[] = { ARP_ADDRESS, SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { SENSORS_I2C_END };
static unsigned int normal_isa[] = { SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/* Insmod parameters */
SENSORS_INSMOD_1(arp);

/* ARP Commands */
#define	ARP_PREPARE	0x01
#define	ARP_RESET_DEV	0x02
#define	ARP_GET_UDID_GEN 0x03
#define	ARP_ASSIGN_ADDR	0x04

/* UDID Fields */
#define ARP_CAPAB	0
#define ARP_VER		1
#define ARP_VEND	2
#define ARP_DEV		4
#define ARP_INT		6
#define ARP_SUBVEND	8
#define ARP_SUBDEV	10
#define ARP_SPECID	12

#define UDID_LENGTH	0x11

static u8 reserved[] =
/* As defined by SMBus Spec. Appendix C */
			{0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x28,
                        0x2c, 0x2d, 0x37, 0x40, 0x41, 0x42,
	                0x43, 0x44, ARP_ADDRESS,
/* As defined by SMBus Spec. Sect. 5.2 */
			0x01, 0x02, 0x03, 0x04, 0x05,
			0x06, 0x07, 0x78, 0x79, 0x7a, 0x7b,
			0x7c, 0x7d, 0x7e, 0x7f,
/* Common PC addresses (bad idea) */
			0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, /* eeproms */
			0x69, /* clock chips */
/* Must end in 0 which is also reserved */
			0x00};

#define SMBUS_ADDRESS_SIZE	0x80
#define ARP_FREE	0	
#define ARP_RESERVED	1
#define ARP_BUSY	2

#define ARP_MAX_DEVICES 8
struct arp_device {
	int status;
	u8 udid[16];	
	u8 saddr;
};

/* Each client has this additional data */
struct arp_data {
	int sysctl_id;

	struct semaphore update_lock;
	char valid;		/* !=0 if following fields are valid */
	unsigned long last_updated;	/* In jiffies */

	u8 address_pool[SMBUS_ADDRESS_SIZE];
	struct arp_device dev[ARP_MAX_DEVICES];
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
int __init sensors_smbusarp_init(void);
static int __init smbusarp_cleanup(void);

static int smbusarp_attach_adapter(struct i2c_adapter *adapter);
static int smbusarp_detect(struct i2c_adapter *adapter, int address,
			 unsigned short flags, int kind);
static int smbusarp_detach_client(struct i2c_client *client);
static int smbusarp_command(struct i2c_client *client, unsigned int cmd,
			  void *arg);

static void smbusarp_inc_use(struct i2c_client *client);
static void smbusarp_dec_use(struct i2c_client *client);
static int smbusarp_init_client(struct i2c_client *client);
static void smbusarp_contents(struct i2c_client *client, int operation,
			    int ctl_name, int *nrels_mag, long *results);
static void smbusarp_update_client(struct i2c_client *client);


/* This is the driver that will be inserted */
static struct i2c_driver smbusarp_driver = {
	/* name */ "SMBUS ARP",
	/* id */ I2C_DRIVERID_ARP,
	/* flags */ I2C_DF_NOTIFY,
	/* attach_adapter */ &smbusarp_attach_adapter,
	/* detach_client */ &smbusarp_detach_client,
	/* command */ &smbusarp_command,
	/* inc_use */ &smbusarp_inc_use,
	/* dec_use */ &smbusarp_dec_use
};

/* These files are created for each bus */
static ctl_table smbusarp_dir_table_template[] = {
	{0}
};

/* Used by init/cleanup */
static int __initdata smbusarp_initialized = 0;

static int smbusarp_id = 0;

int smbusarp_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_detect(adapter, &addr_data, smbusarp_detect);
}

/* This function is called by i2c_detect */
int smbusarp_detect(struct i2c_adapter *adapter, int address,
		  unsigned short flags, int kind)
{
	int i;
	struct i2c_client *new_client;
	struct arp_data *data;
	int err = 0;
	const char *type_name, *client_name;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BLOCK_DATA))
		return(0);

	if (!(new_client = kmalloc(sizeof(struct i2c_client) +
				   sizeof(struct arp_data),
				   GFP_KERNEL))) {
		return(-ENOMEM);
	}

	data = (struct arp_data *) (new_client + 1);
	new_client->addr = address;
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &smbusarp_driver;
#ifdef I2C_PEC_SUPPORTED
	new_client->flags = I2C_CLIENT_PEC;
#else
	new_client->flags = 0;
#endif

	type_name = "arp";
	client_name = "SMBUS ARP client";

	/* Fill in the remaining client fields and put it into the global list */
	strcpy(new_client->name, client_name);

	new_client->id = smbusarp_id++;
	data->valid = 0;
	init_MUTEX(&data->update_lock);

	/* Tell the I2C layer a new client has arrived */
	if ((err = i2c_attach_client(new_client)))
		goto ERROR1;

	/* Register a new directory entry with module sensors */
	if ((i = i2c_register_entry(new_client, type_name,
					smbusarp_dir_table_template,
					THIS_MODULE)) < 0) {
		err = i;
		goto ERROR4;
	}
	data->sysctl_id = i;
	smbusarp_init_client(new_client);
	return 0;

      ERROR4:
	i2c_detach_client(new_client);
      ERROR1:
	kfree(new_client);
	return err;
}

int smbusarp_detach_client(struct i2c_client *client)
{
	int err;

	i2c_deregister_entry(((struct arp_data *) (client->data))->
				 sysctl_id);

	if ((err = i2c_detach_client(client))) {
		printk
		    (KERN_ERR "smbus-arp.o: Client deregistration failed, client not detached.\n");
		return err;
	}

	kfree(client);

	return 0;
}


int smbusarp_command(struct i2c_client *client, unsigned int cmd, void *arg)
{
	return 0;
}

void smbusarp_inc_use(struct i2c_client *client)
{
#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif
}

void smbusarp_dec_use(struct i2c_client *client)
{
#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
}


u8 choose_addr(u8 * pool)
{
	int i;

	for(i = 0; i < 0x7f; i++) {
		if(pool[i] == ARP_FREE)
			return ((u8) i);
	}
	return 0xff;
}

int smbusarp_init_client(struct i2c_client *client)
{
	int ret = -1;
#ifdef I2C_PEC_SUPPORTED
	struct arp_data *data = client->data;
	u8 blk[I2C_SMBUS_BLOCK_MAX];
	u8 *r;
	u8 addr;
	int i;
	int found = 0;
	int newdev = 0;
	
	for(i = 0; i < SMBUS_ADDRESS_SIZE; i++)
		data->address_pool[i] = ARP_FREE;

	r = reserved;
	do {
		data->address_pool[*r] = ARP_RESERVED;
	} while(*r++);

	for (i = 0; i < I2C_CLIENT_MAX ; i++) 
		if (client->adapter->clients[i])
			data->address_pool[client->adapter->clients[i]->addr] = ARP_BUSY;

	ret = i2c_smbus_write_byte(client, ARP_PREPARE);
	if(ret < 0) {
#ifdef DEBUG
		printk(KERN_DEBUG "smbus-arp.o: No ARP response on adapter 0x%X\n", client->adapter->id);
#endif
		return(-1); /* Packet wasn't acked */
	}
	while(1) {
		ret = i2c_smbus_read_block_data(client, ARP_GET_UDID_GEN, blk);
		if(ret != UDID_LENGTH) {
#ifdef DEBUG
			printk(KERN_DEBUG "smbus-arp.o: No/Bad UDID response %d on adapter 0x%X\n", ret, client->adapter->id);
#endif
			if(found)
				return(found);
			else
				return(-1); /* Bad response */
		}
#ifdef DEBUG
		printk(KERN_DEBUG "smbus-arp.o: Good UDID response on adapter 0x%X\n", client->adapter->id);
		printk(KERN_DEBUG "             Cap. 0x%02x  Rev. 0x%02x  Vend. 0x%02x%02x  Dev. 0x%02x%02x\n", blk[0], blk[1], blk[2], blk[3], blk[4], blk[5]);
		printk(KERN_DEBUG "             Int. 0x%02x%02x  Subvend. 0x%02x%02x  Subdev. 0x%02x%02x  Spec. 0x%02x%02x%02x%02x\n", blk[6], blk[7], blk[8], blk[9], blk[10], blk[11], blk[12], blk[13], blk[14], blk[15]);
#endif
/* clean up this... */
		found++;
		do{
			if(data->dev[newdev].status == ARP_FREE)
				break;
		} while(++newdev < ARP_MAX_DEVICES);
		if(newdev == ARP_MAX_DEVICES) {
			printk(KERN_WARNING "smbus-arp.o: No more slots available\n");
			return(-1);
		}

		/* check device slave addr */ 		
		addr = blk[17];
		if(addr != 0xFF) {
			addr >>= 1;
			if(blk[0] & 0xC0) {
				if(data->address_pool[addr] == ARP_FREE) {
#ifdef DEBUG
					printk(KERN_DEBUG "             Free Non-fixed Address 0x%02x\n", addr);
#endif
				} else {
#ifdef DEBUG
					printk(KERN_DEBUG "             Taken Non-fixed Address 0x%02x\n", addr);
#endif
					if((addr = choose_addr(data->address_pool)) == 0xff) {
						printk(KERN_WARNING "smbus-arp.o: Address pool exhausted\n");
						return(-1);
					}
				}
			} else {
#ifdef DEBUG
				printk(KERN_DEBUG "             Fixed Address 0x%02x\n", addr);
#endif
			}
		} else {
#ifdef DEBUG
			printk(KERN_DEBUG "             No Address\n");
#endif
			if((addr = choose_addr(data->address_pool)) == 0xff) {
				printk(KERN_WARNING "smbus-arp.o: Address pool exhausted\n");
				return(-1);
			}

		}
		for(i = 0; i < 16; i++)
		data->dev[newdev].saddr = addr;
			data->dev[newdev].udid[i] = blk[i];

		blk[16] = addr << 1;
		ret = i2c_smbus_write_block_data(client, ARP_ASSIGN_ADDR,
		                                 UDID_LENGTH, blk);
		if(ret) {
#ifdef DEBUG
			printk(KERN_DEBUG "             Bad response, address 0x%02x not assigned\n", addr);
#endif
		} else {
			data->address_pool[addr] = ARP_BUSY;
#ifdef DEBUG
			printk(KERN_DEBUG "             Assigned address 0x%02x\n", addr);
#endif
		}
			/* retry? */

	} /* while 1  */
#else
	printk(KERN_WARNING "smbus-arp.o: A client responded to the ARP address but "
	                    "your kernel does not support SMBus ARP/PEC!\n");
	ret = -EPERM;
#endif
	return(ret);
}

void smbusarp_update_client(struct i2c_client *client)
{
	struct arp_data *data = client->data;

	down(&data->update_lock);

	if ((jiffies - data->last_updated > 300 * HZ) |
	    (jiffies < data->last_updated) || !data->valid) {


		data->last_updated = jiffies;
		data->valid = 1;
	}

	up(&data->update_lock);
}

int __init sensors_smbusarp_init(void)
{
	int res;

	printk("smbus-arp.o version %s (%s)\n", LM_VERSION, LM_DATE);
/* magic force invocation */
	force_arp[0] = -1;
	force_arp[1] = ARP_ADDRESS;
	smbusarp_initialized = 0;
	if ((res = i2c_add_driver(&smbusarp_driver))) {
		printk
		    (KERN_ERR "smbus-arp.o: Driver registration failed, module not inserted.\n");
		smbusarp_cleanup();
		return res;
	}
	smbusarp_initialized++;
	return 0;
}

int __init smbusarp_cleanup(void)
{
	int res;

	if (smbusarp_initialized >= 1) {
		if ((res = i2c_del_driver(&smbusarp_driver))) {
			printk
			    (KERN_ERR "smbus-arp.o: Driver deregistration failed, module not removed.\n");
			return res;
		}
	} else
		smbusarp_initialized--;

	return 0;
}

EXPORT_NO_SYMBOLS;

#ifdef MODULE
MODULE_AUTHOR("Mark D. Studebaker <mdsxyz123@yahoo.com>");
MODULE_DESCRIPTION("SMBUS ARP Driver");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif

int init_module(void)
{
	return sensors_smbusarp_init();
}

int cleanup_module(void)
{
	return smbusarp_cleanup();
}

#endif				/* MODULE */
