/*
 * pca954x.c - Part of lm_sensors, Linux kernel modules for hardware
 *             monitoring
 * This module supports the PCA954x series of I2C multiplexer/switch chips
 * made by Philips Semiconductors.  This includes the
 *	PCA9540, PCA9542, PCA9543, PCA9544, PCA9545, PCA9546, and PCA9548.
 *
 * Copyright (c) 2004 Google, Inc. (Ken Harrenstien)
 *
 * Based on:
 *    i2c-virtual_cb.c from Brian Kuschak <bkuschak@yahoo.com>
 * and
 *    pca9540.c from Jean Delvare <khali@linux-fr.org>, which was
 *	based on pcf8574.c from the same project by Frodo Looijaard,
 *	Philip Edelbrock, Dan Eaton and Aurelien Jarno.
 *
 * These chips are all controlled via the I2C bus itself, and all have a
 * single 8-bit register (normally at 0x70).  The upstream "parent" bus fans
 * out to two, four, or eight downstream busses or channels; which of these
 * are selected is determined by the chip type and register contents.  A
 * mux can select only one sub-bus at a time; a switch can select any
 * combination simultaneously.
 *
 * See Documentation/i2c/virtual_i2c for details on the virtual bus/adapter
 * mechanism.
 *
 *********************************************************************
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

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/i2c-proc.h>
#include <linux/init.h>
#include "i2c-virtual.h"
#include "version.h"

MODULE_LICENSE("GPL");

#define DEBUG 1		/* XXX: Comment out before release? */
#ifdef DEBUG
# define DBG(x) if (pca954x_debug > 0) { x; } else 
#else
# define DBG(x)	{}
#endif

#define PCA954X_NCHANS 8		/* Max # of channels (PCA9548) */

