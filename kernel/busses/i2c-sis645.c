/*
    sis645.c - Part of lm_sensors, Linux kernel modules for hardware
              monitoring

    Copyright (c) 2003 Mark M. Hoffman <mhoffman@lightlink.com>

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

/*
    This module must be considered BETA unless and until
    the chipset manufacturer releases a datasheet.

    The register definitions are based on the SiS630.
    The method for *finding* the registers is based on trial and error.

    A history of changes to this file is available by anonymous CVS:
    http://www2.lm-sensors.nu/~lm78/download.html
*/

/*   25th March 2004
     Support for Sis655 chipsets added by Ken Healy
*/

/*
    Note: we assume there can only be one SiS645 with one SMBus interface
*/

/* #define DEBUG 1 */

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/stddef.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <asm/io.h>
#include "version.h"
#include "sensors_compat.h"

#define DRV_NAME "i2c-sis645"

/* SiS645DX north bridge (defined in 2.4.21) */
#ifndef PCI_DEVICE_ID_SI_646
#define PCI_DEVICE_ID_SI_646 0x0646
#endif

/* SiS648 north bridge (defined in 2.4.21) */
#ifndef PCI_DEVICE_ID_SI_648
#define PCI_DEVICE_ID_SI_648 0x0648
#endif

/* SiS650 north bridge (defined in 2.4.19) */
#ifndef PCI_DEVICE_ID_SI_650
#define PCI_DEVICE_ID_SI_650 0x0650
#endif

/* SiS651 north bridge (defined in 2.4.21)*/
#ifndef PCI_DEVICE_ID_SI_651
#define PCI_DEVICE_ID_SI_651 0x0651
#endif

/* SiS655 north bridge (defined in 2.4.22)*/
#ifndef PCI_DEVICE_ID_SI_655
#define PCI_DEVICE_ID_SI_655 0x0655
#endif

/* SiS746 north bridge (defined in 2.4.21) */
#ifndef PCI_DEVICE_ID_SI_746
#define PCI_DEVICE_ID_SI_746 0x0746
#endif

/* SiS85C503/5513 (LPC Bridge) */
#ifndef PCI_DEVICE_ID_SI_LPC
#define PCI_DEVICE_ID_SI_LPC 0x0018
#endif

/* SiS961 south bridge */
#ifndef PCI_DEVICE_ID_SI_961
#define PCI_DEVICE_ID_SI_961 0x0961
#endif

/* SiS962 south bridge */
#ifndef PCI_DEVICE_ID_SI_962
#define PCI_DEVICE_ID_SI_962 0x0962
#endif

/* SiS963 south bridge */
#ifndef PCI_DEVICE_ID_SI_963
#define PCI_DEVICE_ID_SI_963 0x0963
#endif

/* SMBus ID */
#ifndef PCI_DEVICE_ID_SI_SMBUS
#define PCI_DEVICE_ID_SI_SMBUS 0x16
#endif

/* base address register in PCI config space */
#define SIS645_BAR 0x04

/* SiS645 SMBus registers */
#define SMB_STS      0x00
#define SMB_EN       0x01
#define SMB_CNT      0x02
#define SMB_HOST_CNT 0x03
#define SMB_ADDR     0x04
#define SMB_CMD      0x05
#define SMB_PCOUNT   0x06
#define SMB_COUNT    0x07
#define SMB_BYTE     0x08
#define SMB_DEV_ADDR 0x10
#define SMB_DB0      0x11
#define SMB_DB1      0x12
#define SMB_SAA      0x13

/* register count for request_region */
#define SMB_IOSIZE 0x20

/* Other settings */
#define MAX_TIMEOUT 500

/* SiS645 SMBus constants */
#define SIS645_QUICK      0x00
#define SIS645_BYTE       0x01
#define SIS645_BYTE_DATA  0x02
#define SIS645_WORD_DATA  0x03
#define SIS645_PROC_CALL  0x04
#define SIS645_BLOCK_DATA 0x05

static struct i2c_adapter sis645_adapter;
static u16 sis645_smbus_base = 0;

static inline u8 sis645_read(u8 reg)
{
	return inb(sis645_smbus_base + reg) ;
}

