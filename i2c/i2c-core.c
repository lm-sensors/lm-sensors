/* ------------------------------------------------------------------------- */
/* i2c.c - a device driver for the iic-bus interface			     */
/* ------------------------------------------------------------------------- */
/*   Copyright (C) 1995-98 Simon G. Vogl

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
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.		     */
/* ------------------------------------------------------------------------- */
#define RCSID "$Id: i2c-core.c,v 1.1 1998/11/02 20:29:27 frodo Exp $"
/* ------------------------------------------------------------------------- */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/malloc.h>

#include "i2c.h"
 
/* ----- global defines ---------------------------------------------------- */

/* exclusive access to the bus */
/*#define SPINLOCK*/
#ifdef SPINLOCK
#define I2C_LOCK(adap) spin_lock_irqsave(&adap->lock,adap->lockflags)
#define I2C_UNLOCK(adap) spin_unlock_irqrestore(&adap->lock,adap->lockflags)
#else
#define I2C_LOCK(adap) down(&adap->lock)
#define I2C_UNLOCK(adap) up(&adap->lock) 
#endif

#define DEB(x) if (i2c_debug>=1) x;
#define DEB2(x) if (i2c_debug>=2) x;

/* ----- global variables -------------------------------------------------- */

/**** algorithm list */
struct i2c_algorithm *algorithms[I2C_ALGO_MAX];
int algo_count;

/**** adapter list */
struct i2c_adapter *adapters[I2C_ADAP_MAX];
int adap_count;

/**** drivers list */
struct i2c_driver *drivers[I2C_DRIVER_MAX];
int driver_count;

/**** debug level */
int i2c_debug=1;

/* ---------------------------------------------------    
 * registering functions 
 * --------------------------------------------------- 
 */

/* -----
 * Algorithms - used to access groups of similar hw adapters or
 * specific interfaces like the PCF8584 et al.
 */
int i2c_add_algorithm(struct i2c_algorithm *algo)
{
	int i;

	for (i = 0; i < I2C_ALGO_MAX; i++)
		if (NULL == algorithms[i])
			break;
	if (I2C_ALGO_MAX == i) {
		printk(KERN_WARNING 
		       " i2c: register_algorithm(%s) - enlarge I2C_ALGO_MAX.\n",
			algo->name);
		return -ENOMEM;
	}

	algorithms[i] = algo;
	algo_count++;

	DEB(printk("i2c: algorithm %s registered.\n",algo->name));
	return 0;	
}


int i2c_del_algorithm(struct i2c_algorithm *algo)
{
	int i;

	for (i = 0; i < I2C_ALGO_MAX; i++)
		if (algo == algorithms[i])
			break;
	if (I2C_ALGO_MAX == i) {
		printk(KERN_WARNING 
		       " i2c: unregister_algorithm: [%s] not found.\n",
			algo->name);
		return -ENODEV;
	}
	algorithms[i] = NULL;
	algo_count--;

	DEB(printk("i2c: algorithm unregistered: %s\n",algo->name));
	return 0;    
}


/* -----
 * i2c_add_adapter is called from within the algorithm layer,
 * when a new hw adapter registers. A new device is register to be
 * available for clients.
 */
int i2c_add_adapter(struct i2c_adapter *adap)
{
	int i;

	for (i = 0; i < I2C_ADAP_MAX; i++)
		if (NULL == adapters[i])
			break;
	if (I2C_ADAP_MAX == i) {
		printk(KERN_WARNING 
		       " i2c: register_adapter(%s) - enlarge I2C_ADAP_MAX.\n",
			adap->name);
		return -ENOMEM;
	}


	adapters[i] = adap;
	adap_count++;

	/* init data types */
#ifdef SPINLOCK
	adap->lock = (spinlock_t)SPIN_LOCK_UNLOCKED;
#else
	adap->lock = MUTEX;
#endif

	/* inform drivers of new adapters */
	for (i=0;i<I2C_DRIVER_MAX;i++)
		if (drivers[i]!=NULL && drivers[i]->flags&DF_NOTIFY)
			drivers[i]->attach_adapter(adap);

	DEB(printk("i2c: adapter %s registered.\n",adap->name));
	return 0;	
}

int i2c_del_adapter(struct i2c_adapter *adap)
{
	int i,j;

	for (i = 0; i < I2C_ADAP_MAX; i++)
		if (adap == adapters[i])
			break;
	if (I2C_ADAP_MAX == i) {
		printk(KERN_WARNING 
		       " i2c: unregister_adapter adap [%s] not found.\n",
			adap->name);
		return -ENODEV;
	}

	/* detach any active clients */
	for (j=0;j<I2C_CLIENT_MAX;j++) {
		struct i2c_client *client = adap->clients[j];
		if ( (client!=NULL) 
		     /* && (client->driver->flags & DF_NOTIFY) */ )
			/* detaching devices is unconditional of the set notify
			 * flag, as _all_ clients that reside on the adapter
			 * must be deleted, as this would cause invalid states.
			 */
			i2c_detach_client(client);
	}
	/* all done, now unregister */
	adapters[i] = NULL;
	adap_count--;

	DEB(printk("i2c: adapter unregistered: %s\n",adap->name));
	return 0;    
}

