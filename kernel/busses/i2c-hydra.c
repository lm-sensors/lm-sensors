/*
    i2c-hydra.c - Part of lm_sensors,  Linux kernel modules
                  for hardware monitoring

    i2c Support for the Apple `Hydra' Mac I/O

    Copyright (c) 1999 Geert Uytterhoeven <geert@linux-m68k.org>

    Based on i2c Support for Via Technologies 82C586B South Bridge
    Copyright (c) 1998, 1999 Kyösti Mälkki <kmalkki@cc.hut.fi>

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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <asm/io.h>
#include <asm/system.h>
#include <linux/types.h>

#include "i2c.h"
#include "algo-bit.h"

/* PCI device */
#define VENDOR		PCI_VENDOR_ID_APPLE
#define DEVICE		PCI_DEVICE_ID_APPLE_HYDRA

#define HYDRA_CACHE_PD	0x00000030

#define HYDRA_CPD_PD0	0x00000001	/* CachePD lines */
#define HYDRA_CPD_PD1	0x00000002
#define HYDRA_CPD_PD2	0x00000004
#define HYDRA_CPD_PD3	0x00000008

#define HYDRA_SCLK	HYDRA_CPD_PD0
#define HYDRA_SDAT	HYDRA_CPD_PD1
#define HYDRA_SCLK_OE	0x00000010
#define HYDRA_SDAT_OE	0x00000020

static unsigned long hydra_base;

static inline void pdregw(u32 val)
{
	writel(val, hydra_base+HYDRA_CACHE_PD);
}

static inline u32 pdregr(void)
{
	u32 val = readl(hydra_base+HYDRA_CACHE_PD);
	return val;
}

static void bit_hydra_setscl(void *data, int state)
{
    u32 val = pdregr();
    if (state)
	val &= ~ HYDRA_SCLK_OE;
    else {
	val &= ~HYDRA_SCLK;
	val |= HYDRA_SCLK_OE;
    }
    pdregw(val);
}

static void bit_hydra_setsda(void *data, int state)
{
    u32 val = pdregr();
    if (state)
	val &= ~ HYDRA_SDAT_OE;
    else {
	val &= ~HYDRA_SDAT;
	val |= HYDRA_SDAT_OE;
    }
    pdregw(val);
}

static int bit_hydra_getscl(void *data)
{
	return (pdregr() & HYDRA_SCLK) != 0;
}

static int bit_hydra_getsda(void *data)
{
	return (pdregr() & HYDRA_SDAT) != 0;
}

static int bit_hydra_reg(struct i2c_client *client)
{
	MOD_INC_USE_COUNT;
	return 0;
}

static int bit_hydra_unreg(struct i2c_client *client)
{
	MOD_DEC_USE_COUNT;
	return 0;
}


/* ------------------------------------------------------------------------ */

struct bit_adapter bit_hydra_ops = {
	"Hydra i2c",
	HW_B_HYDRA,
	NULL,
	bit_hydra_setsda,
	bit_hydra_setscl,
	bit_hydra_getsda,
	bit_hydra_getscl,
	bit_hydra_reg,
	bit_hydra_unreg,
	5, 5, 100,	/*waits, timeout */
};


static int find_hydra(void)
{
	struct pci_dev *dev;

	if (!pci_present())
		return -ENODEV;
		
	dev = pci_find_device(VENDOR, DEVICE, NULL);
		
	if (!dev) {
		printk("Hydra not found\n");
		return -ENODEV;
	}

	hydra_base = (unsigned long)ioremap(dev->base_address[0], 0x100);

	return 0;
}

int init_i2c_hydra(void)
{
	if (find_hydra() < 0) {
		printk("Error while reading PCI configuration\n");
		return -ENODEV;
	}

	pdregw(0);	/* clear SCLK_OE and SDAT_OE */

	if (i2c_bit_add_bus(&bit_hydra_ops) == 0) {
		printk("Hydra i2c: Module succesfully loaded\n");
		return 0;
	} else {
		iounmap((void *)hydra_base);
		printk("Hydra i2c: Algo-bit error, couldn't register bus\n");
		return -ENODEV;
	}
}

#ifdef MODULE
MODULE_AUTHOR("Geert Uytterhoeven <geert@linux-m68k.org>");
MODULE_DESCRIPTION("i2c for Apple Hydra Mac I/O");

int init_module(void) 
{
	return init_i2c_hydra();
}

void cleanup_module(void) 
{
	i2c_bit_del_bus(&bit_hydra_ops);
	if (hydra_base) {
		pdregw(0);	/* clear SCLK_OE and SDAT_OE */
		iounmap((void *)hydra_base);
	}
}
#endif