static inline void sis645_write(u8 reg, u8 data)
{
	outb(data, sis645_smbus_base + reg) ;
}

#ifdef CONFIG_HOTPLUG

/* Turns on SMBus device if it is not; return 0 iff successful
 */
static int __devinit sis645_enable_smbus(struct pci_dev *dev)
{
	u8 val = 0;

	pci_read_config_byte(dev, 0x77, &val);

#ifdef DEBUG
	printk(KERN_DEBUG DRV_NAME ": Config byte was 0x%02x.\n", val);
#endif

	pci_write_config_byte(dev, 0x77, val & ~0x10);

	pci_read_config_byte(dev, 0x77, &val);

	if (val & 0x10) {
#ifdef DEBUG
		printk(KERN_DEBUG DRV_NAME ": Config byte stuck!\n");
#endif
		return -1;
	}

	return 0;
}

/* Builds the basic pci_dev for SiS645 SMBus
 */
static int __devinit sis645_build_dev(struct pci_dev **smbus_dev,
			    struct pci_dev *bridge_dev)
{
	struct pci_dev temp_dev;
	u16 vid = 0, did = 0;
	int ret;

	/* fill in the device structure for search */
	memset(&temp_dev, 0, sizeof(temp_dev));
	temp_dev.bus = bridge_dev->bus;
	temp_dev.sysdata = bridge_dev->bus->sysdata;
	temp_dev.hdr_type = PCI_HEADER_TYPE_NORMAL;

	/* the SMBus device is function 1 on the same unit as the ISA bridge */
	temp_dev.devfn = bridge_dev->devfn + 1;

	/* query to make sure */
	ret = pci_read_config_word(&temp_dev, PCI_VENDOR_ID, &vid);
	if (ret || PCI_VENDOR_ID_SI != vid) {
		printk(KERN_ERR DRV_NAME ": Couldn't find SMBus device!\n");
		return ret;
	}
	temp_dev.vendor = vid;

	ret = pci_read_config_word(&temp_dev, PCI_DEVICE_ID, &did);
	if (ret || PCI_DEVICE_ID_SI_SMBUS != did) {
		printk(KERN_ERR DRV_NAME ": Couldn't find SMBus device!\n");
		return ret;
	}
	temp_dev.device = did;

	/* ok, we've got it... request some memory and finish it off */
	*smbus_dev = kmalloc(sizeof(**smbus_dev), GFP_ATOMIC);
	if (NULL == *smbus_dev) {
		printk(KERN_ERR DRV_NAME ": Out of memory!\n");
		return -ENOMEM;
	}

	**smbus_dev = temp_dev;
	
	ret = pci_setup_device(*smbus_dev);
	if (ret) {
		printk(KERN_ERR DRV_NAME ": pci_setup_device failed (0x%08x)\n",ret);
	}
	return ret;
}

/* See if a SMBus can be found, and enable it if possible.
 */
