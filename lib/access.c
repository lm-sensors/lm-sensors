/*
    access.c - Part of libsensors, a Linux library for reading sensor data.
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

#include <stddef.h>
#include <string.h>
#include "access.h"
#include "sensors.h"
#include "data.h"
#include "error.h"
#include "proc.h"

/* Compare two chips name descriptions, to see whether they could match.
   Return 0 if it does not match, return 1 if it does match. */
int sensors_match_chip(sensors_chip_name chip1, sensors_chip_name chip2)
{
  if ((chip1.prefix != SENSORS_CHIP_NAME_PREFIX_ANY) &&
      (chip2.prefix != SENSORS_CHIP_NAME_PREFIX_ANY) &&
      strcmp(chip1.prefix,chip2.prefix))
    return 0;
  if ((chip1.bus != SENSORS_CHIP_NAME_BUS_ANY) && 
      (chip2.bus != SENSORS_CHIP_NAME_BUS_ANY) &&
      (chip1.bus != chip2.bus)) {
    if ((chip1.bus == SENSORS_CHIP_NAME_BUS_ISA) ||
        (chip2.bus == SENSORS_CHIP_NAME_BUS_ISA))
      return 0;
    if ((chip1.bus != SENSORS_CHIP_NAME_BUS_ANY_I2C) &&
        (chip2.bus != SENSORS_CHIP_NAME_BUS_ANY_I2C))
      return 0;
  }
  if ((chip1.addr != chip2.addr) &&
      (chip1.addr != SENSORS_CHIP_NAME_ADDR_ANY) &&
      (chip2.addr != SENSORS_CHIP_NAME_ADDR_ANY))
    return 0;
  return 1;
}

/* Returns, one by one, a pointer to all sensor_chip structs of the
   config file which match with the given chip name. Last should be
   the value returned by the last call, or NULL if this is the first
   call. Returns NULL if no more matches are found. Do not modify
   the struct the return value points to! 
   Note that this visits the list of chips from last to first. Usually,
   you want the match that was latest in the config file. */
sensors_chip *sensors_for_all_config_chips(sensors_chip_name chip_name, 
                                           sensors_chip *last)
{
  int nr,i;
  sensors_chip_name_list chips;

  for (nr = last?(last-sensors_config_chips)-1:sensors_config_chips_count-1; 
       nr >= 0; nr--) {
    chips = sensors_config_chips[nr].chips;
    for (i =0; i < chips.fits_count; i++) {
      if (sensors_match_chip(chips.fits[i],chip_name))
        return sensors_config_chips+nr;
    }
  }
  return NULL;
}

/* Look up a resource in the intern chip list, and return a pointer to it. 
   Do not modify the struct the return value points to! Returns NULL if 
   not found.*/
sensors_chip_feature *sensors_lookup_feature_nr(const char *prefix, int feature)
{
  int i,j;
  sensors_chip_feature *features;
  for (i = 0; sensors_chip_features_list[i].prefix; i++)
    if (!strcmp(sensors_chip_features_list[i].prefix,prefix)) {
      features = sensors_chip_features_list[i].feature;
      for (j=0;  features[j].name; j++) 
        if (features[j].number == feature)
          return features + j;
    }
  return NULL;
}

/* Look up a resource in the intern chip list, and return a pointer to it. 
   Do not modify the struct the return value points to! Returns NULL if 
   not found.*/
sensors_chip_feature *sensors_lookup_feature_name(const char *prefix,
                                                  const char *feature)
{
  int i,j;
  sensors_chip_feature *features;
  for (i = 0; sensors_chip_features_list[i].prefix; i++)
    if (!strcmp(sensors_chip_features_list[i].prefix,prefix)) {
      features = sensors_chip_features_list[i].feature;
      for (j=0;  features[j].name; j++) 
        if (!strcmp(features[j].name,feature))
          return features + j;
    }
  return NULL;
}


/* Check whether the chip name is an 'absolute' name, which can only match
   one chip, or whether it has wildcards. Returns 0 if it is absolute, 1
   if there are wildcards. */
