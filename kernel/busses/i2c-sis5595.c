/*
    sis5595.c - Part of lm_sensors, Linux kernel modules for hardware
              monitoring
    Copyright (c) 1998, 1999  Frodo Looijaard <frodol@dds.nl> and
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

/* Note: we assume there can only be one SIS5595 with one SMBus interface */

/*
   Note: all have mfr. ID 0x1039.
   SUPPORTED		PCI ID		
	5595		0008

   Note: these chips contain a 0008 device which is incompatible with the
         5595. We recognize these by the presence of the listed
         "blacklist" PCI ID and refuse to load.

   NOT SUPPORTED	PCI ID		BLACKLIST PCI ID	
	 540		0008		0540
	 550		0008		0550
	5513		0008		5511
	5581		0008		5597
	5582		0008		5597
	5597		0008		5597
	5598		0008		5597/5598
	 630		0008		0630
	 645		0008		0645
	 646		0008		0646
	 648		0008		0648
	 650		0008		0650
	 651		0008		0651
	 730		0008		0730
	 735		0008		0735
	 745		0008		0745
	 746		0008		0746
*/

/* TO DO: 
 * Add Block Transfers (ugly, but supported by the adapter)
 * Add adapter resets
 */

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/stddef.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <asm/io.h>
#include "version.h"
#include "sensors_compat.h"

MODULE_LICENSE("GPL");

static int blacklist[] = {
			PCI_DEVICE_ID_SI_540,
			PCI_DEVICE_ID_SI_550,
			PCI_DEVICE_ID_SI_630,
			PCI_DEVICE_ID_SI_730,
			PCI_DEVICE_ID_SI_5511, /* 5513 chip has the 0008 device but
						  that ID shows up in other chips so we
						  use the 5511 ID for recognition */
			PCI_DEVICE_ID_SI_5597,
			PCI_DEVICE_ID_SI_5598,
			0x645,
			0x646,
			0x648,
			0x650,
			0x651,
			0x735,
			0x745,
			0x746,
			0 };

/* Length of ISA address segment */
#define SIS5595_EXTENT 8
/* SIS5595 SMBus registers */
#define SMB_STS_LO 0x00
#define SMB_STS_HI 0x01
#define SMB_CTL_LO 0x02
#define SMB_CTL_HI 0x03
#define SMB_ADDR   0x04
#define SMB_CMD    0x05
#define SMB_PCNT   0x06
#define SMB_CNT    0x07
#define SMB_BYTE   0x08
#define SMB_DEV    0x10
#define SMB_DB0    0x11
#define SMB_DB1    0x12
#define SMB_HAA    0x13

/* PCI Address Constants */
#define SMB_INDEX  0x38
#define SMB_DAT    0x39
#define SIS5595_ENABLE_REG 0x40
#define ACPI_BASE  0x90

/* Other settings */
#define MAX_TIMEOUT 500

/* SIS5595 constants */
#define SIS5595_QUICK      0x00
#define SIS5595_BYTE       0x02
#define SIS5595_BYTE_DATA  0x04
#define SIS5595_WORD_DATA  0x06
#define SIS5595_PROC_CALL  0x08
#define SIS5595_BLOCK_DATA 0x0A

/* insmod parameters */

/* If force_addr is set to anything different from 0, we forcibly enable
   the device at the given address. */
static int force_addr = 0;
MODULE_PARM(force_addr, "i");
MODULE_PARM_DESC(force_addr,
		 "Initialize the base address of the i2c controller");

static int sis5595_transaction(void);

static unsigned short sis5595_base = 0;

static u8 sis5595_read(u8 reg)
{
	outb(reg, sis5595_base + SMB_INDEX);
	return inb(sis5595_base + SMB_DAT);
}

static void sis5595_write(u8 reg, u8 data)
{
	outb(reg, sis5595_base + SMB_INDEX);
	outb(data, sis5595_base + SMB_DAT);
}


