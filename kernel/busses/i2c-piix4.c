/*
    piix4.c - Part of lm_sensors, Linux kernel modules for hardware
              monitoring
    Copyright (c) 1998 - 2002 Frodo Looijaard <frodol@dds.nl> and
    Philip Edelbrock <phil@netroedge.com>

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
   Supports:
	Intel PIIX4, 440MX
	Serverworks OSB4, CSB5, CSB6
	SMSC Victory66

   Note: we assume there can only be one device, with one SMBus interface.
*/

#include <linux/module.h>
#include <linux/config.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/stddef.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/apm_bios.h>
#include <asm/io.h>
#include "version.h"
#include "sensors_compat.h"


struct sd {
	const unsigned short mfr;
	const unsigned short dev;
	const unsigned char fn;
	const char *name;
};

/* PIIX4 SMBus address offsets */
#define SMBHSTSTS (0 + piix4_smba)
#define SMBHSLVSTS (1 + piix4_smba)
#define SMBHSTCNT (2 + piix4_smba)
#define SMBHSTCMD (3 + piix4_smba)
#define SMBHSTADD (4 + piix4_smba)
#define SMBHSTDAT0 (5 + piix4_smba)
#define SMBHSTDAT1 (6 + piix4_smba)
#define SMBBLKDAT (7 + piix4_smba)
#define SMBSLVCNT (8 + piix4_smba)
#define SMBSHDWCMD (9 + piix4_smba)
#define SMBSLVEVT (0xA + piix4_smba)
#define SMBSLVDAT (0xC + piix4_smba)

/* count for request_region */
#define SMBIOSIZE 8

/* PCI Address Constants */
#define SMBBA     0x090
#define SMBHSTCFG 0x0D2
#define SMBSLVC   0x0D3
#define SMBSHDW1  0x0D4
#define SMBSHDW2  0x0D5
#define SMBREV    0x0D6

/* Other settings */
#define MAX_TIMEOUT 500
#define  ENABLE_INT9 0

/* PIIX4 constants */
#define PIIX4_QUICK      0x00
#define PIIX4_BYTE       0x04
#define PIIX4_BYTE_DATA  0x08
#define PIIX4_WORD_DATA  0x0C
#define PIIX4_BLOCK_DATA 0x14

/* insmod parameters */

/* If force is set to anything different from 0, we forcibly enable the
   PIIX4. DANGEROUS! */
static int force = 0;
MODULE_PARM(force, "i");
MODULE_PARM_DESC(force, "Forcibly enable the PIIX4. DANGEROUS!");

/* If force_addr is set to anything different from 0, we forcibly enable
   the PIIX4 at the given address. VERY DANGEROUS! */
static int force_addr = 0;
MODULE_PARM(force_addr, "i");
MODULE_PARM_DESC(force_addr,
		 "Forcibly enable the PIIX4 at the given address. "
		 "EXTREMELY DANGEROUS!");

static int fix_hstcfg = 0;
MODULE_PARM(fix_hstcfg, "i");
MODULE_PARM_DESC(fix_hstcfg,
		 "Fix config register. Needed on some boards (Force CPCI735).");

static int piix4_transaction(void);

static unsigned short piix4_smba = 0;

#ifdef CONFIG_X86
/*
 * Get DMI information.
 */

static int __devinit ibm_dmi_probe(void)
{
	extern int is_unsafe_smbus;
	return is_unsafe_smbus;
}
#endif

/* Detect whether a PIIX4 can be found, and initialize it, where necessary.
   Note the differences between kernels with the old PCI BIOS interface and
   newer kernels with the real PCI interface. In compat.h some things are
   defined to make the transition easier. */