int sensors_chip_name_has_wildcards(sensors_chip_name chip)
{
  if ((chip.prefix == SENSORS_CHIP_NAME_PREFIX_ANY) ||
      (chip.bus == SENSORS_CHIP_NAME_BUS_ANY) ||
      (chip.bus == SENSORS_CHIP_NAME_BUS_ANY_I2C) ||
      (chip.bus == SENSORS_CHIP_NAME_ADDR_ANY))
    return 1;
  else
    return 0;
}

/* Look up the label which belongs to this chip. Note that chip should not
   contain wildcard values! *result is newly allocated (free it yourself).
   This function will return 0 on success, and <0 on failure.  */
int sensors_get_label(sensors_chip_name name, int feature, char **result)
{
  sensors_chip *chip;
  sensors_chip_feature *featureptr;
  int i;

  if (sensors_chip_name_has_wildcards(name))
    return -SENSORS_ERR_WILDCARDS;
  if (! (featureptr = sensors_lookup_feature_nr(name.prefix,feature)))
    return -SENSORS_ERR_NO_ENTRY;
  for (chip = NULL; (chip = sensors_for_all_config_chips(name,chip));)
    for (i = 0; i < chip->labels_count; i++)
      if (!strcmp(featureptr->name, chip->labels[i].name)) {
        if (! (*result = strdup(chip->labels[i].name)))
          sensors_fatal_error("sensors_get_label","Allocating label text");
        return 0;
      }
  if (! (*result = strdup(featureptr->name)))
    sensors_fatal_error("sensors_get_label","Allocating label text");
  return 0;
}

/* Read the value of a feature of a certain chip. Note that chip should not
   contain wildcard values! This function will return 0 on success, and <0
   on failure.  */
int sensors_get_feature(sensors_chip_name name, int feature, double *result)
{
  sensors_chip_feature *featureptr;
  sensors_chip *chip;
  sensors_expr *expr = NULL;
  double val;
  int res,i;

  if (sensors_chip_name_has_wildcards(name))
    return -SENSORS_ERR_WILDCARDS;
  if (! (featureptr = sensors_lookup_feature_nr(name.prefix,feature)))
    return -SENSORS_ERR_NO_ENTRY;
  if (! (featureptr->mode && SENSORS_R))
    return -SENSORS_ERR_ACCESS;
  for (chip = NULL; !expr && (chip = sensors_for_all_config_chips(name,chip));)
    for (i = 0; !expr && (i < chip->computes_count); i++)
      if (!strcmp(featureptr->name,chip->computes->name))
        expr = chip->computes->from_proc;
  if (sensors_read_proc(name,feature,&val))
    return -SENSORS_ERR_PROC;
  if (! expr)
    *result = val;
  else if ((res = sensors_eval_expr(expr,val,result)))
    return res;
  return 0;
}
      
/* Set the value of a feature of a certain chip. Note that chip should not
   contain wildcard values! This function will return 0 on success, and <0
   on failure.  */
int sensors_set_value(sensors_chip_name name, int feature, double value)
{
  sensors_chip_feature *featureptr;
  sensors_chip *chip;
  sensors_expr *expr = NULL;
  int i,res;

  if (sensors_chip_name_has_wildcards(name))
    return -SENSORS_ERR_WILDCARDS;
  if (! (featureptr = sensors_lookup_feature_nr(name.prefix,feature)))
    return -SENSORS_ERR_NO_ENTRY;
  if (! (featureptr->mode && SENSORS_W))
    return -SENSORS_ERR_ACCESS;
  for (chip = NULL; !expr && (chip = sensors_for_all_config_chips(name,chip));)
    for (i = 0; !expr && (i < chip->computes_count); i++)
      if (!strcmp(featureptr->name,chip->computes->name))
        expr = chip->computes->to_proc;
  if (expr)
    if ((res = sensors_eval_expr(expr,value,&value)))
      return res;
  if (sensors_write_proc(name,feature,value))
    return -SENSORS_ERR_PROC;
  return 0;
}
