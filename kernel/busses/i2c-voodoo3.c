/*
    voodoo3.c - Part of lm_sensors, Linux kernel modules for hardware
              monitoring
    Copyright (c) 1998, 1999  Frodo Looijaard <frodol@dds.nl>,
    Philip Edelbrock <phil@netroedge.com>,
    Ralph Metzler <rjkm@thp.uni-koeln.de>, and
    Mark D. Studebaker <mdsxyz123@yahoo.com>
    
    Based on code written by Ralph Metzler <rjkm@thp.uni-koeln.de> and
    Simon Vogl

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

/* This interfaces to the I2C bus of the Voodoo3 to gain access to
    the BT869 and possibly other I2C devices. */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <asm/io.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>
#include "version.h"
#include <linux/init.h>

#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif

/* 3DFX defines */
#ifndef PCI_DEVICE_ID_3DFX_VOODOO3
#define PCI_DEVICE_ID_3DFX_VOODOO3 0x05
#endif
#ifndef PCI_DEVICE_ID_3DFX_BANSHEE
#define PCI_DEVICE_ID_3DFX_BANSHEE 0x03
#endif

/* the only registers we use */
#define REG	0x78
#define REG2 	0x70

/* bit locations in the register */
#define DDC_ENAB	0x00040000
#define DDC_SCL_OUT	0x00080000
#define DDC_SDA_OUT	0x00100000
#define DDC_SCL_IN	0x00200000
#define DDC_SDA_IN	0x00400000
#define I2C_ENAB	0x00800000
#define I2C_SCL_OUT	0x01000000
#define I2C_SDA_OUT	0x02000000
#define I2C_SCL_IN	0x04000000
#define I2C_SDA_IN	0x08000000

/* initialization states */
#define INIT2	0x2
#define INIT3	0x4

/* delays */
#define CYCLE_DELAY	10
#define TIMEOUT		50

#ifdef MODULE
static
#else
extern
#endif
int __init i2c_voodoo3_init(void);
static int __init voodoo3_cleanup(void);
static int voodoo3_setup(void);
static void config_v3(struct pci_dev *dev);
static void voodoo3_inc(struct i2c_adapter *adapter);
static void voodoo3_dec(struct i2c_adapter *adapter);

#ifdef MODULE
extern int init_module(void);
extern int cleanup_module(void);
#endif				/* MODULE */


static int __initdata voodoo3_initialized;
static unsigned char *mem;

extern inline void outlong(unsigned int dat)
{
	*((unsigned int *) (mem + REG)) = dat;
}

extern inline unsigned int readlong(void)
{
	return *((unsigned int *) (mem + REG));
}

/* The voo GPIO registers don't have individual masks for each bit
   so we always have to read before writing. */

static void bit_vooi2c_setscl(void *data, int val)
{
	unsigned int r;
	r = readlong();
	if(val)
		r |= I2C_SCL_OUT;
	else
		r &= ~I2C_SCL_OUT;
	outlong(r);
}

static void bit_vooi2c_setsda(void *data, int val)
{
	unsigned int r;
	r = readlong();
	if(val)
		r |= I2C_SDA_OUT;
	else
		r &= ~I2C_SDA_OUT;
	outlong(r);
}

/* The GPIO pins are open drain, so the pins always remain outputs.
   We rely on the i2c-algo-bit routines to set the pins high before
   reading the input from other chips. */

static int bit_vooi2c_getscl(void *data)
{
	return (0 != (readlong() & I2C_SCL_IN));
}

static int bit_vooi2c_getsda(void *data)
{
	return (0 != (readlong() & I2C_SDA_IN));
}

static void bit_vooddc_setscl(void *data, int val)
{
	unsigned int r;
	r = readlong();
	if(val)
		r |= DDC_SCL_OUT;
	else
		r &= ~DDC_SCL_OUT;
	outlong(r);
}

static void bit_vooddc_setsda(void *data, int val)
{
	unsigned int r;
	r = readlong();
	if(val)
		r |= DDC_SDA_OUT;
	else
		r &= ~DDC_SDA_OUT;
	outlong(r);
}

static int bit_vooddc_getscl(void *data)
{
	return (0 != (readlong() & DDC_SCL_IN));
}

static int bit_vooddc_getsda(void *data)
{
	return (0 != (readlong() & DDC_SDA_IN));
}

static struct i2c_algo_bit_data voo_i2c_bit_data = {
	NULL,
	bit_vooi2c_setsda,
	bit_vooi2c_setscl,
	bit_vooi2c_getsda,
	bit_vooi2c_getscl,
	CYCLE_DELAY, CYCLE_DELAY, TIMEOUT
};

static struct i2c_adapter voodoo3_i2c_adapter = {
	"I2C Voodoo3/Banshee adapter",
	I2C_HW_B_VOO,
	NULL,
	&voo_i2c_bit_data,
	voodoo3_inc,
	voodoo3_dec,
	NULL,
	NULL,
};

