/*
 * i2c-virtual.h - Header file for the 'virtual i2c' adapter driver.
 *
 * Copyright (c) 2004  Google, Inc.
 *
 * Based on:
 *    i2c-virtual.h from Brian Kuschak <bkuschak@yahoo.com>
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
#ifndef __I2C_VIRTUAL_H
#define __I2C_VIRTUAL_H

#include <linux/i2c.h>

struct i2c_mux_ctrl {
	struct i2c_client *client;	/* The mux chip/device */

	unsigned long	addr;		
	unsigned long	value;		/* Channel # */

	/* fn which enables the mux */
	int (*select)(struct i2c_adapter *adap, struct i2c_mux_ctrl *mux);

	/* fn which disables the mux */
	int (*deselect)(struct i2c_adapter *adap, struct i2c_mux_ctrl *mux);
};

/* This has to be exposed, since the code which assigns the callbacks must
   use it.
 */
struct i2c_virt_priv {
	struct i2c_adapter  *parent_adap;	/* pointer to parent adapter */
	struct i2c_mux_ctrl  mux;	/* MUX settings for this adapter */
};


static inline struct i2c_adapter *i2c_virt_parent(struct i2c_adapter *adap)
{
	if (adap->algo && (adap->algo->id == I2C_ALGO_VIRT)) {
                return ((struct i2c_virt_priv *)(adap->data))->parent_adap;
        }
        return NULL;
}

/*
 * Called to create a 'virtual' i2c bus which represents a multiplexed bus
 * segment.  The client and mux_val are passed to the select and deselect
 * callback functions to perform hardware-specific mux control.
 */
struct i2c_adapter *i2c_virt_create_adapter(struct i2c_adapter *parent_adap, 
                                           struct i2c_client *client,
                                           unsigned long mux_val, 
                                           void *select_cb,
                                           void *deselect_cb);
int i2c_virt_remove_adapter(struct i2c_adapter *adap);


#endif /* __I2C_VIRTUAL_H */
