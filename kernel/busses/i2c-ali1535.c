/*
    i2c-ali1535.c - Part of lm_sensors, Linux kernel modules for hardware
                    monitoring
    Copyright (c) 2000  Frodo Looijaard <frodol@dds.nl>, 
                        Philip Edelbrock <phil@netroedge.com>, 
                        Mark D. Studebaker <mdsxyz123@yahoo.com>,
                        Dan Eaton <dan.eaton@rocketlogix.com> and 
                        Stephen Rousset<stephen.rousset@rocketlogix.com> 

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
    This is the driver for the SMB Host controller on
    Acer Labs Inc. (ALI) M1535 South Bridge.

    The M1535 is a South bridge for portable systems.
    It is very similar to the M15x3 South bridges also produced
    by Acer Labs Inc.  Some of the registers within the part
    have moved and some have been redefined slightly. Additionally,
    the sequencing of the SMBus transactions has been modified
    to be more consistent with the sequencing recommended by
    the manufacturer and observed through testing.  These
    changes are reflected in this driver and can be identified
    by comparing this driver to the i2c-ali15x3 driver.
    For an overview of these chips see http://www.acerlabs.com

    The SMB controller is part of the 7101 device, which is an
    ACPI-compliant Power Management Unit (PMU).

    The whole 7101 device has to be enabled for the SMB to work.
    You can't just enable the SMB alone.
    The SMB and the ACPI have separate I/O spaces.
    We make sure that the SMB is enabled. We leave the ACPI alone.

    This driver controls the SMB Host only.

    This driver does not use interrupts.
*/


/* Note: we assume there can only be one ALI1535, with one SMBus interface */

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/stddef.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <asm/io.h>
#include <asm/semaphore.h>
#include "version.h"
#include "sensors_compat.h"


/* ALI1535 SMBus address offsets */
#define SMBHSTSTS (0 + ali1535_smba)
#define SMBHSTTYP (1 + ali1535_smba)
#define SMBHSTPORT (2 + ali1535_smba)
#define SMBHSTCMD (7 + ali1535_smba)
#define SMBHSTADD (3 + ali1535_smba)
#define SMBHSTDAT0 (4 + ali1535_smba)
#define SMBHSTDAT1 (5 + ali1535_smba)
#define SMBBLKDAT (6 + ali1535_smba)

/* PCI Address Constants */
#define SMBCOM    0x004
#define SMBREV    0x008
#define SMBCFG    0x0D1
#define SMBBA     0x0E2
#define SMBHSTCFG 0x0F0
#define SMBCLK    0x0F2

/* Other settings */
#define MAX_TIMEOUT 500		/* times 1/100 sec */
#define ALI1535_SMB_IOSIZE 32

/* 
*/
#define ALI1535_SMB_DEFAULTBASE 0x8040

/* ALI1535 address lock bits */
#define ALI1535_LOCK	0x06 < dwe >

/* ALI1535 command constants */
#define ALI1535_QUICK      0x00
#define ALI1535_BYTE       0x10
#define ALI1535_BYTE_DATA  0x20
#define ALI1535_WORD_DATA  0x30
#define ALI1535_BLOCK_DATA 0x40
#define ALI1535_I2C_READ   0x60

#define	ALI1535_DEV10B_EN	0x80	/* Enable 10-bit addressing in */
                                        /*  I2C read                   */
#define	ALI1535_T_OUT		0x08	/* Time-out Command (write)    */
#define	ALI1535_A_HIGH_BIT9	0x08	/* Bit 9 of 10-bit address in  */
                                        /* Alert-Response-Address      */
                                        /* (read)                      */
#define	ALI1535_KILL		0x04	/* Kill Command (write)        */
#define	ALI1535_A_HIGH_BIT8	0x04	/* Bit 8 of 10-bit address in  */
                                        /*  Alert-Response-Address     */
                                        /*  (read)                     */

#define	ALI1535_D_HI_MASK	0x03	/* Mask for isolating bits 9-8 */
                                        /*  of 10-bit address in I2C   */ 
                                        /*  Read Command               */

/* ALI1535 status register bits */
#define ALI1535_STS_IDLE	0x04
#define ALI1535_STS_BUSY	0x08	/* host busy */
#define ALI1535_STS_DONE	0x10	/* transaction complete */
#define ALI1535_STS_DEV		0x20	/* device error */
#define ALI1535_STS_BUSERR	0x40	/* bus error    */
#define ALI1535_STS_FAIL	0x80    /* failed bus transaction */
#define ALI1535_STS_ERR		0xE0	/* all the bad error bits */

#define ALI1535_BLOCK_CLR	0x04	/* reset block data index */

