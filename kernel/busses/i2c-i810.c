/*
    i2c-i810.c - Part of lm_sensors, Linux kernel modules for hardware
              monitoring
    Copyright (c) 1998, 1999, 2000  Frodo Looijaard <frodol@dds.nl>,
    Philip Edelbrock <phil@netroedge.com>,
    Ralph Metzler <rjkm@thp.uni-koeln.de>, and
    Mark D. Studebaker <mdsxyz123@yahoo.com>
    
    Based on code written by Ralph Metzler <rjkm@thp.uni-koeln.de> and
    Simon Vogl

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
   This interfaces to the I810/I815 to provide access to
   the DDC Bus and the I2C Bus.

   SUPPORTED DEVICES	PCI ID
   i810AA		7121           
   i810AB		7123           
   i810E		7125           
   i815			1132           
*/


#include <linux/version.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <asm/io.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>
#include "version.h"
#include <linux/init.h>

#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif

/* PCI defines */
#ifndef PCI_DEVICE_ID_INTEL_82810_IG1
#define PCI_DEVICE_ID_INTEL_82810_IG1 0x7121
#endif
#ifndef PCI_DEVICE_ID_INTEL_82810_IG3
#define PCI_DEVICE_ID_INTEL_82810_IG3 0x7123
#endif
#ifndef PCI_DEVICE_ID_INTEL_82815_2
#define PCI_DEVICE_ID_INTEL_82815_2   0x1132
#endif

static int i810_supported[] = {PCI_DEVICE_ID_INTEL_82810_IG1,
                               PCI_DEVICE_ID_INTEL_82810_IG3,
                               0x7125,
                               PCI_DEVICE_ID_INTEL_82815_2,
                               0 };

/* GPIO register locations */
#define I810_IOCONTROL_OFFSET 0x5000
#define I810_HVSYNC	0x00	/* not used */
#define I810_GPIOA	0x10
#define I810_GPIOB	0x14

/* bit locations in the registers */
#define SCL_DIR_MASK	0x0001
#define SCL_DIR		0x0002
#define SCL_VAL_MASK	0x0004
#define SCL_VAL_OUT	0x0008
#define SCL_VAL_IN	0x0010
#define SDA_DIR_MASK	0x0100
#define SDA_DIR		0x0200
#define SDA_VAL_MASK	0x0400
#define SDA_VAL_OUT	0x0800
#define SDA_VAL_IN	0x1000

/* initialization states */
#define INIT1	0x1
#define INIT2	0x2
#define INIT3	0x4

/* delays */
#define CYCLE_DELAY		10
#define TIMEOUT			50

#ifdef MODULE
static
#else
extern
#endif
int __init i2c_i810_init(void);
static int __init i810i2c_cleanup(void);
static int i810i2c_setup(void);
static void config_i810(struct pci_dev *dev);
static void i810_inc(struct i2c_adapter *adapter);
static void i810_dec(struct i2c_adapter *adapter);

#ifdef MODULE
extern int init_module(void);
extern int cleanup_module(void);
#endif				/* MODULE */

static int __initdata i810i2c_initialized;
static unsigned char *mem;

static inline void outlong(unsigned int dat, int off)
{
	*((unsigned int *) (mem + off)) = dat;
}

static inline unsigned int readlong(int off)
{
	return *((unsigned int *) (mem + off));
}

/* The i810 GPIO registers have individual masks for each bit
   so we never have to read before writing. Nice. */

static void bit_i810i2c_setscl(void *data, int val)
{
	outlong((val ? SCL_VAL_OUT : 0) | SCL_DIR | SCL_DIR_MASK | SCL_VAL_MASK,
	     I810_GPIOB);
}

static void bit_i810i2c_setsda(void *data, int val)
{
 	outlong((val ? SDA_VAL_OUT : 0) | SDA_DIR | SDA_DIR_MASK | SDA_VAL_MASK,
	     I810_GPIOB);
}

/* The GPIO pins are open drain, so the pins always remain outputs.
   We rely on the i2c-algo-bit routines to set the pins high before
   reading the input from other chips. Following guidance in the 815
   prog. ref. guide, we do a "dummy write" of 0 to the register before
   reading which forces the input value to be latched. We presume this
   applies to the 810 as well. This is necessary to get
   i2c_algo_bit bit_test=1 to pass. */

static int bit_i810i2c_getscl(void *data)
{
	outlong(0, I810_GPIOB);
	return (0 != (readlong(I810_GPIOB) & SCL_VAL_IN));
}

static int bit_i810i2c_getsda(void *data)
{
	outlong(0, I810_GPIOB);
	return (0 != (readlong(I810_GPIOB) & SDA_VAL_IN));
}

static void bit_i810ddc_setscl(void *data, int val)
{
	outlong((val ? SCL_VAL_OUT : 0) | SCL_DIR | SCL_DIR_MASK | SCL_VAL_MASK,
	     I810_GPIOA);
}

static void bit_i810ddc_setsda(void *data, int val)
{
 	outlong((val ? SDA_VAL_OUT : 0) | SDA_DIR | SDA_DIR_MASK | SDA_VAL_MASK,
	     I810_GPIOA);
}

