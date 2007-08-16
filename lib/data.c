/*
    data.c - Part of libsensors, a Linux library for reading sensor data.
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

/* Wow, this must be one of the ugliest functions I have ever written.
   The idea is that it parses a chip name. These are valid names:

     lm78-i2c-10-5e		*-i2c-10-5e
     lm78-i2c-10-*		*-i2c-10-*
     lm78-i2c-*-5e		*-i2c-*-5e
     lm78-i2c-*-*		*-i2c-*-*
     lm78-isa-10dd		*-isa-10dd
     lm78-isa-*			*-isa-*
     lm78-*			*-*
     				*
   Here 'lm78' can be any prefix. To complicate matters, such a prefix
   can also contain dashes (like lm78-j, for example!). 'i2c' and 'isa' are
   literal strings, just like all dashes '-' and wildcards '*'. '10' can
   be any decimal i2c bus number. '5e' can be any hexadecimal i2c device
   address, and '10dd' any hexadecimal isa address.

   If '*' is used in prefixes, together with dashes, ambigious parses are
   introduced. In that case, the prefix is kept as small as possible.

   The 'prefix' part in the result is freshly allocated. All old contents
   of res is overwritten. res itself is not allocated. In case of an error
   return (ie. != 0), res is undefined, but all allocations are undone.

   Don't tell me there are bugs in here, because I'll start screaming :-)
*/

int sensors_parse_chip_name(const char *orig_name, sensors_chip_name *res)
{
  char *part2, *part3, *part4;
  char *name = strdup(orig_name);
  char *endptr;

  if (! name)
    sensors_fatal_error("sensors_parse_chip_name","Allocating new name");
  /* First split name in upto four pieces. */
  if ((part4 = strrchr(name,'-')))
    *part4++ = '\0';
  if ((part3 = strrchr(name,'-')))
    *part3++ = '\0';
  if ((part2 = strrchr(name,'-'))) 
    *part2++ = '\0';

  /* No dashes found? */
  if (! part4) {
    if (!strcmp(name,"*")) {
      res->prefix = SENSORS_CHIP_NAME_PREFIX_ANY;
      res->bus = SENSORS_CHIP_NAME_BUS_ANY;
      res->addr = SENSORS_CHIP_NAME_ADDR_ANY;
      goto SUCCES;
    } else 
      goto ERROR;
  }

  /* At least one dash found. Now part4 is either '*', or an address */
  if (!strcmp(part4,"*"))
    res->addr = SENSORS_CHIP_NAME_ADDR_ANY;
  else {
    res->addr = strtoul(part4, &endptr, 16);
    if (*part4 == '\0' || *endptr != '\0' || res->addr < 0)
      goto ERROR;
  }

  /* OK. So let's look at part3. It must either be the number of the
     i2c bus (and then part2 *must* be "i2c"), or it must be "isa",
     or, if part4 was "*", it belongs to 'prefix'. Or no second dash
     was found at all, of course. */
  if (! part3) {
    if (res->addr == SENSORS_CHIP_NAME_ADDR_ANY) {
      res->bus = SENSORS_CHIP_NAME_BUS_ANY;
    } else
      goto ERROR;
  } else if (!strcmp(part3,"isa")) {
    res->bus = SENSORS_CHIP_NAME_BUS_ISA;
    if (part2)
      *(part2-1) = '-';
  } else if (!strcmp(part3,"pci")) {
    res->bus = SENSORS_CHIP_NAME_BUS_PCI;
    if (part2)
      *(part2-1) = '-';
  } else if (part2 && !strcmp(part2,"i2c") && !strcmp(part3,"*"))
    res->bus = SENSORS_CHIP_NAME_BUS_ANY_I2C;
  else if (part2 && !strcmp(part2,"i2c")) {
    res->bus = strtoul(part3, &endptr, 10);
    if (*part3 == '\0' || *endptr != '\0' || res->bus < 0)
      goto ERROR;
  } else if (res->addr == SENSORS_CHIP_NAME_ADDR_ANY) {
    res->bus = SENSORS_CHIP_NAME_BUS_ANY;
    if (part2)
      *(part2-1) = '-';
    *(part3-1) = '-';
  } else
    goto ERROR;
    
  if (!strcmp(name,"*"))
    res->prefix = SENSORS_CHIP_NAME_PREFIX_ANY;
  else if (! (res->prefix = strdup(name)))
    sensors_fatal_error("sensors_parse_chip_name","Allocating new name");
  goto SUCCES;

SUCCES:
  free(name);
  return 0;

ERROR:
  free(name);
  return -SENSORS_ERR_CHIP_NAME;
}

int sensors_snprintf_chip_name(char *str, size_t size,
			       const sensors_chip_name *chip)
{
	if (sensors_chip_name_has_wildcards(chip))
		return -SENSORS_ERR_WILDCARDS;

	switch (chip->bus) {
	case SENSORS_CHIP_NAME_BUS_ISA:
		return snprintf(str, size, "%s-isa-%04x", chip->prefix,
				chip->addr);
	case SENSORS_CHIP_NAME_BUS_PCI:
		return snprintf(str, size, "%s-pci-%04x", chip->prefix,
				chip->addr);
	default:
		return snprintf(str, size, "%s-i2c-%d-%02x", chip->prefix,
				chip->bus, chip->addr);
	}
}

int sensors_parse_i2cbus_name(const char *name, int *res)
{
  char *endptr;

  if (strncmp(name,"i2c-",4)) {
    return -SENSORS_ERR_BUS_NAME;
  }
  name += 4;
  *res = strtoul(name, &endptr, 10);
  if (*name == '\0' || *endptr != '\0' || *res < 0)
    return -SENSORS_ERR_BUS_NAME;
  return 0;
}


int sensors_substitute_chip(sensors_chip_name *name,int lineno)
{
  int i,j;
  for (i = 0; i < sensors_config_busses_count; i++)
    if (sensors_config_busses[i].number == name->bus)
      break;

  if (i == sensors_config_busses_count) {
    sensors_parse_error("Undeclared i2c bus referenced",lineno);
    name->bus = sensors_proc_bus_count;
    return -SENSORS_ERR_BUS_NAME;
  }

  /* Compare the adapter names */
  for (j = 0; j < sensors_proc_bus_count; j++) {
    if (!strcmp(sensors_config_busses[i].adapter,
                sensors_proc_bus[j].adapter)) {
      name->bus = sensors_proc_bus[j].number;
      return 0;
    }
  }

  /* We did not find anything. sensors_proc_bus_count is not a valid
     bus number, so it will never be matched. Good. */
  name->bus = sensors_proc_bus_count;
  return 0;
}

      
int sensors_substitute_busses(void)
{
  int err,i,j,lineno;
  sensors_chip_name_list *chips;
  int res=0;
  
  for(i = 0; i < sensors_config_chips_count; i++) {
    lineno = sensors_config_chips[i].lineno;
    chips = &sensors_config_chips[i].chips;
    for(j = 0; j < chips->fits_count; j++)
      if ((chips->fits[j].bus != SENSORS_CHIP_NAME_BUS_ISA) &&
          (chips->fits[j].bus != SENSORS_CHIP_NAME_BUS_PCI) &&
          (chips->fits[j].bus != SENSORS_CHIP_NAME_BUS_ANY) &&
          (chips->fits[j].bus != SENSORS_CHIP_NAME_BUS_ANY_I2C))
        if ((err = sensors_substitute_chip(chips->fits+j, lineno)))
          res = err;
  }
  return res;
}
