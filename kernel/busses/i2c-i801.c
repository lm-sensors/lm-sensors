/*
    i801.c - Part of lm_sensors, Linux kernel modules for hardware
              monitoring
    Copyright (c) 1998 - 2001  Frodo Looijaard <frodol@dds.nl>,
    Philip Edelbrock <phil@netroedge.com>, and Mark D. Studebaker
    <mdsxyz123@yahoo.com>

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
    SUPPORTED DEVICES	PCI ID
    82801AA		2413           
    82801AB		2423           
    82801BA		2443           
    82801CA/CAM		2483           

    This driver supports several versions of Intel's I/O Controller Hubs (ICH).
    For SMBus support, they are similar to the PIIX4 and are part
    of Intel's '810' and other chipsets.
    See the doc/busses/i2c-i801 file for details.
*/

/* Note: we assume there can only be one I801, with one SMBus interface */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <asm/io.h>
#include <linux/kernel.h>
#include <linux/stddef.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include "version.h"

#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif

#ifndef PCI_DEVICE_ID_INTEL_82801AA_3
#define PCI_DEVICE_ID_INTEL_82801AA_3   0x2413
#endif
#ifndef PCI_DEVICE_ID_INTEL_82801AB_3
#define PCI_DEVICE_ID_INTEL_82801AB_3   0x2423
#endif
#ifndef PCI_DEVICE_ID_INTEL_82801BA_2
#define PCI_DEVICE_ID_INTEL_82801BA_2   0x2443
#endif
#define PCI_DEVICE_ID_INTEL_82801CA_SMBUS	0x2483

static int supported[] = {PCI_DEVICE_ID_INTEL_82801AA_3,
                          PCI_DEVICE_ID_INTEL_82801AB_3,
                          PCI_DEVICE_ID_INTEL_82801BA_2,
			  PCI_DEVICE_ID_INTEL_82801CA_SMBUS,
                          0 };

/* I801 SMBus address offsets */
#define SMBHSTSTS (0 + i801_smba)
#define SMBHSTCNT (2 + i801_smba)
#define SMBHSTCMD (3 + i801_smba)
#define SMBHSTADD (4 + i801_smba)
#define SMBHSTDAT0 (5 + i801_smba)
#define SMBHSTDAT1 (6 + i801_smba)
#define SMBBLKDAT (7 + i801_smba)

/* PCI Address Constants */
#define SMBBA     0x020
#define SMBHSTCFG 0x040
#define SMBREV    0x008

/* Host configuration bits for SMBHSTCFG */
#define SMBHSTCFG_HST_EN      1
#define SMBHSTCFG_SMB_SMI_EN  2
#define SMBHSTCFG_I2C_EN      4

/* Other settings */
#define MAX_TIMEOUT 500
#define  ENABLE_INT9 0

/* I801 command constants */
#define I801_QUICK          0x00
#define I801_BYTE           0x04
#define I801_BYTE_DATA      0x08
#define I801_WORD_DATA      0x0C
#define I801_BLOCK_DATA     0x14
#define I801_I2C_BLOCK_DATA 0x18	/* unimplemented */
#define I801_BLOCK_LAST     0x34
#define I801_I2C_BLOCK_LAST 0x38	/* unimplemented */

/* insmod parameters */

/* If force is set to anything different from 0, we forcibly enable the
   I801. DANGEROUS! */
static int force = 0;
MODULE_PARM(force, "i");
MODULE_PARM_DESC(force, "Forcibly enable the I801. DANGEROUS!");

/* If force_addr is set to anything different from 0, we forcibly enable
   the I801 at the given address. VERY DANGEROUS! */
static int force_addr = 0;
MODULE_PARM(force_addr, "i");
MODULE_PARM_DESC(force_addr,
		 "Forcibly enable the I801 at the given address. "
		 "EXTREMELY DANGEROUS!");

#ifdef MODULE
static
#else
extern
#endif
int __init i2c_i801_init(void);
static int __init i801_cleanup(void);
static int i801_setup(void);
static s32 i801_access(struct i2c_adapter *adap, u16 addr,
		       unsigned short flags, char read_write,
		       u8 command, int size, union i2c_smbus_data *data);
