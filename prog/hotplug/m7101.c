/*
 * m7101.c
 *
 * Initialize the M7101 device on ALi M15x3 Chipsets
 */
/*
    Copyright (c) 2000 Burkhard Kohl <buk@buks.ipn.de>,
    Frank Bauer <frank.bauer@nikocity.de>, and
    Mark D. Studebaker <mdsxyz123@yahoo.com>

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
/* CHANGES:
 * 030799 FB: created this file
 * frank.bauer@nikocity.de
 *
 * 110799 BK: messed it up to insert the M7101 dev after boot
 * buk@buks.ipn.de
 *
 * 160799 BK: grouped portions of the code into functions
 * 
 * June 21, 2000 MDS: Add Copyright and GPL comments,
 *                    distribute as release 0.1.
 *
 * Sept 21, 2000 MDS: Rewrite to use kernel hotplug facility,
 *                    (similar to drivers/pcmcia/cardbus.c),
 *                    fix makefile so it will compile,
 *                    distribute as release 0.2.
 *
 * This code is absolutely experimental - use it at your own 
 * risk. 
 *
 * Warning, this module will work only with 2.4.x kernels.
 * If it works with earlier kernels then it is by luck.
 *
 * Warning, this module does not work with egcs < 2.95.2, use 
 * gcc > 2.7.2 or egcs >= 2.95.2. 
 *
 */


#ifdef M7101_DEBUG
#define DBG(x...) printk(x)
#else
#define DBG(x...)
#endif

#ifndef TRUE
#define TRUE 1
#define FALSE !TRUE
#endif

#include <linux/config.h>
#ifndef CONFIG_HOTPLUG
#error ERROR - You must have 'Support for hot-pluggable devices' enabled in your kernel (under 'general setup')!!
#endif

/* Deal with CONFIG_MODVERSIONS */
#ifdef CONFIG_MODVERSIONS
#define MODVERSIONS
#include <linux/modversions.h>
#endif

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/stddef.h>
#include <linux/ioport.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <asm/types.h>

/* 
    from lm_sensors-2.3.3: 
 */
#define SMBHSTCFG 0x0E0
/* PCI Address Constants */
#define SMBATPC   0x05B		/* used to unlock xxxBA registers */
/* ALI15X3 address lock bits */
#define ALI15X3_LOCK	0x06

/* 
   addresses the i/o space gets mapped to, 
   for M1543C, Bits 16-31 are supposed to be zero	
 */
#define ACPI_IO_SIZE 0x40
#define SMB_IO_SIZE  0x20
#define ACPI_BASE 0x0
#define SMB_BASE 0x1
static u32 acpi_io = 0xEC00;	/* Set I/O Base defaults here */
static u32 smb_io  = 0xE800;
MODULE_PARM(acpi_io, "l"); 
MODULE_PARM(smb_io, "l");
#ifdef M7101_DEBUG
static int debug = 0;
MODULE_PARM(debug, "i");
#endif

/* status, used to indicate that io space needs to be freed */
static struct pci_dev *m7101 = NULL;
static int m7101_inserted = FALSE;
extern void cleanup_module();
 
static rwlock_t m7101_lock = RW_LOCK_UNLOCKED;
static unsigned long m7101_lock_flags = 0;


#ifdef M7101_DEBUG
static void
dump_dev_data(struct pci_dev *dev, int address_data)
{
    if (dev != NULL) {
		printk("m7101: devfn:  0x%4x   class:  0x%4x\n",
				dev->devfn, dev->class);
		printk("m7101: vendor: 0x%4x   device: 0x%4x\n",
				dev->vendor, dev->device);

		if (address_data) {
			printk("m7101: resource[0]: 0x%08lx   resource[1]: 0x%08lx\n",
					dev->resource[0].start, dev->resource[1].start);
			printk("m7101: resource[2]: 0x%08lx   resource[3]: 0x%08lx\n",
					dev->resource[2].start, dev->resource[3].start);
			printk("m7101: resource[4]: 0x%08lx   resource[5]: 0x%08lx\n",
					dev->resource[4].start, dev->resource[5].start);
			printk("m7101: rom_address:     0x%08lx  \n",
					dev->resource[PCI_ROM_RESOURCE].start);
		}
	}
}
#endif

