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
static char alg_rcsid[] = "$Id: algo-bit.c,v 1.7 1998/09/28 06:45:38 i2c Exp i2c $";

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
#include "algo-bit.h"

/* ----- global defines ----------------------------------------------- */
#define DEB(x) if (i2c_debug>=1) x;
#define DEB2(x) if (i2c_debug>=2) x;
#define DEBSTAT(x) if (i2c_debug>=3) x; /* print several statistical values*/
#define DEBPROTO(x) if (i2c_debug>=9) x;
 	/* debug the protocol by showing transferred bits */

/* debugging - slow down transfer to have a look at the data .. 	*/
/* I use this with two leds&resistors, each one connected to sda,scl 	*/
/* respectively. This makes sure that the algorithm works. Some chips   */
/* might not like this, as they have an internal timeout of some mils	*/
/*
#if LINUX_VERSION_CODE >= 0x02016e
#define SLO_IO      jif=jiffies;while(jiffies<=jif+i2c_table[minor].veryslow)\
                        if (need_resched) schedule();
#else
#define SLO_IO      jif=jiffies;while(jiffies<=jif+i2c_table[minor].veryslow)\
			if (need_resched) schedule();
#endif
*/


/* ----- global variables ---------------------------------------------	*/

#ifdef SLO_IO
	int jif;
#endif

/* module parameters:
 */
int i2c_debug=1;
int bit_test=0;	/* see if the line-setting functions work	*/
int bit_scan=0;	/* have a look at what's hanging 'round		*/

/*
 *  This array contains the hw-specific functions for
 *  each port (hardware) type.
 */
struct bit_adapter *bit_adaps[BIT_ADAP_MAX];
int adap_count;
struct i2c_adapter *i2c_adaps[BIT_ADAP_MAX];

/* --- setting states on the bus with the right timing: ---------------	*/

#define setsda(adap,val) adap->setsda(adap->data, val)
#define setscl(adap,val) adap->setscl(adap->data, val)
#define getsda(adap) adap->getsda(adap->data)
#define getscl(adap) adap->getscl(adap->data)

inline void sdalo(struct bit_adapter *adap)
{
    setsda(adap,0);
    udelay(adap->udelay);
}

inline void sdahi(struct bit_adapter *adap)
{
    setsda(adap,1);
    udelay(adap->udelay);
}

inline void scllo(struct bit_adapter *adap)
{
    setscl(adap,0);
    udelay(adap->udelay);
#ifdef SLO_IO
    SLO_IO
#endif
}

/*
 * Raise scl line, and do checking for delays. This is necessary for slower
 * devices.
 */
inline int sclhi(struct bit_adapter *adap)
{
	int start=jiffies;

	setscl(adap,1);

	udelay(adap->udelay);
	if (adap->getscl == NULL )
		return 0;
 	while (! getscl(adap) ) {	
 		/* the hw knows how to read the clock line,
 		 * so we wait until it actually gets high.
 		 * This is safer as some chips may hold it low
 		 * while they are processing data internally. 
 		 */
		setscl(adap,1);
		if (start+adap->timeout <= jiffies) {
			return -ETIMEDOUT;
		}
#if LINUX_VERSION_CODE >= 0x02016e
		if (current->need_resched)
			schedule();
#else
		if (need_resched)
			schedule();
#endif
	}
	DEBSTAT(printk("needed %ld jiffies\n", jiffies-start));
#ifdef SLO_IO
	SLO_IO
#endif
	return 0;
} 


/* --- other auxiliary functions --------------------------------------	*/
void i2c_start(struct bit_adapter *adap) 
{
	/* assert: scl, sda are high */
	DEBPROTO(printk("S "));
	sdalo(adap);
	scllo(adap);
}

void i2c_repstart(struct bit_adapter *adap) 
{
	/* scl, sda may not be high */
	DEBPROTO(printk(" Sr "));
	setsda(adap,1);
	setscl(adap,1);
	udelay(adap->udelay);
	
	sdalo(adap);
	scllo(adap);
}


void i2c_stop(struct bit_adapter *adap) 
{
	DEBPROTO(printk("P\n"));
	/* assert: scl is low */
	sdalo(adap);
	sclhi(adap); 
	sdahi(adap);
}

/* send a byte without start cond., look for arbitration, 
   check ackn. from slave */
