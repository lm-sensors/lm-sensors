/*
    sis630.c - Part of lm_sensors, Linux kernel modules for hardware
              monitoring

    Copyright (c) 2002 Alexander Malysh <amalysh@web.de>

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
   Changes:
   19.11.2002
	Fixed logic by restoring of Host Master Clock
   24.09.2002
	Fixed typo in sis630_access
	Fixed logical error by restoring of Host Master Clock
   21.09.2002
	Added high_clock module option.If this option is set 
	used Host Master Clock 56KHz (default 14KHz).For now we are save old Host 
	Master Clock and after transaction completed restore (otherwise
	it's confuse BIOS and hung Machine).
   18.09.2002
	Added SIS730 as supported
   24.08.2002
   	Fixed the typo in sis630_access (Thanks to Mark M. Hoffman)
	Changed sis630_transaction. Now it's 2x faster (Thanks to Mark M. Hoffman)
*/

/*
   Status: beta

   Supports:
	SIS 630
	SIS 730

   Note: we assume there can only be one device, with one SMBus interface.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/stddef.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <asm/io.h>
#include "version.h"

struct sd {
	const unsigned short mfr;
	const unsigned short dev;
	const unsigned char fn;
	const char *name;
};

/* SIS630 SMBus registers */
#define SMB_STS     0x80 /* status */
#define SMB_EN      0x81 /* status enable */
#define SMB_CNT     0x82
#define SMBHOST_CNT 0x83
#define SMB_ADDR    0x84
#define SMB_CMD     0x85
#define SMB_PCOUNT  0x86 /* processed count */
#define SMB_COUNT   0x87
#define SMB_BYTE    0x88 /* ~0x8F data byte field */
#define SMBDEV_ADDR 0x90
#define SMB_DB0     0x91
#define SMB_DB1     0x92
#define SMB_SAA     0x93

/* register count for request_region */
#define SIS630_SMB_IOREGION 20

/* PCI address constants */
/* acpi base address register  */
#define SIS630_ACPI_BASE_REG 0x74
/* bios control register */
#define SIS630_BIOS_CTL_REG  0x40

/* Other settings */
#define MAX_TIMEOUT   500

/* SIS630 constants */
#define SIS630_QUICK      0x00
#define SIS630_BYTE       0x01
#define SIS630_BYTE_DATA  0x02
#define SIS630_WORD_DATA  0x03
#define SIS630_PCALL      0x04
#define SIS630_BLOCK_DATA 0x05

/* insmod parameters */

/* If force is set to anything different from 0, we forcibly enable the
   SIS630. DANGEROUS! */
static int force = 0;
static int high_clock = 0;
MODULE_PARM(force, "i");
MODULE_PARM_DESC(force, "Forcibly enable the SIS630. DANGEROUS!");
MODULE_PARM(high_clock, "i");
MODULE_PARM_DESC(high_clock, "Set Host Master Clock to 56KHz (default 14KHz).");

static void sis630_do_pause(unsigned int amount);
static int sis630_transaction(int size);

static u8 sis630_read(u8 reg);
static void sis630_write(u8 reg, u8 data);


static unsigned short acpi_base = 0;


u8 sis630_read(u8 reg) {
	return inb(acpi_base + reg);
}

void sis630_write(u8 reg, u8 data) {
	outb(data, acpi_base + reg);
}

/* Internally used pause function */
void sis630_do_pause(unsigned int amount)
{
	current->state = TASK_INTERRUPTIBLE;
	schedule_timeout(amount);
}