static int __devinit piix4_setup(struct pci_dev *PIIX4_dev,
				const struct pci_device_id *id)
{
	unsigned char temp;

	/* match up the function */
	if (PCI_FUNC(PIIX4_dev->devfn) != id->driver_data)
		return -ENODEV;

	printk(KERN_INFO "Found %s device\n", PIIX4_dev->name);

#ifdef CONFIG_X86
	if(ibm_dmi_probe() && PIIX4_dev->vendor == PCI_VENDOR_ID_INTEL) {
		printk(KERN_ERR "i2c-piix4.o: IBM Laptop detected; this module "
			"may corrupt your serial eeprom! Refusing to load "
			"module!\n");
		return -EPERM;
	}
#endif

	/* Determine the address of the SMBus areas */
	if (force_addr) {
		piix4_smba = force_addr & 0xfff0;
		force = 0;
	} else {
		pci_read_config_word(PIIX4_dev, SMBBA, &piix4_smba);
		piix4_smba &= 0xfff0;
		if(piix4_smba == 0) {
			printk(KERN_ERR "i2c-piix4.o: SMB base address "
				"uninitialized - upgrade BIOS or use "
				"force_addr=0xaddr\n");
			return -ENODEV;
		}
	}

	if (!request_region(piix4_smba, SMBIOSIZE, "piix4-smbus")) {
		printk(KERN_ERR "i2c-piix4.o: SMB region 0x%x already in "
			"use!\n", piix4_smba);
		return -ENODEV;
	}

	pci_read_config_byte(PIIX4_dev, SMBHSTCFG, &temp);

	/* Some BIOS will set up the chipset incorrectly and leave a register
	   in an undefined state (causing I2C to act very strangely). */
	if (temp & 0x02) {
		if (fix_hstcfg) {
			printk(KERN_INFO "i2c-piix4.o: Working around buggy "
				"BIOS (I2C)\n");
			temp &= 0xfd;
			pci_write_config_byte(PIIX4_dev, SMBHSTCFG, temp);
		} else {
			printk(KERN_INFO "i2c-piix4.o: Unusual config register "
				"value\n");
			printk(KERN_INFO "i2c-piix4.o: Try using fix_hstcfg=1 "
				"if you experience problems\n");
		}
	}

	/* If force_addr is set, we program the new address here. Just to make
	   sure, we disable the PIIX4 first. */
	if (force_addr) {
		pci_write_config_byte(PIIX4_dev, SMBHSTCFG, temp & 0xfe);
		pci_write_config_word(PIIX4_dev, SMBBA, piix4_smba);
		pci_write_config_byte(PIIX4_dev, SMBHSTCFG, temp | 0x01);
		printk(KERN_INFO "i2c-piix4.o: WARNING: SMBus interface set to "
			"new address %04x!\n", piix4_smba);
	} else if ((temp & 1) == 0) {
		if (force) {
			/* This should never need to be done, but has been
			 * noted that many Dell machines have the SMBus
			 * interface on the PIIX4 disabled!? NOTE: This assumes
			 * I/O space and other allocations WERE done by the
			 * Bios!  Don't complain if your hardware does weird
			 * things after enabling this. :') Check for Bios
			 * updates before resorting to this.
			 */
			pci_write_config_byte(PIIX4_dev, SMBHSTCFG,
					      temp | 1);
			printk(KERN_NOTICE "i2c-piix4.o: WARNING: SMBus "
				"interface has been FORCEFULLY ENABLED!\n");
		} else {
			printk(KERN_ERR "i2c-piix4.o: Host SMBus controller "
				"not enabled!\n");
			release_region(piix4_smba, SMBIOSIZE);
			piix4_smba = 0;
			return -ENODEV;
		}
	}

#ifdef DEBUG
	if ((temp & 0x0E) == 8)
		printk(KERN_DEBUG "i2c-piix4.o: Using Interrupt 9 for "
			"SMBus.\n");
	else if ((temp & 0x0E) == 0)
		printk(KERN_DEBUG "i2c-piix4.o: Using Interrupt SMI# "
			"for SMBus.\n");
	else
		printk(KERN_ERR "i2c-piix4.o: Illegal Interrupt configuration "
			"(or code out of date)!\n");

	pci_read_config_byte(PIIX4_dev, SMBREV, &temp);
	printk(KERN_DEBUG "i2c-piix4.o: SMBREV = 0x%X\n", temp);
	printk(KERN_DEBUG "i2c-piix4.o: SMBA = 0x%X\n", piix4_smba);
#endif				/* DEBUG */

	return 0;
}