/* Addresses to scan */
static unsigned short normal_i2c[] = { /*0x70,*/ SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { SENSORS_I2C_END };
static unsigned int normal_isa[] = { SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/* Insmod/modprobe parameters */

/* Chip type must normally be specified using a parameter of the form
	"force_pca9544=0,0x70"
   The following declares the possible types.
*/
SENSORS_INSMOD_7(pca9540,pca9542,pca9543,pca9544,pca9545,pca9546,pca9548);

static int pca954x_debug;
MODULE_PARM(pca954x_debug, "i");
MODULE_PARM_DESC(pca954x_debug, "Debug output level (0 = none)");

#if 0	/* Considered adding this, but too many issues.  See doc. */
static int pca954x_selmask[32];	/* Max # chips supported, sigh */
MODULE_PARM(pca954x_selmask, "1-32i");
MODULE_PARM_DESC(pca954x_selmask, "List of selmask settings for each mux/swi");
#endif

/* Provide specs for the PCA954x types we know about
 */
static struct pca954x_chipdef {
        enum chips type;
        const char *type_name;
        int pcanum;
        int nchans;
        enum muxtype { pca954x_ismux=0, pca954x_isswi } muxtype;
} pca954x_chipdefs[] = {
        { pca9540, "pca9540", 9540, 2, pca954x_ismux },
        { pca9542, "pca9542", 9542, 2, pca954x_ismux },
        { pca9543, "pca9543", 9543, 2, pca954x_isswi },
        { pca9544, "pca9544", 9544, 4, pca954x_ismux },
        { pca9545, "pca9545", 9545, 4, pca954x_isswi },
        { pca9546, "pca9546", 9546, 4, pca954x_isswi },
        { pca9548, "pca9548", 9548, 8, pca954x_isswi },
};
#define PCA954X_MUX_ENA 0x04	/* Mux enable bit */

/* Each client has this additional data.
   Note the two conventions for identifying a chip's channel (sub-bus):
        "channel" 0 to N-1, where N is the number of channels supported
        "bitmask" 1 to 1 << N-1, so bit 0 is channel 0.
   This use of "channel" is consistent with that in the Philips PCA954X doc.
 */
struct pca954x_data {
	struct i2c_client client;
	int sysctl_id;
	enum chips type;
	enum muxtype muxtype;
	int pcanum;	/* PCA chip number */
	int nchans;	/* # of channels (busses) this chip controls */
	int nchmask;	/* Mask of all channels */

	struct semaphore update_lock;
	int selmask;	/* Bitmask of chans OK to leave on (chn1 == bit0) */
	int curmask;	/* Bitmask of chans currently selected */

	u8 regval;	/* Current chip register value */
	u8 origval;	/* Original value at initialization */

	struct i2c_adapter *virt_adapters[PCA954X_NCHANS];
};

static int  pca954x_attach_adapter(struct i2c_adapter *adapter);
static int  pca954x_detect(struct i2c_adapter *adapter, int address,
                           unsigned short flags, int kind);
static int  pca954x_detach_client(struct i2c_client *client);
static void pca954x_pe_busses(struct i2c_client *client, int operation,
                              int ctl_name, int *nrels_mag, long *results);
static void pca954x_pe_debug(struct i2c_client *client, int operation,
                             int ctl_name, int *nrels_mag, long *results);
static void pca954x_pe_regval(struct i2c_client *client, int operation,
                              int ctl_name, int *nrels_mag, long *results);
static void pca954x_pe_selbus(struct i2c_client *client, int operation,
                              int ctl_name, int *nrels_mag, long *results);
static void pca954x_pe_selmask(struct i2c_client *client, int operation,
                               int ctl_name, int *nrels_mag, long *results);
static void pca954x_pe_type(struct i2c_client *client, int operation,
                            int ctl_name, int *nrels_mag, long *results);
static void pca954x_set_selmask(struct i2c_client *client, int mask);
static u8   pca954x_mask2val(struct i2c_client *client, int mask);
static int  pca954x_val2mask(struct i2c_client *client, u8 val);

static int  pca954x_select_mux(struct i2c_adapter *adap,
                               struct i2c_mux_ctrl *mux);
static int  pca954x_deselect_mux(struct i2c_adapter *adap,
                                 struct i2c_mux_ctrl *mux);
static int  pca954x_select_chan(struct i2c_adapter *adap,
                                struct i2c_client *client, unsigned long chan);
static int pca954x_xfer(struct i2c_adapter *adap,
                        struct i2c_client *client,
                        int read_write, u8 *val);


/* This is the driver that will be inserted */
static struct i2c_driver pca954x_driver = {
	.name		= "PCA954X I2C mux/switch driver",
	.flags		= I2C_DF_NOTIFY,
	.attach_adapter	= pca954x_attach_adapter,
	.detach_client	= pca954x_detach_client,
};


/* -- SENSORS SYSCTL START -- */

#define PCA954X_SYSCTL_BUSSES		1000
#define PCA954X_SYSCTL_DEBUG		1001
#define PCA954X_SYSCTL_REGVAL		1002
#define PCA954X_SYSCTL_SELBUS		1003
#define PCA954X_SYSCTL_SELMASK		1004
#define PCA954X_SYSCTL_TYPE		1005

/* -- SENSORS SYSCTL END -- */

static ctl_table pca954x_dir_table_template[] = {
	{PCA954X_SYSCTL_BUSSES, "busses", NULL, 0, 0444, NULL,
         &i2c_proc_real, &i2c_sysctl_real, NULL, &pca954x_pe_busses},
	{PCA954X_SYSCTL_DEBUG, "debug", NULL, 0, 0644, NULL,
         &i2c_proc_real, &i2c_sysctl_real, NULL, &pca954x_pe_debug},
	{PCA954X_SYSCTL_REGVAL, "regval", NULL, 0, 0444, NULL,
         &i2c_proc_real, &i2c_sysctl_real, NULL, &pca954x_pe_regval},
	{PCA954X_SYSCTL_SELBUS, "selbus", NULL, 0, 0644, NULL,
         &i2c_proc_real, &i2c_sysctl_real, NULL, &pca954x_pe_selbus},
	{PCA954X_SYSCTL_SELMASK, "selmask", NULL, 0, 0644, NULL,
         &i2c_proc_real, &i2c_sysctl_real, NULL, &pca954x_pe_selmask},
	{PCA954X_SYSCTL_TYPE, "type", NULL, 0, 0444, NULL,
         &i2c_proc_real, &i2c_sysctl_real, NULL, &pca954x_pe_type},
	{0}
};

static int pca954x_id = 0;

static int __init pca954x_init(void)
{
	printk("pca954x.o version %s (%s)\n", LM_VERSION, LM_DATE);

        /* Because we've set I2C_DF_NOTIFY in our driver struct,
         * this call results in calling pca954x_attach_adapter() for
         * every adapter currently known, both real and virtual.
         */
	return i2c_add_driver(&pca954x_driver);
}

static void __exit pca954x_exit(void)
{
	i2c_del_driver(&pca954x_driver);
}

/* This function is called whenever a new bus/adapter is added, OR when
   the driver itself is added (for all existing adapters), to see if
   the driver can find a chip address it's responsible for. 
 */
static int pca954x_attach_adapter(struct i2c_adapter *adapter)
{
	DBG(printk(KERN_DEBUG "%s: %0lX\n",
               __FUNCTION__, (unsigned long)adapter));

	/* Apply standard lookup via parameters or probing.
           "addr_data" is defined by the SENSORS_INSMOD_n macro.
         */
	return i2c_detect(adapter, &addr_data, pca954x_detect);
}

/* This function is called by i2c_detect and must be of type
   "i2c_found_addr_proc" as defined in i2c-proc.h.
   For the PCA954x series this should only happen due to "force" parameters
   given to insmod/modprobe, as there is no way to autodetect what flavor
   of chip we have (and the differences matter!)

   Thus, we should never get a "kind" of -1.

   Note the unique potential for infinite recursion here because we are
   creating new adapters (one for each muxed virtual bus).  This will cause
   our attach_adapter() function to be called twice for each new adapter,
   because:

   (1) The call to i2c_add_adapter results in calling the attach_adapter()
	function of all known chip drivers, including this one.

   (2) Because normally this is first called via i2c_add_driver calling
	i2c_add_adapter, each new virtual bus/adapter created here will create
        a new entry in i2c-core's master adapter list at a point that hasn't
        yet been reached by i2c_add_driver's scan, and thus i2c_add_driver
        will cause yet another call to our attach_adapter for each of these
        new bus/adapters.

   Each call to our attach_adapter() causes another call to i2c_detect,
   which would detect the mux all over again if an actual probe was made.

   This is prevented in two ways.  The first line of defense is
   i2c_check_addr() which is called by both i2c-proc.c:i2c_detect()
   and i2c-core.c:i2c_probe() to make sure a bus/addr pair has not already
   been seen; that function now knows how to verify this on all parent
   busses as well.

   The second line of defense is to never put anything in the "normal_i2c"
   or "normal_i2c_range" tables that would induce i2c_detect() to probe
   a PCA954x address.
 */
int pca954x_detect(struct i2c_adapter *adapter, int address,
		   unsigned short flags, int kind)
{
	int i, n;
	struct i2c_client *client;
	struct pca954x_data *data;
	const char *type_name = "";
	int err = 0;

	DBG(printk(KERN_DEBUG "%s: %0lX %0x %0x %d\n",
               __FUNCTION__, (unsigned long)adapter,
               address, flags, kind));

        /* XXX: Remove this after verifying that no more recursion
           is happening!
         */
        if (adapter->algo->id == I2C_ALGO_VIRT) {

            printk(KERN_ERR "%s: Avoiding recursion! %0lX %0x %0x %d\n",
                   __FUNCTION__, (unsigned long)adapter,
                   address, flags, kind);
            goto ERROR0;
        }

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE))
		goto ERROR0;

	/* OK. For now, we presume we have a valid client. We now create the
	   client structure, even though we cannot fill it completely yet. */
	if (!(data = kmalloc(sizeof(struct pca954x_data), GFP_KERNEL))) {
		err = -ENOMEM;
		goto ERROR0;
	}
        memset(data, 0, sizeof(struct pca954x_data));

	client = &data->client;
	client->addr = address;
	client->data = data;
	client->adapter = adapter;
	client->driver = &pca954x_driver;
	client->flags = 0;

	/* The detection is very weak.  In fact, we have no way of
         * telling what kind of mux/swi is there, and the differences matter,
         * so if asked to figure it out we just complain instead.
         */
	if (kind < 0) {
		printk(KERN_ERR "%s: Attempted ill-advised probe at addr %0x",
                       __FUNCTION__, address);
		goto ERROR1;
	}

        /* Read the mux register at addr.  This does two things: it verifies
           that the mux is in fact present, and fetches its current
           contents for possible use with a future deselect algorithm.
        */
        if ((i = i2c_smbus_read_byte(client)) < 0) {
		printk(KERN_WARNING
                       "i2c-%d: pca954x.o failed to read reg at %0x",
                       i2c_adapter_id(adapter), address);
                goto ERROR1;
        }
        data->origval = i;

        if (kind == any_chip) {
		printk(KERN_WARNING
                       "i2c-%d: pca954x.o needs advice on chip type -"
                       " wildly guessing %0x is a PCA9540",
                       i2c_adapter_id(adapter), address);
                kind = pca9540;	/* Make "any" default to PCA9540 */
        }

        /* Look up in table */
        for (i = sizeof(pca954x_chipdefs)/sizeof(pca954x_chipdefs[0]);
             --i >= 0;) {
            if (pca954x_chipdefs[i].type == kind)
                break;
        }
        if (i < 0) {
		printk(KERN_ERR "%s: Internal error: unknown kind (%d)",
                       __FUNCTION__, kind);
		goto ERROR1;
        }
        data->type = kind;
        type_name     = pca954x_chipdefs[i].type_name;
        data->pcanum  = pca954x_chipdefs[i].pcanum;
        data->nchans  = pca954x_chipdefs[i].nchans;
        data->muxtype = pca954x_chipdefs[i].muxtype;
        data->nchmask = (1 << data->nchans) - 1;

        /* Now that we know the mux/swi type, we can analyze the
           pca954x_selmask parameter, if one was specified.

           XXX: Not yet.  Still uncertain whether there is a good way
           to initialize selmask and still continue to detect other chips
           correctly on the right busses, or whether the original reg
           setting (origval) should be retained in case BIOS depends on it.
         */

	/* Fill in remaining client fields and put it into the global list */
	snprintf(client->name, sizeof(client->name),
                 "PCA%d I2C %d-chan %s chip",
                 data->pcanum, data->nchans,
                 (data->muxtype == pca954x_ismux ? "mux" : "switch"));
	client->id = pca954x_id++;
	init_MUTEX(&data->update_lock);

	/* Tell the I2C layer a new client has arrived */
	if ((err = i2c_attach_client(client)))
		goto ERROR1;

	/* Register new /proc directory entries with module sensors */
	if ((i = i2c_register_entry(client, type_name,
				    pca954x_dir_table_template,
				    THIS_MODULE)) < 0) {
		err = i;
		goto ERROR2;
	}
	data->sysctl_id = i;

	/* Initialize the PCA954X chip.
           For now, force unselected.  This had better happen BEFORE any
           other sensors modules are loaded, or their chip addresses will
           stop working if they were actually behind the mux chip.
         */
        pca954x_set_selmask(client, 0);

	/* Now create virtual busses and adapters for them */

#ifdef I2C_REQUIRE_ARBITRATION
        /* This doesn't actually do anything yet; kept as a
           placeholder from the original i2c-virtual_cb.c
         */
	adapter->algo->acquire_exclusive = &acquire_shared_bus;
	adapter->algo->release_exclusive = &release_shared_bus;
#endif

	DBG(printk(KERN_DEBUG "%s: creating %d virt busses\n",
               __FUNCTION__, data->nchans));

        for (i = 0, n = 0; i < data->nchans; ++i) {
            	struct i2c_adapter *virt;
                virt = i2c_virt_create_adapter(adapter, client, i,
                                               &pca954x_select_mux,
                                               &pca954x_deselect_mux);
                if ((data->virt_adapters[i] = virt) != NULL)
                    ++n;
        }
	printk("i2c-%d: Registered %d of %d virtual busses for I2C mux %s\n",
               i2c_adapter_id(adapter), n, i, type_name);

	return 0;

      ERROR2:
	i2c_detach_client(client);
      ERROR1:
	kfree(data);
      ERROR0:
	return err;
}