static void i801_do_pause(unsigned int amount);
static int i801_transaction(void);
static int i801_block_transaction(union i2c_smbus_data *data,
				  char read_write, int i2c_enable);
static void i801_inc(struct i2c_adapter *adapter);
static void i801_dec(struct i2c_adapter *adapter);
static u32 i801_func(struct i2c_adapter *adapter);

#ifdef MODULE
extern int init_module(void);
extern int cleanup_module(void);
#endif				/* MODULE */

static struct i2c_algorithm smbus_algorithm = {
	/* name */ "Non-I2C SMBus adapter",
	/* id */ I2C_ALGO_SMBUS,
	/* master_xfer */ NULL,
	/* smbus_xfer */ i801_access,
	/* slave_send */ NULL,
	/* slave_rcv */ NULL,
	/* algo_control */ NULL,
	/* functionality */ i801_func,
};

static struct i2c_adapter i801_adapter = {
	"unset",
	I2C_ALGO_SMBUS | I2C_HW_SMBUS_I801,
	&smbus_algorithm,
	NULL,
	i801_inc,
	i801_dec,
	NULL,
	NULL,
};

static int __initdata i801_initialized;
static unsigned short i801_smba = 0;
static struct pci_dev *I801_dev = NULL;


/* Detect whether a I801 can be found, and initialize it, where necessary.
   Note the differences between kernels with the old PCI BIOS interface and
   newer kernels with the real PCI interface. In compat.h some things are
   defined to make the transition easier. */
int i801_setup(void)
{
	int error_return = 0;
	int *num = supported;
	unsigned char temp;

	/* First check whether we can access PCI at all */
	if (pci_present() == 0) {
		printk("i2c-i801.o: Error: No PCI-bus found!\n");
		error_return = -ENODEV;
		goto END;
	}

	/* Look for each chip */
	/* Note: we keep on searching until we have found 'function 3' */
	I801_dev = NULL;
	do {
		if((I801_dev = pci_find_device(PCI_VENDOR_ID_INTEL,
					      *num, I801_dev))) {
			if(PCI_FUNC(I801_dev->devfn) != 3)
				continue;
			break;
		}
		num++;
	} while (*num != 0);

	if (I801_dev == NULL) {
		printk
		    ("i2c-i801.o: Error: Can't detect I801, function 3!\n");
		error_return = -ENODEV;
		goto END;
	}

/* Determine the address of the SMBus areas */
	if (force_addr) {
		i801_smba = force_addr & 0xfff0;
		force = 0;
	} else {
		pci_read_config_word(I801_dev, SMBBA, &i801_smba);
		i801_smba &= 0xfff0;
	}

	if (check_region(i801_smba, 8)) {
		printk
		    ("i2c-i801.o: I801_smb region 0x%x already in use!\n",
		     i801_smba);
		error_return = -ENODEV;
		goto END;
	}

	pci_read_config_byte(I801_dev, SMBHSTCFG, &temp);
/* If force_addr is set, we program the new address here. Just to make
   sure, we disable the I801 first. */
	if (force_addr) {
		pci_write_config_byte(I801_dev, SMBHSTCFG, temp & 0xfe);
		pci_write_config_word(I801_dev, SMBBA, i801_smba);
		pci_write_config_byte(I801_dev, SMBHSTCFG, temp | 0x01);
		printk
		    ("i2c-i801.o: WARNING: I801 SMBus interface set to new "
		     "address %04x!\n", i801_smba);
	} else if ((temp & 1) == 0) {
		if (force) {
/* This should never need to be done, but has been noted that
   many Dell machines have the SMBus interface on the PIIX4
   disabled!? NOTE: This assumes I/O space and other allocations WERE
   done by the Bios!  Don't complain if your hardware does weird 
   things after enabling this. :') Check for Bios updates before
   resorting to this.  */
			pci_write_config_byte(I801_dev, SMBHSTCFG,
					      temp | 1);
			printk
			    ("i2c-i801.o: WARNING: I801 SMBus interface has been FORCEFULLY "
			     "ENABLED!\n");
		} else {
			printk
			    ("SMBUS: Error: Host SMBus controller not enabled!\n");
			error_return = -ENODEV;
			goto END;
		}
	}

	/* note: we assumed that the BIOS picked SMBus or I2C Bus timing
	   appropriately (bit 2 in SMBHSTCFG) */
	/* Everything is happy, let's grab the memory and set things up. */
	request_region(i801_smba, 8, "i801-smbus");

#ifdef DEBUG
	if (temp & 0x02)
		printk
		    ("i2c-i801.o: I801 using Interrupt SMI# for SMBus.\n");
	else
		printk
		    ("i2c-i801.o: I801 using PCI Interrupt for SMBus.\n");

	pci_read_config_byte(I801_dev, SMBREV, &temp);
	printk("i2c-i801.o: SMBREV = 0x%X\n", temp);
	printk("i2c-i801.o: I801_smba = 0x%X\n", i801_smba);
#endif				/* DEBUG */

      END:
	return error_return;
}


