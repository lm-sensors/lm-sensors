/*
    i2c-via.c - Part of lm_sensors,  Linux kernel modules
                for hardware monitoring

    i2c Support for Via Technologies 82C586B South Bridge

    Copyright (c) 1998, 1999 Kyösti Mälkki <kmalkki@cc.hut.fi>

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

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <asm/io.h>
#include <linux/types.h>

#if LINUX_VERSION_CODE < 0x020136 /* 2.1.54 */
#include <linux/bios32.h>
#endif

#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>
#include "compat.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,53)
#include <linux/init.h>
#else
#define __init
#define __initdata
#endif

/* PCI device */
#define VENDOR		PCI_VENDOR_ID_VIA
#define DEVICE		PCI_DEVICE_ID_VIA_82C586_3

/* Power management registers */

#define PM_CFG_REVID    0x08  /* silicon revision code */
#define PM_CFG_IOBASE0  0x20
#define PM_CFG_IOBASE1  0x48

#define I2C_DIR		(pm_io_base+0x40)
#define I2C_OUT		(pm_io_base+0x42)
#define I2C_IN		(pm_io_base+0x44)
#define I2C_SCL		0x02   /* clock bit in DIR/OUT/IN register */
#define I2C_SDA		0x04

/* io-region reservation */
#define IOSPACE		0x06
#define IOTEXT		"via-i2c"

/* ----- global defines -----------------------------------------------	*/
#define DEB(x) x	/* silicon revision, io addresses	 	*/
#define DEB2(x) x	/* line status					*/
#define DEBE(x) 	/* 						*/

/* ----- local functions ----------------------------------------------	*/

static u16 pm_io_base;

static void bit_via_setscl(void *data, int state)
{
  outb(state ? inb(I2C_OUT)|I2C_SCL : inb(I2C_OUT)&~I2C_SCL, I2C_OUT);
}

static void bit_via_setsda(void *data, int state)
{
  outb(state ? inb(I2C_OUT)|I2C_SDA : inb(I2C_OUT)&~I2C_SDA, I2C_OUT);
}

static int bit_via_getscl(void *data)
{
  return (0 != (inb(I2C_IN) & I2C_SCL) );
}

static int bit_via_getsda(void *data)
{
  return (0 != (inb(I2C_IN) & I2C_SDA) );
}

static void bit_via_inc(struct i2c_adapter *adapter)
{
	MOD_INC_USE_COUNT;
}

static void bit_via_dec(struct i2c_adapter *adapter)
{
	MOD_DEC_USE_COUNT;
}

/* ------------------------------------------------------------------------ */

static struct i2c_algo_bit_data bit_data = {
	NULL,
	bit_via_setsda,
	bit_via_setscl,
	bit_via_getsda,
	bit_via_getscl,
	5, 5, 100,	/*waits, timeout */
};

static struct i2c_adapter bit_via_ops = {
	"VIA i2c",
	I2C_HW_B_VIA,
	NULL,
	&bit_data,
	bit_via_inc,
	bit_via_dec,
	NULL,
	NULL,
};


/* When exactly was the new pci interface introduced? */
#if LINUX_VERSION_CODE >= 0x020136 /* 2.1.54 */
static int find_via(void)
{
	struct pci_dev *s_bridge;
	u16 base;
	u8 rev;
	
	if (! pci_present())
		return -ENODEV;
		
	s_bridge = pci_find_device(VENDOR, DEVICE, NULL);
		
	if (! s_bridge)	{
		printk("vt82c586b not found\n");
		return -ENODEV;
	}

	if ( PCIBIOS_SUCCESSFUL != 
		pci_read_config_byte(s_bridge, PM_CFG_REVID, &rev))
		return -ENODEV;

	switch(rev)
	{
		case 0x00:	base = PM_CFG_IOBASE0;
				break;
		case 0x01:
		case 0x10:	base = PM_CFG_IOBASE1;
				break;
				
		default	:	base = PM_CFG_IOBASE1;
				/* later revision */	
	}	

	if ( PCIBIOS_SUCCESSFUL !=
		pci_read_config_word(s_bridge, base, &pm_io_base))
		return -ENODEV;
		
	pm_io_base &= (0xff<<8);
	return 0;
}

#else

static int find_via(void)
{
	unsigned char VIA_bus, VIA_devfn;
	u16 base;
	u8 rev;
	
	if (! pcibios_present())
		return -ENODEV;
		
	if(pcibios_find_device(VENDOR, DEVICE, 0, &VIA_bus, &VIA_devfn))
	{
		printk("vt82c586b not found\n");
		return -ENODEV;
	}
	
	if ( PCIBIOS_SUCCESSFUL != 
		pcibios_read_config_byte(VIA_bus, VIA_devfn,
					PM_CFG_REVID, &rev))
		return -ENODEV;

	switch(rev)
	{
		case 0x00:	base = PM_CFG_IOBASE0;
				break;
		case 0x01:
		case 0x10:	base = PM_CFG_IOBASE1;
				break;
				
		default	:	base = PM_CFG_IOBASE1;
				/* later revision */	
	}	

	if ( PCIBIOS_SUCCESSFUL !=
                pcibios_read_config_word(VIA_bus, VIA_devfn, base, &pm_io_base))
		return -ENODEV;

	pm_io_base &= (0xff<<8);
	return 0;
}
#endif

#ifdef MODULE
static
#else
extern
#endif
       int __init i2c_via_init(void)
{
	if (find_via() < 0) {
		printk("Error while reading PCI configuration\n");
		return -ENODEV;
	}

	if ( check_region(I2C_DIR, IOSPACE) < 0) {
		printk("IO 0x%x-0x%x already in use\n",
			I2C_DIR, I2C_DIR+IOSPACE);
		return -EBUSY;
	} else {
		request_region(I2C_DIR, IOSPACE, IOTEXT);
		outb(inb(I2C_DIR) | I2C_SDA | I2C_SCL, I2C_DIR);
		outb(inb(I2C_OUT) | I2C_SDA | I2C_SCL, I2C_OUT);		
	}
			
	if (i2c_bit_add_bus(&bit_via_ops) == 0) {
		printk("i2c-via.o: Module succesfully loaded\n");
		return 0;
	} else {
		outb(inb(I2C_DIR)&~(I2C_SDA|I2C_SCL), I2C_DIR);	
		release_region(I2C_DIR, IOSPACE);
		printk("i2c-via.o: Algo-bit error, couldn't register bus\n");
		return -ENODEV;
	}
}

EXPORT_NO_SYMBOLS;

#ifdef MODULE
MODULE_AUTHOR("Kyösti Mälkki <kmalkki@cc.hut.fi>");
MODULE_DESCRIPTION("i2c for Via vt82c586b southbridge");

int init_module(void) 
{
	return i2c_via_init();
}

void cleanup_module(void) 
{
	i2c_bit_del_bus(&bit_via_ops);
	release_region(I2C_DIR, IOSPACE);
}
#endif