/* return 1 if ok */
int i2c_outb(struct bit_adapter *adap, char c)
{
	int i;
	int sb;
	int ack;

	/* assert: scl is low */
	DEB2(printk(" i2c_outb:%2.2X\n",c&0xff));
	for ( i=7 ; i>=0 ; i-- ) {
		sb = c & ( 1 << i );
		setsda(adap,sb);
		udelay(adap->udelay);
		DEBPROTO(printk("%d",sb!=0));
		if (sclhi(adap)<0) { /* timed out */
			sdahi(adap); /* we don't want to block the net */
			return -ETIMEDOUT;
		};
		/* do arbitration here: 
		 * if ( sb && ! getsda(adap) ) -> ouch! Get out of here.
		 */
		setscl( adap, 0 );
		udelay(adap->udelay);
	}
	sdahi(adap);
	if (sclhi(adap)<0){ /* timeout */
		return -ETIMEDOUT;
	};
	/* read ack: SDA should be pulled down by slave */
	ack=getsda(adap);	/* ack: sda is pulled low ->success.	 */
	DEB2(printk(" i2c_outb: getsda() =  0x%2.2x\n", ~ack ));

	DEBPROTO( printk("[%2.2x]",c&0xff) );
	DEBPROTO(if (0==ack) printk(" A "); else printk(" NA ") );
	scllo(adap);
	return 0==ack;		/* return 1 if device acked	 */
	/* assert: scl is low (sda undef) */
}



int i2c_inb(struct bit_adapter *adap) 
{
	/* read byte via i2c port, without start/stop sequence	*/
	/* acknowledge is sent in i2c_read.			*/
	int i;
	char indata;

	/* assert: scl is low */
	DEB2(printk("i2c_inb.\n"));

	sdahi(adap);
	indata=0;
	for (i=0;i<8;i++) {
		if (sclhi(adap)<0) { /* timeout */
			return -ETIMEDOUT;
		};
		indata *= 2;
		if ( getsda(adap) ) 
		  indata |= 0x01;
		scllo(adap);
	}
	/* assert: scl is low */
    DEBPROTO(printk(" %2.2x", indata & 0xff));
    return (int) (indata & 0xff);
}

/*
 * Sanity check for the adapter hardware - check the reaction of
 * the bus lines only if it seems to be idle.
 */
int test_bus(struct bit_adapter *adap) {
	int scl,sda;
	sda=getsda(adap);
	if (adap->getscl==NULL) {
		printk("i2c(bit): Warning: Adapter can't read from clock line - skipping test.\n");
		return 0;		
	}
	scl=getscl(adap);
	printk("i2c(bit): Adapter: %s scl: %d  sda: %d -- testing...\n",
	adap->name,getscl(adap),getsda(adap));
	if (!scl || !sda ) {
		printk("i2c(bit): %s seems to be busy.\n",adap->name);
		goto bailout;
	}
	sdalo(adap);
	printk("i2c(bit):1 scl: %d  sda: %d \n",getscl(adap),getsda(adap));
	if ( 0 != getsda(adap) ) {
		printk("i2c(bit): %s SDA stuck high!\n",adap->name);
		sdahi(adap);
		goto bailout;
	}
	if ( 0 == getscl(adap) ) {
		printk("i2c(bit): %s SCL unexpected low while pulling SDA low!\n",
			adap->name);
		goto bailout;
	}		
	sdahi(adap);
	printk("i2c(bit):2 scl: %d  sda: %d \n",getscl(adap),getsda(adap));
	if ( 0 == getsda(adap) ) {
		printk("i2c(bit): %s SDA stuck low!\n",adap->name);
		sdahi(adap);
		goto bailout;
	}
	if ( 0 == getscl(adap) ) {
		printk("i2c(bit): %s SCL unexpected low while SDA high!\n",adap->name);
	goto bailout;
	}
	scllo(adap);
	printk("i2c(bit):3 scl: %d  sda: %d \n",getscl(adap),getsda(adap));
	if ( 0 != getscl(adap) ) {
		printk("i2c(bit): %s SCL stuck high!\n",adap->name);
		sclhi(adap);
		goto bailout;
	}
	if ( 0 == getsda(adap) ) {
		printk("i2c(bit): %s SDA unexpected low while pulling SCL low!\n",
			adap->name);
		goto bailout;
	}
	sclhi(adap);
	printk("i2c(bit):4 scl: %d  sda: %d \n",getscl(adap),getsda(adap));
	if ( 0 == getscl(adap) ) {
		printk("i2c(bit): %s SCL stuck low!\n",adap->name);
		sclhi(adap);
		goto bailout;
	}
	if ( 0 == getsda(adap) ) {
		printk("i2c(bit): %s SDA unexpected low while SCL high!\n",
			adap->name);
		goto bailout;
	}
	printk("i2c(bit): %s passed test.\n",adap->name);
	return 0;
bailout:
	sdahi(adap);
	sclhi(adap);
	return -ENODEV;
}

/* ----- Utility functions
 */

inline int try_address(struct bit_adapter *adap,
		       unsigned char addr, int retries)
{
	int i,ret = -1;
	for (i=0;i<retries;i++) {
		ret = i2c_outb(adap,addr);
		if (ret==1)
			break;	/* success! */
		i2c_stop(adap);
		udelay(5/*adap->udelay*/);
		i2c_start(adap);
		udelay(adap->udelay);
	}
	DEB2(if (i) printk("i2c(bit): needed %d retries for %d\n",i,addr));
	return ret;
}

