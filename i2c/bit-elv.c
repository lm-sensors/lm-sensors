/* ------------------------------------------------------------------------- */
/* bit-elv.c i2c-hw access for philips style parallel port adapters	     */
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
static char rcsid[] = "$Id: bit-elv.c,v 1.1 1998/09/05 18:20:09 i2c Exp $";
/*
 * $Log: bit-elv.c,v $
 * Revision 1.1  1998/09/05 18:20:09  i2c
 * Initial revision
 *
 * Revision 1.3  1998/01/20 10:01:29  i2c
 * *** empty log message ***
 *
 * Revision 1.2  1997/06/15 14:21:37  i2c
 * *** empty log message ***
 *
 *
 */

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <asm/io.h>
#include <linux/errno.h>
#include "i2c.h"
#include "i2c-priv.h"


/* ----- global defines -----------------------------------------------	*/
#define DEB(x)		/* should be reasonable open, close &c. 	*/
#define DEB2(x) 	/* low level debugging - very slow 		*/
#define DEBE(x)	x	/* error messages 				*/
#define DEBINIT(x) x	/* detection status messages			*/
/* ----- local functions ----------------------------------------------	*/
#if (BITADAPS) == 1
#define Local inline
#define bit_elv_getscl getscl
#define bit_elv_setscl setscl
#define bit_elv_getsda getsda
#define bit_elv_setsda setsda
#define bit_elv_bit_init bit_init
#define bit_elv_bit_exit bit_exit 
#else
#define Local static
#endif

#define portData(minor) ((char)((int)i2c_table[minor].minor_data))

Local void bit_elv_setscl(int minor, int state)
{
	if (state) {
		portData(minor)&=0xfe;
	} else {
		portData(minor)|=1;
	}
	outb(portData(minor), DATA);
}

Local void bit_elv_setsda(int minor, int state)
{
	if (state) {
		portData(minor)&=0xfd;
	} else {
		portData(minor)|=2;
	}
	outb(portData(minor), DATA);
} 

Local int bit_elv_getscl(int minor)
{
	return ( 0 == ( (inb_p(STAT)) & 0x08 ) );
}

Local int bit_elv_getsda(int minor)
{
	return ( 0 == ( (inb_p(STAT)) & 0x40 ) );
}

Local int bit_elv_bit_init(int minor)
{
	if (check_region(i2c_table[minor].base, 
		(i2c_table[minor].base == 0x3bc)? 3 : 8) < 0 ) {
		return -ENODEV;	
	} else {
						/* test for ELV adap. 	*/
		if (inb(STAT) & 0x80) {		/* BUSY should be high	*/
			DEBINIT(printk("i2c ELV: Busy was low.\n"));
			return -ENODEV;
		} else {
			outb(0x04,CTRL);	/* SLCT auf low		*/
			udelay(40);
			if ( inb(STAT) && 0x10 ) {
				outb(0x0c,CTRL);
				DEBINIT(printk("i2c ELV: Select was high.\n"));
				return -ENODEV;
			}
		}
		request_region(i2c_table[minor].base, 
			(i2c_table[minor].base == 0x3bc)? 3 : 8, 
			"i2c (ELV adapter)");
		portData(minor) = 0;
		outb(0,DATA);
	}
	DEBINIT(printk("i2c%d: scl: %d     sda %d \n",minor,
		bit_elv_getscl(minor),bit_elv_getsda(minor)) );
	return 0;
}

Local void bit_elv_bit_exit(int minor)
{
	outb(0,DATA);
	outb(0x0c,CTRL);
	release_region( i2c_table[minor].base , 
		(i2c_table[minor].base == 0x3bc)? 3 : 8 );
}


/* ------------------------------------------------------------------------
 * Encapsulate the above functions in the correct operations structure.
 * This is only done when more than one hardware adapter is supported.
 */
#if (BITADAPS) > 1
struct i2c_bit_opns bit_elv_ops = {
	bit_elv_setscl,
	bit_elv_setsda,
	bit_elv_getscl,
	bit_elv_getsda,
	bit_elv_bit_init,
	bit_elv_bit_exit
};
#endif
