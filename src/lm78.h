/*
    lm78.h - A Linux module for reading sensor data.
    Copyright (c) 1998  Frodo Looijaard <frodol@dds.nl>

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

#ifndef SENSORS_SRC_LM78
#define SENSORS_SRC_LM78

/* Length of ISA address segment */
#define LM78_EXTENT 8

/* Where are the ISA address/data registers relative to the base address */
#define LM78_ADDR_REG_OFFSET 5
#define LM78_DATA_REG_OFFSET 6

#endif /* ndef SENSORS_SRC_LM78 */
