/* ------------------------------------------------------------------------- */
/* adap-bit.c i2c driver algorithms for bit-shift adapters		     */
/* ------------------------------------------------------------------------- */
/*   Copyright (C) 1995-97 Simon G. Vogl

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
static char alg_rcsid[] = "$Id: algo-pcf.c,v 1.1 1998/09/05 18:20:09 i2c Exp $";

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/malloc.h>
#include <linux/version.h>


#if LINUX_VERSION_CODE >= 0x020100

#  include <asm/uaccess.h>
#else
#  include <asm/segment.h>
#endif


#include <linux/ioport.h>
#include <linux/errno.h>
#include <linux/sched.h>

#include "i2c.h"
#include "algo-pcf.h"

/* ----- global defines ---------------------------------------------------- */
#define DEB(x)		/* should be reasonable open, close &c. 	*/
#define DEB2(x) 	/* low level debugging - very slow 		*/
#define DEBE(x)		/* error messages 				*/
#define DEBI(x) 	/* ioctl and its arguments 			*/
#define DEBACK(x) x 	/* ack failed message				*/
#define DEBSTAT(x) 	/* print several statistical values		*/

#define DEBPROTO(x) 	/* debug the protocol by showing transferred bytes*/


/* ----- global variables ---------------------------------------------	*/

/* module parameters:
 */
static int test=0;	/* see if the line-setting functions work	*/
static int scan=0;	/* have a look at what's hanging 'round		*/

/*
 *  This array contains the hw-specific functions for
 *  each port (hardware) type.
 */
static struct pcf_adapter *pcf_adaps[PCF_ADAP_MAX];
static int adap_count;
static struct i2c_adapter *i2c_adaps[PCF_ADAP_MAX];

/* --- setting states on the bus with the right timing: ---------------	*/

/*
 * Sanity check for the adapter hardware - check the reaction of
 * the bus lines only if it seems to be idle.
 */
static int test_bus(struct pcf_adapter *adap) 
{
}

/* ----- Utility functions
 */


/* send a message to a client.
 */
static int pcf_send(struct i2c_client *client,const char *buf, int count)
{
	struct i2c_adapter *adapter=client->adapter;
	struct pcf_adapter *adap=(struct pcf_adapter*)adapter->data;
	int ret,i;

	DEB2(printk(" i2c_write: wrote %d bytes.\n",wrcount));
	return wrcount;
}


static int pcf_recv(struct i2c_client *client,char *buf,int count)
{
	struct i2c_adapter *adapter = client->adapter;
	struct pcf_adapter *adap = (struct pcf_adapter*)adapter->data;
	unsigned int flags = client->flags;
	char addr;
	int ret=0,i,rdcount; 

	DEB(printk("i2c(bit): i2c_read: %d byte(s) read.\n", rdcount ));
	return rdcount; 
}


/* 
 * alpha version of combined transmit
 */
static int pcf_comb(struct i2c_client *client, char *readbuf,const char *writebuf, 
	int nread, int nwrite, int dir)
{
  /*	struct i2c_adapter *adapter = client->adapter;
	struct pcf_adapter *adap = (struct pcf_adapter*)adapter->data;
	unsigned int flags = client->flags;
	char addr;
	int ret=0,i,rdcount=0,wrcount=0;


	DEB(printk("i2c(bit): i2c_read: %d byte(s) read.\n", rdcount ));
	return wrcount+rdcount;
  */ 
	return 0; 
}


static int algo_control(struct i2c_adapter *adapter, 
	unsigned int cmd, unsigned long arg)
{
	return 0;
}

static int client_register(struct i2c_client *client)
{
	struct i2c_adapter *adapter = client->adapter;
	struct pcf_adapter *adap = (struct pcf_adapter*)adapter->data;

	if (adap->client_register != NULL)
		return adap->client_register(client);
	return 0;
}