static int pca954x_detach_client(struct i2c_client *client)
{
        struct pca954x_data *data = (struct pca954x_data *)(client->data);
	int i, err;

	DBG(printk(KERN_DEBUG "%s: %0lX\n",
               __FUNCTION__, (unsigned long)client));

        /* Must flush everything attached to our channels!
         */
        for (i = 0; i < data->nchans; ++i) {
            if (data->virt_adapters[i]) {
                err = i2c_virt_remove_adapter(data->virt_adapters[i]);
                if (err != 0) {
                    printk(KERN_ERR "pca954x.o: Bus deregistration failed, "
                           "client not detached.\n");
                    return err;
                }
                data->virt_adapters[i] = NULL;
            }
        }

	i2c_deregister_entry(data->sysctl_id);

	if ((err = i2c_detach_client(client))) {
		printk(KERN_ERR "pca954x.o: Client deregistration failed, "
		       "client not detached.\n");
		return err;
	}

	kfree(client->data);

	return 0;
}


/* Implement /proc "regval" - the actual contents of the chip register.
 * This is READ-ONLY.
 */
void pca954x_pe_regval(struct i2c_client *client, int operation,
                       int ctl_name, int *nrels_mag, long *results)
{
	struct pca954x_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
#if 0	/* XXX: For now, always use cached value */
		static void pca954x_update_client(struct i2c_client *client);
		pca954x_update_client(client);
#endif
                results[0] = data->regval;
		*nrels_mag = 1;
	}
}

