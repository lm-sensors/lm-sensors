/* ------------------------------------------------------------------------- */
/* 									     */
/* i2c.h - definitions for the \iic-bus interface			     */
/* 									     */
/* ------------------------------------------------------------------------- */
/*   Copyright (C) 1995 Simon G. Vogl

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
#ifndef _I2C_H
#define _I2C_H

#ifdef __KERNEL__

/* define spinlock to use spinlocks for sync., else use semaphores*/
/*#define SPINLOCK*/

#ifdef SPINLOCK
#include <asm/spinlock.h>	/* for spinlock_t */
#else
#include <asm/semaphore.h>
#endif
/* --- General options ------------------------------------------------	*/

#define I2C_ALGO_MAX	4		/* control memory consumption	*/
#define I2C_ADAP_MAX	16
#define I2C_DRIVER_MAX	16
#define I2C_CLIENT_MAX	32

struct i2c_msg;
struct i2c_algorithm;
struct i2c_adapter;
struct i2c_client;
struct i2c_driver;


/*
 * The master routines are the ones normally used to transmit data to devices
 * on a bus (or read from them). Apart from two basic transfer functions to transmit
 * one message at a time, a more complex version can be used to transmit an arbitrary
 * number of messages without interruption.
 */
extern int i2c_master_send(struct i2c_client *,const char* ,int);
extern int i2c_master_recv(struct i2c_client *,char* ,int);

/* Transfer num messages.
 */
extern int i2c_transfer(struct i2c_adapter *adap, struct i2c_msg msg[],int num);


/*
 * Some adapter types (i.e. PCF 8584 based ones) may support slave behaviuor. 
 * This is not tested/implemented yet and will change in the future.
 */
extern int i2c_slave_send(struct i2c_client *,char*,int);
extern int i2c_slave_recv(struct i2c_client *,char*,int);



/*
 * I2C Message - could be used in the current interface to 
 */
struct i2c_msg {
	unsigned char addr;	/* slave address			*/
	unsigned short flags;		
#define I2C_M_TEN	0x10	/* we have a ten bit chip address	*/
#define I2C_M_TEN0	0x10	/* herein lie the first 2 bits 		*/
#define I2C_M_TEN1	0x12
#define I2C_M_TEN2	0x14
#define I2C_M_TEN3	0x16
#define I2C_M_TENMASK	0x06
#define I2C_M_RD	0x01
	short len;		/* msg length				*/
	char *buf;		/* pointer to msg data			*/
};

/*
 * A driver is capable of handling one or more physical devices present on
 * I2C adapters. This information is used to inform the driver of adapter
 * events.
 */

struct i2c_driver {
	char name[32];
	int id;
	unsigned int flags;		/* div., see below		*/

	/* notifies the driver that a new bus has appeared. This routine
	 * can be used by the driver to test if the bus meets its conditions
	 * & seek for the presence of the chip(s) it supports. If found, it 
	 * registers the client(s) that are on the bus to the i2c admin. via
	 * i2c_attach_client
	 */
	int (*attach_adapter)(struct i2c_adapter *);

	/* tells the driver that a client is about to be deleted & gives it 
	 * the chance to remove its private data. Also, if the client struct
	 * has been dynamically allocated by the driver in the function above,
	 * it must be freed here.
	 */
	int (*detach_client)(struct i2c_client *);
	
	/* a ioctl like command that can be used to perform specific functions
	 * with the device.
	 */
	int (*command)(struct i2c_client *client,unsigned int cmd, void *arg);
	
	/* These two are mainly used for bookkeeping & dynamic unloading of 
	 * kernel modules. inc_use tells the driver that a client is being  
	 * used by another module & that it should increase its ref. counter.
	 * dec_use is the inverse operation.
	 * NB: Make sure you have no circular dependencies, or else you get a 
	 * deadlock when trying to unload the modules.
	 */
	void (*inc_use)(struct i2c_client *client);
	void (*dec_use)(struct i2c_client *client);
};

/*
 * i2c_client identifies a single device (i.e. chip) that is connected to an 
 * i2c bus. The behaviour is defined by the routines of the driver. This
 * function is mainly used for lookup & other admin. functions.
 */
