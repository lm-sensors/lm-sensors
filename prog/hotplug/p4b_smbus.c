/*
 * p4b_smbus.c
 *
 * Initialize the SMBus device on ICH2/2-M/4/4-M (82801BA/BAM/DB/DBM)
 */
/*
    Copyright (c) 2002 Ilja Rauhut <IljaRauhut@web.de> and
    Klaus Woltereck <kw42@gmx.net>,

    Based on the m7101.c hotplug example by:

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
 * February 13,2002 IR First Version
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
 *
 * June 21, 2002, added support for the ICH4, code clean up -- Klaus
 *
 * Apr 13, 2004, modified for ICH4-M (82801DBM) -- Axel Thimm <Axel.Thimm@ATrpms.net>
 *               bugfix: register F2/bit 8 on ICH2* is bit 0 on ICH4*
 */


#ifdef P4Bsmbus_DEBUG
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/stddef.h>
#include <linux/ioport.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <asm/types.h>

#include <linux/sched.h> 
#include <linux/signal.h> 
#include <asm/irq.h>

/* 
rmm    from lm_sensors-2.3.3: 
 */

#define SMB_IO_SIZE  0xF
#define SMB_BASE 0x20


/*
 * some shorter definitions for the ICHx PCI device IDs
 */

#define ICH2 0x2440
#define ICH2_M 0x244c
#define ICH2_SMBUS 0x2443

#define ICH4 0x24c0
#define ICH4_M 0x24cc
#define ICH4_SMBUS 0x24c3

/* status, used to indicate that io space needs to be freed */
static struct pci_dev *i801smbus = NULL;
static int i801smbus_inserted = FALSE;
extern void cleanup_module(void);
 
static rwlock_t i801smbus_lock = RW_LOCK_UNLOCKED;
static unsigned long i801smbus_lock_flags = 0;

/* 
 * Checks whether SMBus is enabled and turns it on in case they are not. 
 * It's done by modifying the i801 function disable register, F2h.
 * ICH2(-M): PCI-Device 0x8086:0x2440(0x244c)
 *           Bit 3: Disables SMBus Host Controller function.
 *           Bit 8: allows SMBus I/O space to be accessible when Bit 3 is set.
 * ICH4(-M): PCI-Device 0x8086:0x24c0(0x24cc)
 *           Bit 3: Disables SMBus Host Controller function.
 *           Bit 0: allows SMBus I/O space to be accessible when Bit 3 is set.
 */
static int
i801smbus_enable(struct pci_dev *dev, u16 testmask, u16 mask){
	u16  val   = 0;

	pci_read_config_word(dev, 0xF2, &val);
	DBG("i801smbus: i801smbus config byte reading 0x%X.\n", val);
	if (val & testmask) {
		pci_write_config_word(dev, 0xF2, val & mask);
		pci_read_config_word(dev, 0xF2, &val);
		if(val & testmask) 
		  {
		    DBG("i801smbus: i801smbus config byte locked:-(\n");
		    return -EIO;
		  }
		else
		  printk("i801smbus: SMBus activated in LPC!\n");
	}
	return 0;

}

/*
 * Builds the basic pci_dev for the i801smbus
 */
static int i801smbus_build(struct pci_dev **i801smbus, struct pci_bus *bus) 
{
	u32 devfn;
	u16 id = 0;
	u16 vid = 0;
	int ret;

	DBG("i801smbus: requesting kernel space for the i801smbus entry.\n");
        *i801smbus = kmalloc(sizeof(**i801smbus), GFP_ATOMIC);
        if(NULL == *i801smbus) {
		printk("i801smbus: out of memory.\n");
		return -ENOMEM;
        }

	/* minimally fill in structure for search */
	/* The device should be on the same bus as the i801. */
        memset(*i801smbus, 0, sizeof(**i801smbus));
        (*i801smbus)->bus    = bus;
        (*i801smbus)->sysdata = bus->sysdata;
        (*i801smbus)->hdr_type = PCI_HEADER_TYPE_NORMAL;

	DBG("i801smbus: now looking for i801smbus.\n");
	for  (id = 0, devfn = 0; devfn < 0xFF; devfn++) {
		(*i801smbus)->devfn = devfn;
	    	ret = pci_read_config_word(*i801smbus, PCI_DEVICE_ID, &id);
		if (ret == 0 && (ICH2_SMBUS == id || ICH4_SMBUS == id)) {
		    	pci_read_config_word(*i801smbus, PCI_VENDOR_ID, &vid);
			if(vid == 0x8086)
				break;
		}
	}
	if (!(ICH2_SMBUS == id || ICH4_SMBUS == id)) {	
		DBG("i801smbus: i801smbus not found although i801 present - strange.\n");
		return -EACCES;
	} else {
		DBG("i801smbus: i801smbus found and enabled. Devfn: 0x%X.\n", devfn);
	}
	/* We now have the devfn and bus of the i801smbus device. 
         * let's put the rest of the device data together.
         */

        (*i801smbus)->vendor = 0x8086;
        (*i801smbus)->hdr_type = PCI_HEADER_TYPE_NORMAL;
        (*i801smbus)->device = id;
 
	return(pci_setup_device(*i801smbus));
}