/* Internally used pause function */
void i801_do_pause(unsigned int amount)
{
	current->state = TASK_INTERRUPTIBLE;
	schedule_timeout(amount);
}

/* Another internally used function */
int i801_transaction(void)
{
	int temp;
	int result = 0;
	int timeout = 0;

#ifdef DEBUG
	printk
	    ("i2c-i801.o: Transaction (pre): CNT=%02x, CMD=%02x, ADD=%02x, DAT0=%02x, "
	     "DAT1=%02x\n", inb_p(SMBHSTCNT), inb_p(SMBHSTCMD),
	     inb_p(SMBHSTADD), inb_p(SMBHSTDAT0), inb_p(SMBHSTDAT1));
#endif

	/* Make sure the SMBus host is ready to start transmitting */
	/* 0x1f = Failed, Bus_Err, Dev_Err, Intr, Host_Busy */
	if ((temp = (0x1f & inb_p(SMBHSTSTS))) != 0x00) {
#ifdef DEBUG
		printk("i2c-i801.o: SMBus busy (%02x). Resetting... \n",
		       temp);
#endif
		outb_p(temp, SMBHSTSTS);
		if ((temp = inb_p(SMBHSTSTS)) != 0x00) {
#ifdef DEBUG
			printk("i2c-i801.o: Failed! (%02x)\n", temp);
#endif
			return -1;
		} else {
#ifdef DEBUG
			printk("i2c-i801.o: Successfull!\n");
#endif
		}
	}

	/* start the transaction by setting bit 6 */
	outb_p(inb(SMBHSTCNT) | 0x040, SMBHSTCNT);

	/* We will always wait for a fraction of a second! */
	do {
		i801_do_pause(1);
		temp = inb_p(SMBHSTSTS);
	} while ((temp & 0x01) && (timeout++ < MAX_TIMEOUT));

	/* If the SMBus is still busy, we give up */
	if (timeout >= MAX_TIMEOUT) {
#ifdef DEBUG
		printk("i2c-i801.o: SMBus Timeout!\n");
		result = -1;
#endif
	}

	if (temp & 0x10) {
		result = -1;
#ifdef DEBUG
		printk("i2c-i801.o: Error: Failed bus transaction\n");
#endif
	}

	if (temp & 0x08) {
		result = -1;
		printk
		    ("i2c-i801.o: Bus collision! SMBus may be locked until next hard\n"
		     "reset. (sorry!)\n");
		/* Clock stops and slave is stuck in mid-transmission */
	}

	if (temp & 0x04) {
		result = -1;
#ifdef DEBUG
		printk("i2c-i801.o: Error: no response!\n");
#endif
	}

	if ((inb_p(SMBHSTSTS) & 0x1f) != 0x00)
		outb_p(inb(SMBHSTSTS), SMBHSTSTS);

	if ((temp = (0x1f & inb_p(SMBHSTSTS))) != 0x00) {
#ifdef DEBUG
		printk
		    ("i2c-i801.o: Failed reset at end of transaction (%02x)\n",
		     temp);
#endif
	}
#ifdef DEBUG
	printk
	    ("i2c-i801.o: Transaction (post): CNT=%02x, CMD=%02x, ADD=%02x, "
	     "DAT0=%02x, DAT1=%02x\n", inb_p(SMBHSTCNT), inb_p(SMBHSTCMD),
	     inb_p(SMBHSTADD), inb_p(SMBHSTDAT0), inb_p(SMBHSTDAT1));
#endif
	return result;
}