struct i2c_client {
	char name[32];
	int id;
	unsigned int flags;		/* div., see below		*/
	unsigned char addr;		/* chip address - NOTE: 7bit 	*/
					/* addresses are stored in the	*/
					/* _LOWER_ 7 bits of this char	*/
					/* 10 bit addresses use the full*/
	                                /* 8 bits & the flags like in   */
	                                /* i2c_msg              	*/
	struct i2c_adapter *adapter;	/* the adapter we sit on	*/
	struct i2c_driver *driver;	/* and our access routines	*/
	void *data;			/* for the clients		*/
};


/*
 * The following structs are for those who like to implement new bus drivers:
 * i2c_algorithm is the interface to a class of hardware solutions which can
 * be addressed using the same bus algorithms - i.e. bit-banging or the PCF8584
 * to name two of the most common.
 */
struct i2c_algorithm {
	char name[32];				/* textual description 	*/
	unsigned int id;       
	/*
	int (*master_send)(struct i2c_client *,const char*,int);
	int (*master_recv)(struct i2c_client *,char*,int);
	int (*master_comb)(struct i2c_client *,char*,const char*,int,int,int);
	*/
	int (*master_xfer)(struct i2c_adapter *adap,struct i2c_msg msgs[], int num);

	/* --- these optional/future use for some adapter types.*/
	int (*slave_send)(struct i2c_adapter *,char*,int);
	int (*slave_recv)(struct i2c_adapter *,char*,int);

	/* --- ioctl like call to set div. parameters. */
	int (*algo_control)(struct i2c_adapter *, unsigned int, unsigned long);

	/* --- administration stuff. */
	int (*client_register)(struct i2c_client *);
	int (*client_unregister)(struct i2c_client *);
};


/*
 * i2c_adapter is the structure used to identify a physical i2c bus along
 * with the access algorithms necessary to access it.
 */
struct i2c_adapter {
	char name[32];	/* some useful name to identify the adapter	*/
	unsigned int id;/* == is algo->id | hwdep.struct->id, 		*/
			/* for registered values see below		*/
	struct i2c_algorithm *algo;/* the algorithm to access the bus	*/

	void *data;	/* private data for the adapter			*/
			/* some data fields that are used by all types	*/
			/* these data fields are readonly to the public	*/
			/* and can be set via the i2c_ioctl call	*/

			/* data fields that are valid for all devices	*/
#ifdef SPINLOCK
	spinlock_t lock;/* used to access the adapter exclusively	*/
	unsigned long lockflags;
#else
	struct semaphore lock;  
#endif
	unsigned int flags;/* flags specifying div. data		*/

	struct i2c_client *clients[I2C_CLIENT_MAX];
	int client_count;

	int timeout;
	int retries;
};


/*flags for the driver struct:
 */
#define DF_NOTIFY	0x01		/* notify on bus (de/a)ttaches 	*/


#if 0 /* deprecate! */
/*flags for the client struct:
 */
#define CF_TEN	0x100000	/* we have a ten bit chip address	*/
#define CF_TEN0	0x100000	/* herein lie the first 2 bits 		*/
#define CF_TEN1	0x110000
#define CF_TEN2	0x120000
#define CF_TEN3	0x130000
#define TENMASK	0x130000
#endif

/* ----- functions exported by i2c.o */

/* administration...
 */
extern int i2c_add_algorithm(struct i2c_algorithm *);
extern int i2c_del_algorithm(struct i2c_algorithm *);

extern int i2c_add_adapter(struct i2c_adapter *);
extern int i2c_del_adapter(struct i2c_adapter *);

extern int i2c_add_driver(struct i2c_driver *);
extern int i2c_del_driver(struct i2c_driver *);

extern int i2c_attach_client(struct i2c_client *);
extern int i2c_detach_client(struct i2c_client *);


/*
 * A utility function used in the attach-phase of drivers. Returns at the first address
 * that acks in the given range.
 */
extern int i2c_probe(struct i2c_client *client, int low_addr, int hi_addr);

/* An ioctl like call to set div. parameters of the adapter.
 */
extern int i2c_control(struct i2c_client *,unsigned int, unsigned long);


/* This call returns a unique low identifier for each registered adapter,
   or -1 if the adapter was not regisitered. */
extern int i2c_adapter_id(struct i2c_adapter *adap);

#endif /* __KERNEL__ */