/* Another internally used function */
int piix4_transaction(void)
{
	int temp;
	int result = 0;
	int timeout = 0;

#ifdef DEBUG
	printk
	    (KERN_DEBUG "i2c-piix4.o: Transaction (pre): CNT=%02x, CMD=%02x, ADD=%02x, DAT0=%02x, "
	     "DAT1=%02x\n", inb_p(SMBHSTCNT), inb_p(SMBHSTCMD),
	     inb_p(SMBHSTADD), inb_p(SMBHSTDAT0), inb_p(SMBHSTDAT1));
#endif

	/* Make sure the SMBus host is ready to start transmitting */
	if ((temp = inb_p(SMBHSTSTS)) != 0x00) {
#ifdef DEBUG
		printk(KERN_DEBUG "i2c-piix4.o: SMBus busy (%02x). Resetting... \n",
		       temp);
#endif
		outb_p(temp, SMBHSTSTS);
		if ((temp = inb_p(SMBHSTSTS)) != 0x00) {
#ifdef DEBUG
			printk(KERN_ERR "i2c-piix4.o: Failed! (%02x)\n", temp);
#endif
			return -1;
		} else {
#ifdef DEBUG
			printk(KERN_DEBUG "i2c-piix4.o: Successfull!\n");
#endif
		}
	}

	/* start the transaction by setting bit 6 */
	outb_p(inb(SMBHSTCNT) | 0x040, SMBHSTCNT);

	/* We will always wait for a fraction of a second! (See PIIX4 docs errata) */
	do {
		i2c_delay(1);
		temp = inb_p(SMBHSTSTS);
	} while ((temp & 0x01) && (timeout++ < MAX_TIMEOUT));

#ifdef DEBUG
	/* If the SMBus is still busy, we give up */
	if (timeout >= MAX_TIMEOUT) {
		printk(KERN_ERR "i2c-piix4.o: SMBus Timeout!\n");
		result = -1;
	}
#endif

	if (temp & 0x10) {
		result = -1;
#ifdef DEBUG
		printk(KERN_ERR "i2c-piix4.o: Error: Failed bus transaction\n");
#endif
	}

	if (temp & 0x08) {
		result = -1;
		printk
		    (KERN_ERR "i2c-piix4.o: Bus collision! SMBus may be locked until next hard\n"
		     "reset. (sorry!)\n");
		/* Clock stops and slave is stuck in mid-transmission */
	}

	if (temp & 0x04) {
		result = -1;
#ifdef DEBUG
		printk(KERN_ERR "i2c-piix4.o: Error: no response!\n");
#endif
	}

	if (inb_p(SMBHSTSTS) != 0x00)
		outb_p(inb(SMBHSTSTS), SMBHSTSTS);

#ifdef DEBUG
	if ((temp = inb_p(SMBHSTSTS)) != 0x00) {
		printk
		    (KERN_ERR "i2c-piix4.o: Failed reset at end of transaction (%02x)\n",
		     temp);
	}
	printk
	    (KERN_DEBUG "i2c-piix4.o: Transaction (post): CNT=%02x, CMD=%02x, ADD=%02x, "
	     "DAT0=%02x, DAT1=%02x\n", inb_p(SMBHSTCNT), inb_p(SMBHSTCMD),
	     inb_p(SMBHSTADD), inb_p(SMBHSTDAT0), inb_p(SMBHSTDAT1));
#endif
	return result;
}

