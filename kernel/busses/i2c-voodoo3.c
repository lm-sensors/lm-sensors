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

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>
#include <linux/init.h>
#include <asm/io.h>
#include "version.h"

MODULE_LICENSE("GPL");

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
#define TIMEOUT		(HZ / 2)


static void config_v3(struct pci_dev *dev);


static unsigned char *mem;

static inline void outlong(unsigned int dat)
{
	*((unsigned int *) (mem + REG)) = dat;
}

static inline unsigned int readlong(void)
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
	readlong();	/* flush posted write */
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
	readlong();	/* flush posted write */
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
	readlong();	/* flush posted write */
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
	readlong();	/* flush posted write */
}

static int bit_vooddc_getscl(void *data)
{
	return (0 != (readlong() & DDC_SCL_IN));
}

static int bit_vooddc_getsda(void *data)
{
	return (0 != (readlong() & DDC_SDA_IN));
}


/* Configures the chip */

void config_v3(struct pci_dev *dev)
{
	unsigned int cadr;

	/* map Voodoo3 memory */
	cadr = dev->resource[0].start;
	cadr &= PCI_BASE_ADDRESS_MEM_MASK;
	mem = ioremap_nocache(cadr, 0x1000);
	if(mem) {
		*((unsigned int *) (mem + REG2)) = 0x8160;
		*((unsigned int *) (mem + REG)) = 0xcffc0020;
		printk("i2c-voodoo3: Using Banshee/Voodoo3 at 0x%p\n", mem);
	}
}

static struct i2c_algo_bit_data voo_i2c_bit_data = {
	.setsda		= bit_vooi2c_setsda,
	.setscl		= bit_vooi2c_setscl,
	.getsda		= bit_vooi2c_getsda,
	.getscl		= bit_vooi2c_getscl,
	.udelay		= CYCLE_DELAY,
	.mdelay		= CYCLE_DELAY,
	.timeout	= TIMEOUT
};

static struct i2c_adapter voodoo3_i2c_adapter = {
	.owner		= THIS_MODULE,
	.name		= "I2C Voodoo3/Banshee adapter",
	.id		= I2C_HW_B_VOO,
	.algo_data		= &voo_i2c_bit_data,
};

static struct i2c_algo_bit_data voo_ddc_bit_data = {
	.setsda		= bit_vooddc_setsda,
	.setscl		= bit_vooddc_setscl,
	.getsda		= bit_vooddc_getsda,
	.getscl		= bit_vooddc_getscl,
	.udelay		= CYCLE_DELAY,
	.mdelay		= CYCLE_DELAY,
	.timeout	= TIMEOUT
};

static struct i2c_adapter voodoo3_ddc_adapter = {
	.owner		= THIS_MODULE,
	.name		= "DDC Voodoo3/Banshee adapter",
	.id		= I2C_HW_B_VOO,
	.algo_data	= &voo_ddc_bit_data,
};


static struct pci_device_id voodoo3_ids[] __devinitdata = {
	{
		.vendor =	PCI_VENDOR_ID_3DFX,
		.device =	PCI_DEVICE_ID_3DFX_VOODOO3,
		.subvendor =	PCI_ANY_ID,
		.subdevice =	PCI_ANY_ID,
	},
	{
		.vendor =	PCI_VENDOR_ID_3DFX,
		.device =	PCI_DEVICE_ID_3DFX_BANSHEE,
		.subvendor =	PCI_ANY_ID,
		.subdevice =	PCI_ANY_ID,
	},
	{ 0, }
};

static int __devinit voodoo3_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	int retval;

	config_v3(dev);
	retval = i2c_bit_add_bus(&voodoo3_i2c_adapter);
	if(retval)
		return retval;
	retval = i2c_bit_add_bus(&voodoo3_ddc_adapter);
	if(retval)
		i2c_bit_del_bus(&voodoo3_i2c_adapter);
	return retval;
}

static void __devexit voodoo3_remove(struct pci_dev *dev)
{
	i2c_bit_del_bus(&voodoo3_i2c_adapter);
 	i2c_bit_del_bus(&voodoo3_ddc_adapter);
}


static struct pci_driver voodoo3_driver = {
	.name		= "voodoo3 smbus",
	.id_table	= voodoo3_ids,
	.probe		= voodoo3_probe,
	.remove		= __devexit_p(voodoo3_remove),
};

static int __init i2c_voodoo3_init(void)
{
	printk("i2c-voodoo3.o version %s (%s)\n", LM_VERSION, LM_DATE);
	return pci_module_init(&voodoo3_driver);
}


static void __exit i2c_voodoo3_exit(void)
{
	pci_unregister_driver(&voodoo3_driver);
	iounmap(mem);
}


MODULE_AUTHOR
    ("Frodo Looijaard <frodol@dds.nl>, Philip Edelbrock <phil@netroedge.com>, Ralph Metzler <rjkm@thp.uni-koeln.de>, and Mark D. Studebaker <mdsxyz123@yahoo.com>");
MODULE_DESCRIPTION("Voodoo3 I2C/SMBus driver");

module_init(i2c_voodoo3_init);
module_exit(i2c_voodoo3_exit);