static struct i2c_algo_bit_data voo_ddc_bit_data = {
	NULL,
	bit_vooddc_setsda,
	bit_vooddc_setscl,
	bit_vooddc_getsda,
	bit_vooddc_getscl,
	CYCLE_DELAY, CYCLE_DELAY, TIMEOUT
};

static struct i2c_adapter voodoo3_ddc_adapter = {
	"DDC Voodoo3/Banshee adapter",
	I2C_HW_B_VOO,
	NULL,
	&voo_ddc_bit_data,
	voodoo3_inc,
	voodoo3_dec,
	NULL,
	NULL,
};

/* Configures the chip */

void config_v3(struct pci_dev *dev)
{
	unsigned int cadr;

	/* map Voodoo3 memory */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,3,13)
	cadr = dev->resource[0].start;
#else
	cadr = dev->base_address[0];
#endif
	cadr &= PCI_BASE_ADDRESS_MEM_MASK;
	mem = ioremap_nocache(cadr, 0x1000);

	*((unsigned int *) (mem + REG2)) = 0x8160;
	*((unsigned int *) (mem + REG)) = 0xcffc0020;
	printk("i2c-voodoo3: Using Banshee/Voodoo3 at 0x%p\n", mem);
}

/* Detect whether a Voodoo3 or a Banshee can be found,
   and initialize it. */
static int voodoo3_setup(void)
{
	struct pci_dev *dev;
	int v3_num;

	v3_num = 0;

	dev = NULL;
	do {
		if ((dev = pci_find_device(PCI_VENDOR_ID_3DFX,
					   PCI_DEVICE_ID_3DFX_VOODOO3,
					   dev))) {
			if (!v3_num)
				config_v3(dev);
			v3_num++;
		}
	} while (dev);

	dev = NULL;
	do {
		if ((dev = pci_find_device(PCI_VENDOR_ID_3DFX,
					   PCI_DEVICE_ID_3DFX_BANSHEE,
					   dev))) {
			if (!v3_num)
				config_v3(dev);
			v3_num++;
		}
	} while (dev);

	if (v3_num > 0) {
		printk("i2c-voodoo3: %d Banshee/Voodoo3 found.\n", v3_num);
		if (v3_num > 1)
			printk("i2c-voodoo3: warning: only 1 supported.\n");
		return 0;
	} else {
		printk("i2c-voodoo3: No Voodoo3 found.\n");
		return -ENODEV;
	}
}

void voodoo3_inc(struct i2c_adapter *adapter)
{
	MOD_INC_USE_COUNT;
}

void voodoo3_dec(struct i2c_adapter *adapter)
{
	MOD_DEC_USE_COUNT;
}

int __init i2c_voodoo3_init(void)
{
	int res;
	printk("i2c-voodoo3.o version %s (%s)\n", LM_VERSION, LM_DATE);
	voodoo3_initialized = 0;
	if ((res = voodoo3_setup())) {
		printk
		    ("i2c-voodoo3.o: Voodoo3 not detected, module not inserted.\n");
		voodoo3_cleanup();
		return res;
	}
	if ((res = i2c_bit_add_bus(&voodoo3_i2c_adapter))) {
		printk("i2c-voodoo3.o: I2C adapter registration failed\n");
	} else {
		printk("i2c-voodoo3.o: I2C bus initialized\n");
		voodoo3_initialized |= INIT2;
	}
	if ((res = i2c_bit_add_bus(&voodoo3_ddc_adapter))) {
		printk("i2c-voodoo3.o: DDC adapter registration failed\n");
	} else {
		printk("i2c-voodoo3.o: DDC bus initialized\n");
		voodoo3_initialized |= INIT3;
	}
	if(!(voodoo3_initialized & (INIT2 | INIT3))) {
		printk("i2c-voodoo3.o: Both registrations failed, module not inserted\n");
		voodoo3_cleanup();
		return res;
	}
	return 0;
}

int __init voodoo3_cleanup(void)
{
	int res;

	iounmap(mem);
	if (voodoo3_initialized & INIT3) {
		if ((res = i2c_bit_del_bus(&voodoo3_ddc_adapter))) {
			printk
			    ("i2c-voodoo3.o: i2c_bit_del_bus failed, module not removed\n");
			return res;
		}
	}
	if (voodoo3_initialized & INIT2) {
		if ((res = i2c_bit_del_bus(&voodoo3_i2c_adapter))) {
			printk
			    ("i2c-voodoo3.o: i2c_bit_del_bus failed, module not removed\n");
			return res;
		}
	}
	return 0;
}

EXPORT_NO_SYMBOLS;

#ifdef MODULE

MODULE_AUTHOR
    ("Frodo Looijaard <frodol@dds.nl>, Philip Edelbrock <phil@netroedge.com>, Ralph Metzler <rjkm@thp.uni-koeln.de>, and Mark D. Studebaker <mdsxyz123@yahoo.com>");
MODULE_DESCRIPTION("Voodoo3 I2C/SMBus driver");


int init_module(void)
{
	return i2c_voodoo3_init();
}

int cleanup_module(void)
{
	return voodoo3_cleanup();
}

#endif				/* MODULE */