static int __devinit sis645_hotplug_smbus(void)
{
	int ret;
	struct pci_dev *smbus_dev, *bridge_dev ;

	if ((bridge_dev = pci_find_device(PCI_VENDOR_ID_SI,
			PCI_DEVICE_ID_SI_961, NULL)))
		printk(KERN_INFO DRV_NAME ": Found SiS961 south bridge.\n");

	else if ((bridge_dev = pci_find_device(PCI_VENDOR_ID_SI,
			PCI_DEVICE_ID_SI_962, NULL)))
		printk(KERN_INFO DRV_NAME ": Found SiS962 [MuTIOL Media IO].\n");

	else if ((bridge_dev = pci_find_device(PCI_VENDOR_ID_SI,
			PCI_DEVICE_ID_SI_963, NULL)))
		printk(KERN_INFO DRV_NAME ": Found SiS963 [MuTIOL Media IO].\n");
		
	else if ((bridge_dev = pci_find_device(PCI_VENDOR_ID_SI,
			PCI_DEVICE_ID_SI_503, NULL)) ||
		(bridge_dev = pci_find_device(PCI_VENDOR_ID_SI,
			PCI_DEVICE_ID_SI_LPC, NULL))) {

		printk(KERN_INFO DRV_NAME ": Found SiS south bridge in compatability mode(?)\n");

		/* look for known compatible north bridges */
		if ((NULL == pci_find_device(PCI_VENDOR_ID_SI, 
				PCI_DEVICE_ID_SI_645, NULL))
			&& (NULL == pci_find_device(PCI_VENDOR_ID_SI,
				PCI_DEVICE_ID_SI_646, NULL))
			&& (NULL == pci_find_device(PCI_VENDOR_ID_SI,
				PCI_DEVICE_ID_SI_648, NULL))
			&& (NULL == pci_find_device(PCI_VENDOR_ID_SI,
				PCI_DEVICE_ID_SI_650, NULL))
			&& (NULL == pci_find_device(PCI_VENDOR_ID_SI,
				PCI_DEVICE_ID_SI_651, NULL))
			&& (NULL == pci_find_device(PCI_VENDOR_ID_SI,
				PCI_DEVICE_ID_SI_655, NULL))
			&& (NULL == pci_find_device(PCI_VENDOR_ID_SI,
				PCI_DEVICE_ID_SI_735, NULL))
			&& (NULL == pci_find_device(PCI_VENDOR_ID_SI,
				PCI_DEVICE_ID_SI_745, NULL))
			&& (NULL == pci_find_device(PCI_VENDOR_ID_SI,
				PCI_DEVICE_ID_SI_746, NULL))) {
			printk(KERN_ERR DRV_NAME ": Can't find suitable host bridge!\n");
			return -ENODEV;
		}
	} else {
		printk(KERN_ERR DRV_NAME ": Can't find suitable south bridge!\n");
		return -ENODEV;
	}

	/* if we get this far, we think the smbus device is present */

	if ((ret = sis645_enable_smbus(bridge_dev)))
		return ret;

	if ((ret = sis645_build_dev(&smbus_dev, bridge_dev)))
		return ret;

	if ((ret = pci_enable_device(smbus_dev))) {
		printk(KERN_ERR DRV_NAME ": Can't pci_enable SMBus device!"
			" (0x%08x)\n", ret);
		return ret;
	}

	pci_insert_device(smbus_dev, smbus_dev->bus);

	return 0;
}
#endif /* CONFIG_HOTPLUG */

/* Execute a SMBus transaction.
   int size is from SIS645_QUICK to SIS645_BLOCK_DATA
 */
static int sis645_transaction(int size)
{
	int temp;
	int result = 0;
	int timeout = 0;

	/* Make sure the SMBus host is ready to start transmitting */
	if (((temp = sis645_read(SMB_CNT)) & 0x03) != 0x00) {
#ifdef DEBUG
		printk(KERN_DEBUG DRV_NAME ": SMBus busy (0x%02x). Resetting...\n",
				temp);
#endif

		/* kill the transaction */
		sis645_write(SMB_HOST_CNT, 0x20);

		/* check it again */
		if (((temp = sis645_read(SMB_CNT)) & 0x03) != 0x00) {
#ifdef DEBUG
			printk(KERN_DEBUG DRV_NAME ": Failed! (0x%02x)\n", temp);
#endif
			return -1;
		} else {
#ifdef DEBUG
			printk(KERN_DEBUG DRV_NAME ": Successful!\n");
#endif
		}
	}

	/* Turn off timeout interrupts, set fast host clock */
	sis645_write(SMB_CNT, 0x20);

	/* clear all (sticky) status flags */
	temp = sis645_read(SMB_STS);
	sis645_write(SMB_STS, temp & 0x1e);

	/* start the transaction by setting bit 4 and size bits */
	sis645_write(SMB_HOST_CNT, 0x10 | (size & 0x07));

	/* We will always wait for a fraction of a second! */
	do {
		i2c_delay(1);
		temp = sis645_read(SMB_STS);
	} while (!(temp & 0x0e) && (timeout++ < MAX_TIMEOUT));

	/* If the SMBus is still busy, we give up */
	if (timeout >= MAX_TIMEOUT) {
		printk(KERN_DEBUG DRV_NAME ": SMBus Timeout! (0x%02x)\n",temp);
		result = -1;
	}

	/* device error - probably missing ACK */
	if (temp & 0x02) {
#ifdef DEBUG
		printk(KERN_DEBUG DRV_NAME ": Failed bus transaction!\n");
#endif
		result = -1;
	}

	/* bus collision */
	if (temp & 0x04) {
#ifdef DEBUG
		printk(KERN_DEBUG DRV_NAME ": Bus collision!\n");
#endif
		result = -1;
	}

	/* Finish up by resetting the bus */
	sis645_write(SMB_STS, temp);
	if ((temp = sis645_read(SMB_STS))) {
#ifdef DEBUG
		printk(KERN_DEBUG DRV_NAME ": Failed reset at end of transaction!"
				" (0x%02x)\n", temp);
#endif
	}

	return result;
}