/* Detect whether a SIS5595 can be found, and initialize it, where necessary.
   Note the differences between kernels with the old PCI BIOS interface and
   newer kernels with the real PCI interface. In compat.h some things are
   defined to make the transition easier. */
int sis5595_setup(struct pci_dev *SIS5595_dev)
{
	u16 a;
	u8 val;
	int *i;

	/* Look for imposters */
	for(i = blacklist; *i != 0; i++) {
		if (pci_find_device(PCI_VENDOR_ID_SI, *i, NULL)) {
			printk("i2c-sis5595.o: Error: Looked for SIS5595 but found unsupported device %.4X\n", *i);
			return -ENODEV;
		}
	}

/* Determine the address of the SMBus areas */
	pci_read_config_word(SIS5595_dev, ACPI_BASE, &sis5595_base);
	if(sis5595_base == 0 && force_addr == 0) {
		printk("i2c-sis5595.o: ACPI base address uninitialized - upgrade BIOS or use force_addr=0xaddr\n");
		return -ENODEV;
	}

	if(force_addr)
		sis5595_base = force_addr & ~(SIS5595_EXTENT - 1);
#ifdef DEBUG
	printk("ACPI Base address: %04x\n", sis5595_base);
#endif
	/* NB: We grab just the two SMBus registers here, but this may still
	 * interfere with ACPI :-(  */
	if (check_region(sis5595_base + SMB_INDEX, 2)) {
		printk
		    ("i2c-sis5595.o: SMBus registers 0x%04x-0x%04x already in use!\n",
		     sis5595_base + SMB_INDEX,
		     sis5595_base + SMB_INDEX + 1);
		return -ENODEV;
	}

	if(force_addr) {
		printk("i2c-sis5595.o: forcing ISA address 0x%04X\n", sis5595_base);
		if (PCIBIOS_SUCCESSFUL !=
		    pci_write_config_word(SIS5595_dev, ACPI_BASE, sis5595_base))
			return -ENODEV;
		if (PCIBIOS_SUCCESSFUL !=
		    pci_read_config_word(SIS5595_dev, ACPI_BASE, &a))
			return -ENODEV;
		if ((a & ~(SIS5595_EXTENT - 1)) != sis5595_base) {
			/* doesn't work for some chips! */
			printk("i2c-sis5595.o: force address failed - not supported?\n");
			return -ENODEV;
		}
	}

	if (PCIBIOS_SUCCESSFUL !=
	    pci_read_config_byte(SIS5595_dev, SIS5595_ENABLE_REG, &val))
		return -ENODEV;
	if((val & 0x80) == 0) {
		printk("sis5595.o: enabling ACPI\n");
		if (PCIBIOS_SUCCESSFUL !=
		    pci_write_config_byte(SIS5595_dev, SIS5595_ENABLE_REG,
		                      val | 0x80))
			return -ENODEV;
		if (PCIBIOS_SUCCESSFUL !=
		    pci_read_config_byte(SIS5595_dev, SIS5595_ENABLE_REG, &val))
			return -ENODEV;
		if((val & 0x80) == 0) {	/* doesn't work for some chips? */
			printk("sis5595.o: ACPI enable failed - not supported?\n");
			return -ENODEV;
		}
	}

	/* Everything is happy, let's grab the memory and set things up. */
	request_region(sis5595_base + SMB_INDEX, 2, "sis5595-smbus");
	return(0);
}


