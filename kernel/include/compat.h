/*
    compar.h - A Linux module for reading sensor data.
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

#ifndef SENSORS_COMPAT_H
#define SENSORS_COMPAT_H

/* This useful macro is not defined in the 2.0 kernels */

#include <linux/version.h>
#ifndef KERNEL_VERSION
#define KERNEL_VERSION(a,b,c) (((a) << 16) | ((b) << 8) | (c))
#endif

#ifdef MODULE
#include <linux/module.h>
#ifndef MODULE_AUTHOR
#define MODULE_AUTHOR(whatever)
#endif
#ifndef MODULE_DESCRIPTION
#define MODULE_DESCRIPTION(whatever)
#endif
#endif /* def MODULE */

#endif /* SENSORS_COMPAT_H */
