/*
 * i2c-virtual.c - Virtual I2C bus driver.
 *
 * Simplifies access to complex multiplexed I2C bus topologies, by presenting
 * each multiplexed bus segment as a virtual I2C adapter.  Supports multi-level
 * mux'ing (mux behind a mux), as well as arbitration for exclusive bus access
 * for those systems which require it (some bladed chassis for example).
 *
 * Copyright (c) 2004 Google, Inc. (Ken Harrenstien)
 *
 * Based on:
 *    i2c-virtual.c from Brian Kuschak <bkuschak@yahoo.com>
 * which was:
 *    Adapted from i2c-adap-ibm_ocp.c 
 *    Original file Copyright 2000-2002 MontaVista Software Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/i2c-id.h>
#include "i2c-virtual.h"
#include <linux/ioport.h>
#include <linux/errno.h>
#include <linux/sched.h>

#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>

#ifdef DEBUG
# define DBG(x) x
#else
# define DBG(x)	{}
#endif

#define I2C_VIRT_PROCMAP 0	/* 1 for /proc/driver/i2c/virtual_i2c_map */

#define VIRT_TIMEOUT		(HZ/2)		/* 500msec */
#define VIRT_RETRIES		3		

/* exclusive access to the bus */
#define I2C_LOCK(adap) down(&adap->bus)
#define I2C_UNLOCK(adap) up(&adap->bus)

#if I2C_VIRT_PROCMAP
/* pointer to the list of i2c_bus_mappings */
static LIST_HEAD(i2c_map_list);
struct i2c_bus_mapping {
	struct i2c_adapter	*parent;	
	struct i2c_adapter	*virt;
	unsigned long		mux_addr;	
	unsigned long		mux_val;
	struct list_head	list; 
};
static void *i2c_virt_add_map(struct i2c_adapter *parent,
                              struct i2c_adapter *virt,
                              unsigned long mux_addr,
                              unsigned long mux_val);
#endif

/* First the I2C 'algorithm' driver code: */

/*
 * Description: acquire exclusive access (if needed), select the mux,
 * perform the transfer on the parent i2c adapter, deselect mux and drop
 * exclusive access.
 */
static int
i2c_virt_master_xfer(struct i2c_adapter *adap, struct i2c_msg msgs[], int num)
{
	struct i2c_virt_priv 	*priv = (struct i2c_virt_priv *)adap->data;
	struct i2c_adapter 	*parent = priv->parent_adap;
	int ret;


#ifdef I2C_REQUIRE_ARBITRATION
	/* Acquire exclusive access to a shared I2C bus _before_ taking the
           local I2C lock to prevent stalling local I2C transactions while
           waiting for exclusive access.
	 */
	if(parent->algo->acquire_exclusive) 
		if ((ret = parent->algo->acquire_exclusive(parent)) < 0)
			return ret;
#endif
	/* Grab the lock for the parent adapter.  We already hold the lock for
           the virtual adapter.  Then select the right mux port and perform
           the transfer.
	 */
	DBG(printk(KERN_DEBUG "%s: Locking parent bus %0lX\n",
                   __FUNCTION__, (unsigned long)parent));
	I2C_LOCK(parent);
	if ((ret = priv->mux.select(parent, &priv->mux)) == 0) {
            ret = parent->algo->master_xfer(parent, msgs, num);
        }
	(void) priv->mux.deselect(parent, &priv->mux);
	I2C_UNLOCK(parent);
	DBG(printk(KERN_DEBUG "%s: Unlocked parent bus %0lX\n",
                   __FUNCTION__, (unsigned long)parent));

#ifdef I2C_REQUIRE_ARBITRATION
    {
	int ret2;
	/* Release exclusive bus access */
	if(parent->algo->release_exclusive) 
		if ((ret2 = parent->algo->release_exclusive(parent)) < 0)
			return ret2;
    }
#endif
	return ret;
}

static int
i2c_virt_smbus_xfer(struct i2c_adapter *adap, u16 addr, 
                    unsigned short flags, char read_write,
                    u8 command, int size, union i2c_smbus_data * data)
{
	struct i2c_virt_priv 	*priv = (struct i2c_virt_priv *)adap->data;
	struct i2c_adapter 	*parent = priv->parent_adap;
	int ret;

#ifdef I2C_REQUIRE_ARBITRATION
	/* Acquire exclusive access to a shared I2C bus _before_ taking the
           local I2C lock to prevent stalling local I2C transactions while
           waiting for exclusive access.
	 */
	if(parent->algo->acquire_exclusive) 
		if ((ret = parent->algo->acquire_exclusive(parent)) < 0)
			return ret;
#endif
	/* Grab the lock for the parent adapter.  We already hold the lock for
           the virtual adapter.  Then select the right mux port and perform
           the transfer.
	 */
	DBG(printk(KERN_DEBUG "%s: Locking parent bus %0lX\n",
                   __FUNCTION__, (unsigned long)parent));
	I2C_LOCK(parent);
	if ((ret = priv->mux.select(parent, &priv->mux)) == 0) {
            ret = parent->algo->smbus_xfer(parent, addr, flags,
                                           read_write, command, size, data);
        }
	(void) priv->mux.deselect(parent, &priv->mux);
	I2C_UNLOCK(parent);
	DBG(printk(KERN_DEBUG "%s: Unlocked parent bus %0lX\n",
                   __FUNCTION__, (unsigned long)parent));

#ifdef I2C_REQUIRE_ARBITRATION
    {
	int ret2;
	/* Release exclusive bus access */
	if(parent->algo->release_exclusive) 
		if ((ret2 = parent->algo->release_exclusive(parent)) < 0)
			return ret2;
    }
#endif
	return ret;
}