/* Another internally used function */
int sis5595_transaction(void)
{
	int temp;
	int result = 0;
	int timeout = 0;

	/* Make sure the SMBus host is ready to start transmitting */
	if (
	    (temp =
	     sis5595_read(SMB_STS_LO) + (sis5595_read(SMB_STS_HI) << 8)) !=
	    0x00) {
#ifdef DEBUG
		printk("i2c-sis5595.o: SMBus busy (%04x). Resetting... \n",
		       temp);
#endif
		sis5595_write(SMB_STS_LO, temp & 0xff);
		sis5595_write(SMB_STS_HI, temp >> 8);
		if (
		    (temp =
		     sis5595_read(SMB_STS_LO) +
		     (sis5595_read(SMB_STS_HI) << 8)) != 0x00) {
#ifdef DEBUG
			printk("i2c-sis5595.o: Failed! (%02x)\n", temp);
#endif
			return -1;
		} else {
#ifdef DEBUG
			printk("i2c-sis5595.o: Successfull!\n");
#endif
		}
	}

	/* start the transaction by setting bit 4 */
	sis5595_write(SMB_CTL_LO, sis5595_read(SMB_CTL_LO) | 0x10);

	/* We will always wait for a fraction of a second! */
	do {
		i2c_delay(1);
		temp = sis5595_read(SMB_STS_LO);
	} while (!(temp & 0x40) && (timeout++ < MAX_TIMEOUT));

	/* If the SMBus is still busy, we give up */
	if (timeout >= MAX_TIMEOUT) {
#ifdef DEBUG
		printk("i2c-sis5595.o: SMBus Timeout!\n");
#endif
		result = -1;
	}

	if (temp & 0x10) {
		result = -1;
#ifdef DEBUG
		printk("i2c-sis5595.o: Error: Failed bus transaction\n");
#endif
	}

	if (temp & 0x20) {
		result = -1;
		printk
		    ("i2c-sis5595.o: Bus collision! SMBus may be locked until next hard\n"
		     "reset (or not...)\n");
		/* Clock stops and slave is stuck in mid-transmission */
	}

	if (
	    (temp =
	     sis5595_read(SMB_STS_LO) + (sis5595_read(SMB_STS_HI) << 8)) !=
	    0x00) {
		sis5595_write(SMB_STS_LO, temp & 0xff);
		sis5595_write(SMB_STS_HI, temp >> 8);
	}

	if (
	    (temp =
	     sis5595_read(SMB_STS_LO) + (sis5595_read(SMB_STS_HI) << 8)) !=
	    0x00) {

#ifdef DEBUG
		printk
		    ("i2c-sis5595.o: Failed reset at end of transaction (%02x)\n",
		     temp);
#endif
	}
	return result;
}

/* Return -1 on error. */
s32 sis5595_access(struct i2c_adapter * adap, u16 addr,
		   unsigned short flags, char read_write,
		   u8 command, int size, union i2c_smbus_data * data)
{
	switch (size) {
	case I2C_SMBUS_QUICK:
		sis5595_write(SMB_ADDR,
			      ((addr & 0x7f) << 1) | (read_write & 0x01));
		size = SIS5595_QUICK;
		break;
	case I2C_SMBUS_BYTE:
		sis5595_write(SMB_ADDR,
			      ((addr & 0x7f) << 1) | (read_write & 0x01));
		if (read_write == I2C_SMBUS_WRITE)
			sis5595_write(SMB_CMD, command);
		size = SIS5595_BYTE;
		break;
	case I2C_SMBUS_BYTE_DATA:
		sis5595_write(SMB_ADDR,
			      ((addr & 0x7f) << 1) | (read_write & 0x01));
		sis5595_write(SMB_CMD, command);
		if (read_write == I2C_SMBUS_WRITE)
			sis5595_write(SMB_BYTE, data->byte);
		size = SIS5595_BYTE_DATA;
		break;
	case I2C_SMBUS_PROC_CALL:
	case I2C_SMBUS_WORD_DATA:
		sis5595_write(SMB_ADDR,
			      ((addr & 0x7f) << 1) | (read_write & 0x01));
		sis5595_write(SMB_CMD, command);
		if (read_write == I2C_SMBUS_WRITE) {
			sis5595_write(SMB_BYTE, data->word & 0xff);
			sis5595_write(SMB_BYTE + 1,
				      (data->word & 0xff00) >> 8);
		}
		size =
		    (size ==
		     I2C_SMBUS_PROC_CALL) ? SIS5595_PROC_CALL :
		    SIS5595_WORD_DATA;
		break;
/*
	case I2C_SMBUS_BLOCK_DATA:
		printk("sis5595.o: Block data not yet implemented!\n");
		return -1;
		break;
*/
	default:
		printk
		    (KERN_WARNING "sis5595.o: Unsupported transaction %d\n", size);
		return -1;
	}

	sis5595_write(SMB_CTL_LO, ((size & 0x0E)));

	if (sis5595_transaction())	/* Error in transaction */
		return -1;

	if ((size != SIS5595_PROC_CALL) &&
	    ((read_write == I2C_SMBUS_WRITE) || (size == SIS5595_QUICK)))
		return 0;


	switch (size) {
	case SIS5595_BYTE:	/* Where is the result put? I assume here it is in
				   SMB_DATA but it might just as well be in the
				   SMB_CMD. No clue in the docs */
	case SIS5595_BYTE_DATA:
		data->byte = sis5595_read(SMB_BYTE);
		break;
	case SIS5595_WORD_DATA:
	case SIS5595_PROC_CALL:
		data->word =
		    sis5595_read(SMB_BYTE) +
		    (sis5595_read(SMB_BYTE + 1) << 8);
		break;
	}
	return 0;
}

