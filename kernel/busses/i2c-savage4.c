/*
    i2c-savage4.c - Part of lm_sensors, Linux kernel modules for hardware
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

/* This interfaces to the I2C bus of the Savage4 to gain access to
   the BT869 and possibly other I2C devices. The DDC bus is not
   yet supported because its register is not memory-mapped.
   However we leave the DDC code here, commented out, to make
   it easier to add later.
*/

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>
#include <linux/init.h>
#include <asm/io.h>
#include "version.h"

/* 3DFX defines */
/* #define PCI_VENDOR_ID_S3		0x5333 */
#define PCI_CHIP_SAVAGE3D	0x8A20
#define PCI_CHIP_SAVAGE3D_MV	0x8A21
#define PCI_CHIP_SAVAGE4	0x8A22
#define PCI_CHIP_SAVAGE2000	0x9102
#define PCI_CHIP_PROSAVAGE_PM	0x8A25
#define PCI_CHIP_PROSAVAGE_KM	0x8A26
#define PCI_CHIP_SAVAGE_MX_MV	0x8c10
#define PCI_CHIP_SAVAGE_MX	0x8c11
#define PCI_CHIP_SAVAGE_IX_MV	0x8c12
#define PCI_CHIP_SAVAGE_IX	0x8c13

#define REG 0xff20	/* Serial Port 1 Register */

/* bit locations in the register */
//#define DDC_ENAB	0x00040000
//#define DDC_SCL_OUT	0x00080000
//#define DDC_SDA_OUT	0x00100000
//#define DDC_SCL_IN	0x00200000
//#define DDC_SDA_IN	0x00400000
#define I2C_ENAB	0x00000020
#define I2C_SCL_OUT	0x00000001
#define I2C_SDA_OUT	0x00000002
#define I2C_SCL_IN	0x00000008
#define I2C_SDA_IN	0x00000010

/* initialization states */
#define INIT2	0x20
/* #define INIT3	0x4 */

/* delays */
#define CYCLE_DELAY	10
#define TIMEOUT		(HZ / 2)


static void config_s4(struct pci_dev *dev);



static unsigned char *mem;

static inline void outlong(unsigned int dat)
{
	*((unsigned int *) (mem + REG)) = dat;
}

static inline unsigned int readlong(void)
{
	return *((unsigned int *) (mem + REG));
}

/* The sav GPIO registers don't have individual masks for each bit
   so we always have to read before writing. */

static void bit_savi2c_setscl(void *data, int val)
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

static void bit_savi2c_setsda(void *data, int val)
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

static int bit_savi2c_getscl(void *data)
{
	return (0 != (readlong() & I2C_SCL_IN));
}

static int bit_savi2c_getsda(void *data)
{
	return (0 != (readlong() & I2C_SDA_IN));
}

/* Configures the chip */

void config_s4(struct pci_dev *dev)
{
	unsigned int cadr;

	/* map memory */
	cadr = dev->resource[0].start;
	cadr &= PCI_BASE_ADDRESS_MEM_MASK;
	mem = ioremap_nocache(cadr, 0x0080000);
	if(mem) {
//		*((unsigned int *) (mem + REG2)) = 0x8160;
		*((unsigned int *) (mem + REG)) = 0x00000020;
		printk("i2c-savage4: Using Savage4 at 0x%p\n", mem);
	}
}


static struct i2c_algo_bit_data sav_i2c_bit_data = {
	.setsda		= bit_savi2c_setsda,
	.setscl		= bit_savi2c_setscl,
	.getsda		= bit_savi2c_getsda,
	.getscl		= bit_savi2c_getscl,
	.udelay		= CYCLE_DELAY,
	.mdelay		= CYCLE_DELAY,
	.timeout	= TIMEOUT
};

static struct i2c_adapter savage4_i2c_adapter = {
	.owner		= THIS_MODULE,
	.name		= "I2C Savage4 adapter",
	.id		= I2C_HW_B_SAVG,
	.algo_data	= &sav_i2c_bit_data,
};

static struct pci_device_id savage4_ids[] __devinitdata = {
	{
		.vendor =	PCI_VENDOR_ID_S3,
		.device =	PCI_CHIP_SAVAGE4,
		.subvendor =	PCI_ANY_ID,
		.subdevice =	PCI_ANY_ID,
	},
	{
		.vendor =	PCI_VENDOR_ID_S3,
		.device =	PCI_CHIP_SAVAGE2000,
		.subvendor =	PCI_ANY_ID,
		.subdevice =	PCI_ANY_ID,
	},
	{ 0, }
};

static int __devinit savage4_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	config_s4(dev);
	return i2c_bit_add_bus(&savage4_i2c_adapter);
}

static void __devexit savage4_remove(struct pci_dev *dev)
{
	i2c_bit_del_bus(&savage4_i2c_adapter);
}


/* Don't register driver to avoid driver conflicts */
/*
static struct pci_driver savage4_driver = {
	.name		= "savage4 smbus",
	.id_table	= savage4_ids,
	.probe		= savage4_probe,
	.remove		= __devexit_p(savage4_remove),
};
*/

static int __init i2c_savage4_init(void)
{
	struct pci_dev *dev;
	const struct pci_device_id *id;

	printk("i2c-savage4.o version %s (%s)\n", LM_VERSION, LM_DATE);
/*
	return pci_module_init(&savage4_driver);
*/
	pci_for_each_dev(dev) {
		id = pci_match_device(savage4_ids, dev);
		if(id)
			if(savage4_probe(dev, id) >= 0)
				return 0;
	}
	return -ENODEV;
}

static void __exit i2c_savage4_exit(void)
{
/*
	pci_unregister_driver(&savage4_driver);
*/
	savage4_remove(NULL);
	iounmap(mem);
}

MODULE_AUTHOR
    ("Frodo Looijaard <frodol@dds.nl>, Philip Edelbrock <phil@netroedge.com>, Ralph Metzler <rjkm@thp.uni-koeln.de>, and Mark D. Studebaker <mdsxyz123@yahoo.com>");
MODULE_DESCRIPTION("Savage4 I2C/SMBus driver");
MODULE_LICENSE("GPL");

module_init(i2c_savage4_init);
module_exit(i2c_savage4_exit);