int sendbytes(struct bit_adapter *adap,const char *buf, int count)
{
	char c;
	const char *temp = buf;
	int retval;
	int wrcount=0;

	while (count > 0) {
		c = *temp;
		DEB2(printk("i2c(bit): %s i2c_write: writing %2.2X\n",
			    adap->name, c&0xff));
		retval = i2c_outb(adap,c);
		if (retval>0) {
			count--; 
			temp++;
			wrcount++;
		} else { /* arbitration or no acknowledge */
			printk("i2c(bit): %s i2c_write: error - bailout.\n",
			       adap->name);
			i2c_stop(adap);
			return -EREMOTEIO; /* got a better one ?? */
		}
#if 0
		/* from asm/delay.h */
		__delay(adap->mdelay * (loops_per_sec / 1000) );
#endif
	}
	return wrcount;
}

inline int readbytes(struct bit_adapter *adap,char *buf,int count)
{
	char *temp = buf;
	int inval;
	int rdcount=0;   	/* counts bytes read */

	while (count > 0) {
		inval = i2c_inb(adap);
/*printk("%#02x ",inval);
if ( ! (count % 16) )
printk("\n");
*/
		if (inval>=0) {
			*temp = inval;
			rdcount++;
		} else {   /* read timed out */
			printk("i2c(bit): i2c_read: i2c_inb timed out.\n");
			break;
		}

		if ( count > 1 ) {		/* send ack */
			sdalo(adap);
			DEBPROTO(printk(" Am "));
		} else {
			sdahi(adap);		/* neg. ack on last byte */
			DEBPROTO(printk(" NAm "));
		}
		if (sclhi(adap)<0) {		/* timeout */
			sdahi(adap);
			printk("i2c(bit): i2c_read: Timeout at ack\n");
			return -ETIMEDOUT;
		};
		scllo(adap);
		sdahi(adap);
		temp++;
		count--;
	}
	return rdcount;
}

inline int bit_doAddress(struct bit_adapter *adap, struct i2c_msg *msg, 
			int retries) 
{
	unsigned short flags = msg->flags;
	unsigned char addr;
	int ret;
	if ( (flags & I2C_M_TEN)  ) { 
		/* a ten bit address */
		addr = 0xf0 | ( flags & I2C_M_TENMASK );
		DEB2(printk("addr0: %d\n",addr));
		/* try extended address code...*/
		ret = try_address(adap, addr, retries);
		if (ret!=1) {
			printk("died at extended address code.\n");
			return -EREMOTEIO;
		}
		/* the remaining 8 bit address */
		ret = i2c_outb(adap,msg->addr);
		if (ret != 1) {
			printk("died at 2nd address code.\n");
			return -EREMOTEIO;
		}
		if ( flags & I2C_M_RD ) {
			i2c_repstart(adap);
			/* okay, now switch into reading mode */
			addr |= 0x01;
			ret = try_address(adap, addr, retries);
			if (ret!=1) {
				printk("died at extended address code.\n");
				return -EREMOTEIO;
			}
		}
	} else {		/* normal 7bit address	*/
		addr = ( msg->addr << 1 );
		if (flags & I2C_M_RD )
			addr |= 1;
		ret = try_address(adap, addr, retries);
		if (ret!=1) {
			return -EREMOTEIO;
		}
	}
	return 0;
}

int bit_xfer(struct i2c_adapter *adapter,
		    struct i2c_msg msgs[], int num)
{
	struct bit_adapter *adap = (struct bit_adapter*)adapter->data;
	struct i2c_msg *pmsg;
	int i,ret;

	i2c_start(adap);
	for (i=0;i<num;i++) {
		pmsg = &msgs[i];
		ret = bit_doAddress(adap,pmsg,adapter->retries);
		if (ret != 0) {
			DEB2(printk("i2c(bit): NAK from device adr %#2x msg #%d\n"
			       ,msgs[i].addr,i));
			return -EREMOTEIO;
		}
		if (pmsg->flags & I2C_M_RD ) {
			/* read bytes into buffer*/
			ret = readbytes(adap,pmsg->buf,pmsg->len);
			DEB2(printk("i2c(bit): read %d bytes.\n",ret));
		} else {
			/* write bytes from buffer */
			ret = sendbytes(adap,pmsg->buf,pmsg->len);
			DEB2(printk("i2c(bit): wrote %d bytes.\n",ret));
		}
		if (i<num-1) {
			i2c_repstart(adap);
		}
	}
	i2c_stop(adap);
	return num;
}


int algo_control(struct i2c_adapter *adapter, 
	unsigned int cmd, unsigned long arg)
{
	return 0;
}

