/*
    Motherboard SMBus (or i2c) Support for VIA VT82C586B South Bridge
    Based on bit-lp.c by Simon Vogl 

    Copyright (c) 1998 Kyösti Mälkki 

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

#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <asm/io.h>
#include <linux/types.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE < 0x020136 /* 2.1.54 */
#include <linux/bios32.h>
#endif


#include "i2c.h"
#include "algo-bit.h"

/* Power management registers */
#define PM_IO_BASE	pm_io_base
#define PM_IO_BASE_REGISTER 0x48
#define I2C_DIR		(PM_IO_BASE+0x40)
#define I2C_OUT		(PM_IO_BASE+0x42)
#define I2C_IN		(PM_IO_BASE+0x44)
#define I2C_SCL		0x02
#define I2C_SDA		0x04

/* io-region reservation */
#define IOSPACE		0x06
#define IOTEXT		"Via VT82C586B i2c"

/* ----- global defines -----------------------------------------------	*/
#define DEB(x)		/* should be reasonable open, close &c. 	*/
#define DEB2(x) 	/* low level debugging - very slow 		*/
#define DEBE(x)	x	/* error messages 				*/

/* ----- local functions ----------------------------------------------	*/

u32 pm_io_base;

static void bit_mb_setscl(void *data, int state)
{
  outb(state ? inb(I2C_OUT)|I2C_SCL : inb(I2C_OUT)&~I2C_SCL, I2C_OUT);
}

static void bit_mb_setsda(void *data, int state)
{
  outb(state ? inb(I2C_OUT)|I2C_SDA : inb(I2C_OUT)&~I2C_SDA, I2C_OUT);
}

static int bit_mb_getscl(void *data)
{
  return (0 != (inb(I2C_IN) & I2C_SCL) );
}

static int bit_mb_getsda(void *data)
{
  return (0 != (inb(I2C_IN) & I2C_SDA) );
}

static int bit_mb_reg(struct i2c_client *client)
{
	MOD_INC_USE_COUNT;
	return 0;
}

static int bit_mb_unreg(struct i2c_client *client)
{
	MOD_DEC_USE_COUNT;
	return 0;
}

/* ------------------------------------------------------------------------ */

struct bit_adapter bit_mb_ops = {
	"VT82C586B i2c",
	HW_B_MB,
	NULL,
	bit_mb_setsda,
	bit_mb_setscl,
	bit_mb_getsda,
	bit_mb_getscl,
	bit_mb_reg,
	bit_mb_unreg,
	10, 10, 100,		/*waits, timeout */
};

u32 pm_io_base=0;

static int bit_mb_init(void)
{
	if (check_region(I2C_DIR, IOSPACE) < 0 ) {
		return -ENODEV;
	} else {
		request_region(I2C_DIR, IOSPACE, IOTEXT);
		
		/* Set lines to output and make them high */
		outb(inb(I2C_DIR) | I2C_SCL | I2C_SDA, I2C_DIR);
		bit_mb_setsda(NULL, 1);
		bit_mb_setscl(NULL, 1);
	}
	return 0;
}

static void bit_mb_exit(void)
{
	release_region(I2C_DIR, IOSPACE);
}

/* When exactly was the new pci interface introduced? */
#if LINUX_VERSION_CODE >= 0x020136 /* 2.1.54 */

static u32 find_i2c(void)
{
	struct pci_dev *s_bridge;
	u32 val;
	
	if (! pci_present())
		return(0);
		
	s_bridge = pci_find_device(
		PCI_VENDOR_ID_VIA, PCI_DEVICE_ID_VIA_82C586_3, NULL);
	if (! s_bridge) return 0;

	if ( PCIBIOS_SUCCESSFUL !=
		pci_read_config_dword(s_bridge, PM_IO_BASE_REGISTER, &val) ) 
		return 0;

	return (val & (0xff<<8));
}

#else

static u32 find_i2c(void)
{
	unsigned char VIA_bus, VIA_devfn;
	u32 val;
	
	if (! pcibios_present())
		return(0);
		
	if(pcibios_find_device(
		PCI_VENDOR_ID_VIA, PCI_DEVICE_ID_VIA_82C586_3, 0, &VIA_bus, 
		&VIA_devfn))
		return 0;

	if ( PCIBIOS_SUCCESSFUL !=
                pcibios_read_config_dword(VIA_bus, VIA_devfn,
			PM_IO_BASE_REGISTER, &val))
		return 0;
	return (val & (0xff<<8));
}

#endif


#ifdef MODULE
MODULE_AUTHOR("Kyosti Malkki <kmalkki@cc.hut.fi>");
MODULE_DESCRIPTION("SMBus for VT82C586");

int init_module(void) 
{
	pm_io_base = find_i2c();	
	if (pm_io_base == 0) return -ENODEV;
	if (bit_mb_init()==0) {
		i2c_bit_add_bus(&bit_mb_ops);
	} else {
		return -ENODEV;
	}
	printk("Using i2c bus on motherboard\n");
	return 0;
}

void cleanup_module(void) 
{
	i2c_bit_del_bus(&bit_mb_ops);
	bit_mb_exit();
}
#endif
