/*
    i2c-ipmb.c - Part of lm_sensors, Linux kernel modules for hardware
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
    THIS DOESN'T WORK YET - DON'T BOTHER TRYING IT.	
    This implements an i2c adapter for the BMC IPMB.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/ipmi.h>
#include "version.h"


static u32 i2c_ipmb_func(struct i2c_adapter *adapter);
int ipmb_access(struct i2c_adapter *adap,struct i2c_msg msgs[], 
	                   int num);


/* I2C Data */
static struct i2c_algorithm i2c_ipmb_algorithm = {
	.name = "IPMB algorithm",
	.id = I2C_ALGO_IPMB,
	.master_xfer = ipmb_access,
	.functionality = i2c_ipmb_func,
};

#define MAX_IPMB_ADAPTERS 8
static struct i2c_adapter i2c_ipmb_adapter[MAX_IPMB_ADAPTERS];

/* IPMI Data */
#define IPMI_IPMB_CHANNEL	0
static ipmi_user_t i2c_ipmb_user;
static unsigned char ipmi_version_major;
static unsigned char ipmi_version_minor;
static const char msgdata[IPMI_MAX_ADDR_SIZE];   /* ?? */
static struct ipmi_system_interface_addr address = {
	IPMI_SYSTEM_INTERFACE_ADDR_TYPE,
	IPMI_BMC_CHANNEL,
	0
};	/* send address */
static struct ipmi_ipmb_addr ipmb_address = {
	IPMI_IPMB_ADDR_TYPE,
	IPMI_IPMB_CHANNEL,
	0,
	0
};	/* send address */
static long msgid;		/* message ID */
static int interfaces;		/* number of BMC's found */
static struct ipmi_msg tx_message;	/* send message */
static unsigned char tx_msg_data[IPMI_MAX_MSG_LENGTH + 50];
static unsigned char rx_msg_data[IPMI_MAX_MSG_LENGTH + 50]; /* sloppy */

/* IPMI Message defs */
/* Network Function Codes */
#define IPMI_NETFN_APP		0x06
/* Commands */
#define IPMI_ENABLE_CHANNEL	0x32
#define IPMI_SEND_MSG		0x34
#define IPMI_GET_CHANNEL_INFO	0x42	/* unfortunately, IPMI 1.5 only */
#define IPMI_MASTER_WR		0x52


/************** Message Sending **************/

static void ipmb_i2c_send_message(struct ipmi_addr *address,
                                  int id, struct ipmi_msg * msg)
{
	int err;

	if((err = ipmi_request(i2c_ipmb_user, address, id, msg, 0)))
		printk(KERN_INFO "i2c-ipmb.o: ipmi_request error %d\n",
			err);
}

/* not used */
static void ipmb_i2c_bmc_send_message(int id, struct ipmi_msg * msg)
{
	address.channel = IPMI_BMC_CHANNEL;
	ipmb_i2c_send_message((struct ipmi_addr *) &address, id, msg);
}

/* this is for sending commands like master w/r */
static void ipmb_i2c_ipmb_send_message(int id, struct ipmi_msg * msg)
{
/*
	address.channel = IPMI_IPMB_CHANNEL;
*/
	address.channel = IPMI_BMC_CHANNEL;
	ipmb_i2c_send_message((struct ipmi_addr *) &address, id, msg);
}

#if 0
/* not used */
/* this is for an smi message, not for things like master w/r */
static void ipmb_i2c_ipmb_send_smi(int addr, int id, struct ipmi_msg * msg)
{
	ipmb_address.slave_addr = addr;
	ipmb_address.lun = 0;  /* pass in */
	ipmb_i2c_send_message((struct ipmi_addr *) &ipmb_address, id, msg);
}

/* not used */
/* Compose and send a "Enable Channel Receive" message */
static void ipmb_enable_channel_rcv(int channel, int code)
{
	tx_message.netfn = IPMI_NETFN_APP;
	tx_message.cmd = IPMI_ENABLE_CHANNEL;	
	tx_message.data_len = 2;
	tx_message.data = tx_msg_data;
	tx_msg_data[0] = channel;
	tx_msg_data[1] = code;
	ipmb_i2c_bmc_send_message(msgid++, &tx_message);
}

/* not used */
/* Compose and send a "Get Channel Info" message (1.5 only) */
static void ipmb_get_channel_info(int channel)
{
	tx_message.netfn = IPMI_NETFN_APP;
	tx_message.cmd = IPMI_GET_CHANNEL_INFO;	
	tx_message.data_len = 1;
	tx_message.data = tx_msg_data;
	tx_msg_data[0] = channel;
	ipmb_i2c_bmc_send_message(msgid++, &tx_message);
}
#endif /* 0 */

/* Compose and send a "Master W/R" message */
static void ipmb_master_wr(int bus, u8 addr, u8 rdcount,
                           u8 wrcount, u8 *wrdata)
{
	printk(KERN_INFO "i2c-ipmb.o: trying bus %d ...\n", bus);
	tx_message.netfn = IPMI_NETFN_APP;
	tx_message.cmd = IPMI_MASTER_WR;	
	tx_message.data_len = 3 + wrcount;
	tx_msg_data[0] = bus & 0x0f;
	tx_msg_data[1] = addr << 1;
	tx_msg_data[2] = rdcount;
	if(wrcount > 0)
		memcpy(tx_msg_data + 3, wrdata, wrcount);
	tx_message.data = tx_msg_data;
	ipmb_i2c_ipmb_send_message(msgid++, &tx_message);
}

int xchan;
int xbus;
/* look for channels. IPMI 1.5 defines multiple channels and an
   easy way to get the information.
   We don't bother using SDR type 14 for IPMI 1.0; that isn't
   always present anyway. We could try "enable message channel receive"
   with a channel state = 2 (query) but 1.0 is likely to have only
   IPMB busses anyway. Therefore, we assume there is only
   the IPMB at channel 0.
*/
static void ipmb_get_all_channel_info(void)

