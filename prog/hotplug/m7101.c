/*
 * m7101.c
 *
 * Initialize the M7101 device on ALi M15x3 Chipsets
 */
/*
    Copyright (c) 2000 Burkhard Kohl <buk@buks.ipn.de>
    and Frank Bauer <frank.bauer@nikocity.de>

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
 * This code is absolutely experimental - use it at your own 
 * risk. 
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

/* Deal with CONFIG_MODVERSIONS */
#if CONFIG_MODVERSIONS==1
#define MODVERSIONS
#include <linux/modversions.h>
#endif

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/stddef.h>
#include <linux/ioport.h>
#include <linux/errno.h>
#include <linux/malloc.h>
#include <asm/spinlock.h>

/* 
    from lm_sensors-2.3.3: 
 */
#define SMBHSTCFG 0x0E0
/* PCI Address Constants */
#define SMBCOM    0x004
#define ACPIBA    0x010
#define SMBBA     0x014
#define SMBATPC   0x05B		/* used to unlock xxxBA registers */
#define SMBHSTCFG 0x0E0
#define SMBSLVC   0x0E1
#define SMBCLK    0x0E2
#define SMBREV    0x008
/* ALI15X3 address lock bits */
#define ALI15X3_LOCK	0x06
/* ALI15X3 command constants */
#define ALI15X3_ABORT      0x02
#define ALI15X3_T_OUT      0x04
#define ALI15X3_QUICK      0x00
#define ALI15X3_BYTE       0x10
#define ALI15X3_BYTE_DATA  0x20
#define ALI15X3_WORD_DATA  0x30
#define ALI15X3_BLOCK_DATA 0x40
#define ALI15X3_BLOCK_CLR  0x80
/* ALI15X3 status register bits */
#define ALI15X3_STS_IDLE	0x04
#define ALI15X3_STS_BUSY	0x08
#define ALI15X3_STS_DONE	0x10
#define ALI15X3_STS_DEV		0x20	/* device error */
#define ALI15X3_STS_COLL	0x40	/* collision or no response */
#define ALI15X3_STS_TERM	0x80	/* terminated by abort */
#define ALI15X3_STS_ERR		0xE0	/* all the bad error bits */


/* 
   addresses the i/o space gets mapped to, 
   for M1543C, Bits 16-31 are supposed to be zero	
 */
#define ACPI_IO_SIZE 0x40
#define SMB_IO_SIZE  0x20
#define ACPI_BASE 0x0
#define SMB_BASE 0x1
static u32 acpi_io_parm = 0;
static u32 smb_io_parm = 0;
MODULE_PARM(acpi_io_parm, "l"); 
MODULE_PARM(smb_io_parm, "l");

/* status, used to indicate that io space needs to be freed */
static struct pci_dev *m7101 = NULL;
static int m7101_attached = FALSE;
static int m7101_inserted = FALSE;
extern void cleanup_module();
 
static	rwlock_t m7101_lock = RW_LOCK_UNLOCKED;
static  unsigned long m7101_lock_flags = 0;


/*
 * This function was shamelessly stolen from the kernel pci 
 * driver code.
 */
static void 
pci_read_bases(struct pci_dev *dev, unsigned int howmany)
{
    unsigned int reg;
    u32 l;

    for(reg=0; reg<howmany; reg++) {
        pci_read_config_dword(dev, PCI_BASE_ADDRESS_0 + (reg << 2), &l);
        if (l == 0xffffffff)
            continue;
        dev->base_address[reg] = l;
        if ((l & (PCI_BASE_ADDRESS_SPACE | PCI_BASE_ADDRESS_MEM_TYPE_MASK))
            == (PCI_BASE_ADDRESS_SPACE_MEMORY | PCI_BASE_ADDRESS_MEM_TYPE_64)) {            reg++;
            pci_read_config_dword(dev, PCI_BASE_ADDRESS_0 + (reg << 2), &l);
            if (l) {
#if BITS_PER_LONG == 64
                dev->base_address[reg-1] |= ((unsigned long) l) << 32;
#else
                DBG("PCI: Unable to handle 64-bit address for device %02x:%02x\n",
                    dev->bus->number, dev->devfn);
                dev->base_address[reg-1] = 0;
#endif
            }
        }
    }
}