/* Implement /proc "debug" - a non-zero value enables debug output, if
 * the code was compiled with it.  This actually applies to all mux chips,
 * not just a single instance, but that's OK.
 */
void pca954x_pe_debug(struct i2c_client *client, int operation,
                      int ctl_name, int *nrels_mag, long *results)
{
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		results[0] = pca954x_debug;
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			pca954x_debug = results[0];
		}
	}
}

/* Implement /proc "busses" - list of (virtual) adapter IDs that constitute
   the busses that this chip connects to.
   This is READ-ONLY.
 */
void pca954x_pe_busses(struct i2c_client *client, int operation,
                       int ctl_name, int *nrels_mag, long *results)
{
	struct pca954x_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		int i, lasti;
                if ((lasti = data->nchans) > *nrels_mag)
			lasti = *nrels_mag;
                for (i = 0; i < lasti; ++i) {
                        results[i] = i2c_adapter_id(data->virt_adapters[i]);
                }
		*nrels_mag = lasti;
	}
}

/* Implement /proc "selmask" - the value specifies the current default
 * channel mask.  Sub-bus 0 == bit 0.
 # A value of 0 means "none"
 */
void pca954x_pe_selmask(struct i2c_client *client, int operation,
                        int ctl_name, int *nrels_mag, long *results)
{
	struct pca954x_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		results[0] = data->selmask;
		*nrels_mag = 1;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
                        pca954x_set_selmask(client, results[0]);
		}
	}
}


