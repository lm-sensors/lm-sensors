/* ------------------------------------------------------------------------- */
/* bit-velle.c i2c-hw access for Velleman K9000 adapters		     */
/* ------------------------------------------------------------------------- */
/*   Copyright (C) 1995-96 Simon G. Vogl

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
static char rcsid[] = "$Id: bit-velle.c,v 1.4 1998/12/30 08:36:08 i2c Exp i2c $";
/*
 * $Log: bit-velle.c,v $
 * Revision 1.4  1998/12/30 08:36:08  i2c
 * *** empty log message ***
 *
 * Revision 1.3  1998/09/15 18:50:04  i2c
 * *** empty log message ***
 *
 * Revision 1.2  1998/09/13 16:54:55  i2c
 * *** empty log message ***
 *
 * Revision 1.1  1998/07/29 08:09:35  i2c
 * Initial revision
 *
 * Revision 1.1  1998/01/20 10:01:29  i2c
 * Initial revision
 *
 * Revision 1.1  1996/11/17 11:00:03  i2c
 * Initial revision
 *
 *
 */

#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/string.h>  /* for 2.0 kernels to get NULL   */
#include <asm/errno.h>     /* for 2.0 kernels to get ENODEV */
#include <asm/io.h>

#include "i2c.h"
#include "algo-bit.h"

/* ----- global defines -----------------------------------------------	*/
#define DEB(x)		/* should be reasonable open, close &c. 	*/
#define DEB2(x) 	/* low level debugging - very slow 		*/
#define DEBE(x)	x	/* error messages 				*/

					/* Pin Port  Inverted	name	*/
#define I2C_SDA		0x02		/*  ctrl bit 1 	(inv)	*/
#define I2C_SCL		0x08		/*  ctrl bit 3 	(inv)	*/

#define I2C_SDAIN	0x10		/* stat bit 4		*/
#define I2C_SCLIN	0x08		/*  ctrl bit 3 (inv) (reads own output) */

#define I2C_DMASK	0xfd
#define I2C_CMASK	0xf7


/* --- Convenience defines for the parallel port:			*/
#define BASE	(unsigned int)(data)
#define DATA	BASE			/* Centronics data port		*/
#define STAT	(BASE+1)		/* Centronics status port	*/
#define CTRL	(BASE+2)		/* Centronics control port	*/

#define DEFAULT_BASE 0x378
int base=0;

/* ----- local functions --------------------------------------------------- */

void bit_velle_setscl(void *data, int state)
{
	if (state) {
		outb(inb(CTRL) & I2C_CMASK,   CTRL);
	} else {
		outb(inb(CTRL) | I2C_SCL, CTRL);
	}
	
}

void bit_velle_setsda(void *data, int state)
{
	if (state) {
		outb(inb(CTRL) & I2C_DMASK , CTRL);
	} else {
		outb(inb(CTRL) | I2C_SDA, CTRL);
	}
	
} 

int bit_velle_getscl(void *data)
{
	return ( 0 == ( (inb(CTRL)) & I2C_SCLIN ) );
}

int bit_velle_getsda(void *data)
{
	return ( 0 != ( (inb(STAT)) & I2C_SDAIN ) );
}

int bit_velle_init(void)
{
	if (check_region(base,(base == 0x3bc)? 3 : 8) < 0 ) {
		DEBE(printk("i2c_init: Port %#x already in use.\n", base));
		return -ENODEV;
	} else {
		request_region(base, (base == 0x3bc)? 3 : 8, 
			"i2c (Vellemann adapter)");
		bit_velle_setsda((void*)base,1);
		bit_velle_setscl((void*)base,1);
	}
	return 0;
}

void bit_velle_exit(void)
{	
	release_region( base , (base == 0x3bc)? 3 : 8 );
}


int bit_velle_reg(struct i2c_client *client)
{
	MOD_INC_USE_COUNT;
	return 0;
}

int bit_velle_unreg(struct i2c_client *client)
{
	MOD_DEC_USE_COUNT;
	return 0;
}

/* ------------------------------------------------------------------------
 * Encapsulate the above functions in the correct operations structure.
 * This is only done when more than one hardware adapter is supported.
 */
struct bit_adapter bit_velle_ops = {
	"Philips Parallel port adapter",
	HW_B_LP,
	NULL,
	bit_velle_setsda,
	bit_velle_setscl,
	bit_velle_getsda,
	bit_velle_getscl,
	bit_velle_reg,
	bit_velle_unreg,
	10, 10, 100,		/*	waits, timeout */
};

#ifdef MODULE
MODULE_AUTHOR("Simon G. Vogl <simon@tk.uni-linz.ac.at>");
MODULE_DESCRIPTION("I2C-Bus adapter routines for Velleman K9000 adapter");

MODULE_PARM(base, "i");

int init_module(void) 
{
	if (base==0) {
		/* probe some values */
		base=DEFAULT_BASE;
		bit_velle_ops.data=(void*)DEFAULT_BASE;
		if (bit_velle_init()==0) {
			i2c_bit_add_bus(&bit_velle_ops);
		} else {
			return -ENODEV;
		}
	} else {
		bit_velle_ops.data=(void*)base;
		if (bit_velle_init()==0) {
			i2c_bit_add_bus(&bit_velle_ops);
		} else {
			return -ENODEV;
		}
	}
	printk("bit_velle: found device at %#x.\n",base);
	return 0;
}

void cleanup_module(void) 
{
	i2c_bit_del_bus(&bit_velle_ops);
	bit_velle_exit();
}

#endif