static void sis5595_inc(struct i2c_adapter *adapter)
{
#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif
}

static void sis5595_dec(struct i2c_adapter *adapter)
{
#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
}

u32 sis5595_func(struct i2c_adapter *adapter)
{
	return I2C_FUNC_SMBUS_QUICK | I2C_FUNC_SMBUS_BYTE |
	    I2C_FUNC_SMBUS_BYTE_DATA | I2C_FUNC_SMBUS_WORD_DATA |
	    I2C_FUNC_SMBUS_PROC_CALL;
}


static struct i2c_algorithm smbus_algorithm = {
	.name		= "Non-I2C SMBus adapter",
	.id		= I2C_ALGO_SMBUS,
	.smbus_xfer	= sis5595_access,
	.functionality	= sis5595_func,
};

static struct i2c_adapter sis5595_adapter = {
	.name		= "unset",
	.id		= I2C_ALGO_SMBUS | I2C_HW_SMBUS_SIS5595,
	.algo		= &smbus_algorithm,
	.inc_use	= sis5595_inc,
	.dec_use	= sis5595_dec,
};


static struct pci_device_id sis5595_ids[] __devinitdata = {
	{
		.vendor =	PCI_VENDOR_ID_SI,
		.device =	PCI_DEVICE_ID_SI_503,
		.subvendor =	PCI_ANY_ID,
		.subdevice =	PCI_ANY_ID,
	},
	{ 0, }
};

static int __devinit sis5595_probe(struct pci_dev *dev, const struct pci_device_id *id)
{

	if (sis5595_setup(dev)) {
		printk
		    ("i2c-sis5595.o: SIS5595 not detected, module not inserted.\n");

		return -ENODEV;
	}

	sprintf(sis5595_adapter.name, "SMBus SIS5595 adapter at %04x",
		sis5595_base + SMB_INDEX);
	i2c_add_adapter(&sis5595_adapter);

	return 0;
}

static void __devexit sis5595_remove(struct pci_dev *dev)
{
	i2c_del_adapter(&sis5595_adapter);
	release_region(sis5595_base + SMB_INDEX, 2);
}


static struct pci_driver sis5595_driver = {
	.name		= "sis5595 smbus",
	.id_table	= sis5595_ids,
	.probe		= sis5595_probe,
	.remove		= __devexit_p(sis5595_remove),
};

static int __init i2c_sis5595_init(void)
{
	printk("i2c-sis5595.o version %s (%s)\n", LM_VERSION, LM_DATE);
	return pci_module_init(&sis5595_driver);
}


static void __exit i2c_sis5595_exit(void)
{
	pci_unregister_driver(&sis5595_driver);
}



MODULE_AUTHOR("Frodo Looijaard <frodol@dds.nl>");
MODULE_DESCRIPTION("SIS5595 SMBus driver");

module_init(i2c_sis5595_init);
module_exit(i2c_sis5595_exit);