/* Implement /proc "selbus" - the list of values is considered to be virtual
   adapter IDs, as an alternative representation of the select mask.
   No values at all means "none".
 */
void pca954x_pe_selbus(struct i2c_client *client, int operation,
                       int ctl_name, int *nrels_mag, long *results)
{
	struct pca954x_data *data = client->data;
        int i, nres;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
                for (i = 0, nres = 0; i < data->nchans; ++i) {
                    if ((data->selmask & (1<<i)) && (nres < *nrels_mag)) {
                        results[nres] = i2c_adapter_id(data->virt_adapters[i]);
                        ++nres;
                    }
                }
		*nrels_mag = nres;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		int newmask = 0;
		for (nres = 0; nres < *nrels_mag; ++nres) {
                    /* Ugly, but shouldn't happen very often */
                    for (i = 0; i < data->nchans; ++i) {
                        if (results[nres]
                            == i2c_adapter_id(data->virt_adapters[i])) {
				newmask |= (1<<i);
                        }
                    }
		}
                pca954x_set_selmask(client, newmask);
	}
}


/* Implement /proc "type" - expose details of the mux type.
   Due to limitations of the sensor package conventions, we can only provide
   an integer array rather than strings:
	[0] = chip number: 9540 for "PCA9540", etc.
        [1] = mux type: 0 for mux, 1 for switch
        [2] = # channels: 2, 4, or 8
 */