/*
 * Description: Implements device specific ioctls.  
 * We don't support any yet.
 */
static int
i2c_virt_algo_control(struct i2c_adapter *adap, unsigned int cmd,
                      unsigned long arg)
{
	return 0;
}

/* Virtual adapter functionality; returns functionality of parent adapter.
 * OK if parent itself is virtual, as this will go all the way up to the
 * real adapter.
 */
static u32
i2c_virt_functionality(struct i2c_adapter *adap)
{
	struct i2c_virt_priv 	*priv = (struct i2c_virt_priv *)adap->data;
	struct i2c_adapter 	*parent = priv->parent_adap;

	return parent->algo->functionality(parent);
}

/* ===================================================================== */

/* The 'adapter' driver code: */

static int
i2c_virt_reg(struct i2c_client *client)
{
	return 0;
}
static int
i2c_virt_unreg(struct i2c_client *client)
{
	return 0;
}


/*
 * Called to create a 'virtual' i2c bus which represents a multiplexed bus
 * segment.  Client and mux_val are passed to the select and deselect
 * callback functions to perform hardware-specific mux control.
 */
struct i2c_adapter *i2c_virt_create_adapter(struct i2c_adapter *parent_adap, 
                                            struct i2c_client *client,
                                            unsigned long mux_val, 
                                            void *select_cb,
                                            void *deselect_cb)
{
	struct i2c_adapter *adap;
	struct i2c_virt_priv *priv;
	struct i2c_algorithm *algo;

	if (!(adap = kmalloc(sizeof (struct i2c_adapter)
                             + sizeof(struct i2c_virt_priv)
                             + sizeof(struct i2c_algorithm), 
                             GFP_KERNEL))) {
		printk(KERN_ERR "i2c_virt_register_adap: Failed allocation\n");
		return NULL;
	}

	memset(adap, 0, sizeof(struct i2c_adapter)
                      + sizeof(struct i2c_virt_priv)
		      + sizeof(struct i2c_algorithm));
        priv = (struct i2c_virt_priv *) (adap+1);
        algo = (struct i2c_algorithm *) (priv+1);

        /* Set up private adapter data */
	priv->parent_adap = parent_adap;
	priv->mux.client = client;
	priv->mux.addr = client->addr;
	priv->mux.value = mux_val;
	priv->mux.select = select_cb;
	priv->mux.deselect = deselect_cb;

        /* Need to do algo dynamically because we don't know ahead
           of time what sort of physical adapter we'll be dealing with.
        */
        algo->id = I2C_ALGO_VIRT;
        strcpy(algo->name, "Virtual I2C algorithm driver");
        algo->master_xfer = (parent_adap->algo->master_xfer
                             ? i2c_virt_master_xfer : NULL);
        algo->smbus_xfer = (parent_adap->algo->smbus_xfer
                             ? i2c_virt_smbus_xfer : NULL);
        algo->slave_send = NULL;
        algo->slave_recv = NULL;
        algo->algo_control = i2c_virt_algo_control;
        algo->functionality = i2c_virt_functionality;
#ifdef I2C_REQUIRE_ARBITRATION
	algo->acquire_exclusive = NULL;
	algo->release_exclusive = NULL;
#endif

        /* Now fill out new adapter structure */
	adap->inc_use = NULL;
	adap->dec_use = NULL;
	snprintf(adap->name, sizeof(adap->name),
                 "Virtual I2C (i2c-%d, mux %02lx:%02lx)",
                 i2c_adapter_id(parent_adap),
                 (unsigned long)client->addr, mux_val);
	adap->id = I2C_HW_VIRT | algo->id;
	adap->algo = algo;
        adap->algo_data = NULL;		/* XXX: Use this?? */
	adap->client_register = i2c_virt_reg;
	adap->client_unregister = i2c_virt_unreg;
	adap->data = priv;
	adap->flags = 0;
	adap->timeout = VIRT_TIMEOUT;
	adap->retries = VIRT_RETRIES;

	DBG(printk(KERN_DEBUG "%s: Adding virt bus %0lX\n",
                   __FUNCTION__, (unsigned long)adap));

	if (i2c_add_adapter_nolock(adap) < 0) {
            kfree(adap);
            return NULL;
        }

	printk(KERN_NOTICE "i2c%d: Virtual I2C bus "
		"(Physical bus i2c%d, multiplexer %02lx port %ld)\n",
		i2c_adapter_id(adap), i2c_adapter_id(parent_adap), 
		priv->mux.addr, mux_val);

#if I2C_VIRT_PROCMAP
        i2c_virt_add_map(priv->parent_adap, adap,
                         priv->mux.addr, priv->value);
#endif

	MOD_INC_USE_COUNT;
	DBG(printk(KERN_DEBUG "%s: Added virt bus %0lX\n",
                   __FUNCTION__, (unsigned long)adap));

	return adap;
}

