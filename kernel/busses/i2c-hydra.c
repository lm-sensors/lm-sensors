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
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>
#include <linux/init.h>
#include <asm/io.h>
#include <asm/system.h>
#include <asm/param.h>	/* for HZ */
#include "sensors_compat.h"

MODULE_LICENSE("GPL");


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
	pdregr();	/* flush posted write */
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
	pdregr();	/* flush posted write */
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
#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif
}

static void bit_hydra_dec(struct i2c_adapter *adap)
{
#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
}

/* ------------------------------------------------------------------------ */

static struct i2c_algo_bit_data bit_hydra_data = {
	.setsda		= bit_hydra_setsda,
	.setscl		= bit_hydra_setscl,
	.getsda		= bit_hydra_getsda,
	.getscl		= bit_hydra_getscl,
	.udelay		= 5,
	.mdelay		= 5,
	.timeout	= HZ
};

static struct i2c_adapter bit_hydra_ops = {
	.name		= "Hydra i2c",
	.id		= I2C_HW_B_HYDRA,
	.algo_data	= &bit_hydra_data,
	.inc_use	= bit_hydra_inc,
	.dec_use	= bit_hydra_dec,
};

static struct pci_device_id hydra_ids[] __devinitdata = {
	{
		.vendor =	PCI_VENDOR_ID_APPLE,
		.device =	PCI_DEVICE_ID_APPLE_HYDRA,
		.subvendor =	PCI_ANY_ID,
		.subdevice =	PCI_ANY_ID,
	},
	{ 0, }
};

static int __devinit hydra_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	unsigned int base_addr;

	base_addr = dev->resource[0].start;
	hydra_base = (unsigned long) ioremap(base_addr, 0x100);

	pdregw(0);		/* clear SCLK_OE and SDAT_OE */
 	return i2c_bit_add_bus(&bit_hydra_ops);
}

static void __devexit hydra_remove(struct pci_dev *dev)
{
	pdregw(0);	/* clear SCLK_OE and SDAT_OE */
	i2c_bit_del_bus(&bit_hydra_ops);
	iounmap((void *) hydra_base);
}


static struct pci_driver hydra_driver = {
	.name		= "hydra smbus",
	.id_table	= hydra_ids,
	.probe		= hydra_probe,
	.remove		= __devexit_p(hydra_remove),
};

static int __init i2c_hydra_init(void)
{
	return pci_module_init(&hydra_driver);
}


static void __exit i2c_hydra_exit(void)
{
	pci_unregister_driver(&hydra_driver);
}



MODULE_AUTHOR("Geert Uytterhoeven <geert@linux-m68k.org>");
MODULE_DESCRIPTION("i2c for Apple Hydra Mac I/O");

module_init(i2c_hydra_init);
module_exit(i2c_hydra_exit);