/* -----
 * What follows is the "upwards" interface: commands for talking to clients,
 * which implement the functions to access the physical information of the
 * chips.
 */

int i2c_add_driver(struct i2c_driver *driver)
{
	int i;

	for (i = 0; i < I2C_DRIVER_MAX; i++)
		if (NULL == drivers[i])
			break;
	if (I2C_DRIVER_MAX == i) {
		printk(KERN_WARNING 
		       " i2c: register_driver(%s) - enlarge I2C_DRIVER_MAX.\n",
			driver->name);
		return -ENOMEM;
	}

	drivers[i] = driver;
	driver_count++;

	DEB(printk("i2c: driver %s registered.\n",driver->name));

	/* now look for instances of driver on our adapters
	 */
	if ( driver->flags&DF_NOTIFY )
	for (i=0;i<I2C_ADAP_MAX;i++)
		if (adapters[i]!=NULL)
			driver->attach_adapter(adapters[i]);

	return 0;
}

int i2c_del_driver(struct i2c_driver *driver)
{
	int i,j,k;

	for (i = 0; i < I2C_DRIVER_MAX; i++)
		if (driver == drivers[i])
			break;
	if (I2C_DRIVER_MAX == i) {
		printk(KERN_WARNING " i2c: unregister_driver: [%s] not found\n",
			driver->name);
		return -ENODEV;
	}
	/* Have a look at each adapter, if clients of this driver are still
	 * attached. If so, detach them to be able to kill the driver afterwards.
	 */
	DEB2(printk("i2c: unregister_driver - looking for clients.\n"));
	/* removing clients does not depend on the notify flag, else 
	 * invalid operation might (will!) result, when using stale client
	 * pointers.
	 */
	for (k=0;k<I2C_ADAP_MAX;k++) {
		struct i2c_adapter *adap = adapters[k];
		if (adap == NULL) /* skip empty entries. */
			continue;
		DEB2(printk("i2c: examining adapter %s:\n",adap->name));
		for (j=0;j<I2C_CLIENT_MAX;j++) { 
			struct i2c_client *client = adap->clients[j];
			if (client != NULL && client->driver == driver) {
				DEB2(printk("i2c:   detaching client %s:\n",
					client->name));
				/*i2c_detach_client(client);*/
				driver->detach_client(client);
			}
		}
	}
	drivers[i] = NULL;
	driver_count--;

	DEB(printk("i2c: driver unregistered: %s\n",driver->name));
	return 0;
}


int i2c_attach_client(struct i2c_client *client)
{
	struct i2c_adapter *adapter = client->adapter;
	struct i2c_algorithm *algo  = adapter->algo;
	int i;

	for (i = 0; i < I2C_CLIENT_MAX; i++)
		if (NULL == adapter->clients[i])
			break;
	if (I2C_CLIENT_MAX == i) {
		printk(KERN_WARNING 
		       " i2c: attach_client(%s) - enlarge I2C_CLIENT_MAX.\n",
			client->name);
		return -ENOMEM;
	}

	adapter->clients[i] = client;
	adapter->client_count++;
	if (algo->client_register != NULL) 
		algo->client_register(client);
	DEB(printk("i2c: client [%s] registered to adapter [%s](pos. %d).\n",
		client->name, adapter->name,i));
	return 0;
}


int i2c_detach_client(struct i2c_client *client)
{
	struct i2c_adapter *adapter = client->adapter;
	struct i2c_algorithm *algo  = adapter->algo;
	int i;

	for (i = 0; i < I2C_CLIENT_MAX; i++)
		if (client == adapter->clients[i])
			break;
	if (I2C_CLIENT_MAX == i) {
		printk(KERN_WARNING " i2c: unregister_client [%s] not found\n",
			client->name);
		return -ENODEV;
	}

	if (algo->client_unregister != NULL) 
		algo->client_unregister(client);
	/*	client->driver->detach_client(client);*/
	adapter->clients[i] = NULL;
	adapter->client_count--;
	DEB(printk("i2c: client [%s] unregistered.\n",client->name));
	return 0;    
}

