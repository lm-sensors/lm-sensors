/*
    i2c-ipmi.c - Part of lm_sensors, Linux kernel modules for hardware
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
    This implements a "dummy" i2c adapter for clients to access the
    BMC via IPMI messages. Supports only one BMC and one client for now!
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/ipmi.h>
#include "version.h"


static u32 i2c_ipmi_func(struct i2c_adapter *adapter);
static int bmcclient_i2c_send_message(struct i2c_adapter *, char *, int);

static void i2c_ipmi_inc_use(struct i2c_adapter *adapter)
{
#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif
}

static void i2c_ipmi_dec_use(struct i2c_adapter *adapter)
{
#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
}

/* I2C Data */
static struct i2c_algorithm i2c_ipmi_algorithm = {
	.name = "IPMI algorithm",
	.id = I2C_ALGO_IPMI,
	.slave_send = &bmcclient_i2c_send_message,
	.functionality = &i2c_ipmi_func,
};

static struct i2c_adapter i2c_ipmi_adapter = {
	.name		= "IPMI adapter",
	.id		= I2C_ALGO_IPMI | I2C_HW_IPMI,
	.algo		= &i2c_ipmi_algorithm,
	.inc_use	= &i2c_ipmi_inc_use,
	.dec_use	= &i2c_ipmi_dec_use,
};

/* IPMI Data */
static ipmi_user_t i2c_ipmi_user;
static unsigned char ipmi_version_major;
static unsigned char ipmi_version_minor;
static const char msgdata[IPMI_MAX_ADDR_SIZE];   /* ?? */
static struct ipmi_addr address = {
	IPMI_SYSTEM_INTERFACE_ADDR_TYPE,
	IPMI_BMC_CHANNEL,
	{0}
};	/* send address */
static int interfaces;		/* number of BMC's found */


/* Dummy adapter... */
static u32 i2c_ipmi_func(struct i2c_adapter *adapter)
{
	return 0;
}

/************** Message Sending **************/

static int find_client(struct i2c_client * client)
{
	int i;

	for (i = 0; i < I2C_CLIENT_MAX; i++)
		if (client == i2c_ipmi_adapter.clients[i])
			return i;
	return -1;
}

static void ipmi_i2c_send_message(int id, struct ipmi_msg * msg)
{
	ipmi_request(i2c_ipmi_user, &address, (long) id, msg, 0);
}

/* This is the message send function exported to the client
   via the i2c_adapter struct.
   We use the existing (but unused) slave_send function pointer.
   Hence the ugly casts. */
static int bmcclient_i2c_send_message(struct i2c_adapter *clnt,
                                      char * mesg, int id)
{
	struct ipmi_msg *msg = (struct ipmi_msg *) mesg;
	struct i2c_client *client = (struct i2c_client *) clnt;
        int clientid;
	
#ifdef DEBUG
	if(msg->data == NULL)
		printk(KERN_INFO "i2c-ipmi.o: Send 0x%x\n", msg->cmd);
	else
		printk(KERN_INFO "i2c-ipmi.o: Send 0x%x 0x%x 0x%x\n", msg->cmd, msg->data[0], msg->data[1]);
#endif
	/* save the client number in the upper 8 bits of the message id */
	if((clientid = find_client(client)) < 0) {
		printk(KERN_WARNING "i2c-ipmi.o: Request from unknown client\n");
		return -1;      
	}

	id = (id & 0xffffff) | (clientid << 24);
	ipmi_i2c_send_message(id, msg);
	return 0;
}

/************** Message Receiving **************/

static void ipmi_i2c_msg_handler(struct ipmi_recv_msg *msg,
				  void            *handler_data)
{
	int rcvid = msg->msgid & 0xffffff;
	int clientid = (msg->msgid >> 24) & 0xff;

#ifdef DEBUG
	if (msg->msg.data[0] != 0)
		printk(KERN_WARNING "i2c-ipmi.o: Error 0x%x on cmd 0x%x/0x%x\n",
		       msg->msg.data[0], msg->msg.netfn, msg->msg.cmd);
#endif
	/* Protect ourselves here; verify the client and its callback
	   since the client may have gone away since
	   the message was sent! */
	if(clientid < I2C_CLIENT_MAX &&
	   i2c_ipmi_adapter.clients[clientid] != NULL &&
	   i2c_ipmi_adapter.clients[clientid]->driver->command != NULL)
	   	(* i2c_ipmi_adapter.clients[clientid]->driver->command)
		     (i2c_ipmi_adapter.clients[clientid], rcvid, msg);
	else {
		printk(KERN_WARNING "i2c-ipmi.o: Response for unknown client\n");
		ipmi_free_recv_msg(msg);
	}
}

static struct ipmi_user_hndl ipmi_hndlrs =
{
	.ipmi_recv_hndl           = ipmi_i2c_msg_handler,
};

/**************** Initialization ****************/

/* callback for each BMC found */
static void ipmi_register_bmc(int ipmi_intf)
{
	int error;

	if(interfaces > 0) {	/* 1 max for now */
		printk(KERN_INFO
		       "i2c-ipmi.o: Additional IPMI interface %d not supported\n",
		       ipmi_intf);
		return;
	}

	error = ipmi_create_user(ipmi_intf, &ipmi_hndlrs, NULL, &i2c_ipmi_user);
	if (error < 0) {
		printk(KERN_ERR "i2c-ipmi.o: Unable to register with ipmi\n");
		return;
	}

	error = i2c_add_adapter(&i2c_ipmi_adapter);
	if (error) {
		printk(KERN_ERR "i2c-ipmi.o: Adapter registration failed, "
		       "module i2c-ipmi.o is not inserted\n.");
		return;
	}

	ipmi_get_version(i2c_ipmi_user, &ipmi_version_major,
	                 &ipmi_version_minor);
	printk(KERN_INFO
	       "i2c-ipmi.o: Registered IPMI interface %d with version %d.%d\n",
	       ipmi_intf, ipmi_version_major, ipmi_version_minor);
	interfaces++;
}

static void ipmi_new_smi(int if_num)
{
	ipmi_register_bmc(if_num);
}

static void ipmi_smi_gone(int if_num)
{
	if (interfaces >= 1) {
		i2c_del_adapter(&i2c_ipmi_adapter);
		ipmi_destroy_user(i2c_ipmi_user);
		interfaces--;
	}
}

static struct ipmi_smi_watcher smi_watcher =
{
	.new_smi  = ipmi_new_smi,
	.smi_gone = ipmi_smi_gone
};

static int __init i2c_ipmi_init(void)
{
	int rv;

	printk(KERN_INFO "i2c-ipmi.o version %s (%s)\n", LM_VERSION, LM_DATE);

	rv = ipmi_smi_watcher_register(&smi_watcher);
	if (rv) {
		printk(KERN_WARNING
		       "ipmi_watchdog: can't register smi watcher\n");
		return rv;
	}

	printk(KERN_INFO "i2c-ipmi.o: BMC access for i2c modules initialized.\n");
	return 0;
}


static void __exit i2c_ipmi_exit(void)
{
	ipmi_smi_watcher_unregister(&smi_watcher);
	ipmi_smi_gone(0);
}

MODULE_AUTHOR("M. D. Studebaker <mdsxyz123@yahoo.com>");
MODULE_DESCRIPTION("IPMI-BMC access through i2c");
MODULE_LICENSE("GPL");

module_init(i2c_ipmi_init);
module_exit(i2c_ipmi_exit);