static void
dump_dev_data(struct pci_dev *dev, int address_data)
{
    if (dev != NULL) {
		printk("m7101: devfn:  0x%4x   class:  0x%4x\n",
				dev->devfn, dev->class);
		printk("m7101: vendor: 0x%4x   device: 0x%4x\n",
				dev->vendor, dev->device);

		if (address_data) {
			printk("m7101: base_address[0]: 0x%08lx   base_address[1]: 0x%08lx\n",
					dev->base_address[0], dev->base_address[1]);
			printk("m7101: base_address[2]: 0x%08lx   base_address[3]: 0x%08lx\n",
					dev->base_address[2], dev->base_address[3]);
			printk("m7101: base_address[4]: 0x%08lx   base_address[5]: 0x%08lx\n",
					dev->base_address[4], dev->base_address[5]);
			printk("m7101: rom_address:     0x%08lx  \n",
					dev->rom_address);
		}
	}
}

/* 
 * Checks whether PMU and SMB are enabled and turns them on in case they are not. 
 * It's done by clearing Bit 2 in M1533 config space 5Fh.
 * I/O 
 */
static int
m7101_enable(struct pci_dev *dev, u32 *devfn){
	u8  val   = 0;
	u16 id    = 0;

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

	/* The device should be on the same bus as the M1533. */
	DBG("m7101: now looking for M7101.\n");
	for  (id = 0, *devfn = 0; *devfn < 0xFF; (*devfn)++) {
    	pcibios_read_config_word(dev->bus->number, *devfn, PCI_DEVICE_ID, &id);
		if (PCI_DEVICE_ID_AL_M7101 == id) {
				break;
		}
	}
	if (PCI_DEVICE_ID_AL_M7101 != id) {	
		DBG("m7101: M7101 not found although M1533 present - strange.\n");
		return -EACCES;
	} else {
		DBG("m7101: M7101 found and enabled. Devfn: 0x%X.\n", *devfn);
	}
	return 0;
}

/*
 * Builds the basic pci_dev for the M7101
 * 
 */
static int
m7101_build(struct pci_dev **m7101, u32 devfn, struct pci_bus *bus) 
{
	u32 val = 0;

	/* We now have the devfn and bus of the M7101 device. 
     * let's put the device data together.
     */
    if ((pcibios_read_config_dword(bus->number, devfn, PCI_VENDOR_ID, &val) ||
        /* some broken boards return 0 if a slot is empty: */
    	val == 0xffffffff || val == 0x00000000 || 
		val == 0x0000ffff || val == 0xffff0000)) {

		m7101 = NULL;
		return -ENODEV;
	} else {
		DBG("m7101: requesting kernel space for the m7101 entry.\n");
        *m7101 = kmalloc(sizeof(**m7101), GFP_ATOMIC);
        if(NULL == *m7101)
        {
            printk("m7101: out of memory.\n");
			return -ENOMEM;
        }
        memset(*m7101, 0, sizeof(**m7101));
        (*m7101)->bus    = bus;
        (*m7101)->devfn  = devfn;
        (*m7101)->vendor = val & 0xffff;
        (*m7101)->device = (val >> 16) & 0xffff;
	}
	return 0;
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
		m7101->base_address[base_idx] = 
		addr | PCI_BASE_ADDRESS_SPACE_IO;
	}
	
	return 0;
}

/* 
 * Maintain the ordering of the other devices.
 * We know that first_on_bus and dev are on the same bus. 
 */
static void
m7101_insert(struct pci_dev *m7101)
{
	struct pci_dev *first_on_bus = NULL, *dev = NULL;
	u32 devfn = 0;
	unsigned char bus_number = m7101->bus->number;

	/* Insert device into global chain of devices. */
	/* We have at least one other sibling on this bus */				 
	DBG("m7101: find first device in chain.\n");
	first_on_bus = NULL; devfn = 0;
	while (NULL == (first_on_bus = pci_find_slot(bus_number, devfn)) ) {
		devfn++;
	}
	write_lock_irqsave(m7101_lock, m7101_lock_flags);
	if (first_on_bus->devfn < m7101->devfn) {
		DBG("m7101: inserting M7101 after other devices on bus 0x%X.\n",
			bus_number);
		dev = first_on_bus;
		while (dev->sibling) {
			if (dev->sibling->devfn > m7101->devfn) {
				break;
			}
			dev = dev->sibling;
		}
		m7101->next    = dev->next;
		m7101->sibling = dev->sibling;
		dev->next  	   = m7101;
		dev->sibling   = m7101;
	} else {
		/*
		 * If we insert dev before first_on_bus, we have to find the 
		 * device pointing to first_on_bus.
		 */
		DBG("m7101: inserting M7101 as first device on bus 0x%X.\n", 
			 bus_number);

		m7101->next    	= first_on_bus;
		m7101->sibling	= first_on_bus;
		dev = pci_devices;
		if (dev == first_on_bus) {
			dev = m7101;
		} else {
			while (dev->next != first_on_bus) {
				dev = dev->next;	
			}
			/* We can't miss first_on_bus, can we ? */
			dev->next = m7101;
		}
	}
	write_unlock_irqrestore(m7101_lock, m7101_lock_flags);
}