/* All-inclusive block transaction function */
int i801_block_transaction(union i2c_smbus_data *data, char read_write, 
                           int i2c_enable)
{
	int i, len;
	int smbcmd;
	int temp;
	int result = 0;
	int timeout = 0;
        unsigned char hostc, errmask;

        if (i2c_enable) {
                if (read_write == I2C_SMBUS_WRITE) {
                        /* set I2C_EN bit in configuration register */
                        pci_read_config_byte(I801_dev, SMBHSTCFG, &hostc);
                        pci_write_config_byte(I801_dev, SMBHSTCFG, 
                                              hostc | SMBHSTCFG_I2C_EN);
                } else {
                        printk("i2c-i801.o: "
                               "I2C_SMBUS_I2C_BLOCK_READ not supported!\n");
                        return -1;
                }
        }

	if (read_write == I2C_SMBUS_WRITE) {
		len = data->block[0];
		if (len < 1)
			len = 1;
		if (len > 32)
			len = 32;
		outb_p(len, SMBHSTDAT0);
		outb_p(data->block[1], SMBBLKDAT);
	} else {
		len = 32;	/* max for reads */
	}

	for (i = 1; i <= len; i++) {
		if (i == len && read_write == I2C_SMBUS_READ)
			smbcmd = I801_BLOCK_LAST;
		else
			smbcmd = I801_BLOCK_DATA;

		outb_p((smbcmd & 0x3C) + (ENABLE_INT9 & 1), SMBHSTCNT);

#ifdef DEBUG
		printk
		    ("i2c-i801.o: Transaction (pre): CNT=%02x, CMD=%02x, ADD=%02x, "
		     "DAT0=%02x, BLKDAT=%02x\n", inb_p(SMBHSTCNT),
		     inb_p(SMBHSTCMD), inb_p(SMBHSTADD), inb_p(SMBHSTDAT0),
		     inb_p(SMBBLKDAT));
#endif

		/* Make sure the SMBus host is ready to start transmitting */
		temp = inb_p(SMBHSTSTS);
                if (i == 1) {
                    /* Erronenous conditions before transaction: 
                     * Byte_Done, Failed, Bus_Err, Dev_Err, Intr, Host_Busy */
                    errmask=0x9f; 
                } else {
                    /* Erronenous conditions during transaction: 
                     * Failed, Bus_Err, Dev_Err, Intr */
                    errmask=0x1e; 
                }
		if (temp & errmask) {
#ifdef DEBUG
			printk
			    ("i2c-i801.o: SMBus busy (%02x). Resetting... \n",
			     temp);
#endif
			outb_p(temp, SMBHSTSTS);
			if (((temp = inb_p(SMBHSTSTS)) & errmask) != 0x00) {
				printk
				    ("i2c-i801.o: Reset failed! (%02x)\n",
				     temp);
				result = -1;
                                goto END;
			}
			if (i != 1) {
                                result = -1;  /* if die in middle of block transaction, fail */
                                goto END;
                        }
		}

		/* start the transaction by setting bit 6 */
		if (i==1) outb_p(inb(SMBHSTCNT) | 0x040, SMBHSTCNT);

		/* We will always wait for a fraction of a second! */
		do {
			temp = inb_p(SMBHSTSTS);
			i801_do_pause(1);
		}
		    while (
			   (((i >= len) && (temp & 0x01))
			    || ((i < len) && !(temp & 0x80)))
			   && (timeout++ < MAX_TIMEOUT));

		/* If the SMBus is still busy, we give up */
		if (timeout >= MAX_TIMEOUT) {
			result = -1;
#ifdef DEBUG
			printk("i2c-i801.o: SMBus Timeout!\n");
#endif
		}

		if (temp & 0x10) {
			result = -1;
#ifdef DEBUG
			printk
			    ("i2c-i801.o: Error: Failed bus transaction\n");
#endif
		} else if (temp & 0x08) {
			result = -1;
			printk
			    ("i2c-i801.o: Bus collision! SMBus may be locked until next hard"
			     " reset. (sorry!)\n");
			/* Clock stops and slave is stuck in mid-transmission */
		} else if (temp & 0x04) {
			result = -1;
#ifdef DEBUG
			printk("i2c-i801.o: Error: no response!\n");
#endif
		}

		if (i == 1 && read_write == I2C_SMBUS_READ) {
			len = inb_p(SMBHSTDAT0);
			if (len < 1)
				len = 1;
			if (len > 32)
				len = 32;
			data->block[0] = len;
		}

                /* Retrieve/store value in SMBBLKDAT */
		if (read_write == I2C_SMBUS_READ)
			data->block[i] = inb_p(SMBBLKDAT);
		if (read_write == I2C_SMBUS_WRITE && i+1 <= len)
			outb_p(data->block[i+1], SMBBLKDAT);
		if ((temp & 0x9e) != 0x00)
                        outb_p(temp, SMBHSTSTS);  /* signals SMBBLKDAT ready */

		temp = inb_p(SMBHSTSTS);
		if ((temp = (0x1e & inb_p(SMBHSTSTS))) != 0x00) {
#ifdef DEBUG
			printk
			    ("i2c-i801.o: Failed reset at end of transaction (%02x)\n",
			     temp);
#endif
		}
#ifdef DEBUG
		printk
		    ("i2c-i801.o: Transaction (post): CNT=%02x, CMD=%02x, ADD=%02x, "
		     "DAT0=%02x, BLKDAT=%02x\n", inb_p(SMBHSTCNT),
		     inb_p(SMBHSTCMD), inb_p(SMBHSTADD), inb_p(SMBHSTDAT0),
		     inb_p(SMBBLKDAT));
#endif

		if (result < 0) {
			goto END;
                }
	}
        result = 0;
END:
        if (i2c_enable) {
                /* restore saved configuration register value */
		pci_write_config_byte(I801_dev, SMBHSTCFG, hostc);
        }
	return result;
}

