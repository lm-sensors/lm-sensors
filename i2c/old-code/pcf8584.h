/* -------------------------------------------------------------------- */
/* PCF 8584 global defines						*/
/* -------------------------------------------------------------------- */
/*   Copyright (C) 19996 Simon G. Vogl

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
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.		*/
/* --------------------------------------------------------------------	*/
/* $Id: pcf8584.h,v 1.1 1998/07/29 08:09:51 i2c Exp $
 * $Log: pcf8584.h,v $
 * Revision 1.1  1998/07/29 08:09:51  i2c
 * Initial revision
 *
 */


/* ----- Control register bits ----------------------------------------	*/
#define PCF_PIN  0x80
#define PCF_ESO  0x40
#define PCF_ES1  0x20
#define PCF_ES2  0x10
#define PCF_ENI  0x08
#define PCF_STA  0x04
#define PCF_STO  0x02
#define PCF_ACK  0x01

/* ----- Status register bits -----------------------------------------	*/
/*#define PCF_PIN  0x80    as above*/

#define PCF_INI 0x40   /* 1 if not initialized */
#define PCF_STS 0x20   
#define PCF_BER 0x10
#define PCF_AD0 0x08
#define PCF_LRB 0x08
#define PCF_AAS 0x04
#define PCF_LAB 0x02
#define PCF_BB  0x01

/* ----- Chip clock frequencies ---------------------------------------	*/
#define PCF_CLK3    0x00
#define PCF_CLK443  0x10
#define PCF_CLK6    0x14
#define PCF_CLK8    0x18
#define PCF_CLK12   0x1c

/* ----- transmission frequencies -------------------------------------	*/
#define PCF_TRNS90 0x00  /*  90 kHz */
#define PCF_TRNS45 0x01  /*  45 kHz */
#define PCF_TRNS11 0x02  /*  11 kHz */
#define PCF_TRNS15 0x03  /* 1.5 kHz */


/* ----- Access to internal registers according to ES1,ES2 ------------	*/
/* they are mapped to the data port ( a0 = 0 ) 				*/
/* available when ESO == 0 :						*/

#define PCF_OWNADR	0
#define PCF_INTREG	PCF_ES2
#define PCF_CLKREG	PCF_ES1