int sis630_transaction(int size) {
        int temp;
        int result = 0;
        int timeout = 0;
	u8 oldclock;

        /*
	  Make sure the SMBus host is ready to start transmitting.
	*/
	if ((temp = sis630_read(SMB_CNT) & 0x03) != 0x00) {
#ifdef DEBUG
                printk(KERN_DEBUG "i2c-sis630.o: SMBus busy (%02x). "
			"Resetting...\n",temp);
#endif
		/* kill smbus transaction */
		sis630_write(SMBHOST_CNT, 0x20);

		if ((temp = sis630_read(SMB_CNT) & 0x03) != 0x00) {
#ifdef DEBUG
                        printk(KERN_DEBUG "i2c-sis630.o: Failed! (%02x)\n",
				temp);
#endif
			return -1;
                } else {
#ifdef DEBUG
                        printk(KERN_DEBUG "i2c-sis630.o: Successfull!\n");
#endif
		}
        }

	/* save old clock, so we can prevent machine to hung */
	oldclock = sis630_read(SMB_CNT);

#ifdef DEBUG
	printk(KERN_DEBUG "i2c-sis630.o: saved clock 0x%02x\n", oldclock);
#endif

	/* disable timeout interrupt , set Host Master Clock to 56KHz if requested */
	if (high_clock > 0)
		sis630_write(SMB_CNT, 0x20);
	else
		sis630_write(SMB_CNT, (oldclock & ~0x40));

	/* clear all sticky bits */
	temp = sis630_read(SMB_STS);
	sis630_write(SMB_STS, temp & 0x1e);

	/* start the transaction by setting bit 4 and size */
	sis630_write(SMBHOST_CNT,0x10 | (size & 0x07));

        /* We will always wait for a fraction of a second! */
        do {
                sis630_do_pause(1);
                temp = sis630_read(SMB_STS);
        } while (!(temp & 0x0e) && (timeout++ < MAX_TIMEOUT));

        /* If the SMBus is still busy, we give up */
        if (timeout >= MAX_TIMEOUT) {
#ifdef DEBUG
                printk(KERN_DEBUG "i2c-sis630.o: SMBus Timeout!\n");
#endif
		result = -1;
        }

        if (temp & 0x02) {
                result = -1;
#ifdef DEBUG
                printk(KERN_DEBUG "i2c-sis630.o: Error: Failed bus "
			"transaction\n");
#endif
	}

        if (temp & 0x04) {
                result = -1;
                printk(KERN_ERR "i2c-sis630.o: Bus collision! "
			"SMBus may be locked until next hard reset (or not...)\n");
		/* 
		   TBD: Datasheet say:
		   	the software should clear this bit and restart SMBUS operation
		*/
        }

        /* clear all status "sticky" bits */
	sis630_write(SMB_STS, temp);

#ifdef DEBUG
	printk(KERN_DEBUG "i2c-sis630.o: SMB_CNT before clock restore 0x%02x\n", sis630_read(SMB_CNT));
#endif

	/*
	* restore old Host Master Clock if high_clock is set
	* and oldclock was not 56KHz
	*/
	if (high_clock > 0 && !(oldclock & 0x20))
		sis630_write(SMB_CNT,(sis630_read(SMB_CNT) & ~0x20));

#ifdef DEBUG
	printk(KERN_DEBUG "i2c-sis630.o: SMB_CNT after clock restore 0x%02x\n", sis630_read(SMB_CNT));
#endif

        return result;
}

/* Return -1 on error. */
s32 sis630_access(struct i2c_adapter * adap, u16 addr,
		   unsigned short flags, char read_write,
		   u8 command, int size, union i2c_smbus_data * data)
{

	switch (size) {
		case I2C_SMBUS_QUICK:
			sis630_write(SMB_ADDR, ((addr & 0x7f) << 1) | (read_write & 0x01));
			size = SIS630_QUICK;
			break;
		case I2C_SMBUS_BYTE:
			sis630_write(SMB_ADDR, ((addr & 0x7f) << 1) | (read_write & 0x01));
			if (read_write == I2C_SMBUS_WRITE)
				sis630_write(SMB_CMD, command);
			size = SIS630_BYTE;
			break;
		case I2C_SMBUS_BYTE_DATA:
			sis630_write(SMB_ADDR, ((addr & 0x7f) << 1) | (read_write & 0x01));
			sis630_write(SMB_CMD, command);
			if (read_write == I2C_SMBUS_WRITE)
				sis630_write(SMB_BYTE, data->byte);
			size = SIS630_BYTE_DATA;
			break;
		case I2C_SMBUS_PROC_CALL:
		case I2C_SMBUS_WORD_DATA:
			sis630_write(SMB_ADDR,((addr & 0x7f) << 1) | (read_write & 0x01));
			sis630_write(SMB_CMD, command);
			if (read_write == I2C_SMBUS_WRITE) {
				sis630_write(SMB_BYTE, data->word & 0xff);
				sis630_write(SMB_BYTE + 1,(data->word & 0xff00) >> 8);
			}
			size = (size == I2C_SMBUS_PROC_CALL ? SIS630_PCALL : SIS630_WORD_DATA);
			break;
		case I2C_SMBUS_BLOCK_DATA:
/* it's not implemented yet, but comming soon */
#if 0
			sis630_write(SMB_ADDR,((addr & 0x7f) << 1) | (read_write & 0x01));
			sis630_write(SMB_CMD, command);
			size = SIS630_BLOCK_DATA;
#endif
		default:
			printk("Unsupported I2C size\n");
			return -1;
			break;
	}


	if (sis630_transaction(size))
		return -1;

        if ((size != SIS630_PCALL) &&
		((read_write == I2C_SMBUS_WRITE) || (size == SIS630_QUICK))) {
                return 0;
	}

	switch(size) {
		case SIS630_BYTE:
		case SIS630_BYTE_DATA:
			data->byte = sis630_read(SMB_BYTE);
			break;
		case SIS630_PCALL:
		case SIS630_WORD_DATA:
			data->word = sis630_read(SMB_BYTE) + (sis630_read(SMB_BYTE + 1) << 8);
			break;
		default:
			return -1;
			break;
	}

	return 0;
}