/* Return -1 on error. See smbus.h for more information */
s32 i801_access(struct i2c_adapter * adap, u16 addr, unsigned short flags,
		char read_write, u8 command, int size,
		union i2c_smbus_data * data)
{

	switch (size) {
	case I2C_SMBUS_PROC_CALL:
		printk("i2c-i801.o: I2C_SMBUS_PROC_CALL not supported!\n");
		return -1;
	case I2C_SMBUS_QUICK:
		outb_p(((addr & 0x7f) << 1) | (read_write & 0x01),
		       SMBHSTADD);
		size = I801_QUICK;
		break;
	case I2C_SMBUS_BYTE:
		outb_p(((addr & 0x7f) << 1) | (read_write & 0x01),
		       SMBHSTADD);
		if (read_write == I2C_SMBUS_WRITE)
			outb_p(command, SMBHSTCMD);
		size = I801_BYTE;
		break;
	case I2C_SMBUS_BYTE_DATA:
		outb_p(((addr & 0x7f) << 1) | (read_write & 0x01),
		       SMBHSTADD);
		outb_p(command, SMBHSTCMD);
		if (read_write == I2C_SMBUS_WRITE)
			outb_p(data->byte, SMBHSTDAT0);
		size = I801_BYTE_DATA;
		break;
	case I2C_SMBUS_WORD_DATA:
		outb_p(((addr & 0x7f) << 1) | (read_write & 0x01),
		       SMBHSTADD);
		outb_p(command, SMBHSTCMD);
		if (read_write == I2C_SMBUS_WRITE) {
			outb_p(data->word & 0xff, SMBHSTDAT0);
			outb_p((data->word & 0xff00) >> 8, SMBHSTDAT1);
		}
		size = I801_WORD_DATA;
		break;
	case I2C_SMBUS_BLOCK_DATA:
	case I2C_SMBUS_I2C_BLOCK_DATA:
		outb_p(((addr & 0x7f) << 1) | (read_write & 0x01),
		       SMBHSTADD);
		outb_p(command, SMBHSTCMD);
		/* Block transactions are very different from piix4 block
		   and from the other i801 transactions. Handle in the
		   i801_block_transaction() routine. */
		return i801_block_transaction(data, read_write, 
                                              size==I2C_SMBUS_I2C_BLOCK_DATA);
	}

