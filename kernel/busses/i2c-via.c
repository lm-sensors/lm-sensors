/*
    i2c-via.c - Part of lm_sensors,  Linux kernel modules
                for hardware monitoring

    i2c Support for Via Technologies 82C586B South Bridge

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
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>
#include <linux/init.h>
#include <asm/io.h>
#include <asm/param.h>	/* for HZ */
#include "version.h"
#include "sensors_compat.h"

/* Power management registers */

#define PM_CFG_REVID    0x08	/* silicon revision code */
#define PM_CFG_IOBASE0  0x20
#define PM_CFG_IOBASE1  0x48

#define I2C_DIR		(pm_io_base+0x40)
#define I2C_OUT		(pm_io_base+0x42)
#define I2C_IN		(pm_io_base+0x44)
#define I2C_SCL		0x02	/* clock bit in DIR/OUT/IN register */
#define I2C_SDA		0x04

/* io-region reservation */
#define IOSPACE		0x06
#define IOTEXT		"via-i2c"

static u16 pm_io_base = 0;

/*
   It does not appear from the datasheet that the GPIO pins are
   open drain. So a we set a low value by setting the direction to
   output and a high value by setting the direction to input and
   relying on the required I2C pullup. The data value is initialized
   to 0 in via_init() and never changed.
*/

static void bit_via_setscl(void *data, int state)
{
	outb(state ? inb(I2C_DIR) & ~I2C_SCL : inb(I2C_DIR) | I2C_SCL,
	     I2C_DIR);
}

static void bit_via_setsda(void *data, int state)
{
	outb(state ? inb(I2C_DIR) & ~I2C_SDA : inb(I2C_DIR) | I2C_SDA,
	     I2C_DIR);
}

static int bit_via_getscl(void *data)
{
	return (0 != (inb(I2C_IN) & I2C_SCL));
}

static int bit_via_getsda(void *data)
{
	return (0 != (inb(I2C_IN) & I2C_SDA));
}

static void bit_via_inc(struct i2c_adapter *adapter)
{
#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif
}

static void bit_via_dec(struct i2c_adapter *adapter)
{
#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
}

static struct i2c_algo_bit_data bit_data = {
	.setsda		= bit_via_setsda,
	.setscl		= bit_via_setscl,
	.getsda		= bit_via_getsda,
	.getscl		= bit_via_getscl,
	.udelay		= 5,
	.mdelay		= 5,
	.timeout	= HZ
};

static struct i2c_adapter vt586b_adapter = {
	.name		= "VIA i2c",
	.id		= I2C_HW_B_VIA,
	.algo_data	= &bit_data,
	.inc_use	= bit_via_inc,
	.dec_use	= bit_via_dec,
};


static struct pci_device_id vt586b_ids[] __devinitdata = {
	{ PCI_VENDOR_ID_VIA, PCI_DEVICE_ID_VIA_82C586_3, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
	{ 0, }
};

static int __devinit vt586b_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	u16 base;
	u8 rev;
	int res;

	if (pm_io_base) {
		printk(KERN_ERR "i2c-via.o: Will only support one host\n");
		return -EBUSY;
	}

	pci_read_config_byte(dev, PM_CFG_REVID, &rev);

	switch (rev) {
	case 0x00:
		base = PM_CFG_IOBASE0;
		break;
	case 0x01:
	case 0x10:
		base = PM_CFG_IOBASE1;
		break;

	default:
		base = PM_CFG_IOBASE1;
		/* later revision */
	}

	pci_read_config_word(dev, base, &pm_io_base);
	pm_io_base &= (0xff << 8);

	if (! request_region(I2C_DIR, IOSPACE, IOTEXT)) {
	    printk("i2c-via.o: IO 0x%x-0x%x already in use\n",
		   I2C_DIR, I2C_DIR + IOSPACE);
	    return -EBUSY;
	}
	outb(inb(I2C_DIR) & ~(I2C_SDA | I2C_SCL), I2C_DIR);
	outb(inb(I2C_OUT) & ~(I2C_SDA | I2C_SCL), I2C_OUT);
	
	res = i2c_bit_add_bus(&vt586b_adapter);
	if ( res < 0 ) {
		release_region(I2C_DIR, IOSPACE);
		pm_io_base = 0;
		return res;
	}
	return 0;
}

static void __devexit vt586b_remove(struct pci_dev *dev)
{
	i2c_bit_del_bus(&vt586b_adapter);
	release_region(I2C_DIR, IOSPACE);
	pm_io_base = 0;
}


/* Don't register driver to avoid driver conflicts */
/*
static struct pci_driver vt586b_driver = {
	.name		= "vt586b smbus",
	.id_table	= vt586b_ids,
	.probe		= vt586b_probe,
	.remove		= __devexit_p(vt586b_remove),
};
*/

static int __init i2c_vt586b_init(void)
{
	struct pci_dev *dev;
	const struct pci_device_id *id;

	printk("i2c-via.o version %s (%s)\n", LM_VERSION, LM_DATE);
/*
	return pci_module_init(&vt586b_driver);
*/
	pci_for_each_dev(dev) {
		id = pci_match_device(vt586b_ids, dev);
		if(id)
			if(vt586b_probe(dev, id) >= 0)
				return 0;
	}
	return -ENODEV;
}


static void __exit i2c_vt586b_exit(void)
{
/*
	pci_unregister_driver(&vt586b_driver);
*/
	vt586b_remove(NULL);
}


MODULE_AUTHOR("Kyösti Mälkki <kmalkki@cc.hut.fi>");
MODULE_DESCRIPTION("i2c for Via vt82c586b southbridge");
MODULE_LICENSE("GPL");

module_init(i2c_vt586b_init);
module_exit(i2c_vt586b_exit);
