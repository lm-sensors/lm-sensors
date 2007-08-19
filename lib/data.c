/*
    data.c - Part of libsensors, a Linux library for reading sensor data.
    Copyright (c) 1998, 1999  Frodo Looijaard <frodol@dds.nl>
    Copyright (C) 2007        Jean Delvare <khali@linux-fr.org>

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

/* this define needed for strndup() */
#define _GNU_SOURCE

#include <stdlib.h>
#include <string.h>

#include "access.h"
#include "error.h"
#include "data.h"
#include "sensors.h"
#include "../version.h"

const char *libsensors_version = LM_VERSION;

sensors_chip *sensors_config_chips = NULL;
int sensors_config_chips_count = 0;
int sensors_config_chips_max = 0;

sensors_bus *sensors_config_busses = NULL;
int sensors_config_busses_count = 0;
int sensors_config_busses_max = 0;

sensors_chip_features *sensors_proc_chips = NULL;
int sensors_proc_chips_count = 0;
int sensors_proc_chips_max = 0;

sensors_bus *sensors_proc_bus = NULL;
int sensors_proc_bus_count = 0;
int sensors_proc_bus_max = 0;

static int sensors_substitute_chip(sensors_chip_name *name,int lineno);

/*
   Parse a chip name to the internal representation. These are valid names:

     lm78-i2c-10-5e		*-i2c-10-5e
     lm78-i2c-10-*		*-i2c-10-*
     lm78-i2c-*-5e		*-i2c-*-5e
     lm78-i2c-*-*		*-i2c-*-*
     lm78-isa-10dd		*-isa-10dd
     lm78-isa-*			*-isa-*
     lm78-*			*-*

   Here 'lm78' can be any prefix. 'i2c' and 'isa' are
   literal strings, just like all dashes '-' and wildcards '*'. '10' can
   be any decimal i2c bus number. '5e' can be any hexadecimal i2c device
   address, and '10dd' any hexadecimal isa address.

   The 'prefix' part in the result is freshly allocated. All old contents
   of res is overwritten. res itself is not allocated. In case of an error
   return (ie. != 0), res is undefined, but all allocations are undone.
*/

int sensors_parse_chip_name(const char *name, sensors_chip_name *res)
{
	char *dash;

	/* First, the prefix. It's either "*" or a real chip name. */
	if (!strncmp(name, "*-", 2)) {
		res->prefix = SENSORS_CHIP_NAME_PREFIX_ANY;
		name += 2;
	} else {
		if (!(dash = strchr(name, '-')))
			return -SENSORS_ERR_CHIP_NAME;
		res->prefix = strndup(name, dash - name);
		if (!res->prefix)
			sensors_fatal_error("sensors_parse_chip_name",
					    "Allocating name prefix");
		name = dash + 1;
	}

	/* Then we have either a sole "*" (all chips with this name) or a bus
	   type and an address. */
	if (!strcmp(name, "*")) {
		res->bus.type = SENSORS_BUS_TYPE_ANY;
		res->bus.nr = SENSORS_BUS_NR_ANY;
		res->addr = SENSORS_CHIP_NAME_ADDR_ANY;
		return 0;
	}

	if (!(dash = strchr(name, '-')))
		goto ERROR;
	if (!strncmp(name, "i2c", dash - name))
		res->bus.type = SENSORS_BUS_TYPE_I2C;
	else if (!strncmp(name, "isa", dash - name))
		res->bus.type = SENSORS_BUS_TYPE_ISA;
	else if (!strncmp(name, "pci", dash - name))
		res->bus.type = SENSORS_BUS_TYPE_PCI;
	else
		goto ERROR;
	name = dash + 1;

	/* Some bus types (i2c) have an additional bus number. For these, the
	   next part is either a "*" (any bus of that type) or a decimal
	   number. */
	switch (res->bus.type) {
	case SENSORS_BUS_TYPE_I2C:
		if (!strncmp(name, "*-", 2)) {
			res->bus.nr = SENSORS_BUS_NR_ANY;
			name += 2;
			break;
		}

		res->bus.nr = strtoul(name, &dash, 10);
		if (*name == '\0' || *dash != '-' || res->bus.nr < 0)
			goto ERROR;
		name = dash + 1;
		break;
	default:
		res->bus.nr = SENSORS_BUS_NR_ANY;
	}

	/* Last part is the chip address, or "*" for any address. */
	if (!strcmp(name, "*")) {
		res->addr = SENSORS_CHIP_NAME_ADDR_ANY;
	} else {
		res->addr = strtoul(name, &dash, 16);
		if (*name == '\0' || *dash != '\0' || res->addr < 0)
			goto ERROR;
	}

	return 0;

ERROR:
	free(res->prefix);
	return -SENSORS_ERR_CHIP_NAME;
}

int sensors_snprintf_chip_name(char *str, size_t size,
			       const sensors_chip_name *chip)
{
	if (sensors_chip_name_has_wildcards(chip))
		return -SENSORS_ERR_WILDCARDS;

	switch (chip->bus.type) {
	case SENSORS_BUS_TYPE_ISA:
		return snprintf(str, size, "%s-isa-%04x", chip->prefix,
				chip->addr);
	case SENSORS_BUS_TYPE_PCI:
		return snprintf(str, size, "%s-pci-%04x", chip->prefix,
				chip->addr);
	case SENSORS_BUS_TYPE_I2C:
		return snprintf(str, size, "%s-i2c-%hd-%02x", chip->prefix,
				chip->bus.nr, chip->addr);
	}

	return -SENSORS_ERR_CHIP_NAME;
}

int sensors_parse_i2cbus_name(const char *name, int *res)
{
	char *endptr;

	if (strncmp(name, "i2c-", 4)) {
		return -SENSORS_ERR_BUS_NAME;
	}
	name += 4;
	*res = strtoul(name, &endptr, 10);
	if (*name == '\0' || *endptr != '\0' || *res < 0)
		return -SENSORS_ERR_BUS_NAME;
	return 0;
}

int sensors_substitute_chip(sensors_chip_name *name, int lineno)
{
	int i, j;
	for (i = 0; i < sensors_config_busses_count; i++)
		if (name->bus.type == SENSORS_BUS_TYPE_I2C &&
		    sensors_config_busses[i].number == name->bus.nr)
			break;

	if (i == sensors_config_busses_count) {
		sensors_parse_error("Undeclared i2c bus referenced", lineno);
		name->bus.nr = sensors_proc_bus_count;
		return -SENSORS_ERR_BUS_NAME;
	}

	/* Compare the adapter names */
	for (j = 0; j < sensors_proc_bus_count; j++) {
		if (!strcmp(sensors_config_busses[i].adapter,
			    sensors_proc_bus[j].adapter)) {
			name->bus.nr = sensors_proc_bus[j].number;
			return 0;
		}
	}

	/* We did not find anything. sensors_proc_bus_count is not
	   a valid bus number, so it will never be matched. Good. */
	name->bus.nr = sensors_proc_bus_count;
	return 0;
}

int sensors_substitute_busses(void)
{
	int err, i, j, lineno;
	sensors_chip_name_list *chips;
	int res = 0;

	for (i = 0; i < sensors_config_chips_count; i++) {
		lineno = sensors_config_chips[i].lineno;
		chips = &sensors_config_chips[i].chips;
		for (j = 0; j < chips->fits_count; j++) {
			/* We can only substitute if a specific bus number
			   is given. */
			if (chips->fits[j].bus.nr == SENSORS_BUS_NR_ANY)
				continue;

			err = sensors_substitute_chip(&chips->fits[j], lineno);
			if (err)
				res = err;
		}
	}
	return res;
}