/* 
 * Checks whether PMU and SMB are enabled and turns them on in case they are not. 
 * It's done by clearing Bit 2 in M1533 config space 5Fh.
 * I/O 
 */
static int
m7101_enable(struct pci_dev *dev){
	u8  val   = 0;

	pci_read_config_byte(dev, 0x5F, &val);
	DBG("m7101: M7101 config byte reading 0x%X.\n", val);
	if (val & 0x4) {
		pci_write_config_byte(dev, 0x5F, val & 0xFB);
		pci_read_config_byte(dev, 0x5F, &val);
		if(val & 0x4) {
			DBG("m7101: Can't enable PMU/SMB, config byte locked:-(\n");
			return -EIO;
		}
	}
	return 0;

}

/*
 * Builds the basic pci_dev for the M7101
 */
static int
m7101_build(struct pci_dev **m7101, struct pci_bus *bus) 
{
	u32 devfn;
	u16 id = 0;
	u16 vid = 0;
	int ret;

	DBG("m7101: requesting kernel space for the m7101 entry.\n");
        *m7101 = kmalloc(sizeof(**m7101), GFP_ATOMIC);
        if(NULL == *m7101) {
		printk("m7101: out of memory.\n");
		return -ENOMEM;
        }

	/* minimally fill in structure for search */
	/* The device should be on the same bus as the M1533. */
        memset(*m7101, 0, sizeof(**m7101));
        (*m7101)->bus    = bus;
        (*m7101)->sysdata = bus->sysdata;
        (*m7101)->hdr_type = PCI_HEADER_TYPE_NORMAL;

	DBG("m7101: now looking for M7101.\n");
	for  (id = 0, devfn = 0; devfn < 0xFF; devfn++) {
		(*m7101)->devfn = devfn;
	    	ret = pci_read_config_word(*m7101, PCI_DEVICE_ID, &id);
		if (ret == 0 && PCI_DEVICE_ID_AL_M7101 == id) {
		    	pci_read_config_word(*m7101, PCI_VENDOR_ID, &vid);
			if(vid == PCI_VENDOR_ID_AL)
				break;
		}
	}
	if (PCI_DEVICE_ID_AL_M7101 != id) {	
		DBG("m7101: M7101 not found although M1533 present - strange.\n");
		return -EACCES;
	} else {
		DBG("m7101: M7101 found and enabled. Devfn: 0x%X.\n", devfn);
	}
	/* We now have the devfn and bus of the M7101 device. 
         * let's put the rest of the device data together.
         */

        (*m7101)->vendor = PCI_VENDOR_ID_AL;
        (*m7101)->hdr_type = PCI_HEADER_TYPE_NORMAL;
        (*m7101)->device = PCI_DEVICE_ID_AL_M7101;
	return(pci_setup_device(*m7101));
}

/*
 * unlock the address registers if necessary 
 */
static int
m7101_unlock_registers(struct pci_dev *m7101)
{
	u8 val = 0;

	if (pci_read_config_byte(m7101, SMBATPC, &val))  {
		DBG("m7101: failed to read SMBATPC\n");
		return -EIO;
	} else {
		if(val & ALI15X3_LOCK) {
			DBG("m7101: need to unlock the address registers\n");
			val &= ~ALI15X3_LOCK;
	    		if (pci_write_config_byte(m7101, SMBATPC, val)) {
				DBG("m7101: failed to write SMBATPC\n");
				return -EIO;
			}
  		}
	}

	return 0;
}


static int
m7101_set_io_address(struct pci_dev *m7101, u32 addr, int base_idx, int size)
{
	if (!addr) {
		printk("m7101: Sorry - cannot assign address 0 to region.\n");
		return -EINVAL;
	} 
	if (check_region(addr, size)) {
		printk("m7101: Requested IO-region 0x%x already in use.\n", addr);
		return -EBUSY;
	} else { 
		m7101->resource[base_idx].start = addr;
	}
	
	return 0;
}


