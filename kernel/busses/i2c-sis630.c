/*
    SIS 630 SMBUS access implementation based on i2c-sis5595.

    Status: beta

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
   24.08.2002
   	Fixed the typo in sis630_access (Thanks to Mark M. Hoffman)
	Changed sis630_transaction. Now it's 2x faster (Thanks to Mark M. Hoffman)
*/

/*
   TODO:
     Implement block data write/read
     Check SMBALT# : why it can't be cleared ????
     Test on 2.2 kernel
*/
/*
   Supports:
	SIS 630

   Note: we assume there can only be one device, with one SMBus interface.
*/

#include <linux/version.h>
#include <linux/module.h>
#include <asm/io.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/stddef.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/i2c.h>
#include "version.h"
#include <linux/init.h>

#ifndef I2C_HW_SMBUS_SIS630
#define I2C_HW_SMBUS_SIS630	0x08
#endif

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
MODULE_PARM(force, "i");
MODULE_PARM_DESC(force, "Forcibly enable the SIS630. DANGEROUS!")


#ifdef MODULE
static
#else
extern
#endif
int __init i2c_sis630_init(void);
static int __init i2c_sis630_cleanup(void);
static int sis630_setup(void);
static s32 sis630_access(struct i2c_adapter *adap, u16 addr,
			  unsigned short flags, char read_write,
			  u8 command, int size,
			  union i2c_smbus_data *data);
static void sis630_do_pause(unsigned int amount);
static int sis630_transaction(int size);
static void sis630_inc(struct i2c_adapter *adapter);
static void sis630_dec(struct i2c_adapter *adapter);
static u32 sis630_func(struct i2c_adapter *adapter);
static u8 sis630_read(u8 reg);
static void sis630_write(u8 reg, u8 data);


static struct i2c_algorithm smbus_algorithm = {
	/* name */ "Non-I2C SMBus adapter",
	/* id */ I2C_ALGO_SMBUS,
	/* master_xfer */ NULL,
	/* smbus_access */ sis630_access,
	/* slave_send */ NULL,
	/* slave_rcv */ NULL,
	/* algo_control */ NULL,
	/* functionality */ sis630_func,
};

static struct i2c_adapter sis630_adapter = {
	"unset",
	I2C_ALGO_SMBUS | I2C_HW_SMBUS_SIS630,
	&smbus_algorithm,
	NULL,
	sis630_inc,
	sis630_dec,
	NULL,
	NULL,
};

static unsigned short acpi_base = 0;
static int __initdata sis630_initialized;

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

        /*
	  Make sure the SMBus host is ready to start transmitting.
	*/
	if ((temp = sis630_read(SMB_CNT) & 0x03) != 0x00) {
#ifdef DEBUG
                printk(KERN_DEBUG "i2c-sis630.o: SMBus busy (%02x). "
			"Resetting...\n",temp);
#endif
		/* kill smbus transaction */
		sis630_write(SMBHOST_CNT, 0x02);

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

	/* disable timeout interrupt and set clock to 56KHz */
	sis630_write(SMB_CNT, 0x20);

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
		/* TBD: Datasheet say:
		   the software should clear this bit and restart SMBUS operation
		*/
        }

        /* clear all status "sticky" bits */
	sis630_write(SMB_STS, temp);

        return result;
}

/* Return -1 on error. */
s32 sis630_access(struct i2c_adapter * adap, u16 addr,
		   unsigned short flags, char read_write,
		   u8 command, int size, union i2c_smbus_data * data) {

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

        if ((size != I2C_SMBUS_PROC_CALL) &&
		((read_write == I2C_SMBUS_WRITE) || (size == I2C_SMBUS_QUICK))) {
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

void sis630_inc(struct i2c_adapter *adapter) {
	MOD_INC_USE_COUNT;
}

void sis630_dec(struct i2c_adapter *adapter) {
	MOD_DEC_USE_COUNT;
}

u32 sis630_func(struct i2c_adapter *adapter) {
	return I2C_FUNC_SMBUS_QUICK | I2C_FUNC_SMBUS_BYTE | I2C_FUNC_SMBUS_BYTE_DATA |
		I2C_FUNC_SMBUS_WORD_DATA | I2C_FUNC_SMBUS_PROC_CALL;
}

int sis630_setup(void) {
	unsigned char b;
	struct pci_dev *sis630_dev = NULL,*tmp = NULL;

	/* First check whether we can access PCI at all */
	if (pci_present() == 0) {
		printk("i2c-sis630.o: Error: No PCI-bus found!\n");
		return -ENODEV;
	}

	/* Look for the SIS630 */
	if (!(sis630_dev = pci_find_device(PCI_VENDOR_ID_SI,
					    PCI_DEVICE_ID_SI_503,
					    sis630_dev))) {
		printk(KERN_ERR "i2c-sis630.o: Error: Can't detect SIS630!\n");
		return -ENODEV;
	}
	tmp = pci_find_device(PCI_VENDOR_ID_SI,PCI_DEVICE_ID_SI_630,NULL);
	if (tmp == NULL && force == 0) {
		printk(KERN_ERR "i2c-sis630.o: Error: Can't detect SIS630!\n");
		return -ENODEV;
	}
	else if (tmp == NULL && force > 0) {
		printk(KERN_NOTICE "i2c-sis630.o: WARNING: Can't detect SIS630 , but "
			"loading because of force option enabled\n");
	}

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

int __init i2c_sis630_init(void) {
	int res;
	printk("i2c-sis630.o version %s (%s)\n", LM_VERSION, LM_DATE);

	if (sis630_initialized) {
		printk(KERN_ERR "i2c-sis630.o: Oops, sis630_init called a second time!\n");
		return -EBUSY;
	}

	sis630_initialized = 0;
	if ((res = sis630_setup())) {
		printk(KERN_ERR "i2c-sis630.o: SIS630 not detected, module not inserted.\n");
		i2c_sis630_cleanup();
		return res;
	}
	sis630_initialized++;
	sprintf(sis630_adapter.name, "SMBus SIS630 adapter at %04x",
		acpi_base + SMB_STS);
	if ((res = i2c_add_adapter(&sis630_adapter))) {
		printk(KERN_ERR "i2c-sis630.o: Adapter registration failed, "
			"module not inserted.\n");
		i2c_sis630_cleanup();
		return res;
	}
	sis630_initialized++;
	printk(KERN_INFO "i2c-sis630.o: SIS630 bus detected and initialized\n");

	return 0;
}

int __init i2c_sis630_cleanup(void) {
	int res;
	if (sis630_initialized >= 2) {
		if ((res = i2c_del_adapter(&sis630_adapter))) {
			printk(KERN_ERR "i2c-sis630.o: i2c_del_adapter failed, module not"
				"removed\n");
			return res;
		} else
			sis630_initialized--;
	}
	if (sis630_initialized >= 1) {
		release_region(acpi_base + SMB_STS, SIS630_SMB_IOREGION);
		sis630_initialized--;
	}
	return 0;
}


EXPORT_NO_SYMBOLS;

#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif

#ifdef MODULE
MODULE_AUTHOR("Alexander Malysh <amalysh@web.de>");
MODULE_DESCRIPTION("SIS630 SMBus driver");

int init_module(void) {
	return i2c_sis630_init();
}

int cleanup_module(void) {
	return i2c_sis630_cleanup();
}

#endif	/* MODULE */