/* Initialize the module */
int init_module(void)
{
	struct pci_bus *bus = NULL;
	struct pci_dev *dev = NULL;
	int ret = 0;
	u16 testmask = 0, mask = 0;

	DBG("i801smbus: init_module().\n");

	/* Are we on a PCI-Board? */
	if (!pci_present()) {
		printk("i801smbus: No PCI bus found - sorry.\n");
		return -ENODEV;
	}

	/* We want to be sure that the i801smbus is not present yet. */
	dev = pci_find_device(0x8086, ICH2_SMBUS, NULL);

	if (dev) 
	  {
	    printk("i801smbus: SMBus already active\n");
	    return -EPERM;
	  }
	
	dev = pci_find_device(0x8086, ICH4_SMBUS, NULL);

	if (dev) 
	  {
	    printk("i801smbus: SMBus already active\n");
	    return -EPERM;
	  }
	
	/* Are we operating a i801 chipset */
	if ((dev = pci_find_device(0x8086, ICH2, NULL)) != 0)
	  {
	    printk("i801smbus: found Intel ICH2 (82801BA).\n");
	    testmask = 0x008;
	    mask = 0xfef7;
	  }
	else if ((dev = pci_find_device(0x8086, ICH2_M, NULL)) != 0)
	  {
	    printk("i801smbus: found Intel ICH2-M (82801BAM).\n");
	    testmask = 0x008;
	    mask = 0xfef7;
	  }
	else if ((dev = pci_find_device(0x8086, ICH4, NULL)) != 0)
	  {
	    printk("i801smbus: found Intel ICH4 (82801DB).\n");
	    testmask = 0x008;
	    mask = 0xfff6;
	  }
	else if ((dev = pci_find_device(0x8086, ICH4_M, NULL)) != 0)
	  {
	    printk("i801smbus: found Intel ICH4-M (82801DBM).\n");
	    testmask = 0x008;
	    mask = 0xfff6;
	  }
	else
	  {
	    printk("i801smbus: INTEL ICH2/2-M/4/4-M (82801BA/BAM/DB/DBM) not found.\n");
	    return -ENODEV ;
	  }

	/* we need the bus pointer later */
	bus = dev->bus;


	if ( (ret = i801smbus_enable(dev, testmask, mask)) ) 
	  {
	    printk("i801smbus: Unable to turn on i801smbus device - sorry!\n");
	    return ret;
	  }
     
	if ( (ret = i801smbus_build(&i801smbus, bus)) )
		return ret;


	if ( (ret = pci_enable_device(i801smbus)) ) {
		printk("i801smbus: Unable to pci_enable i801smbus device!\n");
		return ret;
	}
    
	DBG("i801smbus: now inserting.\n");
	pci_insert_device(i801smbus, i801smbus->bus);
	printk("i801smbus: Enabled\n");
	i801smbus_inserted = TRUE;
	return 0;
}


void cleanup_module(void)
{
	write_lock_irqsave(&i801smbus_lock, i801smbus_lock_flags);
	if (i801smbus_inserted) {
		pci_remove_device(i801smbus);
		i801smbus_inserted = FALSE;
	}
	write_unlock_irqrestore(&i801smbus_lock, i801smbus_lock_flags);

	if (NULL != i801smbus)
	  {
	    kfree(i801smbus);
	  }
	printk("i801smbus: SMBus device removed\n");
}

EXPORT_NO_SYMBOLS;

#ifdef MODULE
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif
MODULE_AUTHOR("Ilja Rauhut <IljaRauhut@web.de>, "
              "Burkhard Kohl <bku@buks.ipn.de>, "
	      "Frank Bauer <frank.bauer@nikocity.de>, "
	      "Mark Studebaker <mdsxyz123@yahoo.com>,"
              "and Klaus Woltereck <kw42@gmx.net>");
MODULE_DESCRIPTION("i801smbus PCI Inserter");

#endif				/* MODULE */