/* Return -1 on error. */
static s32 sis645_access(struct i2c_adapter * adap, u16 addr,
		   unsigned short flags, char read_write,
		   u8 command, int size, union i2c_smbus_data * data)
{

	switch (size) {
	case I2C_SMBUS_QUICK:
		sis645_write(SMB_ADDR, ((addr & 0x7f) << 1) | (read_write & 0x01));
		size = SIS645_QUICK;
		break;

	case I2C_SMBUS_BYTE:
		sis645_write(SMB_ADDR, ((addr & 0x7f) << 1) | (read_write & 0x01));
		if (read_write == I2C_SMBUS_WRITE)
			sis645_write(SMB_CMD, command);
		size = SIS645_BYTE;
		break;

	case I2C_SMBUS_BYTE_DATA:
		sis645_write(SMB_ADDR, ((addr & 0x7f) << 1) | (read_write & 0x01));
		sis645_write(SMB_CMD, command);
		if (read_write == I2C_SMBUS_WRITE)
			sis645_write(SMB_BYTE, data->byte);
		size = SIS645_BYTE_DATA;
		break;

	case I2C_SMBUS_PROC_CALL:
	case I2C_SMBUS_WORD_DATA:
		sis645_write(SMB_ADDR, ((addr & 0x7f) << 1) | (read_write & 0x01));
		sis645_write(SMB_CMD, command);
		if (read_write == I2C_SMBUS_WRITE) {
			sis645_write(SMB_BYTE, data->word & 0xff);
			sis645_write(SMB_BYTE + 1, (data->word & 0xff00) >> 8);
		}
		size = (size == I2C_SMBUS_PROC_CALL ? SIS645_PROC_CALL : SIS645_WORD_DATA);
		break;

	case I2C_SMBUS_BLOCK_DATA:
		/* TO DO: */
		printk(KERN_INFO DRV_NAME ": SMBus block not implemented!\n");
		return -1;
		break;

	default:
		printk(KERN_INFO DRV_NAME ": Unsupported I2C size\n");
		return -1;
		break;
	}

	if (sis645_transaction(size))
		return -1;

	if ((size != SIS645_PROC_CALL) &&
			((read_write == I2C_SMBUS_WRITE) || (size == SIS645_QUICK)))
		return 0;

	switch (size) {
	case SIS645_BYTE:
	case SIS645_BYTE_DATA:
		data->byte = sis645_read(SMB_BYTE);
		break;

	case SIS645_WORD_DATA:
	case SIS645_PROC_CALL:
		data->word = sis645_read(SMB_BYTE) +
				(sis645_read(SMB_BYTE + 1) << 8);
		break;
	}
	return 0;
}

static void sis645_inc(struct i2c_adapter *adapter)
{
#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif
}

static void sis645_dec(struct i2c_adapter *adapter)
{
#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
}

static u32 sis645_func(struct i2c_adapter *adapter)
{
	return I2C_FUNC_SMBUS_QUICK | I2C_FUNC_SMBUS_BYTE |
	    I2C_FUNC_SMBUS_BYTE_DATA | I2C_FUNC_SMBUS_WORD_DATA |
	    I2C_FUNC_SMBUS_PROC_CALL;
}

static struct i2c_algorithm smbus_algorithm = {
	.name		= "Non-I2C SMBus adapter",
	.id		= I2C_ALGO_SMBUS,
	.smbus_xfer	= sis645_access,
	.functionality	= sis645_func,
};

