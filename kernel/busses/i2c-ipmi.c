/*
    i2c-bmc.c - Part of lm_sensors, Linux kernel modules for hardware
            monitoring
    Copyright (c) 2002 Mark D. Studebaker <mdsxyz123@yahoo.com>

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

/*
	This implements an
*/

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/ipmi.h>
#include "version.h"


static void bmc_inc_use(struct i2c_adapter *adapter);
static void bmc_dec_use(struct i2c_adapter *adapter);
static u32 bmc_func(struct i2c_adapter *adapter);

#ifdef MODULE
static
#else
extern
#endif
int __init i2c_bmc_init(void);
static int __init bmc_cleanup(void);

#ifdef MODULE
extern int init_module(void);
extern int cleanup_module(void);
#endif				/* MODULE */

/* I2C Data */
/* This is the actual algorithm we define */
static struct i2c_algorithm bmc_algorithm = {
	/* name */ "IPMI algorithm",
	/* id */ I2C_ALGO_IPMI,
	/* master_xfer */ NULL,
	/* smbus_access */ NULL,
	/* slave_send */ NULL,
	/* slave_rcv */ NULL,
	/* algo_control */ NULL,
	/* functionality */ &bmc_func,
};

static struct i2c_adapter bmc_adapter = {
	/* name */ "IPMI adapter",
	/* id */ I2C_ALGO_IPMI | I2C_HW_IPMI,
	/* algorithm */ &bmc_algorithm,
	/* algo_data */ NULL,
	/* inc_use */ &bmc_inc_use,
	/* dec_use */ &bmc_dec_use,
	/* data */ NULL,
	/* Other fields not initialized */
};

/* IPMI Data */
static ipmi_user_t i2c_bmc_user;
static ipmi_user_t i2c_ipmi_user;
static unsigned char ipmi_version_major;
static unsigned char ipmi_version_minor;
static const char msgdata[IPMI_MAX_ADDR_SIZE];   /* ?? */
static struct ipmi_addr address = {
	IPMI_SYSTEM_INTERFACE_ADDR_TYPE,
	IPMI_BMC_CHANNEL,
	NULL
};	/* send address */
static struct ipmi_msg message;	/* send message */
static long msgid;		/* message ID */
static int interfaces;
static int (*rcv_callback)(struct i2c_client *client,unsigned int cmd, void *arg);

/* Used in bmc_init/cleanup */
/*
static int __initdata bmc_initialized;
*/

void bmc_inc_use(struct i2c_adapter *adapter)
{
#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif
}

void bmc_dec_use(struct i2c_adapter *adapter)
{
#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
}

/* We can't do a thing... */
static u32 bmc_func(struct i2c_adapter *adapter)
{
	return 0;
}

/*-----------------------------------*/

void bmc_i2c_send_message(int id, struct ipmi_msg * msg)
{
	ipmi_request(i2c_bmc_user, &address, id, msg, 0);
}

void bmcclient_i2c_send_message(struct i2c_client *client, int id, struct ipmi_msg * msg)
{
	id = (id & 0xffff) | 0x10000;
	rcv_callback = client->driver->command;
	bmc_i2c_send_message(id, msg);
}


static void ipmi_i2c_msg_handler(struct ipmi_recv_msg *msg,
				  void            *handler_data)
{
	int rcvid = msg->msgid & 0xffff;
	int client = (msg->msgid >> 16) & 0xf;

	if (msg->msg.data[0] != 0) {
		printk(KERN_ERR "IPMI BMC response: Error 0x%x on cmd 0x%x\n",
		       msg->msg.data[0],
		       msg->msg.cmd);
	} else {
/*
		printk(KERN_INFO "IPMI BMC response: No error on cmd 0x%x\n",
		       msg->msg.cmd);
*/
	}       


	if(client == 1 && rcv_callback != NULL)
		(*rcv_callback)(NULL, client, msg);
	else
		ipmi_free_recv_msg(msg);
}

static struct ipmi_user_hndl ipmi_hndlrs =
{
	.ipmi_recv_hndl           = ipmi_i2c_msg_handler,
};

/* callback for each BMC found */
static void ipmi_register_bmc(int ipmi_intf)
{
	unsigned long flags;
	int           rv = -EBUSY;

	if(interfaces > 0) {	/* 1 max for now */
		printk(KERN_INFO
		       "i2c-bmc.o: Additional IPMI interface %d not supported\n",
		       ipmi_intf);
		return;
	}

	rv = ipmi_create_user(ipmi_intf, &ipmi_hndlrs, NULL, &i2c_bmc_user);
	if (rv < 0) {
		printk(KERN_ERR "i2c-bmc.o: Unable to register with ipmi\n");
		return;
	}

	ipmi_get_version(i2c_bmc_user,
			 &ipmi_version_major,
			 &ipmi_version_minor);

	if ((rv = i2c_add_adapter(&bmc_adapter))) {
		printk(KERN_ERR "i2c-bmc.o: Adapter registration failed, "
		       "module i2c-bmc.o is not inserted\n.");
		return;
	}

	printk(KERN_INFO
	       "i2c-bmc.o: Registered IPMI interface %d with version %d.%d\n",
	       ipmi_intf, ipmi_version_major, ipmi_version_minor);
	interfaces++;
}


static void ipmi_new_smi(int if_num)
{
	ipmi_register_bmc(if_num);
}

static void ipmi_smi_gone(int if_num)
{
	/* TBD */
}

static struct ipmi_smi_watcher smi_watcher =
{
	.new_smi  = ipmi_new_smi,
	.smi_gone = ipmi_smi_gone
};

int __init i2c_bmc_init(void)
{
	int rv;

	printk(KERN_INFO "i2c-bmc.o version %s (%s)\n", LM_VERSION, LM_DATE);

	rv = ipmi_smi_watcher_register(&smi_watcher);
	if (rv) {
		printk(KERN_WARNING
		       "ipmi_watchdog: can't register smi watcher\n");
		return rv;
	}

	printk(KERN_INFO "i2c-bmc.o: BMC access for i2c modules initialized.\n");
	return 0;
}

int __init bmc_cleanup(void)
{
	ipmi_smi_watcher_unregister(&smi_watcher);
	if (interfaces >= 1) {
		i2c_del_adapter(&bmc_adapter);
		ipmi_destroy_user(i2c_bmc_user);
		interfaces--;
	}
	return 0;
}

#ifdef MODULE
EXPORT_SYMBOL(bmcclient_i2c_send_message);

MODULE_AUTHOR("M. D. Studebaker <mdsxyz123@yahoo.com>");
MODULE_DESCRIPTION("BMC bus access through i2c");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif

int init_module(void)
{
	return i2c_bmc_init();
}

int cleanup_module(void)
{
	return bmc_cleanup();
}

#endif				/* MODULE */
