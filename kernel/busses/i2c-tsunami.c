/*
    i2c-tsunami.c - Part of lm_sensors, Linux kernel modules for hardware
              monitoring
    Copyright (c) 2001  Oleg Vdovikin <vdovikin@jscc.ru>
    
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

/* This interfaces to the I2C bus of the Tsunami/Typhoon 21272 chipsets 
   to gain access to the on-board I2C devices. 

   For more information refer to Compaq's 
	"Tsunami/Typhoon 21272 Chipset Hardware Reference Manual"
	Order Number: DS-0025-TE
*/ 

#include <linux/version.h>
#include <linux/module.h>
#include <asm/io.h>
#include <asm/hwrpb.h>
#include <asm/core_tsunami.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>
#include "version.h"
#include <linux/init.h>

MODULE_LICENSE("GPL");

/* Memory Presence Detect Register (MPD-RW) bits 
   with except of reserved RAZ bits */

#define MPD_DR	0x8	/* Data receive - RO */
#define MPD_CKR	0x4	/* Clock receive - RO */
#define MPD_DS	0x2	/* Data send - Must be a 1 to receive - WO */
#define MPD_CKS	0x1	/* Clock send - WO */

static inline void writempd(unsigned long value)
{
	TSUNAMI_cchip->mpd.csr = value;
	mb();
}

static inline unsigned long readmpd(void)
{
	return TSUNAMI_cchip->mpd.csr;
}

static void bit_tsunami_setscl(void *data, int val)
{
	/* read currently setted bits to modify them */
	unsigned long bits = readmpd() >> 2; /* assume output == input */

	if (val)
		bits |= MPD_CKS;
	else
		bits &= ~MPD_CKS;

	writempd(bits);
}

static void bit_tsunami_setsda(void *data, int val)
{
	/* read currently setted bits to modify them */
	unsigned long bits = readmpd() >> 2; /* assume output == input */

	if (val)
		bits |= MPD_DS;
	else
		bits &= ~MPD_DS;

	writempd(bits);
}

/* The MPD pins are open drain, so the pins always remain outputs.
   We rely on the i2c-algo-bit routines to set the pins high before
   reading the input from other chips. */

static int bit_tsunami_getscl(void *data)
{
	return (0 != (readmpd() & MPD_CKR));
}

static int bit_tsunami_getsda(void *data)
{
	return (0 != (readmpd() & MPD_DR));
}

static struct i2c_algo_bit_data tsunami_i2c_bit_data = {
	
	bit_tsunami_setsda,
	bit_tsunami_setscl,
	bit_tsunami_getsda,
	bit_tsunami_getscl,
	10, 10, HZ/2	/* delays/timeout */
};

static struct i2c_adapter tsunami_i2c_adapter = {
	.owner		= THIS_MODULE,
	.name		= "I2C Tsunami/Typhoon adapter",
	.id		= I2C_HW_B_TSUNA,
	.algo_data		= &tsunami_i2c_bit_data,
};


static struct pci_driver tsunami_driver = {
	.name		= "tsunami smbus",
	.id_table	= tsunami_ids,
	.probe		= tsunami_probe,
	.remove		= __devexit_p(tsunami_remove),
};

static int __init i2c_tsunami_init(void)
{
	printk("i2c-tsunami.o version %s (%s)\n", LM_VERSION, LM_DATE);

	if (hwrpb->sys_type != ST_DEC_TSUNAMI) {
		printk("i2c-tsunami.o: not Tsunami based system (%d), module not inserted.\n", hwrpb->sys_type);
		return -ENXIO;
	} else {
		printk("i2c-tsunami.o: using Cchip MPD at 0x%lx.\n", &TSUNAMI_cchip->mpd);
	}
	i2c_bit_add_bus(&tsunami_i2c_adapter);
}


static void __exit i2c_tsunami_exit(void)
{
	i2c_bit_del_bus(&tsunami_i2c_adapter);
}



MODULE_AUTHOR("Oleg I. Vdovikin <vdovikin@jscc.ru>");
MODULE_DESCRIPTION("Tsunami I2C/SMBus driver");

module_init(i2c_tsunami_init);
module_exit(i2c_tsunami_exit);