static struct i2c_adapter sis645_adapter = {
	.name		= "unset",
	.id		= I2C_ALGO_SMBUS | I2C_HW_SMBUS_SIS645,
	.algo		= &smbus_algorithm,
	.inc_use	= sis645_inc,
	.dec_use	= sis645_dec,
};

static struct pci_device_id sis645_ids[] __devinitdata = {
	{
		.vendor = PCI_VENDOR_ID_SI,
		.device = PCI_DEVICE_ID_SI_SMBUS,
		.subvendor = PCI_ANY_ID,
		.subdevice = PCI_ANY_ID,
	},
	{ 0, }
};

static int __devinit sis645_probe(struct pci_dev *dev,
				const struct pci_device_id *id)
{
	u16 ww = 0;
	int retval;

	if (sis645_smbus_base) {
		dev_err(dev, "Only one device supported.\n");
		return -EBUSY;
	}

	pci_read_config_word(dev, PCI_CLASS_DEVICE, &ww);
	if (PCI_CLASS_SERIAL_SMBUS != ww) {
		dev_err(dev, "Unsupported device class 0x%04x!\n", ww);
		return -ENODEV;
	}

	sis645_smbus_base = pci_resource_start(dev, SIS645_BAR);
	if (!sis645_smbus_base) {
		dev_err(dev, "SiS645 SMBus base address "
			"not initialized!\n");
		return -EINVAL;
	}
	dev_info(dev, "SiS645 SMBus base address: 0x%04x\n",
			sis645_smbus_base);

	/* Everything is happy, let's grab the memory and set things up. */
	if (!request_region(sis645_smbus_base, SMB_IOSIZE, "sis645-smbus")) {
		dev_err(dev, "SMBus registers 0x%04x-0x%04x "
			"already in use!\n", sis645_smbus_base,
			sis645_smbus_base + SMB_IOSIZE - 1);

		sis645_smbus_base = 0;
		return -EINVAL;
	}

	sprintf(sis645_adapter.name, "SiS645 SMBus adapter at 0x%04x",
			sis645_smbus_base);

	if ((retval = i2c_add_adapter(&sis645_adapter))) {
		dev_err(dev, "Couldn't register adapter!\n");
		release_region(sis645_smbus_base, SMB_IOSIZE);
		sis645_smbus_base = 0;
	}

	return retval;
}

static void __devexit sis645_remove(struct pci_dev *dev)
{
	if (sis645_smbus_base) {
		i2c_del_adapter(&sis645_adapter);
		release_region(sis645_smbus_base, SMB_IOSIZE);
		sis645_smbus_base = 0;
	}
}

static struct pci_driver sis645_driver = {
	.name		= "sis645 smbus",
	.id_table	= sis645_ids,
	.probe		= sis645_probe,
	.remove		= __devexit_p(sis645_remove),
};

static int __init i2c_sis645_init(void)
{
	printk(KERN_INFO DRV_NAME ".o version %s (%s)\n", LM_VERSION, LM_DATE);

	/* if the required device id is not present, try to HOTPLUG it first */
	if (!pci_find_device(PCI_VENDOR_ID_SI, PCI_DEVICE_ID_SI_SMBUS, NULL)) {

		printk(KERN_INFO DRV_NAME ": "
			"Attempting to enable SiS645 SMBus device\n");

#ifdef CONFIG_HOTPLUG
		sis645_hotplug_smbus();
#else
		printk(KERN_INFO DRV_NAME ": "
			"Requires kernel with CONFIG_HOTPLUG, sorry!\n");
#endif
	}

	return pci_module_init(&sis645_driver);
}

static void __exit i2c_sis645_exit(void)
{
	pci_unregister_driver(&sis645_driver);
}

MODULE_AUTHOR("Mark M. Hoffman <mhoffman@lightlink.com>");
MODULE_DESCRIPTION("SiS645 SMBus driver");
MODULE_LICENSE("GPL");

/* Register initialization functions using helper macros */
module_init(i2c_sis645_init);
module_exit(i2c_sis645_exit);