/* ALI1535 device address register bits */
#define	ALI1535_RD_ADDR		0x01	/* Read/Write Bit in Device    */
                                        /*  Address field              */
                                        /*  -> Write = 0               */
                                        /*  -> Read  = 1               */
#define	ALI1535_SMBIO_EN	0x04	/* SMB I/O Space enable        */


static unsigned short ali1535_smba = 0;
DECLARE_MUTEX(i2c_ali1535_sem);


/* Detect whether a ALI1535 can be found, and initialize it, where necessary.
   Note the differences between kernels with the old PCI BIOS interface and
   newer kernels with the real PCI interface. In compat.h some things are
   defined to make the transition easier. */
int ali1535_setup(struct pci_dev *ALI1535_dev)
{
	int error_return = 0;
	unsigned char temp;

/* Check the following things:
	- SMB I/O address is initialized
	- Device is enabled
	- We can use the addresses
*/

/* Determine the address of the SMBus area */
	pci_read_config_word(ALI1535_dev, SMBBA, &ali1535_smba);
	ali1535_smba &= (0xffff & ~(ALI1535_SMB_IOSIZE - 1));
	if (ali1535_smba == 0) {
		printk
		    ("i2c-ali1535.o: ALI1535_smb region uninitialized - upgrade BIOS?\n");
		error_return = -ENODEV;
	}

	if (error_return == -ENODEV)
		goto END;

	if (check_region(ali1535_smba, ALI1535_SMB_IOSIZE)) {
		printk
		    ("i2c-ali1535.o: ALI1535_smb region 0x%x already in use!\n",
		     ali1535_smba);
		error_return = -ENODEV;
	}

	if (error_return == -ENODEV)
		goto END;

	/* check if whole device is enabled */
	pci_read_config_byte(ALI1535_dev, SMBCFG, &temp);
	if ((temp & ALI1535_SMBIO_EN) == 0) {
		printk
		    ("i2c-ali1535.o: SMB device not enabled - upgrade BIOS?\n");
		error_return = -ENODEV;
		goto END;
	}

/* Is SMB Host controller enabled? */
	pci_read_config_byte(ALI1535_dev, SMBHSTCFG, &temp);
	if ((temp & 1) == 0) {
		printk
		    ("i2c-ali1535.o: SMBus controller not enabled - upgrade BIOS?\n");
		error_return = -ENODEV;
		goto END;
	}

/* set SMB clock to 74KHz as recommended in data sheet */
	pci_write_config_byte(ALI1535_dev, SMBCLK, 0x20);

	/* Everything is happy, let's grab the memory and set things up. */
	request_region(ali1535_smba, ALI1535_SMB_IOSIZE, "ali1535-smb");

#ifdef DEBUG
/*
  The interrupt routing for SMB is set up in register 0x77 in the
  1533 ISA Bridge device, NOT in the 7101 device.
  Don't bother with finding the 1533 device and reading the register.
  if ((....... & 0x0F) == 1)
     printk("i2c-ali1535.o: ALI1535 using Interrupt 9 for SMBus.\n");
*/
	pci_read_config_byte(ALI1535_dev, SMBREV, &temp);
	printk("i2c-ali1535.o: SMBREV = 0x%X\n", temp);
	printk("i2c-ali1535.o: ALI1535_smba = 0x%X\n", ali1535_smba);
#endif				/* DEBUG */

      END:
	return error_return;
}


#define MAX_TIMEOUT_HEROS		100
#define MAX_TIMEOUT_BLOCK_HEROS	500
#define MAX_TRY_HEROS			3

/* believe it or not this is the delay technique recommended by ALI */
static void ali1535_delay_loop(void)
{
	int i;

	for(i=0;i<30;i++)
		outb_p(0,0xEB);
}

static int ali1535_wait_for_status(int count,int status)
{
	int i;
	int dat;

	for (i = 0; i < count; i++) {
		ali1535_delay_loop();
		dat = inb_p(SMBHSTSTS);
		if (dat == status)
			break;
	}

	return i == count ? 1 : 0;
}

