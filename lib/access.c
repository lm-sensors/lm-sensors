/*
    access.c - Part of libsensors, a Linux library for reading sensor data.
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
#include <math.h>
#include "access.h"
#include "sensors.h"
#include "data.h"
#include "error.h"
#include "proc.h"
#include "general.h"

static int sensors_do_this_chip_sets(sensors_chip_name name);

/* Compare two chips name descriptions, to see whether they could match.
   Return 0 if it does not match, return 1 if it does match. */
int sensors_match_chip(sensors_chip_name chip1, sensors_chip_name chip2)
{
  if ((chip1.prefix != SENSORS_CHIP_NAME_PREFIX_ANY) &&
      (chip2.prefix != SENSORS_CHIP_NAME_PREFIX_ANY) &&
      strcasecmp(chip1.prefix,chip2.prefix))
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
                                           const sensors_chip *last)
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
const sensors_chip_feature *sensors_lookup_feature_nr(const char *prefix, 
                                                      int feature)
{
  int i,j;
  const sensors_chip_feature *features;
  for (i = 0; sensors_chip_features_list[i].prefix; i++)
    if (!strcasecmp(sensors_chip_features_list[i].prefix,prefix)) {
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
const sensors_chip_feature *sensors_lookup_feature_name(const char *prefix,
                                                        const char *feature)
{
  int i,j;
  const sensors_chip_feature *features;
  for (i = 0; sensors_chip_features_list[i].prefix; i++)
    if (!strcasecmp(sensors_chip_features_list[i].prefix,prefix)) {
      features = sensors_chip_features_list[i].feature;
      for (j=0;  features[j].name; j++) 
        if (!strcasecmp(features[j].name,feature))
          return features + j;
    }
  return NULL;
}


/* Check whether the chip name is an 'absolute' name, which can only match
   one chip, or whether it has wildcards. Returns 0 if it is absolute, 1
   if there are wildcards. */
int sensors_chip_name_has_wildcards(sensors_chip_name chip)
{
  extern int foundsysfs;

  if(foundsysfs) {
    if (chip.prefix == SENSORS_CHIP_NAME_PREFIX_ANY)
      return 1;
    else
      return 0;
  }     
  if ((chip.prefix == SENSORS_CHIP_NAME_PREFIX_ANY) ||
      (chip.bus == SENSORS_CHIP_NAME_BUS_ANY) ||
      (chip.bus == SENSORS_CHIP_NAME_BUS_ANY_I2C) ||
      (chip.addr == SENSORS_CHIP_NAME_ADDR_ANY))
    return 1;
  else
    return 0;
}

/* Look up the label which belongs to this chip. Note that chip should not
   contain wildcard values! *result is newly allocated (free it yourself).
   This function will return 0 on success, and <0 on failure.  
   Changed behaviour as of libsensors.so.1: now we actually take notice
   of the logical class, as documented, but not done before! */
int sensors_get_label(sensors_chip_name name, int feature, char **result)
{
  const sensors_chip *chip;
  const sensors_chip_feature *featureptr;
  const sensors_chip_feature *alt_featureptr;
  int i;

  *result=NULL;
  if (sensors_chip_name_has_wildcards(name))
    return -SENSORS_ERR_WILDCARDS;
  if (! (featureptr = sensors_lookup_feature_nr(name.prefix,feature)))
    return -SENSORS_ERR_NO_ENTRY;
  if (featureptr->logical_mapping == SENSORS_NO_MAPPING)
    alt_featureptr = NULL;
  else if (! (alt_featureptr =
                   sensors_lookup_feature_nr(name.prefix,
                                             featureptr->logical_mapping)))
    return -SENSORS_ERR_NO_ENTRY;
  for (chip = NULL; (chip = sensors_for_all_config_chips(name,chip));)
    for (i = 0; i < chip->labels_count; i++)
      if (!strcasecmp(featureptr->name, chip->labels[i].name)) {
        if (*result)
          free(*result);
        if (! (*result = strdup(chip->labels[i].value)))
          sensors_fatal_error("sensors_get_label","Allocating label text");
        return 0; /* Exact match always overrules! */
      } else if (alt_featureptr && 
                 !strcasecmp(alt_featureptr->name, chip->labels[i].name)) {
        if (*result)
          free(*result);
        if (! (*result = strdup(chip->labels[i].value)))
          sensors_fatal_error("sensors_get_label","Allocating label text");
      }
  if (! (*result = strdup(featureptr->name)))
    sensors_fatal_error("sensors_get_label","Allocating label text");
  return 0;
}

int sensors_get_ignored(sensors_chip_name name, int feature)
{
  const sensors_chip *chip;
  const sensors_chip_feature *featureptr;
  const sensors_chip_feature *alt_featureptr;
  int i,res;

  res = 1; /* Default: valid */
  if (sensors_chip_name_has_wildcards(name))
    return -SENSORS_ERR_WILDCARDS;
  if (! (featureptr = sensors_lookup_feature_nr(name.prefix,feature)))
    return -SENSORS_ERR_NO_ENTRY;
  if (featureptr->logical_mapping == SENSORS_NO_MAPPING)
    alt_featureptr = NULL;
  else if (! (alt_featureptr =
                   sensors_lookup_feature_nr(name.prefix,
                                             featureptr->logical_mapping)))
    return -SENSORS_ERR_NO_ENTRY;
  for (chip = NULL; (chip = sensors_for_all_config_chips(name,chip));)
    for (i = 0; i < chip->ignores_count; i++)
      if (!strcasecmp(featureptr->name, chip->ignores[i].name))
        return 0; /* Exact match always overrules! */
      else if (alt_featureptr &&
               !strcasecmp(alt_featureptr->name, chip->ignores[i].name))
        res = 0;
  return res;
}

/* Read the value of a feature of a certain chip. Note that chip should not
   contain wildcard values! This function will return 0 on success, and <0
   on failure. */
int sensors_get_feature(sensors_chip_name name, int feature, double *result)
{
  const sensors_chip_feature *main_feature;
  const sensors_chip_feature *alt_feature;
  const sensors_chip *chip;
  const sensors_expr *expr = NULL;
  double val;
  int res,i;
  int final_expr=0;

  if (sensors_chip_name_has_wildcards(name))
    return -SENSORS_ERR_WILDCARDS;
  if (! (main_feature = sensors_lookup_feature_nr(name.prefix,feature)))
    return -SENSORS_ERR_NO_ENTRY;
  if (main_feature->compute_mapping == SENSORS_NO_MAPPING)
    alt_feature = NULL;
  else if (! (alt_feature =
                   sensors_lookup_feature_nr(name.prefix,
                                             main_feature->compute_mapping)))
    return -SENSORS_ERR_NO_ENTRY;
  if (! (main_feature->mode & SENSORS_MODE_R))
    return -SENSORS_ERR_ACCESS_R;
  for (chip = NULL; !expr && (chip = sensors_for_all_config_chips(name,chip));)
    for (i = 0; !final_expr && (i < chip->computes_count); i++) {
      if (!strcasecmp(main_feature->name,chip->computes[i].name)) {
        expr = chip->computes[i].from_proc;
        final_expr = 1;
      } else if (alt_feature && 
                 !strcasecmp(alt_feature->name,chip->computes[i].name))
        expr = chip->computes[i].from_proc;
    }
  if (sensors_read_proc(name,feature,&val))
    return -SENSORS_ERR_PROC;
  if (! expr)
    *result = val;
  else if ((res = sensors_eval_expr(name,expr,val,result)))
    return res;
  return 0;
}
      
/* Set the value of a feature of a certain chip. Note that chip should not
   contain wildcard values! This function will return 0 on success, and <0
   on failure. */
int sensors_set_feature(sensors_chip_name name, int feature, double value)
{
  const sensors_chip_feature *main_feature;
  const sensors_chip_feature *alt_feature;
  const sensors_chip *chip;
  const sensors_expr *expr = NULL;
  int i,res;
  int final_expr=0;
  double to_write;

  if (sensors_chip_name_has_wildcards(name))
    return -SENSORS_ERR_WILDCARDS;
  if (! (main_feature = sensors_lookup_feature_nr(name.prefix,feature)))
    return -SENSORS_ERR_NO_ENTRY;
  if (main_feature->compute_mapping == SENSORS_NO_MAPPING)
    alt_feature = NULL;
  else if (! (alt_feature =
                   sensors_lookup_feature_nr(name.prefix,
                                             main_feature->compute_mapping)))
    return -SENSORS_ERR_NO_ENTRY;
  if (! (main_feature->mode & SENSORS_MODE_W))
    return -SENSORS_ERR_ACCESS_W;
  for (chip = NULL; !expr && (chip = sensors_for_all_config_chips(name,chip));)
    for (i = 0; !final_expr && (i < chip->computes_count); i++)
      if (!strcasecmp(main_feature->name,chip->computes[i].name)) {
        expr = chip->computes->to_proc;
        final_expr = 1;
      } else if (alt_feature && 
                 !strcasecmp(alt_feature->name,chip->computes[i].name))
        expr = chip->computes[i].to_proc;

  to_write = value;
  if (expr)
    if ((res = sensors_eval_expr(name,expr,value,&to_write)))
      return res;
  if (sensors_write_proc(name,feature,to_write))
    return -SENSORS_ERR_PROC;
  return 0;
}

const sensors_chip_name *sensors_get_detected_chips (int *nr)
{
  const sensors_chip_name *res;
  res =  *nr >= sensors_proc_chips_count?NULL:&sensors_proc_chips[*nr].name;
  (*nr)++;
  return res;
}

const char *sensors_get_adapter_name(int bus_nr)
{
  int i;

  if (bus_nr == SENSORS_CHIP_NAME_BUS_ISA)
    return "ISA adapter";
  if (bus_nr == SENSORS_CHIP_NAME_BUS_DUMMY)
    return "Dummy adapter";
  for (i=0; i < sensors_proc_bus_count; i++)
    if (sensors_proc_bus[i].number == bus_nr)
      return sensors_proc_bus[i].adapter;
  return NULL;
}

const char *sensors_get_algorithm_name(int bus_nr)
{
  int i;

  if (bus_nr == SENSORS_CHIP_NAME_BUS_ISA)
    return "ISA algorithm";
  if (bus_nr == SENSORS_CHIP_NAME_BUS_DUMMY)
    return "Dummy algorithm";
  for (i=0; i < sensors_proc_bus_count; i++)
    if (sensors_proc_bus[i].number == bus_nr)
      return sensors_proc_bus[i].algorithm;
  return NULL;
}

/* nr1-1 is the last main feature found; nr2-1 is the last subfeature found */
const sensors_feature_data *sensors_get_all_features (sensors_chip_name name, 
                                                      int *nr1, int*nr2)
{
  sensors_chip_feature *feature_list;
  int i;

  for (i = 0; sensors_chip_features_list[i].prefix; i++)
    if (!strcasecmp(sensors_chip_features_list[i].prefix,name.prefix)) {
      feature_list=sensors_chip_features_list[i].feature;
      if (!*nr1 && !*nr2) { /* Return the first entry */
        if (!feature_list[0].name) /* The list may be empty */
          return NULL;
        *nr1 = *nr2 = 1;
        return (sensors_feature_data *) (feature_list + 0);
      }
      for ((*nr2)++; feature_list[*nr2-1].name; (*nr2)++) 
        if (feature_list[*nr2-1].logical_mapping == feature_list[*nr1-1].number)
          return (sensors_feature_data *) (feature_list + *nr2 - 1);
      for ((*nr1)++; feature_list[*nr1-1].name && 
                  (feature_list[*nr1-1].logical_mapping != SENSORS_NO_MAPPING);
           (*nr1)++);
      *nr2 = *nr1;
      if (! feature_list[*nr1-1].name)
        return NULL;
      return (sensors_feature_data *) (feature_list + *nr1 - 1);
    }
  return NULL;
}

int sensors_eval_expr(sensors_chip_name chipname, const sensors_expr *expr,
                      double val, double *result)
{
  double res1,res2;
  int res;
  const sensors_chip_feature *feature;

  if (expr->kind == sensors_kind_val) {
    *result = expr->data.val;
    return 0;
  }
  if (expr->kind == sensors_kind_source) {
    *result = val;
    return 0;
  }
  if (expr->kind == sensors_kind_var) {
    if (! (feature = sensors_lookup_feature_name(chipname.prefix,
                                                expr->data.var)))
      return SENSORS_ERR_NO_ENTRY;
    if (! (res = sensors_get_feature(chipname,feature->number,result)))
      return res;
    return 0;
  }
  if ((res = sensors_eval_expr(chipname,expr->data.subexpr.sub1,val,&res1)))
    return res;
  if (expr->data.subexpr.sub2 &&
      (res = sensors_eval_expr(chipname,expr->data.subexpr.sub2,val,&res2)))
    return res;
  switch(expr->data.subexpr.op) {
  case sensors_add:
    *result = res1 + res2;
    return 0;
  case sensors_sub:
    *result = res1 - res2;
    return 0;
  case sensors_multiply:
    *result = res1 * res2;
    return 0;
  case sensors_divide:
    if (res2 == 0.0)
      return -SENSORS_ERR_DIV_ZERO;
    *result = res1 / res2;
    return 0;
  case sensors_negate:
    *result = -res1;
    return 0;
  case sensors_exp:
    *result = exp(res1);
    return 0;
  case sensors_log:
    if (res1 < 0.0)
      return -SENSORS_ERR_DIV_ZERO;
    *result = log(res1);
    return 0;
  }
  return 0;
}

/* Execute all set statements for this particular chip. The chip may not 
   contain wildcards!  This function will return 0 on success, and <0 on 
   failure. */
int sensors_do_this_chip_sets(sensors_chip_name name)
{
  sensors_chip *chip;
  double value;
  int i, j;
  int err = 0, res;
  const sensors_chip_feature *feature;
  int *feature_list = NULL;
  int feature_count = 0;
  int feature_max = 0;
  int feature_nr;

  for(chip = NULL; (chip = sensors_for_all_config_chips(name,chip));)
    for(i = 0; i < chip->sets_count; i++) {
      feature = sensors_lookup_feature_name(name.prefix,chip->sets[i].name);
      if (! feature) {
        sensors_parse_error("Unknown feature name",chip->sets[i].lineno);
        err = SENSORS_ERR_NO_ENTRY;
        continue;
      }
      feature_nr = feature->number;

      /* Check whether we already set this feature */
      for(j = 0; j < feature_count; j++)
        if(feature_list[j] == feature_nr)
          break;
      if (j != feature_count)
        continue;
      sensors_add_array_el(&feature_nr,&feature_list,&feature_count,
                        &feature_max, sizeof(int));

      res = sensors_eval_expr(name,chip->sets[i].value,0,&value);
      if (res) {
        sensors_parse_error("Parsing expression",chip->sets[i].lineno);
        err = res;
        continue;
      }
      if ((res = sensors_set_feature(name,feature_nr,value))) {
        err = res;
        continue;
      }
    }
  free(feature_list);
  return err;
}

/* Execute all set statements for this particular chip. The chip may contain
   wildcards!  This function will return 0 on success, and <0 on failure. */
int sensors_do_chip_sets(sensors_chip_name name)
{
  int nr,this_res;
  const sensors_chip_name *found_name;
  int res = 0;

  for (nr = 0; (found_name = sensors_get_detected_chips(&nr));)
    if (sensors_match_chip(name,*found_name)) {
      this_res = sensors_do_this_chip_sets(*found_name);
      if (! res)
        res = this_res;
    }
  return res;
}

/* Execute all set statements for all detected chips. This is the same as
   calling sensors_do_chip_sets with an all wildcards chip name */
int sensors_do_all_sets(void)
{
  sensors_chip_name name = { SENSORS_CHIP_NAME_PREFIX_ANY, 
                             SENSORS_CHIP_NAME_BUS_ANY,
                             SENSORS_CHIP_NAME_ADDR_ANY };
  return sensors_do_chip_sets(name);
}