/* Return -1 on error. */
s32 piix4_access(struct i2c_adapter * adap, u16 addr,
		 unsigned short flags, char read_write,
		 u8 command, int size, union i2c_smbus_data * data)
{
	int i, len;

	switch (size) {
	case I2C_SMBUS_QUICK:
		outb_p(((addr & 0x7f) << 1) | (read_write & 0x01),
		       SMBHSTADD);
		size = PIIX4_QUICK;
		break;
	case I2C_SMBUS_BYTE:
		outb_p(((addr & 0x7f) << 1) | (read_write & 0x01),
		       SMBHSTADD);
		if (read_write == I2C_SMBUS_WRITE)
			outb_p(command, SMBHSTCMD);
		size = PIIX4_BYTE;
		break;
	case I2C_SMBUS_BYTE_DATA:
		outb_p(((addr & 0x7f) << 1) | (read_write & 0x01),
		       SMBHSTADD);
		outb_p(command, SMBHSTCMD);
		if (read_write == I2C_SMBUS_WRITE)
			outb_p(data->byte, SMBHSTDAT0);
		size = PIIX4_BYTE_DATA;
		break;
	case I2C_SMBUS_WORD_DATA:
		outb_p(((addr & 0x7f) << 1) | (read_write & 0x01),
		       SMBHSTADD);
		outb_p(command, SMBHSTCMD);
		if (read_write == I2C_SMBUS_WRITE) {
			outb_p(data->word & 0xff, SMBHSTDAT0);
			outb_p((data->word & 0xff00) >> 8, SMBHSTDAT1);
		}
		size = PIIX4_WORD_DATA;
		break;
	case I2C_SMBUS_BLOCK_DATA:
		outb_p(((addr & 0x7f) << 1) | (read_write & 0x01),
		       SMBHSTADD);
		outb_p(command, SMBHSTCMD);
		if (read_write == I2C_SMBUS_WRITE) {
			len = data->block[0];
			if (len < 0)
				len = 0;
			if (len > 32)
				len = 32;
			outb_p(len, SMBHSTDAT0);
			i = inb_p(SMBHSTCNT);	/* Reset SMBBLKDAT */
			for (i = 1; i <= len; i++)
				outb_p(data->block[i], SMBBLKDAT);
		}
		size = PIIX4_BLOCK_DATA;
		break;
	default:
		printk
		    (KERN_WARNING "i2c-piix4.o: Unsupported transaction %d\n", size);
		return -1;
	}

	outb_p((size & 0x1C) + (ENABLE_INT9 & 1), SMBHSTCNT);

	if (piix4_transaction())	/* Error in transaction */
		return -1;

	if ((read_write == I2C_SMBUS_WRITE) || (size == PIIX4_QUICK))
		return 0;


	switch (size) {
	case PIIX4_BYTE:	/* Where is the result put? I assume here it is in
				   SMBHSTDAT0 but it might just as well be in the
				   SMBHSTCMD. No clue in the docs */

		data->byte = inb_p(SMBHSTDAT0);
		break;
	case PIIX4_BYTE_DATA:
		data->byte = inb_p(SMBHSTDAT0);
		break;
	case PIIX4_WORD_DATA:
		data->word = inb_p(SMBHSTDAT0) + (inb_p(SMBHSTDAT1) << 8);
		break;
	case PIIX4_BLOCK_DATA:
		data->block[0] = inb_p(SMBHSTDAT0);
		i = inb_p(SMBHSTCNT);	/* Reset SMBBLKDAT */
		for (i = 1; i <= data->block[0]; i++)
			data->block[i] = inb_p(SMBBLKDAT);
		break;
	}
	return 0;
}

static void piix4_inc(struct i2c_adapter *adapter)
{
#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif
}

static void piix4_dec(struct i2c_adapter *adapter)
{
#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
}

u32 piix4_func(struct i2c_adapter *adapter)
{
	return I2C_FUNC_SMBUS_QUICK | I2C_FUNC_SMBUS_BYTE |
	    I2C_FUNC_SMBUS_BYTE_DATA | I2C_FUNC_SMBUS_WORD_DATA |
	    I2C_FUNC_SMBUS_BLOCK_DATA;
}

static struct i2c_algorithm smbus_algorithm = {
	.name		= "Non-I2C SMBus adapter",
	.id		= I2C_ALGO_SMBUS,
	.smbus_xfer	= piix4_access,
	.functionality	= piix4_func,
};