/* Return -1 on error. */
s32 ali1535_access(struct i2c_adapter * adap, u16 addr,
		   unsigned short flags, char read_write, u8 command,
		   int size, union i2c_smbus_data * data)
{
	int i, len;
	s32 result = 0;
	int timeout = 0;
	int oldsize = size;

	down(&i2c_ali1535_sem);

repeat:
	if(timeout++ > MAX_TRY_HEROS) {
		result = -1;
		goto EXIT;
	}

	/* clear status */
	outb_p(0xFF, SMBHSTSTS);

	if (ali1535_wait_for_status(MAX_TIMEOUT_HEROS,ALI1535_STS_IDLE))
		goto repeat;

	outb_p(ALI1535_KILL,SMBHSTTYP);

	if (ali1535_wait_for_status(MAX_TIMEOUT_HEROS,ALI1535_STS_FAIL))
		goto repeat;

	/* clear status */
	outb_p(0xFF, SMBHSTSTS);

	if (ali1535_wait_for_status(MAX_TIMEOUT_HEROS,ALI1535_STS_IDLE))
		goto repeat;

	switch (size) {
	case I2C_SMBUS_QUICK:
		outb_p(((addr & 0x7f) << 1) | (read_write & 0x01), SMBHSTADD);
		ali1535_delay_loop();
		size = ALI1535_QUICK;
		outb_p(size, SMBHSTTYP);	/* output command */
		ali1535_delay_loop();
		break;
	case I2C_SMBUS_BYTE:
		outb_p(((addr & 0x7f) << 1) | (read_write & 0x01), SMBHSTADD);
		ali1535_delay_loop();
		size = ALI1535_BYTE;
		outb_p(size, SMBHSTTYP);	/* output command */
		ali1535_delay_loop();
		if (read_write == I2C_SMBUS_WRITE) {
			outb_p(command, SMBHSTCMD);
			ali1535_delay_loop();
		}
		break;
	case I2C_SMBUS_BYTE_DATA:
		outb_p(((addr & 0x7f) << 1) | (read_write & 0x01), SMBHSTADD);
		ali1535_delay_loop();
		size = ALI1535_BYTE_DATA;
		outb_p(size, SMBHSTTYP);	/* output command */
		ali1535_delay_loop();
		outb_p(command, SMBHSTCMD);
		ali1535_delay_loop();
		if (read_write == I2C_SMBUS_WRITE) {
			outb_p(data->byte, SMBHSTDAT0);
			ali1535_delay_loop();
		}
		break;
	case I2C_SMBUS_WORD_DATA:
		outb_p(((addr & 0x7f) << 1) | (read_write & 0x01), SMBHSTADD);
		ali1535_delay_loop();
		size = ALI1535_WORD_DATA;
		outb_p(size, SMBHSTTYP);	/* output command */
		outb_p(command, SMBHSTCMD);
		ali1535_delay_loop();
		if (read_write == I2C_SMBUS_WRITE) {
			outb_p(data->word & 0xff, SMBHSTDAT0);
			ali1535_delay_loop();
			outb_p((data->word & 0xff00) >> 8, SMBHSTDAT1);
			ali1535_delay_loop();
		}
		break;
	case I2C_SMBUS_BLOCK_DATA:
		outb_p(((addr & 0x7f) << 1) | (read_write & 0x01), SMBHSTADD);
		ali1535_delay_loop();
		size = ALI1535_BLOCK_DATA;
		outb_p(size, SMBHSTTYP);	/* output command */
		outb_p(command, SMBHSTCMD);
		ali1535_delay_loop();
		if (read_write == I2C_SMBUS_WRITE) {
			len = data->block[0];
			if (len < 0) {
				len = 0;
				data->block[0] = len;
			}
			if (len > 32) {
				len = 32;
				data->block[0] = len;
			}
			outb_p(len, SMBHSTDAT0);
			ali1535_delay_loop();
			{
				int val;

				val = inb_p(SMBHSTTYP) | ALI1535_BLOCK_CLR;
				ali1535_delay_loop();
				outb_p(val, SMBHSTTYP);	/* Reset SMBBLKDAT */
				ali1535_delay_loop();
			}
			for (i = 1; i <= len; i++) {
				outb_p(data->block[i], SMBBLKDAT);
				ali1535_delay_loop();
			}
		}
		break;
	default:
		printk
		    (KERN_WARNING "i2c-ali1535.o: Unsupported transaction %d\n", size);
		result = -1;
		goto EXIT;
	}

	/* start the transaction */
	outb_p(0xFF, SMBHSTPORT);
	ali1535_delay_loop();

	if (ali1535_wait_for_status(size == ALI1535_BLOCK_DATA ?
                                            MAX_TIMEOUT_BLOCK_HEROS :
                                            MAX_TIMEOUT_HEROS ,
                                       ALI1535_STS_IDLE | ALI1535_STS_DONE)) {
		size = oldsize;
		goto repeat;
	}

	/* clear status */
	outb_p(0xFF, SMBHSTSTS);
	ali1535_delay_loop();
	
	if(inb_p(SMBHSTSTS) != ALI1535_STS_IDLE) {
		size = oldsize;
		goto repeat;
	}

	if ((read_write == I2C_SMBUS_WRITE) || (size == ALI1535_QUICK)) {
		result = 0;
		goto EXIT;
        }

	switch (size) {
	case ALI1535_BYTE:	/* Result put in SMBHSTDAT0 */
		data->byte = inb_p(SMBHSTDAT0);
		ali1535_delay_loop();
		break;
	case ALI1535_BYTE_DATA:
		data->byte = inb_p(SMBHSTDAT0);
		ali1535_delay_loop();
		break;
	case ALI1535_WORD_DATA:
		data->word = inb_p(SMBHSTDAT0) + (inb_p(SMBHSTDAT1) << 8);
		ali1535_delay_loop();
		break;
	case ALI1535_BLOCK_DATA:
		len = inb_p(SMBHSTDAT0);
		ali1535_delay_loop();
		if (len > 32)
			len = 32;
		data->block[0] = len;
		{
			int val;

			val = inb_p(SMBHSTTYP) | ALI1535_BLOCK_CLR;
			ali1535_delay_loop();
			outb_p(val, SMBHSTTYP);	/* Reset SMBBLKDAT */
			ali1535_delay_loop();
		}
		for (i = 1; i <= data->block[0]; i++) {
			data->block[i] = inb_p(SMBBLKDAT);
			ali1535_delay_loop();
#ifdef DEBUG
			printk
			    ("i2c-ali1535.o: Blk: len=%d, i=%d, data=%02x\n",
			     len, i, data->block[i]);
#endif	/* DEBUG */
		}
		break;
	}
EXIT:
	up(&i2c_ali1535_sem);
	return result;
}

