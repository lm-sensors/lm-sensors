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
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/i2c-proc.h>
#include <linux/init.h>
#include "version.h"

#define DEBUG 1

/* So that the module will still compile without i2c-2.6.4;
   if inserted, it will just put out a message and exit in that case */
#ifdef I2C_CLIENT_PEC
#define I2C_PEC_SUPPORTED
#else
#define I2C_DRIVERID_ARP        902    /* SMBus ARP Client              */
#endif

/* Addresses to scan */
#define	ARP_ADDRESS	0x61
static unsigned short normal_i2c[] = { ARP_ADDRESS, SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { SENSORS_I2C_END };
static unsigned int normal_isa[] = { SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/* Insmod parameters */
SENSORS_INSMOD_1(arp);

static int reset;
MODULE_PARM(reset, "i");
MODULE_PARM_DESC(reset,
		 "Send an ARP Reset Device message to each bus");

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
                        0x37, ARP_ADDRESS,
/* As defined by SMBus Spec. Sect. 5.2 */
			0x01, 0x02, 0x03, 0x04, 0x05,
			0x06, 0x07, 0x78, 0x79, 0x7a, 0x7b,
			0x7c, 0x7d, 0x7e, 0x7f,
/* Common PC addresses (bad idea) */
			0x2d, 0x48, 0x49, /* sensors */
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
	u8 udid[UDID_LENGTH];
	u8 dev_cap;
	u8 dev_ver;
	u16 dev_vid;
	u16 dev_did;
	u16 dev_int;
	u16 dev_svid;
	u16 dev_sdid;
	u32 dev_vsid;
	u8 saddr;
};

/* Each client has this additional data */
struct arp_data {
	struct i2c_client client;
	int sysctl_id;

	struct semaphore update_lock;
	char valid;		/* !=0 if following fields are valid */
	unsigned long last_updated;	/* In jiffies */

	u8 address_pool[SMBUS_ADDRESS_SIZE];
	struct arp_device dev[ARP_MAX_DEVICES];
};


static int smbusarp_attach_adapter(struct i2c_adapter *adapter);
static int smbusarp_detect(struct i2c_adapter *adapter, int address,
			 unsigned short flags, int kind);
static int smbusarp_detach_client(struct i2c_client *client);

static int smbusarp_init_client(struct i2c_client *client);
static void smbusarp_contents(struct i2c_client *client, int operation,
			    int ctl_name, int *nrels_mag, long *results);

static struct i2c_driver smbusarp_driver = {
	.name		= "SMBUS ARP",
	.id		= I2C_DRIVERID_ARP,
	.flags		= I2C_DF_NOTIFY,
	.attach_adapter	= smbusarp_attach_adapter,
	.detach_client	= smbusarp_detach_client,
};

/* -- SENSORS SYSCTL START -- */
#define ARP_SYSCTL1 1000
#define ARP_SYSCTL2 1001
#define ARP_SYSCTL3 1002
#define ARP_SYSCTL4 1003
#define ARP_SYSCTL5 1004
#define ARP_SYSCTL6 1005
#define ARP_SYSCTL7 1006
#define ARP_SYSCTL8 1007

/* -- SENSORS SYSCTL END -- */

static ctl_table smbusarp_dir_table_template[] = {
	{ARP_SYSCTL1, "0", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &smbusarp_contents},
	{ARP_SYSCTL2, "1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &smbusarp_contents},
	{ARP_SYSCTL3, "2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &smbusarp_contents},
	{ARP_SYSCTL4, "3", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &smbusarp_contents},
	{ARP_SYSCTL5, "4", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &smbusarp_contents},
	{ARP_SYSCTL6, "5", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &smbusarp_contents},
	{ARP_SYSCTL7, "6", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &smbusarp_contents},
	{ARP_SYSCTL8, "7", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &smbusarp_contents},
	{0}
};

static int smbusarp_attach_adapter(struct i2c_adapter *adapter)
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

	/* must support block data w/ HW PEC or SW PEC */
	if ((!i2c_check_functionality(adapter,
	        I2C_FUNC_SMBUS_BLOCK_DATA | I2C_FUNC_SMBUS_HWPEC_CALC)) &&
	    (!i2c_check_functionality(adapter,
	        I2C_FUNC_SMBUS_BLOCK_DATA_PEC)))
		return(0);

	if (!(data = kmalloc(sizeof(struct arp_data), GFP_KERNEL))) {
		return(-ENOMEM);
	}

	new_client = &data->client;
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

	strcpy(new_client->name, client_name);
	data->valid = 0;
	init_MUTEX(&data->update_lock);

	if ((err = i2c_attach_client(new_client)))
		goto ERROR1;

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
	kfree(data);
	return err;
}

static int smbusarp_detach_client(struct i2c_client *client)
{
	int err;

	i2c_deregister_entry(((struct arp_data *) (client->data))->
				 sysctl_id);

	if ((err = i2c_detach_client(client))) {
		printk
		    (KERN_ERR "smbus-arp.o: Client deregistration failed, client not detached.\n");
		return err;
	}

	kfree(client->data);

	return 0;
}


static u8 choose_addr(u8 * pool)
{
	int i;

	for(i = 0; i < 0x7f; i++) {
		if(pool[i] == ARP_FREE)
			return ((u8) i);
	}
	return 0xff;
}

static int smbusarp_init_client(struct i2c_client *client)
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
	
	for(i = 0; i < ARP_MAX_DEVICES; i++)
		data->dev[i].status = ARP_FREE;

	for(i = 0; i < SMBUS_ADDRESS_SIZE; i++)
		data->address_pool[i] = ARP_FREE;

	r = reserved;
	do {
		data->address_pool[*r] = ARP_RESERVED;
	} while(*r++);

	for (i = 0; i < I2C_CLIENT_MAX ; i++) 
		if (client->adapter->clients[i])
			data->address_pool[client->adapter->clients[i]->addr] =
			                                   ARP_BUSY;

	if(reset)
		i2c_smbus_write_byte(client, ARP_RESET_DEV);
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
		addr = blk[16];
		if(addr != 0xFF) {
			addr >>= 1;
			if(blk[0] & 0xC0) {
				if(data->address_pool[addr] == ARP_FREE) {
#ifdef DEBUG
					printk(KERN_DEBUG "             Requested free Non-fixed Address 0x%02x\n", addr);
#endif
				} else {
#ifdef DEBUG
					printk(KERN_DEBUG "             Requested busy Non-fixed Address 0x%02x\n", addr);
#endif
					if((addr =
					    choose_addr(data->address_pool)) ==
					                       0xff) {
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
		/* store things both ways */
		for(i = 0; i < UDID_LENGTH; i++)
			data->dev[newdev].udid[i] = blk[i];
		data->dev[newdev].saddr = addr;
		data->dev[newdev].status = ARP_BUSY;
		data->dev[newdev].dev_cap = blk[0];
		data->dev[newdev].dev_ver = blk[1];
		data->dev[newdev].dev_vid = (blk[2] << 8) | blk[3];
		data->dev[newdev].dev_did = (blk[4] << 8) | blk[5];
		data->dev[newdev].dev_int = (blk[6] << 8) | blk[7];
		data->dev[newdev].dev_svid = (blk[8] << 8) | blk[9];
		data->dev[newdev].dev_sdid = (blk[10] << 8) | blk[11];
		data->dev[newdev].dev_vsid = (blk[12] << 24) | (blk[13] << 16) |
		                             (blk[14] << 8) | blk[15] ;

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


/* reassign address on writex */
void smbusarp_contents(struct i2c_client *client, int operation,
		     int ctl_name, int *nrels_mag, long *results)
{
	int nr = ctl_name - ARP_SYSCTL1;
	struct arp_data *data = client->data;
	int ret;
	u8 save;
	u8 a;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		if(data->dev[nr].status == ARP_BUSY) {
			results[0] = data->dev[nr].saddr;
			results[1] = data->dev[nr].dev_cap;
			results[2] = data->dev[nr].dev_ver;
			results[3] = data->dev[nr].dev_vid;
			results[4] = data->dev[nr].dev_did;
			results[5] = data->dev[nr].dev_int;
			results[6] = data->dev[nr].dev_svid;
			results[7] = data->dev[nr].dev_sdid;
			results[8] = data->dev[nr].dev_vsid;
			*nrels_mag = 9;
		} else {
			*nrels_mag = 0;
		}
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		a = results[0];
		if(*nrels_mag >= 1 &&
		   a < SMBUS_ADDRESS_SIZE &&
		   data->dev[nr].status == ARP_BUSY &&
		   data->address_pool[a] == ARP_FREE) {
			save = data->dev[nr].udid[16];
			data->dev[nr].udid[16] = a << 1;
			ret = i2c_smbus_write_block_data(client, ARP_ASSIGN_ADDR,
                                 UDID_LENGTH, data->dev[nr].udid);
			if(ret) {
				data->dev[nr].udid[16] = save;
#ifdef DEBUG
				printk(KERN_DEBUG "smbus-arp Bad response, address 0x%02x not assigned\n", a);
#endif
			} else {
				data->dev[nr].saddr = a;
				data->address_pool[a] = ARP_BUSY;
#ifdef DEBUG
				printk(KERN_DEBUG "smbus-arp Assigned address 0x%02x\n", a);
#endif
			}
		} else {
			printk(KERN_WARNING "smbus-arp Bad address 0x%02x\n", a);
		}
	}
}

static int __init sm_smbusarp_init(void)
{
	printk("smbus-arp.o version %s (%s)\n", LM_VERSION, LM_DATE);
/* magic force invocation */
	force_arp[0] = -1;
	force_arp[1] = ARP_ADDRESS;
	return i2c_add_driver(&smbusarp_driver);
}

static void __exit sm_smbusarp_exit(void)
{
	i2c_del_driver(&smbusarp_driver);
}



MODULE_AUTHOR("Mark D. Studebaker <mdsxyz123@yahoo.com>");
MODULE_DESCRIPTION("SMBUS ARP Driver");
MODULE_LICENSE("GPL");

module_init(sm_smbusarp_init);
module_exit(sm_smbusarp_exit);