{
#if 0
	if(ipmi_version_major > 1 ||
	   (ipmi_version_major == 1 && ipmi_version_minor >= 5))
		ipmb_get_channel_info(0);
#endif
/*
	else
		scan SDR's for type 14
	else {
		for all xchan 0-15
		ipmb_enable_channel_rcv(xchan, 2);
*/
	xbus = 0;
		ipmb_master_wr(xbus, 0x2d, 1, 1, "\0");
/*
*/
/*
	else
		assume IPMB at channel 0 only
*/
}

/************** Message Receiving **************/

/* not used */
static void ipmb_rcv_channel_info(struct ipmi_msg *msg)
{
	u8 channel, type, protocol;

	channel = msg->data[1] & 0x0f;
	type = msg->data[2] & 0x7f;
	protocol = msg->data[3] & 0x1f;
	printk(KERN_INFO "i2c-ipmb.o: Channel %d; type 0x%x; protocol 0x%x\n",
	                  channel, type, protocol);
}
/*
	return i2c_add_adapter(&i2c_ipmb_adapter);
	if (error) {
		printk(KERN_ERR "i2c-ipmb.o: Adapter registration failed, "
		       "module i2c-ipmb.o is not inserted\n.");
		return;
	}
*/
/*
	if(channel < 7)
		ipmb_get_channel_info(channel + 1);
*/
static void ipmb_rcv_master_resp(struct ipmi_msg *msg)
{
	if(++xbus > 0x0f)
		return;
	ipmb_master_wr(xbus, 0x2d, 1, 1, "\0");
}

#if 0
/* not used */
static void ipmb_rcv_channel_enable(struct ipmi_msg *msg)
{
	int state;

	state = msg->data[2] & 1;

	printk(KERN_INFO "i2c-ipmb.o: Channel %d; state %d\n",
	                  xchan, state);
		if(++xchan > 0x0f)
			return;
	ipmb_enable_channel_rcv(xchan, 2);
}
#endif /* 0 */

static void ipmb_i2c_msg_handler(struct ipmi_recv_msg *msg,
				  void            *handler_data)
{
	int rcvid = msg->msgid & 0xffffff;
	int client = (msg->msgid >> 24) & 0xf;

	if (msg->msg.data[0] != 0)
		printk(KERN_WARNING "i2c-ipmb.o: Error 0x%x on cmd 0x%x/0x%x\n",
		       msg->msg.data[0], msg->msg.netfn & 0xfe, msg->msg.cmd);
/*
	else
*/
		ipmb_rcv_master_resp(&(msg->msg));





	ipmi_free_recv_msg(msg);
}

static struct ipmi_user_hndl ipmb_hndlrs =
{
	.ipmi_recv_hndl           = ipmb_i2c_msg_handler,
};

/*************** I2C funtions *******************/

/* Return -1 on error. */
int ipmb_access(struct i2c_adapter *adap,struct i2c_msg msgs[], 
	                   int num)
{






}

static u32 i2c_ipmb_func(struct i2c_adapter *adapter)
{
	return 0; /* fixme */
}

/**************** Initialization ****************/

/* callback for each BMC found */
static void ipmb_register(int ipmi_intf)
{
	unsigned long flags;
	int rv;

	if(interfaces > 0) {	/* 1 max for now */
		printk(KERN_INFO
		       "i2c-ipmb.o: Additional IPMI interface %d not supported\n",
		       ipmi_intf);
		return;
	}

	rv = ipmi_create_user(ipmi_intf, &ipmb_hndlrs, NULL, &i2c_ipmb_user);
	if (rv < 0) {
		printk(KERN_ERR "i2c-ipmb.o: Unable to register with ipmi\n");
		return;
	}

	ipmi_get_version(i2c_ipmb_user, &ipmi_version_major,
	                 &ipmi_version_minor);
	printk(KERN_INFO
	       "i2c-ipmb.o: Registered IPMI interface %d with version %d.%d\n",
	       ipmi_intf, ipmi_version_major, ipmi_version_minor);
	interfaces++;

	ipmb_get_all_channel_info();
}

static void ipmb_new_smi(int if_num)
{
	ipmb_register(if_num);
}

static void ipmb_smi_gone(int if_num)
{
	if (interfaces >= 1) {
/*
		i2c_del_adapter(&i2c_ipmb_adapter);
*/
		ipmi_destroy_user(i2c_ipmb_user);
		interfaces--;
	}
}

static struct ipmi_smi_watcher smi_watcher =
{
	.new_smi  = ipmb_new_smi,
	.smi_gone = ipmb_smi_gone
};

static int __init i2c_ipmb_init(void)
{
	int rv;

	printk(KERN_INFO "i2c-ipmb.o version %s (%s)\n", LM_VERSION, LM_DATE);

	rv = ipmi_smi_watcher_register(&smi_watcher);
	if (rv) {
		printk(KERN_WARNING
		       "ipmi_watchdog: can't register smi watcher\n");
		return rv;
	}

	printk(KERN_INFO "i2c-ipmb.o: BMC access for i2c modules initialized.\n");
	return 0;
}


static void __exit i2c_ipmi_exit(void)
{
	ipmi_smi_watcher_unregister(&smi_watcher);
	ipmb_smi_gone(0);
}

MODULE_AUTHOR("M. D. Studebaker <mdsxyz123@yahoo.com>");
MODULE_DESCRIPTION("IPMB-BMC access through i2c");
MODULE_LICENSE("GPL");

module_init(i2c_ipmb_init);
module_exit(i2c_ipmi_exit);