void pca954x_pe_type(struct i2c_client *client, int operation,
                        int ctl_name, int *nrels_mag, long *results)
{
	struct pca954x_data *data = client->data;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		if (*nrels_mag >= 3) {
			results[0] = data->pcanum;
			results[1] = data->muxtype;
			results[2] = data->nchans;
                        *nrels_mag = 3;
                }
	}
}


/* Set default select mask.
   Since this can be executed while something else is using the mux or
   one of its sub-busses, the main consideration is making sure we don't
   mess up anything in progress.

   This is accomplished by locking the parent bus that the mux resides on,
   and then calling our internal write function.

   It would not work to just call i2c_smbus_write_byte() because
   by the time that returns, the values we're trying to update
   could already be wrong.  This can happen if something else grabs
   the bus and writes to the mux (eg for doing I/O to a sub-bus
   device), leading to a race condition for updating
   selmask, regval, and curmask.

   It doesn't help to use an update_lock for those values because
   you still have to contend with other threads trying to select
   sub-busses and would encounter deadlocks.

 */
static void pca954x_set_selmask(struct i2c_client *client, int mask)
{
	struct pca954x_data *data = client->data;

	DBG(printk(KERN_DEBUG
                   "%s: Locking bus %d mux %d (%0lX) "
                   "for write of selmask %0x\n",
                   __FUNCTION__, i2c_adapter_id(client->adapter),
                   client->id, (unsigned long)client, mask));

	down(&client->adapter->bus);
	DBG(printk(KERN_DEBUG "%s: Starting write.\n", __FUNCTION__));
	data->selmask = (mask & data->nchmask);
        (void) pca954x_select_chan(client->adapter, client, -1);
	up(&client->adapter->bus);

	DBG(printk(KERN_DEBUG "%s: Unlocked mux (%0lX) for write.\n",
                   __FUNCTION__, (unsigned long)client));
}

#if 0 /* Not needed at the moment; see pca954x_pe_regval() */

/* Read pca954x register into cached value
 */
static void pca954x_update_client(struct i2c_client *client)
{
	struct pca954x_data *data = client->data;

	DBG(printk(KERN_DEBUG
                   "%s: Locking bus %d mux %d (%0lX) for read\n",
                   __FUNCTION__, i2c_adapter_id(client->adapter),
                   client->id, (unsigned long)client));

	down(&client->adapter->bus);
	DBG(printk(KERN_DEBUG "%s: Starting read.\n", __FUNCTION__));
        pca954x_xfer(client->adapter, client, I2C_SMBUS_READ, &data->regval);
        data->curmask = pca954x_val2mask(client, data->regval);
	up(&client->adapter->bus);

	DBG(printk(KERN_DEBUG "%s: Unlocked mux (%0lX) for read.\n",
                   __FUNCTION__, (unsigned long)client));
}
#endif


static u8 pca954x_mask2val(struct i2c_client *client, int mask)
{
	struct pca954x_data *data = (struct pca954x_data *)(client->data);
        int i;

        mask &= data->nchmask;		/* Sanitize mask */
        if (mask == 0)
		return 0;		/* Deselecting all, turn off */
        if (data->muxtype == pca954x_isswi)
		return mask;		/* Switch, can enable all at once! */

        /* Mux, can only pick one.  Select lowest chan bit */
        for (i=0; (mask & (1<<i)) == 0; ++i);
        return PCA954X_MUX_ENA | i;
}

static int pca954x_val2mask(struct i2c_client *client, u8 val)
{
	struct pca954x_data *data = (struct pca954x_data *)(client->data);

        if (data->muxtype == pca954x_isswi)
		return val & data->nchmask;	/* Switch val is == mask */

        /* Mux - mask off low bits to get 0-based chan # as bit shift */
        if (val & PCA954X_MUX_ENA)
                return 1 << (val & (data->nchans-1));
        return 0;
}

/*****************************************************************************
 * All of the following functions should only be called after the parent
 * adapter bus semaphore (adapter->bus) has been acquired, so we cannot
 * call the high-level i2c_smbus_write_byte() function -- it will deadlock
 * attempting to acquire the same bus lock.  Instead do directly what it
 * would do, by means of pca954x_xfer().  Sigh!
 *
 * These functions are allowed to block.
 * 
 *****************************************************************************/


