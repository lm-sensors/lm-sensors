/*
    piix4.h - Part of a Linux module for reading sensor data.
    Copyright (c) 1998  Alexander Larsson <alla@lysator.liu.se>,
    Frodo Looijaard <frodol@dds.nl> and Philip Edelbrock <phil@netroedge.com>

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

/* This file was taken almost directly from the first (lm_sensors) project */
/* Some things will probably need to change or will simply not be used.    */


#include <asm/types.h>
#include <linux/unistd.h>
/*
        I/O Space Constants
*/
#define SMBHSTSTS PIIX4_smba
#define SMBHSLVSTS PIIX4_smba + 1
#define SMBHSTCNT PIIX4_smba + 2
#define SMBHSTCMD PIIX4_smba + 3
#define SMBHSTADD PIIX4_smba + 4
#define SMBHSTDAT0 PIIX4_smba + 5
#define SMBHSTDAT1 PIIX4_smba + 6
#define SMBBLKDAT PIIX4_smba + 7
#define SMBSLVCNT PIIX4_smba + 8
#define SMBSHDWCMD PIIX4_smba + 9
#define SMBSLVEVT PIIX4_smba + 0xA
#define SMBSLVDAT PIIX4_smba + 0xC

/*
        PCI Address Constants
*/
#define SMBBA     0x090
#define SMBHSTCFG 0x0D2
#define SMBSLVC   0x0D3
#define SMBSHDW1  0x0D4
#define SMBSHDW2  0x0D5
#define SMBREV    0x0D6

/*
        Contants for other uses
*/
#define MAX_TIMEOUT 500
#define SMBUS_READ	1
#define SMBUS_WRITE	0
/* Set to 1 to enable (don't comment out completely)
   This is known to freeze some machines (as Frodo knows ;')
   in the current implemented state, so beware! */
#define  ENABLE_INT9 0

#define SMBUSTABLE_SIZE 64

/*
	Prototypes 
*/
extern int SMBus_PIIX4_Init(void);
extern void SMBus_PIIX4_Cleanup(void);
extern int SMBus_PIIX4_Access(u8 addr, char read_write, u8 command, int size,
                        union SMBus_Data *data);

extern int SMBus_PIIX4_Initialized;
