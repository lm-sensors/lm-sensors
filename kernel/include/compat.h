/*
    compat.h - Part of lm_sensors, Linux kernel modules for hardware
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

/* copy_from/to_usr is called memcpy_from/to_fs in 2.0 kernels 
   get_user was redefined in 2.1 kernels to use two arguments, and returns
   an error code */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,1,4))
#define copy_from_user memcpy_fromfs
#define copy_to_user memcpy_tofs
#define get_user_data(to,from) ((to) = get_user(from),0)
#else
#include <asm/uaccess.h>
#define get_user_data(to,from) get_user(to,from)
#endif

/* Add a scheduling fix for the new code in kernel 2.1.127 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,1,127))
#define schedule_timeout(x) ( current->timeout = jiffies + (x), schedule() )
#endif

/* If the new PCI interface is not present, fall back on the old PCI BIOS
   interface. We also define some things to unite both interfaces. Not
   very nice, but it works like a charm. 
   device is the 2.1 struct pci_dev, bus is the 2.0 bus number, dev is the
   2.0 device/function code, com is the PCI command, and res is the result. */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,1,54))
#define pci_present pcibios_present
#define pci_read_config_byte_united(device,bus,dev,com,res) \
                            pcibios_read_config_byte(bus,dev,com,res)
#define pci_read_config_word_united(device,bus,dev,com,res) \
                            pcibios_read_config_word(bus,dev,com,res)
#define pci_write_config_byte_united(device,bus,dev,com,res) \
                            pcibios_write_config_byte(bus,dev,com,res)
#define pci_write_config_word_united(device,bus,dev,com,res) \
                            pcibios_write_config_word(bus,dev,com,res)
#else
#define pci_read_config_byte_united(device,bus,dev,com,res) \
                            pci_read_config_byte(device,com,res)
#define pci_read_config_word_united(device,bus,dev,com,res) \
                            pci_read_config_word(device,com,res)
#define pci_write_config_byte_united(device,bus,dev,com,res) \
                            pci_write_config_byte(device,com,res)
#define pci_write_config_word_united(device,bus,dev,com,res) \
                            pci_write_config_byte(device,com,res)
#endif

/* I hope this is always correct, even for the PPC, but I really think so.
   And yes, the kernel version is exactly correct */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,1,0))
#include <linux/mm.h>
#define ioremap vremap
#define iounmap vfree
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,3,1))
#define init_MUTEX(s) do { *(s) = MUTEX; } while(0)
#endif

/* For old 2.0 kernels */
#ifndef PCI_DEVICE_ID_VIA_82C586_3  
#define PCI_DEVICE_ID_VIA_82C586_3  0x3040
#endif

#endif /* SENSORS_COMPAT_H */