int i2c_init(void)
{
	/* clear algorithms */
	memset(algorithms,0,sizeof(algorithms));
	memset(adapters,0,sizeof(adapters));
	memset(drivers,0,sizeof(drivers));
	algo_count=0;
	adap_count=0;
	driver_count=0;
	
	printk("i2c module initialized.\n");
	return 0;
}


/* ----------------------------------------------------
 * the functional interface to the i2c busses.
 * ----------------------------------------------------
 */

int i2c_transfer(struct i2c_adapter * adap, struct i2c_msg msgs[],int num)
{
	int ret;

	DEB(printk("master_xfer: %s with %d msgs.\n",adap->name,num));

	I2C_LOCK(adap);
	ret = adap->algo->master_xfer(adap,msgs,num);
	I2C_UNLOCK(adap);

	return ret;
}

int i2c_master_send(struct i2c_client *client,const char *buf ,int count)
{
	int ret;
	struct i2c_adapter *adap=client->adapter;
	struct i2c_msg msg;
	msg.addr   = client->addr;
	msg.flags = client->flags & ( I2C_M_TEN|I2C_M_TENMASK );
	msg.len = count;
	(const char *)msg.buf = buf;

	DEB(printk("master_send: writing %d bytes on %s.\n",
		count,client->adapter->name));

	I2C_LOCK(adap);
	ret = adap->algo->master_xfer(adap,&msg,1);
	I2C_UNLOCK(adap);

	/* if everything went ok (i.e. 1 msg transmitted), return #bytes
	 * transmitted, else error code.
	 */
	return (ret == 1 )? count : ret;
}

int i2c_master_recv(struct i2c_client *client, char *buf ,int count)
{
	struct i2c_adapter *adap=client->adapter;
	struct i2c_msg msg;
	int ret;
	msg.addr   = client->addr;
	msg.flags = client->flags & ( I2C_M_TEN|I2C_M_TENMASK );
	msg.flags |= I2C_M_RD;
	msg.len = count;
	msg.buf = buf;

	DEB(printk("master_recv: reading %d bytes on %s.\n",
		count,client->adapter->name));

	I2C_LOCK(adap);
	ret = adap->algo->master_xfer(adap,&msg,1);
	I2C_UNLOCK(adap);

	/* if everything went ok (i.e. 1 msg transmitted), return #bytes
	 * transmitted, else error code.
	 */
	return (ret == 1 )? count : ret;
}


int i2c_control(struct i2c_client *client,
	unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct i2c_adapter *adap = client->adapter;

	DEB2(printk("i2c ioctl, cmd: 0x%x, arg: %#lx\n", cmd, arg));
	switch ( cmd ) {
		case I2C_RETRIES:
			adap->retries = arg;
			break;
		case I2C_TIMEOUT:
			adap->timeout = arg;
			break;
		default:
			if (adap->algo->algo_control!=NULL)
				ret = adap->algo->algo_control(adap,cmd,arg);
	}
	return ret;
}

int i2c_probe(struct i2c_client *client, int low_addr, int hi_addr)
{
        int i;
	struct i2c_msg msg;
	struct i2c_msg *pmsg = &msg;
	msg.flags=client->flags & (I2C_M_TENMASK | I2C_M_TEN );
	msg.buf = NULL;
	msg.len = 0;
        I2C_LOCK(client->adapter);
        for (i = low_addr; i <= hi_addr; i++) {
                client->addr=i;
		/* TODO: implement a control statement in the algo layer 
		 * that does address lookup only.
		 */
                if (1 == client->adapter->
		    algo->master_xfer(client->adapter,pmsg,1))
                        break;
        }
        I2C_UNLOCK(client->adapter);
        return (i <= hi_addr) ? i : -1;
}

#ifdef MODULE
MODULE_AUTHOR("Simon G. Vogl <simon@tk.uni-linz.ac.at>");
MODULE_DESCRIPTION("I2C-Bus main module");
MODULE_PARM(i2c_debug, "i");
MODULE_PARM_DESC(i2c_debug,"debug level");

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,1,0))
EXPORT_SYMBOL(i2c_add_algorithm);
EXPORT_SYMBOL(i2c_del_algorithm);
EXPORT_SYMBOL(i2c_add_adapter);
EXPORT_SYMBOL(i2c_del_adapter);
EXPORT_SYMBOL(i2c_add_driver);
EXPORT_SYMBOL(i2c_del_driver);
EXPORT_SYMBOL(i2c_attach_client);
EXPORT_SYMBOL(i2c_detach_client);

EXPORT_SYMBOL(i2c_master_send);
EXPORT_SYMBOL(i2c_master_recv);
EXPORT_SYMBOL(i2c_control);
EXPORT_SYMBOL(i2c_transfer);
#endif

int init_module(void) 
{
	return i2c_init();
}

void cleanup_module(void) 
{
}
#endif