static void ali1535_inc(struct i2c_adapter *adapter)
{
#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif
}

static void ali1535_dec(struct i2c_adapter *adapter)
{
#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
}

u32 ali1535_func(struct i2c_adapter *adapter)
{
	return I2C_FUNC_SMBUS_QUICK | I2C_FUNC_SMBUS_BYTE |
	    I2C_FUNC_SMBUS_BYTE_DATA | I2C_FUNC_SMBUS_WORD_DATA |
	    I2C_FUNC_SMBUS_BLOCK_DATA;
}

static struct i2c_algorithm smbus_algorithm = {
	.name		= "Non-i2c SMBus adapter",
	.id		= I2C_ALGO_SMBUS,
	.smbus_xfer	= ali1535_access,
	.functionality	= ali1535_func,
};

static struct i2c_adapter ali1535_adapter = {
	.name		= "unset",
	.id		= I2C_ALGO_SMBUS | I2C_HW_SMBUS_ALI1535,
	.algo		= &smbus_algorithm,
	.inc_use	= ali1535_inc,
	.dec_use	= ali1535_dec,
};


static struct pci_device_id ali1535_ids[] __devinitdata = {
	{
	.vendor =	PCI_VENDOR_ID_AL,
	.device =	PCI_DEVICE_ID_AL_M7101,
	.subvendor =	PCI_ANY_ID,
	.subdevice =	PCI_ANY_ID,
	},
	{ 0, }
};

static int __devinit ali1535_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	if (ali1535_setup(dev)) {
		printk
		    ("i2c-ali1535.o: ALI1535 not detected, module not inserted.\n");
		return -ENODEV;
	}

	sprintf(ali1535_adapter.name, "SMBus ALI1535 adapter at %04x",
		ali1535_smba);
	return i2c_add_adapter(&ali1535_adapter);
}

static void __devexit ali1535_remove(struct pci_dev *dev)
{
	i2c_del_adapter(&ali1535_adapter);
	release_region(ali1535_smba, ALI1535_SMB_IOSIZE);
}


static struct pci_driver ali1535_driver = {
	.name		= "ali1535 smbus",
	.id_table	= ali1535_ids,
	.probe		= ali1535_probe,
	.remove		= __devexit_p(ali1535_remove),
};

static int __init i2c_ali1535_init(void)
{
	printk("i2c-ali1535.o version %s (%s)\n", LM_VERSION, LM_DATE);
	return pci_module_init(&ali1535_driver);
}

static void __exit i2c_ali1535_exit(void)
{
	pci_unregister_driver(&ali1535_driver);
}

MODULE_AUTHOR
    ("Frodo Looijaard <frodol@dds.nl>, Philip Edelbrock <phil@netroedge.com>, "
     "Mark D. Studebaker <mdsxyz123@yahoo.com> and Dan Eaton <dan.eaton@rocketlogix.com>");
MODULE_DESCRIPTION("ALI1535 SMBus driver");
MODULE_LICENSE("GPL");

module_init(i2c_ali1535_init);
module_exit(i2c_ali1535_exit);
