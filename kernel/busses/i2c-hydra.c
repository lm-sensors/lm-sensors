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

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <asm/io.h>
#include <asm/system.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>
#include <linux/init.h>

#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif

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
	writel(val, hydra_base + HYDRA_CACHE_PD);
}

static inline u32 pdregr(void)
{
	u32 val = readl(hydra_base + HYDRA_CACHE_PD);
	return val;
}

static void bit_hydra_setscl(void *data, int state)
{
	u32 val = pdregr();
	if (state)
		val &= ~HYDRA_SCLK_OE;
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
		val &= ~HYDRA_SDAT_OE;
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

static void bit_hydra_inc(struct i2c_adapter *adap)
{
	MOD_INC_USE_COUNT;
}

static void bit_hydra_dec(struct i2c_adapter *adap)
{
	MOD_DEC_USE_COUNT;
}

/* ------------------------------------------------------------------------ */

static struct i2c_algo_bit_data bit_hydra_data = {
	NULL,
	bit_hydra_setsda,
	bit_hydra_setscl,
	bit_hydra_getsda,
	bit_hydra_getscl,
	5, 5, 100,		/*waits, timeout */
};

static struct i2c_adapter bit_hydra_ops = {
	"Hydra i2c",
	I2C_HW_B_HYDRA,
	NULL,
	&bit_hydra_data,
	bit_hydra_inc,
	bit_hydra_dec,
	NULL,
	NULL,
};


static int find_hydra(void)
{
	struct pci_dev *dev;
	unsigned int base_addr;

	if (!pci_present())
		return -ENODEV;

	dev = pci_find_device(VENDOR, DEVICE, NULL);
	if (!dev) {
		printk("Hydra not found\n");
		return -ENODEV;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,3,13)
	base_addr = dev->resource[0].start;
#else
	base_addr = dev->base_address[0];
#endif
	hydra_base = (unsigned long) ioremap(base_addr, 0x100);

	return 0;
}

#ifdef MODULE
static
#else
extern
#endif
int __init i2c_hydra_init(void)
{
	if (find_hydra() < 0) {
		printk("Error while reading PCI configuration\n");
		return -ENODEV;
	}

	pdregw(0);		/* clear SCLK_OE and SDAT_OE */

	if (i2c_bit_add_bus(&bit_hydra_ops) == 0) {
		printk("Hydra i2c: Module succesfully loaded\n");
		return 0;
	} else {
		iounmap((void *) hydra_base);
		printk
		    ("Hydra i2c: Algo-bit error, couldn't register bus\n");
		return -ENODEV;
	}
}

EXPORT_NO_SYMBOLS;

#ifdef MODULE
MODULE_AUTHOR("Geert Uytterhoeven <geert@linux-m68k.org>");
MODULE_DESCRIPTION("i2c for Apple Hydra Mac I/O");

int init_module(void)
{
	return i2c_hydra_init();
}

void cleanup_module(void)
{
	i2c_bit_del_bus(&bit_hydra_ops);
	if (hydra_base) {
		pdregw(0);	/* clear SCLK_OE and SDAT_OE */
		iounmap((void *) hydra_base);
	}
}
#endif