static struct i2c_adapter piix4_adapter = {
	.name		= "unset",
	.id		= I2C_ALGO_SMBUS | I2C_HW_SMBUS_PIIX4,
	.algo		= &smbus_algorithm,
	.inc_use	= piix4_inc,
	.dec_use	= piix4_dec,
};

#ifndef PCI_DEVICE_ID_SERVERWORKS_CSB6
#define PCI_DEVICE_ID_SERVERWORKS_CSB6 0x0203
#endif

static struct pci_device_id piix4_ids[] __devinitdata = {
	{
		.vendor =	PCI_VENDOR_ID_INTEL,
		.device =	PCI_DEVICE_ID_INTEL_82371AB_3,
		.subvendor =	PCI_ANY_ID,
		.subdevice =	PCI_ANY_ID,
		.driver_data =	3
	},
	{
		.vendor =	PCI_VENDOR_ID_SERVERWORKS,
		.device =	PCI_DEVICE_ID_SERVERWORKS_OSB4,
		.subvendor =	PCI_ANY_ID,
		.subdevice =	PCI_ANY_ID,
		.driver_data =	0,
	},
	{
		.vendor =	PCI_VENDOR_ID_SERVERWORKS,
		.device =	PCI_DEVICE_ID_SERVERWORKS_CSB5,
		.subvendor =	PCI_ANY_ID,
		.subdevice =	PCI_ANY_ID,
		.driver_data =	0,
	},
	{
		.vendor =	PCI_VENDOR_ID_SERVERWORKS,
		.device =	PCI_DEVICE_ID_SERVERWORKS_CSB6,
		.subvendor =	PCI_ANY_ID,
		.subdevice =	PCI_ANY_ID,
		.driver_data =	0,
	},
	{
		.vendor =	PCI_VENDOR_ID_INTEL,
		.device =	PCI_DEVICE_ID_INTEL_82443MX_3,
		.subvendor =	PCI_ANY_ID,
		.subdevice =	PCI_ANY_ID,
		.driver_data =	3,
	},
	{
		.vendor =	PCI_VENDOR_ID_EFAR,
		.device =	PCI_DEVICE_ID_EFAR_SLC90E66_3,
		.subvendor =	PCI_ANY_ID,
		.subdevice =	PCI_ANY_ID,
		.driver_data =	0,
	},
	{ 0, }
};

static int __devinit piix4_probe(struct pci_dev *dev,
				const struct pci_device_id *id)
{
	int retval;

	retval = piix4_setup(dev, id);
	if (retval)
		return retval;

	sprintf(piix4_adapter.name, "SMBus PIIX4 adapter at %04x",
		piix4_smba);

	if ((retval = i2c_add_adapter(&piix4_adapter))) {
		printk(KERN_ERR "i2c-piix4.o: Couldn't register adapter!\n");
		release_region(piix4_smba, SMBIOSIZE);
		piix4_smba = 0;
	}

	return retval;
}

static void __devexit piix4_remove(struct pci_dev *dev)
{
	if (piix4_smba) {
		i2c_del_adapter(&piix4_adapter);
		release_region(piix4_smba, SMBIOSIZE);
		piix4_smba = 0;
	}
}

static struct pci_driver piix4_driver = {
	.name		= "piix4 smbus",
	.id_table	= piix4_ids,
	.probe		= piix4_probe,
	.remove		= __devexit_p(piix4_remove),
};

static int __init i2c_piix4_init(void)
{
	printk("i2c-piix4.o version %s (%s)\n", LM_VERSION, LM_DATE);
	return pci_module_init(&piix4_driver);
}

static void __exit i2c_piix4_exit(void)
{
	pci_unregister_driver(&piix4_driver);
}

MODULE_AUTHOR("Frodo Looijaard <frodol@dds.nl> and "
		"Philip Edelbrock <phil@netroedge.com>");
MODULE_DESCRIPTION("PIIX4 SMBus driver");
MODULE_LICENSE("GPL");

module_init(i2c_piix4_init);
module_exit(i2c_piix4_exit);