/* ----- commands for the ioctl like i2c_command call:
 * note that additional calls are defined in the algorithm and hw 
 *	dependent layers - these can be listed here, or see the 
 *	corresponding header files.
 */
				/* -> bit-adapter specific ioctls	*/
#define I2C_RETRIES	0x0701  /* number times a device adress should  */
				/* be polled when not acknowledging 	*/
#define I2C_TIMEOUT	0x0702	/* set timeout - call with int 		*/


/* this is for i2c-dev.c	*/
#define I2C_SLAVE	0x0703	/* Change slave address			*/
				/* Attn.: Slave address is 7 bits long, */
				/* 	these are to be passed as the	*/
				/*	lowest 7 bits in the arg.	*/
				/* for 10-bit addresses pass lower 8bits*/
#define I2C_TENBIT	0x0704	/* 	with 0-3 as arg to this call	*/
				/*	a value <0 resets to 7 bits	*/
/* ... algo-bit.c recognizes */
#define I2C_UDELAY	0x0705  /* set delay in microsecs between each  */
				/* written byte (except address)	*/
#define I2C_MDELAY	0x0706	/* millisec delay between written bytes */

#if 0
#define I2C_ADDR	0x0707	/* Change adapter's \iic address 	*/
				/* 	...not supported by all adap's	*/

#define I2C_RESET	0x07fd	/* reset adapter			*/
#define I2C_CLEAR	0x07fe	/* when lost, use to clear stale info	*/
#define I2C_V_SLOW	0x07ff  /* set jiffies delay call with int 	*/

#define I2C_INTR	0x0708	/* Pass interrupt number - 2be impl.	*/

#endif

/*
 * ---- Driver types -----------------------------------------------------
 */

#define I2C_DRIVERID_MSP3400     1
#define I2C_DRIVERID_TUNER       2
#define I2C_DRIVERID_VIDEOTEXT   3
#define I2C_DRIVERID_GL518SM     4

/*
 * ---- Adapter types ----------------------------------------------------
 *
 * First, we distinguish between several algorithms to access the hardware
 * interface types, as a PCF 8584 needs other care than a bit adapter.
 */

#define ALGO_NONE	0x00000
#define ALGO_BIT	0x10000	/* bit style adapters			*/
#define ALGO_PCF	0x20000	/* PCF 8584 style adapters		*/

#define ALGO_MASK	0xf0000	/* Mask for algorithms			*/
#define ALGO_SHIFT	0x10	/* right shift to get index values 	*/

#define I2C_HW_ADAPS	0x10000	/* number of different hw implements per*/
				/* 	algorithm layer module		*/
#define I2C_HW_MASK	0xffff	/* space for indiv. hw implmentations	*/


/* hw specific modules that are defined per algorithm layer
 */

/* --- Bit algorithm adapters 						*/
#define HW_B_LP		0x00	/* Parallel port Philips style adapter	*/
#define HW_B_LPC	0x01	/* Parallel port, over control reg.	*/
#define HW_B_SER	0x02	/* Serial line interface		*/
#define HW_B_ELV	0x03	/* ELV Card				*/
#define HW_B_VELLE	0x04	/* Vellemann K8000			*/
#define HW_B_BT848	0x05	/* BT848 video boards			*/
#define HW_B_WNV	0x06	/* Winnov Videums			*/
#define HW_B_MB         0x07    /* Via vt82c586b			*/

/* --- PCF 8584 based algorithms					*/
#define HW_P_LP		0x00	/* Parallel port interface		*/
#define HW_P_ISA	0x01	/* generic ISA Bus inteface card	*/
#define HW_P_ELEK	0x02	/* Elektor ISA Bus inteface card	*/




/* ----- I2C-DEV: char device interface stuff ------------------------- */

#define I2C_MAJOR	89		/* Device major number		*/


#  if LINUX_VERSION_CODE < 0x020100
/* Hack to make this thing compile under 2.0.xx kernels
 */

#  ifdef MODULE
#    define MODULE_AUTHOR(noone)
#    define MODULE_DESCRIPTION(none)
#    define MODULE_PARM(no,param)
#    define MODULE_PARM_DESC(no,description)
#    define EXPORT_SYMBOL(noexport)
#    define EXPORT_NO_SYMBOLS
#  endif

#  ifndef NULL
#    define NULL ( (void *) 0 )
#  endif
#endif

#  ifndef ENODEV
#    include <asm/errno.h>
#  endif
#endif
