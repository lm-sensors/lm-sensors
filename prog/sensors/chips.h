/*
    chips.h - Part of sensors, a user-space program for hardware monitoring
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

#ifndef PROG_SENSORS_CHIPS_H
#define PROG_SENSORS_CHIPS_H

#include "lib/sensors.h"

extern void print_unknown_chip(const sensors_chip_name *name);

extern void print_asb100(const sensors_chip_name *name);
extern void print_ds1621(const sensors_chip_name *name);
extern void print_mtp008(const sensors_chip_name *name);
extern void print_lm75(const sensors_chip_name *name);
extern void print_adm1021(const sensors_chip_name *name);
extern void print_adm1024(const sensors_chip_name *name);
extern void print_adm1025(const sensors_chip_name *name);
extern void print_adm1026(const sensors_chip_name *name);
extern void print_adm9240(const sensors_chip_name *name);
extern void print_lm78(const sensors_chip_name *name);
extern void print_sis5595(const sensors_chip_name *name);
extern void print_via686a(const sensors_chip_name *name);
extern void print_gl518(const sensors_chip_name *name);
extern void print_gl520(const sensors_chip_name *name);
extern void print_lm80(const sensors_chip_name *name);
extern void print_lm85(const sensors_chip_name *name);
extern void print_w83781d(const sensors_chip_name *name);
extern void print_w83792d(const sensors_chip_name *name);
extern void print_w83l785ts(const sensors_chip_name *name);
extern void print_w83627ehf(const sensors_chip_name *name);
extern void print_maxilife(const sensors_chip_name *name);
extern void print_ddcmon(const sensors_chip_name *name);
extern void print_eeprom(const sensors_chip_name *name);
extern void print_lm87(const sensors_chip_name *name);
extern void print_it87(const sensors_chip_name *name);
extern void print_fscpos(const sensors_chip_name *name);
extern void print_fscscy(const sensors_chip_name *name);
extern void print_fscher(const sensors_chip_name *name);
extern void print_pcf8591(const sensors_chip_name *name);
extern void print_vt1211(const sensors_chip_name *name);
extern void print_smsc47m1(const sensors_chip_name *name);
extern void print_pc87360(const sensors_chip_name *name);
extern void print_pc87364(const sensors_chip_name *name);
extern void print_pc87366(const sensors_chip_name *name);
extern void print_lm92(const sensors_chip_name *name);
extern void print_vt8231(const sensors_chip_name *name);
extern void print_bmc(const sensors_chip_name *name);
extern void print_lm83(const sensors_chip_name *name);
extern void print_lm90(const sensors_chip_name *name);
extern void print_lm63(const sensors_chip_name *name);
extern void print_lm93(const sensors_chip_name *name);
extern void print_max1619(const sensors_chip_name *name);
extern void print_xeontemp(const sensors_chip_name *name);
extern void print_max6650(const sensors_chip_name *name);
extern void print_adm1031(const sensors_chip_name *name);
extern void print_smsc47b397(const sensors_chip_name *name);

#endif /* def PROG_SENSORS_CHIPS_H */
