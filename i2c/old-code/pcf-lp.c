/* ------------------------------------------------------------------------- */
/* pcf-lp.c i2c-hw access for PCF 8584 on bidir. parallel ports		     */
/* ------------------------------------------------------------------------- */
/* 
-----.-.---....------..-.-.---.... Oh bugger! Hope it works this time......

    Copyright (C) 1995-97 Simon G. Vogl

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
static char rcsid[] = "$Id: pcf-lp.c,v 1.1 1998/09/05 18:20:09 i2c Exp $";
/*
 * $Log: pcf-lp.c,v $
 * Revision 1.1  1998/09/05 18:20:09  i2c
 * Initial revision
 *
 * Revision 1.4  1998/01/20 10:01:29  i2c
 * *** empty log message ***
 *
 * Revision 1.3  1997/06/15 14:21:37  i2c
 * removed debugging messages
 *
 * Revision 1.2  1997/06/03 06:00:10  i2c
 * first version that works
 *
 * Revision 1.2  1996/11/20 20:20:46  i2c
 * first version for ISA bus.
 *
 * Revision 1.1  1996/11/17 11:00:03  i2c
 * Initial revision
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

#define DEBHW(x) x 	/* lowest level hardware debug -- log changes of*/
			/* the bit lines...				*/
			
#define lpPort(port)	(i2c_table[minor].base+(port))


/* ----- local variables ----------------------------------------------	*/

static unsigned char ctrl;			/* remember line states	*/ 

						/* (re)set line states	*/
#define Strobe(arg) if (!arg) ctrl|= 0x01; else ctrl &=!0x01;\
			outb_p(ctrl,lpPort(2))	/* Strobe -- pin 1	*/
#define AutoLF(arg) if (!arg) ctrl|= 0x02; else ctrl &=!0x02;\
			outb_p(ctrl,lpPort(2))	/* AutoLF -- pin 14	*/
#define Init(arg)   if ( arg) ctrl|= 0x04; else ctrl &=!0x04;\
			outb(ctrl,lpPort(2))	/* Init   -- pin 16	*/
#define Select(arg) if (!arg) ctrl|= 0x08; else ctrl &=!0x08;\
			outb_p(ctrl,lpPort(2))	/* Sel. in - pin 17	*/
						/* 0x10 -- Int Enable	*/
#define ReadBit(arg) if(arg) ctrl|= 0x20; else ctrl &=!0x20;\
			outb_p(ctrl,lpPort(2))	/* direction bit	*/

#define inStr() (! (0x01 & inb(lpPort(2)) ) )	/* Strobe -- pin 1	*/
#define inALF() (! (0x02 & inb(lpPort(2)) ) )	/* AutoLF -- pin 14	*/
#define inIni() (  (0x04 & inb(lpPort(2)) ) )	/* Init   -- pin 16	*/
#define inSel() (! (0x08 & inb(lpPort(2)) ) )	/* Sel. in - pin 17	*/
						/* 0x10 -- Int Enable	*/
/*						
#define Argl() printk("(wr-%d cs-%d a0-%d  (%#2x, %#2x))\n", \
		inStr(),inALF(),inIni(),inb(lpPort(0)),inb(lpPort(1)) ) 
*/
#define Argl() /**/

#define wr(arg)	Strobe(arg); Argl()		/* write line		*/
#define cs(arg)	AutoLF(arg); Argl()		/* chip select		*/
#define a0(arg)	Init  (arg); Argl()		/* address line		*/
        
/* ----- local functions ----------------------------------------------	*/
/*
#if (PCFADAPS) > 1
#  define Local static
#else
#  define Local inline
#endif
*/
#define Local /**/

/* --------- okay, I borrowed it. Stays here for debugging.
 * check the epp status. After a EPP transfer, it should be true that
 * 1) the TIMEOUT bit (SPP_STR.0) is clear
 * 2) the READY bit (SPP_STR.7) is set
 * returns 1 if okay
 */
static int ex_ppa_check_epp_status(int minor)
{
	char r;
	inb(STAT);
	r = inb(STAT);

	if (r & 1) {
		outb(r, STAT);
		outb(r&0xfe, STAT);
		printk("timed out on port 0x%04x\n", BASE);
		return 0;
	}
	if (!(r & 0x80)) {
		return 0;
	}
	return 1;
}


Local	int  pcf_read (int minor, int adr)
{
	int ret;
	a0(adr);
	ex_ppa_check_epp_status(minor);
	ret = inb(lpPort(4));
	return ret;
}


Local	void pcf_write (int minor, int adr,char data)
{
	a0(adr);
	ex_ppa_check_epp_status(minor);
	outb(data,lpPort(4));
}


Local	int  pcf_init (int minor)
{
	char ctrl,ecr;		 	/* parallel port registers 	*/

	if (check_region(i2c_table[minor].base, 8) < 0 ) {
		return -ENODEV;	
	} else {
		request_region(i2c_table[minor].base,  8, "i2c (pcf-lp)");
	}

	/* This is the standard detection algorithm for ECP ports	*/
	/* now detect, what kind of parallel port we have:		*/
	ecr = inb(i2c_table[minor].base+0x402);
	ctrl= inb(i2c_table[minor].base+0x002);
	if (ecr!=ctrl) {		/* Okay, we seem to have an extended port :)	*/
		if (check_region(i2c_table[minor].base + 0x400, 8) < 0 ) {
			DEBE(printk(	"i2c_init, port %#x: ext. ports already in use "
				"using only standard driver.\n",
				i2c_table[minor].base));
			return -ENODEV;
		} else {
			if ( (ecr & 0x03) != 0x01 ) {	/* FIFO empty? 	*/
				printk("FIFO not empty\n");
				return -ENODEV;	
			}

			outb(0x34,i2c_table[minor].base+0x402);
			if (inb(i2c_table[minor].base+0x402) != 0x35 ){	/* bits 0,1 readonly	*/
				printk("bits 0,1 not rd/only");
				return -ENODEV;
			}
			/* Okay, now we should have an ECP-capable printer port	*/
			printk("i2c%d: ECP parallel port.\n",minor);
			request_region(i2c_table[minor].base+0x0400, 3 , 
				"i2c (Extended Parallel port adapter)");
			outb(0x94 ,lpPort(0x402));	/* EPP mode 	*/
			ctrl = inb(CTRL);

			if (i2c_table[minor].irq > 0){	/* enable int.	*/
				outb(inb(lpPort(0x402)|0x10), lpPort(0x402));
				outb(ctrl|0x10,CTRL);
			}
			
			ex_ppa_check_epp_status(minor);
			printk("status reads %2x\n",pcf_read(minor,1));
			ex_ppa_check_epp_status(minor);
			printk("data   reads %2x\n",pcf_read(minor,0));
		}
	} else {
		printk("i2c%d: Need EPP port. Sorry.\n",minor);
		return -ENODEV;
	}
	/* Okay, now it's time to init my data structures */
	
	return 0;
}


Local	void pcf_exit	(int minor)
{
	release_region( i2c_table[minor].base,  8 );
	release_region( i2c_table[minor].base + 0x0400,  3 );
}


/* ------------------------------------------------------------------------
 * Encapsulate the above functions in the correct operations structure.
 * This is only done when more than one hardware adapter is supported.
 */
#if (PCFADAPS) > 1
struct i2c_pcf_ops pcf_lp_ops = {
	pcf_read,
	pcf_write,
	pcf_init,
	pcf_exit
};
#endif