u32 sis630_func(struct i2c_adapter *adapter) {
	return I2C_FUNC_SMBUS_QUICK | I2C_FUNC_SMBUS_BYTE | I2C_FUNC_SMBUS_BYTE_DATA |
		I2C_FUNC_SMBUS_WORD_DATA | I2C_FUNC_SMBUS_PROC_CALL;
}

int sis630_setup(struct pci_dev *sis630_dev) {
	unsigned char b;

	/*
	   Enable ACPI first , so we can accsess reg 74-75
	   in acpi io space and read acpi base addr
	*/
	if (PCIBIOS_SUCCESSFUL !=
	    pci_read_config_byte(sis630_dev, SIS630_BIOS_CTL_REG,&b)) {
		printk(KERN_ERR "i2c-sis630.o: Error: Can't read bios ctl reg\n");
		return -ENODEV;
	}
	/* if ACPI already anbled , do nothing */
	if (!(b & 0x80) &&
	    PCIBIOS_SUCCESSFUL !=
	    pci_write_config_byte(sis630_dev,SIS630_BIOS_CTL_REG,b|0x80)) {
		printk(KERN_ERR "i2c-sis630.o: Error: Can't enable ACPI\n");
		return -ENODEV;
	}
	/* Determine the ACPI base address */
	if (PCIBIOS_SUCCESSFUL !=
	    pci_read_config_word(sis630_dev,SIS630_ACPI_BASE_REG,&acpi_base)) {
		printk(KERN_ERR "i2c-sis630.o: Error: Can't determine ACPI base address\n");
		return -ENODEV;
	}

#ifdef DEBUG
	printk(KERN_DEBUG "i2c-sis630.o: ACPI base at 0x%04x\n", acpi_base);
#endif

	/* Everything is happy, let's grab the memory and set things up. */
	if (!request_region(acpi_base + SMB_STS, SIS630_SMB_IOREGION, "sis630-smbus")){
		printk(KERN_ERR "i2c-sis630.o: SMBus registers 0x%04x-0x%04x "
			"already in use!\n",acpi_base + SMB_STS, acpi_base + SMB_SAA);
		return -ENODEV;
	}

	return 0;
}


static struct i2c_algorithm smbus_algorithm = {
	.name		= "Non-I2C SMBus adapter",
	.id		= I2C_ALGO_SMBUS,
	.smbus_xfer	= sis630_access,
	.functionality	= sis630_func,
};

static struct i2c_adapter sis630_adapter = {
	.owner		= THIS_MODULE,
	.name		= "unset",
	.id		= I2C_ALGO_SMBUS | I2C_HW_SMBUS_SIS630,
	.algo		= &smbus_algorithm,
};


static struct pci_device_id sis630_ids[] __devinitdata = {
	{
		.vendor =	PCI_VENDOR_ID_SI,
		.device =	PCI_DEVICE_ID_SI_630,
		.subvendor =	PCI_ANY_ID,
		.subdevice =	PCI_ANY_ID,
	},
	{
		.vendor =	PCI_VENDOR_ID_SI,
		.device =	PCI_DEVICE_ID_SI_730,
		.subvendor =	PCI_ANY_ID,
		.subdevice =	PCI_ANY_ID,
	},
	{ 0, }
};

static int __devinit sis630_probe(struct pci_dev *dev, const struct pci_device_id *id)
{

	if (sis630_setup(dev)) {
		printk(KERN_ERR "i2c-sis630.o: SIS630 comp. bus not detected, module not inserted.\n");

		return -ENODEV;
	}

	sprintf(sis630_adapter.name, "SMBus SIS630 adapter at %04x",
		acpi_base + SMB_STS);
	i2c_add_adapter(&sis630_adapter);
}

static void __devexit sis630_remove(struct pci_dev *dev)
{
	i2c_del_adapter(&sis630_adapter);
}


static struct pci_driver sis630_driver = {
	.name		= "sis630 smbus",
	.id_table	= sis630_ids,
	.probe		= sis630_probe,
	.remove		= __devexit_p(sis630_remove),
};

static int __init i2c_sis630_init(void)
{
	printk("i2c-sis630.o version %s (%s)\n", LM_VERSION, LM_DATE);
	return pci_module_init(&sis630_driver);
}


static void __exit i2c_sis630_exit(void)
{
	pci_unregister_driver(&sis630_driver);
	release_region(acpi_base + SMB_STS, SIS630_SMB_IOREGION);
}




MODULE_LICENSE("GPL");

MODULE_AUTHOR("Alexander Malysh <amalysh@web.de>");
MODULE_DESCRIPTION("SIS630 SMBus driver");

module_init(i2c_sis630_init);
module_exit(i2c_sis630_exit);
