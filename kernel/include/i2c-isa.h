/*
    i2c-isa.h - Part of lm_sensors, Linux kernel modules for hardware
                monitoring
    Copyright (c) 1998, 1999  Frodo Looijaard <frodol@dds.nl>

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


#ifndef SENSORS_SENSORS_ISA_H
#define SENSORS_SENSORS_ISA_H

#ifdef __KERNEL__

#include <linux/i2c.h>

/* Detect whether we are on the isa bus. If this returns true, all i2c
   access will fail! */
#define i2c_is_isa_client(clientptr) \
        ((clientptr)->adapter->algo->id == I2C_ALGO_ISA)
#define i2c_is_isa_adapter(adapptr) \
        ((adapptr)->algo->id == I2C_ALGO_ISA)

#endif				/* def __KERNEL__ */

#endif				/* ndef SENSORS_ISA_H */
