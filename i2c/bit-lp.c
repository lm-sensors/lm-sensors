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
static char rcsid[] = "$Id: bit-lp.c,v 1.6 1998/12/30 08:36:08 i2c Exp i2c $";
/* ------------------------------------------------------------------------- */

#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <asm/io.h>

#include "i2c.h"
#include "algo-bit.h"

#define DEFAULT_BASE 0x378
int base=0;

/* Note: all we need to know is the base address of the parallel port, so
 * instead of having a dedicated struct to store this value, we store this
 * int in the pointer field (=bit_lp_ops.data) itself.
 */

/* Note2: as the hw that implements the i2c bus on the parallel port is 
 * incompatible with other epp stuff etc., we access the port exclusively
 * and don't cooperate with parport functions.
 */

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
#define BASE	(unsigned int)(data)
#define DATA	BASE			/* Centronics data port		*/
#define STAT	(BASE+1)		/* Centronics status port	*/
#define CTRL	(BASE+2)		/* Centronics control port	*/

/* ----- local functions ----------------------------------------------	*/

void bit_lp_setscl(void *data, int state)
{
	/*be cautious about state of the control register - 
		touch only the one bit needed*/
	if (state) {
		outb(inb(CTRL)|I2C_SCL,   CTRL);
	} else {
		outb(inb(CTRL)&I2C_CMASK, CTRL);
	}
}

void bit_lp_setsda(void *data, int state)
{
	if (state) {
		outb(I2C_DMASK , DATA);
	} else {
		outb(I2C_SDA , DATA);
	}
}

int bit_lp_getscl(void *data)
{
	return ( 0 != ( (inb(STAT)) & I2C_SCLIN ) );
}

int bit_lp_getsda(void *data)
{
	return ( 0 != ( (inb(STAT)) & I2C_SDAIN ) );
}

int bit_lp_init(void)
{
	if (check_region(base,(base == 0x3bc)? 3 : 8) < 0 ) {
		return -ENODEV;
	} else {
		request_region(base,(base == 0x3bc)? 3 : 8,
			"i2c (parallel port adapter)");
		/* reset hardware to sane state */
		bit_lp_setsda((void*)base,1);
		bit_lp_setscl((void*)base,1);
	}
	return 0;
}

void bit_lp_exit(void)
{
	release_region( base , (base == 0x3bc)? 3 : 8 );
}

int bit_lp_reg(struct i2c_client *client)
{
	MOD_INC_USE_COUNT;
	return 0;
}

int bit_lp_unreg(struct i2c_client *client)
{
	MOD_DEC_USE_COUNT;
	return 0;
}

/* ------------------------------------------------------------------------
 * Encapsulate the above functions in the correct operations structure.
 * This is only done when more than one hardware adapter is supported.
 */
struct bit_adapter bit_lp_ops = {
	"Philips Parallel port adapter",
	HW_B_LP,
	NULL,
	bit_lp_setsda,
	bit_lp_setscl,
	bit_lp_getsda,
	bit_lp_getscl,
	bit_lp_reg,
	bit_lp_unreg,
	80, 80, 100,		/*	waits, timeout */
};

#ifdef MODULE
MODULE_AUTHOR("Simon G. Vogl <simon@tk.uni-linz.ac.at>");
MODULE_DESCRIPTION("I2C-Bus adapter routines for Philips parallel port adapter");

MODULE_PARM(base, "i");

#ifndef LM_SENSORS
EXPORT_NO_SYMBOLS;
#endif

int init_module(void) 
{
	if (base==0) {
		/* probe some values */
		base=DEFAULT_BASE;
		bit_lp_ops.data=(void*)DEFAULT_BASE;
		if (bit_lp_init()==0) {
			i2c_bit_add_bus(&bit_lp_ops);
		} else {
			return -ENODEV;
		}
	} else {
		bit_lp_ops.data=(void*)base;
		if (bit_lp_init()==0) {
			i2c_bit_add_bus(&bit_lp_ops);
		} else {
			return -ENODEV;
		}
	}
	printk("bit_lp: found device at %#x.\n",base);
	return 0;
}

void cleanup_module(void) 
{
	i2c_bit_del_bus(&bit_lp_ops);
	bit_lp_exit();
}

#endif