static int pca954x_select_mux(struct i2c_adapter *adap,
                              struct i2c_mux_ctrl *mux)
{
        return pca954x_select_chan(adap, mux->client, mux->value);
}

static int pca954x_deselect_mux(struct i2c_adapter *adap,
                                struct i2c_mux_ctrl *mux) 
{
        return pca954x_select_chan(adap, mux->client, -1);
}

/* Select channel.
 * PCA954x I2C-controlled bus mux/switch chips mainly differ in
 *	whether the chip is a mux or a switch:
 *	Mux: value addressed, only 1 bus can be selected.
 *	Swi: bitmask selected, any/all busses can be selected.
 */
static int pca954x_select_chan(struct i2c_adapter *adap,
                               struct i2c_client *client,
                               unsigned long chan)
{
	struct pca954x_data *data = (struct pca954x_data *)(client->data);
        int maskbit;
        u8 newregval;
        int ret;

        if (chan == -1) {		/* Op: clever deselect (to selmask) */
            if ((data->muxtype == pca954x_ismux)
                && (data->curmask & data->selmask)) {
                /* If mux, and current chan allowed by selmask, do nothing */
                newregval = data->regval;	/* No change */
            } else {
                /* Anything else just reverts to selmask */
                newregval = pca954x_mask2val(client, data->selmask);
            }

        } else {			/* Op: select chan */
            maskbit = 1 << chan;
            if (maskbit & data->curmask) {	/* If already selected, */
                newregval = data->regval;	/* no change */
            } else if ((data->muxtype == pca954x_ismux)
                       || !(maskbit & data->selmask)) {
                newregval = pca954x_mask2val(client, maskbit);
            } else
                newregval = pca954x_mask2val(client, data->selmask);
        }

        /* Only clobber control reg when value changes, to avoid
           unnecessary mux writes.  For safety and initialization,
           ALWAYS force the write if 0 is desired.
        */
        ret = (!newregval || (data->regval != newregval));
        DBG(printk(KERN_DEBUG
                   "%s: Mux %d: chan %d, val %02x - %s\n",
                   __FUNCTION__,
                   client->id, (int)chan, newregval,
                   (ret ? "writing" : "skipping" ) ));
        if (ret) {
		ret = pca954x_xfer(adap, client, I2C_SMBUS_WRITE, &newregval);

                if (ret < 0) {
                    printk(KERN_ERR "%s: I2C mux %d select failed "
                           "(addr=%02x, val=%02x, err=%d)!\n", 
                           __FUNCTION__, client->id,
                           client->addr, newregval, ret);
                } else {
                    data->regval = newregval;
                    data->curmask = pca954x_val2mask(client, newregval);
                }
        }
        return ret;
}


static int pca954x_xfer(struct i2c_adapter *adap,
                        struct i2c_client *client,
                        int read_write, u8 *val)
{
	int ret;

	if (adap->algo->master_xfer) {
		/* Use I2C transfer */
                struct i2c_msg msg;
                char buf[1];

                msg.addr  = client->addr;
                msg.flags = (read_write == I2C_SMBUS_READ ? I2C_M_RD : 0);
                msg.len	  = 1;
                buf[0] 	  = *val;
                msg.buf   = buf;
                ret = adap->algo->master_xfer(adap, &msg, 1);
                if (!ret && (read_write == I2C_SMBUS_READ))
			*val = buf[0];

        } else if (adap->algo->smbus_xfer) {
		/* Use SMBus transfer */
                union i2c_smbus_data data;
                ret = adap->algo->smbus_xfer(adap,
                                             client->addr,
                                             client->flags & I2C_M_TEN,
                                             read_write,
                                             *val,
                                             I2C_SMBUS_BYTE, &data);
                if (!ret && (read_write == I2C_SMBUS_READ))
			*val = data.byte;
        } else {
            printk(KERN_ERR "%s: no smbus_xfer or master_xfer for "
                   "adapter %0lX, client %0lX\n",
                   __FUNCTION__, (unsigned long)adap, (unsigned long)client);
            ret = -1;
        }
        return ret;
}


/* ------------------ */

MODULE_AUTHOR("Ken Harrenstien <klh@google.com>");
MODULE_DESCRIPTION("PCA954X I2C mux/switch driver");

module_init(pca954x_init);
module_exit(pca954x_exit);