int client_unregister(struct i2c_client *client)
{
	struct i2c_adapter *adapter = client->adapter;
	struct pcf_adapter *adap = (struct pcf_adapter*)adapter->data;

	if (adap->client_unregister != NULL)
		return adap->client_unregister(client);
	return 0;
}

/* -----exported algorithm data: -------------------------------------	*/

struct i2c_algorithm pcf_algo = {
	"PCF 8584 algorithm",
	ALGO_BIT,
	pcf_send,			/* master_xmit		*/
	pcf_recv,			/* master_recv		*/
	pcf_comb,			/* master_comb		*/
	NULL,				/* slave_xmit		*/
	NULL,				/* slave_recv		*/
	algo_control,			/* ioctl		*/
	client_register,
	client_unregister,
};

/* 
 * registering functions to load algorithms at runtime 
 */
int i2c_pcf_add_bus(struct pcf_adapter *adap)
{
	int i,ack;
	struct i2c_adapter *i2c_adap;

	for (i = 0; i < PCF_ADAP_MAX; i++)
		if (NULL == pcf_adaps[i])
			break;
	if (PCF_ADAP_MAX == i)
		return -ENOMEM;

	if (test) {
		int ret = test_bus(adap);
		if (ret<0)
			return -ENODEV;
	}
	i2c_adap = kmalloc(sizeof(struct i2c_adapter), GFP_KERNEL);
	if (i2c_adap == NULL)
		return -ENOMEM;

	pcf_adaps[i] = adap;
	adap_count++;
	DEB(printk("i2c(bit): algorithm %s registered.\n",adap->name));

	/* register new adapter to i2c module... */

	memset(i2c_adap,0,sizeof(struct i2c_adapter));
	strcpy(i2c_adap->name,adap->name);
	i2c_adap->id = pcf_algo.id | adap->id;
	i2c_adap->algo = &pcf_algo;
	i2c_adap->data = adap;
	i2c_adap->timeout = 100;	/* default values, should	*/
	i2c_adap->retries = 3;		/* be replaced by defines	*/
	i2c_adaps[i] = i2c_adap;
	i2c_add_adapter(i2c_adap);

	/* scan bus */
	if (scan) {
#if 0
		printk(KERN_INFO "i2c(bit): scanning bus %s.\n", adap->name);
		for (i = 0x00; i < 0xff; i+=2) {
			i2c_start(adap);
			ack = i2c_outb(adap,i);
			i2c_stop(adap);
			if (ack>0) {
				printk(KERN_INFO 
				"i2c(bit):  found chip at addr=0x%2x\n",i>>1);
			} 
		}
#endif
	}
	return 0;
}


int i2c_pcf_del_bus(struct pcf_adapter *adap)
{
	int i;

	for (i = 0; i < PCF_ADAP_MAX; i++)
		if ( adap == pcf_adaps[i])
			break;
	if ( PCF_ADAP_MAX == i) {
		printk(KERN_WARNING "i2c(bit): could not unregister bus: %s\n",
			adap->name);
		return -ENODEV;
	}

	pcf_adaps[i] = NULL;
	i2c_del_adapter(i2c_adaps[i]);
	kfree(i2c_adaps[i]);
	i2c_adaps[i] = NULL;
	adap_count--;
	DEB(printk("i2c(bit): adapter unregistered: %s\n",adap->name));

	return 0;
}

int algo_pcf_init (void)
{
	int i;

	for (i=0;i<PCF_ADAP_MAX;i++) {
		pcf_adaps[i]=NULL;
	}
	adap_count=0;
	i2c_add_algorithm(&pcf_algo);
	return 0;
}

#ifdef MODULE
MODULE_PARM(test, "i");
MODULE_PARM(scan, "i");

EXPORT_SYMBOL(i2c_pcf_add_bus);
EXPORT_SYMBOL(i2c_pcf_del_bus);


int init_module(void) 
{
	return algo_pcf_init();
}

void cleanup_module(void) 
{
	i2c_del_algorithm(&pcf_algo);
}
#endif