	/* 'size' is really the transaction type */
	outb_p((size & 0x3C) + (ENABLE_INT9 & 1), SMBHSTCNT);

	if (i801_transaction())	/* Error in transaction */
		return -1;

	if ((read_write == I2C_SMBUS_WRITE) || (size == I801_QUICK))
		return 0;


	switch (size) {
	case I801_BYTE:	/* Result put in SMBHSTDAT0 */
		data->byte = inb_p(SMBHSTDAT0);
		break;
	case I801_BYTE_DATA:
		data->byte = inb_p(SMBHSTDAT0);
		break;
	case I801_WORD_DATA:
		data->word = inb_p(SMBHSTDAT0) + (inb_p(SMBHSTDAT1) << 8);
		break;
	}
	return 0;
}

void i801_inc(struct i2c_adapter *adapter)
{
	MOD_INC_USE_COUNT;
}

void i801_dec(struct i2c_adapter *adapter)
{
	MOD_DEC_USE_COUNT;
}

u32 i801_func(struct i2c_adapter *adapter)
{
	return I2C_FUNC_SMBUS_QUICK | I2C_FUNC_SMBUS_BYTE |
	    I2C_FUNC_SMBUS_BYTE_DATA | I2C_FUNC_SMBUS_WORD_DATA |
	    I2C_FUNC_SMBUS_BLOCK_DATA | I2C_FUNC_SMBUS_WRITE_I2C_BLOCK;
}

int __init i2c_i801_init(void)
{
	int res;
	printk("i2c-i801.o version %s (%s)\n", LM_VERSION, LM_DATE);
#ifdef DEBUG
/* PE- It might be good to make this a permanent part of the code! */
	if (i801_initialized) {
		printk
		    ("i2c-i801.o: Oops, i801_init called a second time!\n");
		return -EBUSY;
	}
#endif
	i801_initialized = 0;
	if ((res = i801_setup())) {
		printk
		    ("i2c-i801.o: I801 not detected, module not inserted.\n");
		i801_cleanup();
		return res;
	}
	i801_initialized++;
	sprintf(i801_adapter.name, "SMBus I801 adapter at %04x",
		i801_smba);
	if ((res = i2c_add_adapter(&i801_adapter))) {
		printk
		    ("i2c-i801.o: Adapter registration failed, module not inserted.\n");
		i801_cleanup();
		return res;
	}
	i801_initialized++;
	printk("i2c-i801.o: I801 bus detected and initialized\n");
	return 0;
}

int __init i801_cleanup(void)
{
	int res;
	if (i801_initialized >= 2) {
		if ((res = i2c_del_adapter(&i801_adapter))) {
			printk
			    ("i2c-i801.o: i2c_del_adapter failed, module not removed\n");
			return res;
		} else
			i801_initialized--;
	}
	if (i801_initialized >= 1) {
		release_region(i801_smba, 8);
		i801_initialized--;
	}
	return 0;
}

EXPORT_NO_SYMBOLS;

#ifdef MODULE

MODULE_AUTHOR
    ("Frodo Looijaard <frodol@dds.nl>, Philip Edelbrock <phil@netroedge.com>, and Mark D. Studebaker <mdsxyz123@yahoo.com>");
MODULE_DESCRIPTION("I801 SMBus driver");

int init_module(void)
{
	return i2c_i801_init();
}

int cleanup_module(void)
{
	return i801_cleanup();
}

#endif				/* MODULE */
