/* ------------------------------------------------------------------------- */
/* bit-lp.c i2c-hw access for philips style parallel port adapters	     */
/* ------------------------------------------------------------------------- */
/*   Copyright (C) 1995-97 Simon G. Vogl

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
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.		     */
/* ------------------------------------------------------------------------- */
static char rcsid[] = "$Id: bit-lp-multi.c,v 1.1 1998/09/05 18:20:09 i2c Exp $";
/* ------------------------------------------------------------------------- */

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <asm/io.h>


#include "i2c.h"
#include "algo-bit.h"


#define BIT_LP_MAX	3	/* max. number of addresses to probe	*/

struct bit_lp_data {
	unsigned int  base;	/* base address				*/
};

static struct bit_lp_data bit_lp_adaps[BIT_LP_MAX] = {
	{0,},
	{0,},
	{0,},
	};
static int adap_count=0;

/* ----- global defines -----------------------------------------------	*/
#define DEB(x)		/* should be reasonable open, close &c. 	*/
#define DEB2(x) 	/* low level debugging - very slow 		*/
#define DEBE(x)	x	/* error messages 				*/

/* ----- printer port defines ------------------------------------------*/
					/* Pin Port  Inverted	name	*/
#define I2C_ON		0x20		/* 12 status N	paper		*/
					/* ... only for phil. not used  */
#define I2C_SDA		0x80		/*  9 data   N	data7		*/
#define I2C_SCL		0x08		/* 17 ctrl   N	dsel		*/

#define I2C_SDAIN	0x80		/* 11 stat   Y	busy		*/
#define I2C_SCLIN	0x08		/* 15 stat   Y	enable		*/

#define I2C_DMASK	0x7f
#define I2C_CMASK	0xf7

/* --- Convenience defines for the parallel port:			*/
#define BASE	bit_lp_adaps[minor].base
#define DATA	BASE			/* Centronics data port		*/
#define STAT	(BASE+1)		/* Centronics status port	*/
#define CTRL	(BASE+2)		/* Centronics control port	*/

/* ----- local functions ----------------------------------------------	*/

static void bit_lp_setscl(int minor, int state)
{
	if (state) {
		outb(I2C_SCL,   CTRL);
	} else {
		outb(I2C_CMASK, CTRL);
	}
}

static void bit_lp_setsda(int minor, int state)
{
	if (state) {
		outb(I2C_DMASK , DATA);
	} else {
		outb(I2C_SDA , DATA);
	}
}

static int bit_lp_getscl(int minor)
{
	return ( 0 != ( (inb(STAT)) & I2C_SCLIN ) );
}

static int bit_lp_getsda(int minor)
{
	return ( 0 != ( (inb(STAT)) & I2C_SDAIN ) );
}

static int bit_lp_init(int minor)
{
	if (check_region(bit_lp_adaps[minor].base,
		(bit_lp_adaps[minor].base == 0x3bc)? 3 : 8) < 0 ) {
		return -ENODEV;	
	} else {
		request_region(bit_lp_adaps[minor].base, 
			(bit_lp_adaps[minor].base == 0x3bc)? 3 : 8, 
			"i2c (parallel port adapter)");
		bit_lp_setsda(minor,1);
		bit_lp_setscl(minor,1);
	}
	printk("i2c%d: scl: %d     sda %d \n",minor,bit_lp_getscl(minor),bit_lp_getsda(minor));
	return 0;
}

static void bit_lp_exit(int minor)
{	
	release_region( bit_lp_adaps[minor].base , 
		(bit_lp_adaps[minor].base == 0x3bc)? 3 : 8 );
	release_region( bit_lp_adaps[minor].base + 0x0400, 8 );
}


/* ------------------------------------------------------------------------
 * Encapsulate the above functions in the correct operations structure.
 * This is only done when more than one hardware adapter is supported.
 */
struct bit_adapter bit_lp_ops = {
	"Philips Parallel port adapter",
	bit_lp_setscl,
	bit_lp_setsda,
	bit_lp_getscl,
	bit_lp_getsda,
	bit_lp_init,
	bit_lp_exit
};

#ifdef MODULE
static int base[BIT_LP_MAX+1] = { [0 ... BIT_LP_MAX] = 0 };
MODULE_PARM(base, "1-" __MODULE_STRING(BIT_LP_MAX) "i");

/*
EXPORT_SYMBOL(i2c_bit_add_bus);
EXPORT_SYMBOL(i2c_bit_del_bus);
*/
int init_module(void) 
{
       /* Work out how many ports we have, then get parport_share to parse
          the irq values. */
	unsigned int i;
	for (i = 0; i < BIT_LP_MAX && base[i]; i++) {
		bit_lp_adaps[adap_count].base = base[i];
		if (bit_lp_init(adap_count)==0) {
/*			i2c_bit_add_bus(struct bit_adapter *algo);*/
/*			bit_add_adapter();*/
			adap_count++;
		}
	}
	if (i==0) {			/* no values specified -> probe */
	}

/*      count += probe_one_port(0x3bc, irq[0], dma[0]);
        count += probe_one_port(0x378, irq[0], dma[0]);
        count += probe_one_port(0x278, irq[0], dma[0]);
*/
	printk("bit_lp: found %d devices.\n",adap_count);

	if (adap_count==0)
		return -ENODEV;
	else
		return 0;
}

void cleanup_module(void) 
{
	int i;
	for (i=0;i<adap_count;i++) {
		int i2c_bit_del_bus(struct bit_adapter *algo);
		bit_lp_exit(i);
	}
/*	i2c_del_algorithm(&alg_bit_opns);*/
}

#endif