static int bit_i810ddc_getscl(void *data)
{
	outlong(0, I810_GPIOA);
	return (0 != (readlong(I810_GPIOA) & SCL_VAL_IN));
}

static int bit_i810ddc_getsda(void *data)
{
	outlong(0, I810_GPIOA);
	return (0 != (readlong(I810_GPIOA) & SDA_VAL_IN));
}

static struct i2c_algo_bit_data i810_i2c_bit_data = {
	NULL,
	bit_i810i2c_setsda,
	bit_i810i2c_setscl,
	bit_i810i2c_getsda,
	bit_i810i2c_getscl,
	CYCLE_DELAY, CYCLE_DELAY, TIMEOUT
};

static struct i2c_adapter i810_i2c_adapter = {
	"I810/I815 I2C Adapter",
	I2C_HW_B_I810,
	NULL,
	&i810_i2c_bit_data,
	i810_inc,
	i810_dec,
	NULL,
	NULL,
};

static struct i2c_algo_bit_data i810_ddc_bit_data = {
	NULL,
	bit_i810ddc_setsda,
	bit_i810ddc_setscl,
	bit_i810ddc_getsda,
	bit_i810ddc_getscl,
	CYCLE_DELAY, CYCLE_DELAY, TIMEOUT
};

static struct i2c_adapter i810_ddc_adapter = {
	"I810/I815 DDC Adapter",
	I2C_HW_B_I810,
	NULL,
	&i810_ddc_bit_data,
	i810_inc,
	i810_dec,
	NULL,
	NULL,
};


/* Configures the chip */
void config_i810(struct pci_dev *dev)
{
	unsigned long cadr;

	/* map I810 memory */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,3,13)
	cadr = dev->resource[1].start;
#else
	cadr = dev->base_address[1];
#endif
	cadr += I810_IOCONTROL_OFFSET;
	cadr &= PCI_BASE_ADDRESS_MEM_MASK;
	mem = ioremap_nocache(cadr, 0x1000);
	bit_i810i2c_setscl(NULL, 1);
	bit_i810i2c_setsda(NULL, 1);
	bit_i810ddc_setscl(NULL, 1);
	bit_i810ddc_setsda(NULL, 1);
}

/* Detect whether a supported device can be found,
   and initialize it */
static int i810i2c_setup(void)
{
	struct pci_dev *dev = NULL;
	int *num = i810_supported;

	do {
		if ((dev = pci_find_device(PCI_VENDOR_ID_INTEL,
					   *num++, dev))) {
			config_i810(dev);
			printk("i2c-i810.o: i810/i815 found.\n");
			return 0;
		}
	} while (*num != 0);

	return -ENODEV;
}


void i810_inc(struct i2c_adapter *adapter)
{
	MOD_INC_USE_COUNT;
}

void i810_dec(struct i2c_adapter *adapter)
{
	MOD_DEC_USE_COUNT;
}

int __init i2c_i810_init(void)
{
	int res;
	printk("i2c-i810.o version %s (%s)\n", LM_VERSION, LM_DATE);

	i810i2c_initialized = 0;
	if ((res = i810i2c_setup())) {
		printk
		    ("i2c-i810.o: i810/i815 not detected, module not inserted.\n");
		i810i2c_cleanup();
		return res;
	}
	if ((res = i2c_bit_add_bus(&i810_i2c_adapter))) {
		printk("i2c-i810.o: I2C adapter registration failed\n");
	} else {
		printk("i2c-i810.o: I810/I815 I2C bus initialized\n");
		i810i2c_initialized |= INIT2;
	}
	if ((res = i2c_bit_add_bus(&i810_ddc_adapter))) {
		printk("i2c-i810.o: DDC adapter registration failed\n");
	} else {
		printk("i2c-i810.o: I810/I815 DDC bus initialized\n");
		i810i2c_initialized |= INIT3;
	}
	if(!(i810i2c_initialized & (INIT2 | INIT3))) {
		printk("i2c-i810.o: Both registrations failed, module not inserted\n");
		i810i2c_cleanup();
		return res;
	}
	return 0;
}

int __init i810i2c_cleanup(void)
{
	int res;

	iounmap(mem);
	if (i810i2c_initialized & INIT3) {
		if ((res = i2c_bit_del_bus(&i810_ddc_adapter))) {
			printk
			    ("i2c-i810.o: i2c_del_adapter failed, module not removed\n");
			return res;
		}
	}
	if (i810i2c_initialized & INIT2) {
		if ((res = i2c_bit_del_bus(&i810_i2c_adapter))) {
			printk
			    ("i2c-i810.o: i2c_del_adapter failed, module not removed\n");
			return res;
		}
	}
	i810i2c_initialized = 0;
	return 0;
}

EXPORT_NO_SYMBOLS;

#ifdef MODULE

MODULE_AUTHOR
    ("Frodo Looijaard <frodol@dds.nl>, Philip Edelbrock <phil@netroedge.com>, Ralph Metzler <rjkm@thp.uni-koeln.de>, and Mark D. Studebaker <mdsxyz123@yahoo.com>");
MODULE_DESCRIPTION("I810/I815 I2C/DDC driver");


int init_module(void)
{
	return i2c_i810_init();
}

int cleanup_module(void)
{
	return i810i2c_cleanup();
}

#endif				/* MODULE */