int i2c_virt_remove_adapter(struct i2c_adapter *adap)
{
	int ret;

	DBG(printk(KERN_DEBUG "%s: Removing virt bus %0lX\n",
                   __FUNCTION__, (unsigned long)adap));

	if ((ret = i2c_del_adapter_nolock(adap)) < 0)
		return ret;
        kfree(adap);

	MOD_DEC_USE_COUNT;
	return 0;
}


/* ------------------------------------------------------------------ */

#if I2C_VIRT_PROCMAP
/* Dubious stuff to implement a /proc/driver/i2c/virtual_i2c_map
 * listing of all virtual busses.
 *
 * XXX: LOCKING???
 */
static int i2c_virt_read_proc(char *page, char **start, off_t off,
                         int count, int *eof, void *data)
{
	int len;
	char *buf = page;
	struct list_head *l;
	struct i2c_bus_mapping *pmap;

        /* XXX: BUFFER BOUNDS CHECKING? */

        buf += sprintf(buf, "i2c bus_mapping\n");

	list_for_each(l, &i2c_map_list) {
		pmap = list_entry(l, struct i2c_bus_mapping, list);

		if(pmap)
        		buf += sprintf(buf, "base=iic%d  mux_addr=%02hx, mux_val=%02hx  =>  iic%d\n", 
				i2c_adapter_id(pmap->parent),
                                pmap->mux_addr, pmap->mux_val, 
				i2c_adapter_id(pmap->virt));
	}

        len = buf - page;
        if (len <= off+count) *eof = 1;
        *start = page + off;
        len -= off;
        if (len>count) len = count;
        if (len<0) len = 0;
        return len;
}


static void *i2c_virt_add_map(struct i2c_adapter *parent,
                              struct i2c_adapter *virt,
                              unsigned long mux_addr,
                              unsigned long mux_val)
{
	struct i2c_bus_mapping *pmap;

	if((pmap = kmalloc(sizeof(struct i2c_bus_mapping), GFP_KERNEL))
           == NULL) {
		printk(KERN_ERR "%s: Failed to allocate bus mapping\n",
                       __FUNCTION__);
		return;
	}
	pmap->virt = virt;
	pmap->parent = parent;
	pmap->mux_addr = mux_addr;
	pmap->mux_val = mux_val;

        if (list_empty(&i2c_map_list)) {
            /* If first time, ensure proc entry exists */
            /* XXX: Maybe do this with module_init() */
            create_proc_read_entry("driver/i2c/virtual_i2c_map", 0, NULL,
                               &i2c_virt_read_proc, NULL);
        }
	list_add_tail(&pmap->list, &i2c_map_list);
}


#if 0 /* Not used by anything, but kept for a while in case */

/* Called to find out which i2c bus to use to get to a specific bus segment.
 */
struct i2c_adapter *i2c_lookup_adapter(struct i2c_adapter *base,
                                       unsigned long mux_addr, 
                                       unsigned long mux_val)
{	
	struct list_head *l;
	struct i2c_bus_mapping *pmap;

	list_for_each(l, &i2c_map_list) {
		pmap = list_entry(l, struct i2c_bus_mapping, list);
		if(pmap->parent == base
                   && pmap->mux_addr == mux_addr
                   && pmap->mux_val == mux_val)
			return pmap->virt;
	}
	return NULL;	/* none found */
}

EXPORT_SYMBOL(i2c_lookup_adapter);
#endif /* 0 */


#endif /* I2C_VIRT_PROCMAP */

EXPORT_SYMBOL(i2c_virt_create_adapter);
EXPORT_SYMBOL(i2c_virt_remove_adapter);

MODULE_AUTHOR("Ken Harrenstien (from Brian Kuschak)");
MODULE_DESCRIPTION("Virtual I2C driver for multiplexed I2C busses");
MODULE_LICENSE("GPL");
