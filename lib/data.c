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

#include "error.h"
#include "data.h"
#include "sensors.h"
#include "../version.h"

const char *libsensors_version = LM_VERSION;
const char *libsensors_date = LM_DATE;

sensors_chip *sensors_config_chips = NULL;
int sensors_config_chips_count = 0;
int sensors_config_chips_max = 0;

sensors_bus *sensors_config_busses = NULL;
int sensors_config_busses_count = 0;
int sensors_config_busses_max = 0;

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
  int i;

  /* Play it safe */
  res->busname = NULL;
  
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
    if ((strlen(part4) > 4) || (strlen(part4) == 0))
      goto ERROR;
    res->addr = 0;
    for (i = 0; ; i++) { 
      switch (part4[i]) {
      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
        res->addr = res->addr * 16 + part4[i] - '0';
        break;
      case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
        res->addr = res->addr * 16 + part4[i] - 'a' + 10;
        break;
      case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
        res->addr = res->addr * 16 + part4[i] - 'A' + 10;
        break;
      case 0:
        goto DONE;
      default:
        goto ERROR;
      }
    }
DONE:;
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
  } else if (part2 && !strcmp(part2,"i2c") && !strcmp(part3,"*"))
    res->bus = SENSORS_CHIP_NAME_BUS_ANY_I2C;
  else if (part2 && !strcmp(part2,"i2c")) {
    if ((strlen(part3) > 3) || (strlen(part3) == 0))
      goto ERROR;
    res->bus = 0;
    for (i = 0; ; i++) { 
      switch (part3[i]) {
      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
        res->bus = res->bus * 10 + part3[i] - '0';
        break;
      case 0:
        goto DONE2;
      default:
        goto ERROR;
      }
    }
DONE2:;
  } else if (res->addr == SENSORS_CHIP_NAME_ADDR_ANY) {
    res->bus = SENSORS_CHIP_NAME_BUS_ANY;
    if (part2)
      *(part2-1) = '-';
    *(part3-1) = '-';
  } else if(part3 && part4) {
    res->bus = SENSORS_CHIP_NAME_BUS_DUMMY;
    if (! (res->busname = strdup(part3)))
      sensors_fatal_error("sensors_parse_chip_name","Allocating new busname");
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

int sensors_parse_i2cbus_name(const char *name, int *res)
{
  int i;

  if (! strcmp(name,"isa")) {
    *res = SENSORS_CHIP_NAME_BUS_ISA;
    return 0;
  }
  if (strncmp(name,"i2c-",4)) {
    *res = SENSORS_CHIP_NAME_BUS_DUMMY;
    return 0;
  }
  name += 4;
  if ((strlen(name) > 3) || (strlen(name) == 0))
    return -SENSORS_ERR_BUS_NAME;
  *res = 0;
  for (i = 0; ; i++) { 
    switch (name[i]) {
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      *res = *res * 10 + name[i] - '0';
      break;
    case 0:
      return 0;
    default:
      return -SENSORS_ERR_BUS_NAME;
    }
  }
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

  for (j = 0; j < sensors_proc_bus_count; j++) {
    if (!strcmp(sensors_config_busses[i].adapter,
                sensors_proc_bus[j].adapter) &&
        !strcmp(sensors_config_busses[i].algorithm,
                sensors_proc_bus[j].algorithm)) 
      break;
  }

  /* Well, if we did not find anything, j = sensors_proc_bus_count; so if
     we set this chip's bus number to j, it will never be matched. Good. */
  name->bus = j;
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
          (chips->fits[j].bus != SENSORS_CHIP_NAME_BUS_DUMMY) &&
          (chips->fits[j].bus != SENSORS_CHIP_NAME_BUS_ANY) &&
          (chips->fits[j].bus != SENSORS_CHIP_NAME_BUS_ANY_I2C))
        if ((err = sensors_substitute_chip(chips->fits+j, lineno)))
          res = err;
  }
  return res;
}