int client_register(struct i2c_client *client)
{
	struct i2c_adapter *adapter = client->adapter;
	struct bit_adapter *adap = (struct bit_adapter*)adapter->data;

	if (adap->client_register != NULL)
		return adap->client_register(client);
	return 0;
}

int client_unregister(struct i2c_client *client)
{
	struct i2c_adapter *adapter = client->adapter;
	struct bit_adapter *adap = (struct bit_adapter*)adapter->data;

	if (adap->client_unregister != NULL)
		return adap->client_unregister(client);
	return 0;
}

/* -----exported algorithm data: -------------------------------------	*/

struct i2c_algorithm bit_algo = {
	"Bit-shift algorithm",
	ALGO_BIT,
	bit_xfer,
#if 0
	bit_send,			/* master_xmit		*/
	bit_recv,			/* master_recv		*/
	bit_comb,			/* master_comb		*/
#endif
	NULL,				/* slave_xmit		*/
	NULL,				/* slave_recv		*/
	algo_control,			/* ioctl		*/
	client_register,
	client_unregister,
};

/* 
 * registering functions to load algorithms at runtime 
 */
int i2c_bit_add_bus(struct bit_adapter *adap)
{
	int i,ack;
	struct i2c_adapter *i2c_adap;

	for (i = 0; i < BIT_ADAP_MAX; i++)
		if (NULL == bit_adaps[i])
			break;
	if (BIT_ADAP_MAX == i)
		return -ENOMEM;

	if (bit_test) {
		int ret = test_bus(adap);
		if (ret<0)
			return -ENODEV;
	}
	i2c_adap = kmalloc(sizeof(struct i2c_adapter), GFP_KERNEL);
	if (i2c_adap == NULL)
		return -ENOMEM;

	bit_adaps[i] = adap;
	adap_count++;
	DEB2(printk("i2c(bit): hw routines for %s registered.\n",adap->name));

	/* register new adapter to i2c module... */

	memset(i2c_adap,0,sizeof(struct i2c_adapter));
	strcpy(i2c_adap->name,adap->name);
	i2c_adap->id = bit_algo.id | adap->id;
	i2c_adap->algo = &bit_algo;
	i2c_adap->data = adap;
	i2c_adap->timeout = 100;	/* default values, should	*/
	i2c_adap->retries = 3;		/* be replaced by defines	*/
	i2c_adaps[i] = i2c_adap;
	i2c_add_adapter(i2c_adap);

	/* scan bus */
	if (bit_scan) {
		printk(KERN_INFO " i2c(bit): scanning bus %s.\n", adap->name);
		for (i = 0x00; i < 0xff; i+=2) {
			i2c_start(adap);
			ack = i2c_outb(adap,i);
			i2c_stop(adap);
			if (ack>0) {
				printk("(%02x)",i>>1); 
			} else 
				printk("."); 
		}
		printk("\n");
	}
	return 0;
}


int i2c_bit_del_bus(struct bit_adapter *adap)
{
	int i;

	for (i = 0; i < BIT_ADAP_MAX; i++)
		if ( adap == bit_adaps[i])
			break;
	if ( BIT_ADAP_MAX == i) {
		printk(KERN_WARNING " i2c(bit): could not unregister bus: %s\n",
			adap->name);
		return -ENODEV;
	}

	bit_adaps[i] = NULL;
	i2c_del_adapter(i2c_adaps[i]);
	kfree(i2c_adaps[i]);
	i2c_adaps[i] = NULL;
	adap_count--;
	DEB2(printk("i2c(bit): adapter unregistered: %s\n",adap->name));

	return 0;
}

int algo_bit_init (void)
{
	int i;

	for (i=0;i<BIT_ADAP_MAX;i++) {
		bit_adaps[i]=NULL;
	}
	adap_count=0;
	i2c_add_algorithm(&bit_algo);
	return 0;
}

#ifdef MODULE
MODULE_AUTHOR("Simon G. Vogl <simon@tk.uni-linz.ac.at>");
MODULE_DESCRIPTION("I2C-Bus bit-banging algorithm");

MODULE_PARM(bit_test, "i");
MODULE_PARM(bit_scan, "i");
MODULE_PARM(i2c_debug,"i");

MODULE_PARM_DESC(bit_test, "Test the lines of the bus to see if it is stuck");
MODULE_PARM_DESC(bit_scan, "Scan for active chips on the bus");
MODULE_PARM_DESC(i2c_debug,"debug level - 0 off; 1 normal; 2,3 more verbose; 9 bit-protocol");


EXPORT_SYMBOL(i2c_bit_add_bus);
EXPORT_SYMBOL(i2c_bit_del_bus);


int init_module(void) 
{
	return algo_bit_init();
}

void cleanup_module(void) 
{
	i2c_del_algorithm(&bit_algo);
}
#endif