/* Initialize the module */
int init_module(void)
{
	struct pci_bus *bus = NULL;
	struct pci_dev *dev = NULL;
	u32 devfn = 0;
    u32 acpi_addr = 0, smb_addr = 0;
	int ret = 0;

	DBG("m7101: init_module().\n");

	/* Are we on a PCI-Board? */
	if (!pci_present()) {
		printk("m7101: No pci board found - sorry.\n");
		return -ENODEV;
	}

	/* We want to be sure that the M7101 is not present yet. */
    dev = pci_find_device(PCI_VENDOR_ID_AL, PCI_DEVICE_ID_AL_M7101, NULL);
    if (dev) {
		printk("m7101: M7101 already present, no need to run this module.\n");
		dev = NULL;
		return -EPERM;
    }
	
	/* Are we operating a M15x3 chipset */
    dev = pci_find_device(PCI_VENDOR_ID_AL, PCI_DEVICE_ID_AL_M1533, NULL);
    if (NULL == dev) {
		printk("m7101: This is not an ALi M15x3 chipset.\n");
		return -ENODEV ;
    } else {
		/* we need the bus pointer later */
		bus = dev->bus;
	}

	if ( (ret = m7101_enable(dev, &devfn)) ) {
		printk("m7101: Unable to turn on M7101 device - sorry!\n");
		return ret;
	}

	if ( (ret = m7101_build(&m7101, devfn, bus)) ){
		return ret;
	}

    /* thus, now we have a rudimentary dev for M7101 ... */
    /* check wether register need to be unlocked. */
	if ( (ret = m7101_unlock_registers(m7101)) ){
		return ret;
	}

	/* Just for debugging purposes */
	pci_read_bases(m7101, 6);

	/*
	 * Handle address assignment for ACPI_IO 
	 * If address is already assigned do nothing.
	 */
	acpi_addr = m7101->base_address[ACPI_BASE];
	if (!acpi_addr) {
		ret = m7101_set_io_address(m7101, acpi_io_parm,ACPI_BASE, ACPI_IO_SIZE);
		if (ret)
			return ret;
	}

	/*
	 * Handle address assignment for SMB_IO
	 * If address is already assigned then do nothing.
	 */
	smb_addr = m7101->base_address[SMB_BASE];
	if (!smb_addr) {
		ret = m7101_set_io_address(m7101, smb_io_parm, SMB_BASE, SMB_IO_SIZE);
		if (ret)
			return ret;
	}

#ifdef M7101_DEBUG
	dump_dev_data(m7101, 1);
#endif


	DBG("m7101: now inserting.\n");
	m7101_insert(m7101);
	m7101_inserted = TRUE;

	DBG("m7101: now dumping global list.\n");
	for (dev =  pci_devices; NULL != dev->next ;  dev = dev->next) {
#ifdef M7101_DEBUG
		dump_dev_data(dev, 0);
		dump_dev_data(dev->next, 0);
		dump_dev_data(dev->sibling, 0);
		printk("\n");
#endif
	}

	/* Attach the device to the proc listing. */
	DBG("m7101: Attaching device to proc file system.\n");
	if ( (ret = pci_proc_attach_device(m7101)) ){
		printk("m7101: could not attach device to proc fs.\n");
		/* Get it out of the way or /proc/bus/pci will be garbled. */
		cleanup_module();
		return ret;	
	} else {
		m7101_attached = TRUE;
		printk("m7101: device inserted.\n");
	}

	return 0;
}


void cleanup_module()
{

	struct pci_dev *dev;

	if (m7101_attached) {
		pci_proc_detach_device(m7101);
		m7101_attached = FALSE;
	}

	write_lock_irqsave(m7101_lock, m7101_lock_flags);
	if (m7101_inserted) {
		if (pci_devices == m7101)
			pci_devices = m7101->next;
		else {
			dev = pci_devices;
			while (dev->next != m7101) {
				dev = dev->next;
			}
			dev->next = m7101->next;
			if (dev->sibling) {
				dev->sibling = m7101->sibling;
			}
		}
		m7101_inserted = FALSE;
	}
	write_unlock_irqrestore(m7101_lock, m7101_lock_flags);

	if (NULL != m7101)
		kfree(m7101);

	printk("m7101: bye bye\n");

}