/* Initialize the module */
int init_module(void)
{
	struct pci_bus *bus = NULL;
	struct pci_dev *dev = NULL;
	u32 acpi_addr = 0, smb_addr = 0;
	int ret = 0;

	DBG("m7101: init_module().\n");

	/* Are we on a PCI-Board? */
	if (!pci_present()) {
		printk("m7101: No PCI bus found - sorry.\n");
		return -ENODEV;
	}

	/* We want to be sure that the M7101 is not present yet. */
	dev = pci_find_device(PCI_VENDOR_ID_AL, PCI_DEVICE_ID_AL_M7101, NULL);
	if (dev) {
		printk("m7101: M7101 already present, no need to run this module.\n");
#ifdef M7101_DEBUG
		if(debug)
		{
			printk("m7101: removing device for testing\n");
			pci_remove_device(dev);
			m7101_inserted = 0;
			return 0;
		}
		else
#endif
			return -EPERM;
	}
	
	/* Are we operating a M15x3 chipset */
	dev = pci_find_device(PCI_VENDOR_ID_AL, PCI_DEVICE_ID_AL_M1533, NULL);
	if (NULL == dev) {
		printk("m7101: ALi M15xx chipset not found.\n");
		return -ENODEV ;
	}
	/* we need the bus pointer later */
	bus = dev->bus;

	if ( (ret = m7101_enable(dev)) ) {
		printk("m7101: Unable to turn on M7101 device - sorry!\n");
		return ret;
	}

	if ( (ret = m7101_build(&m7101, bus)) ){
		return ret;
	}

	/* thus, now we have a rudimentary dev for M7101 ... */
	/* check wether register need to be unlocked. */
	if ( (ret = m7101_unlock_registers(m7101)) ){
		return ret;
	}

	/*
	 * Handle address assignment for ACPI_IO 
	 * If address is already assigned do nothing.
	 */
	acpi_addr = m7101->resource[ACPI_BASE].start;
	if (!acpi_addr) {
		acpi_io |= PCI_BASE_ADDRESS_SPACE_IO;
		ret = m7101_set_io_address(m7101, acpi_io,ACPI_BASE, ACPI_IO_SIZE);
		if (ret)
			return ret;
		acpi_addr = acpi_io;
	}

	/*
	 * Handle address assignment for SMB_IO
	 * If address is already assigned then do nothing.
	 */
	smb_addr = m7101->resource[SMB_BASE].start;
	if (!smb_addr) {
		smb_io |= PCI_BASE_ADDRESS_SPACE_IO;
		ret = m7101_set_io_address(m7101, smb_io, SMB_BASE, SMB_IO_SIZE);
		if (ret)
			return ret;
		smb_addr = smb_io;
	}

#ifdef M7101_DEBUG
	dump_dev_data(m7101, 1);
#endif


	if ( (ret = pci_enable_device(m7101)) ) {
		printk("m7101: Unable to pci_enable M7101 device!\n");
		return ret;
	}

	DBG("m7101: now inserting.\n");
	pci_insert_device(m7101, m7101->bus);
	printk("m7101: Enabled with ACPI I/O address 0x%04X and SMB I/O address 0x%04X\n",
		acpi_addr, smb_addr);
	m7101_inserted = TRUE;

	return 0;
}


void cleanup_module()
{
	write_lock_irqsave(m7101_lock, m7101_lock_flags);
	if (m7101_inserted) {
		pci_remove_device(m7101);
		m7101_inserted = FALSE;
	}
	write_unlock_irqrestore(m7101_lock, m7101_lock_flags);

	if (NULL != m7101)
		kfree(m7101);

	printk("m7101: bye bye\n");
}

EXPORT_NO_SYMBOLS;

#ifdef MODULE
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif
MODULE_AUTHOR("Burkhard Kohl <bku@buks.ipn.de>, "
	      "Frank Bauer <frank.bauer@nikocity.de>, "
	      "and Mark Studebaker <mdsxyz123@yahoo.com>");
MODULE_DESCRIPTION("M7101 PCI Inserter");

#endif				/* MODULE */
